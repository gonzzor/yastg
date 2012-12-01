#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <malloc.h>
#include <dlfcn.h>

#include "config.h"
#include "common.h"
#include "log.h"
#include "mtrandom.h"
#include "cli.h"
#include "sarray.h"
#include "ptrlist.h"
#include "server.h"
#include "sector.h"
#include "base.h"
#include "inventory.h"
#include "player.h"
#include "universe.h"
#include "parseconfig.h"
#include "civ.h"
#include "id.h"
#include "names.h"
#include "module.h"

#define PORT "2049"
#define BACKLOG 16

const char* options = "d";
int detached = 0;
LIST_HEAD(cli_root);

extern int sockfd;

struct server {
	pthread_t thread;
	int fd[2];
	int running;
};

static void parseopts(int argc, char **argv)
{
	char c;
	while ((c = getopt(argc, argv, options)) > 0) {
		switch (c) {
		case 'd':
			printf("Detached mode\n");
			detached = 1;
			break;
		default:
			exit(1);
		}
	}
}

static void write_msg(int fd, struct signal *msg, char *msgdata)
{
	if (write(fd, msg, sizeof(*msg)) < 1)
		bug("%s", "server signalling fd is closed");
	if (msg->cnt && write(fd, msgdata, msg->cnt) < 1)
		bug("%s", "server signalling fd is closed");
}

static int cmd_help(void *ptr, char *param)
{
	cli_print_help(stdout, &cli_root);
	return 0;
}

static int cmd_insmod(void *ptr, char *param)
{
	int r;

	if (!param) {
		mprintf("usage: insmod <file name.so>\n");
		return 0;
	}

	r = module_insert(param);
	if (r != 0)
		mprintf("Error inserting module: %s\n",
				(r == MODULE_DL_ERROR ? dlerror() : module_strerror(r))
		       );

	return 0;
}

static int cmd_lsmod(void *ptr, char *param)
{
	struct module *m;

	if (list_empty(&modules_loaded)) {
		mprintf("No modules are currently loaded\n");
		return 0;
	}

	mprintf("%-16s %-8s\n", "Module", "Size");
	list_for_each_entry(m, &modules_loaded, list)
		mprintf("%-16s %-8d\n", m->name, m->size);

	return 0;
}

static int cmd_wall(void *_server, char *message)
{
	struct server *server = _server;
	if (message) {
		struct signal msg = {
			.cnt = strlen(message) + 1,
			.type = MSG_WALL
		};
		write_msg(server->fd[1], &msg, message);
	} else {
		mprintf("usage: wall <message>\n");
	}
	return 0;
}

static int cmd_pause(void *_server, char *param)
{
	struct server *server = _server;
	struct signal msg = {
		.cnt = 0,
		.type = MSG_PAUSE
	};
	write_msg(server->fd[1], &msg, NULL);
	return 0;
}

static int cmd_resume(void *_server, char *param)
{
	struct server *server = _server;
	struct signal msg = {
		.cnt = 0,
		.type = MSG_CONT
	};
	write_msg(server->fd[1], &msg, NULL);
	return 0;
}

static void _cmd_rmmod(struct module *m)
{
	int r;

	r = module_remove(m);
	if (r)
		mprintf("Error removing module: %s\n",
				(r == MODULE_DL_ERROR ? dlerror() : module_strerror(r))
		       );
}

static int cmd_rmmod(void *ptr, char *name)
{
	struct module *m;
	int r;

	if (!name) {
		mprintf("usage: rmmod <module>\n");
		return 0;
	}

	list_for_each_entry(m, &modules_loaded, list) {
		if (strcmp(m->name, name) == 0) {
			_cmd_rmmod(m);
			return 0;
		}
	}

	mprintf("Module %s is not currently loaded\n", name);
	return 0;
}

static int cmd_memstat(void *ptr, char *param)
{
	struct mallinfo minfo = mallinfo();
	mprintf("Memory statistics:\n"
	        "  Memory allocated with sbrk by malloc:           %d bytes\n"
	        "  Number of chunks not in use:                    %d\n"
	        "  Number of chunks allocated with mmap:           %d\n"
	        "  Memory allocated with mmap:                     %d bytes\n"
	        "  Memory occupied by chunks handed out by malloc: %d bytes\n"
	        "  Memory occupied by free chunks:                 %d bytes\n"
	        "  Size of top-most releasable chunk:              %d bytes\n",
		minfo.arena, minfo.ordblks, minfo.hblks, minfo.hblkhd,
		minfo.uordblks, minfo.fordblks, minfo.keepcost);
	return 0;
}

static int cmd_stats(void *ptr, char *param)
{
	mprintf("Statistics:\n"
	        "  Size of universe:          %lu sectors\n"
	        "  Number of users known:     %s\n"
	        "  Number of users connected: %s\n",
		ptrlist_len(&univ->sectors), "FIXME", "FIXME");
	return 0;
}

static int cmd_quit(void *_server, char *param)
{
	struct server *server = _server;
	mprintf("Bye!\n");
	server->running = 0;
	return 0;
}

int main(int argc, char **argv)
{
	char *line = malloc(256); /* FIXME */
	struct configtree *ctree;
	struct civ *cv;
	struct sector *s, *t;
	size_t st, su;
	struct civ *civs = civ_create();
	struct server server;

	/* Open log file */
	log_init();
	log_printfn("main", "YASTG initializing");
	log_printfn("main", "This is %s, built %s %s", PACKAGE_VERSION, __DATE__, __TIME__);

	/* Initialize */
	server.running = 1;
	srand(time(NULL));
	mtrandom_init();
	init_id();

	/* Parse command line options */
	parseopts(argc, argv);

	/* Register server console commands */
	printf("Registering server console commands\n");
	cli_add_cmd(&cli_root, "help", cmd_help, &server, NULL);
	cli_add_cmd(&cli_root, "insmod", cmd_insmod, &server, NULL);
	cli_add_cmd(&cli_root, "lsmod", cmd_lsmod, &server, NULL);
	cli_add_cmd(&cli_root, "wall", cmd_wall, &server, NULL);
	cli_add_cmd(&cli_root, "pause", cmd_pause, &server, NULL);
	cli_add_cmd(&cli_root, "resume", cmd_resume, &server, NULL);
	cli_add_cmd(&cli_root, "rmmod", cmd_rmmod, &server, NULL);
	cli_add_cmd(&cli_root, "stats", cmd_stats, &server, NULL);
	cli_add_cmd(&cli_root, "memstat", cmd_memstat, &server, NULL);
	cli_add_cmd(&cli_root, "quit", cmd_quit, &server, NULL);

	/* Load config files */
	printf("Parsing configuration files\n");
	printf("  civilizations: ");
	civ_load_all(civs);
	printf("done, %lu civs loaded.\n", list_len(&civs->list));
	/* Create universe */
	printf("Creating universe\n");
	univ = universe_create();
	printf("Loading names ... ");
	names_init(&univ->avail_base_names);
	names_init(&univ->avail_player_names);
	names_load(&univ->avail_base_names, "data/placeprefix", "data/placenames", NULL, "data/placesuffix");
	names_load(&univ->avail_player_names, NULL, "data/firstnames", "data/surnames", NULL);
	printf("done.\n");

	universe_init(civs);

	/* Start server thread */
	if (pipe(server.fd) != 0)
		die("%s", "Could not create server pipe");
	if (pthread_create(&server.thread, NULL, server_main, &server.fd[0]) != 0)
		die("%s", "Could not launch server thread");

	mprintf("Welcome to YASTG %s, built %s %s.\n\n", PACKAGE_VERSION, __DATE__, __TIME__);
	mprintf("Universe has %lu sectors in total\n", ptrlist_len(&univ->sectors));

	while (server.running) {
		mprintf("console> ");
		fgets(line, 256, stdin); /* FIXME */
		chomp(line);

		if (strlen(line) > 0 && cli_run_cmd(&cli_root, line) < 0)
			mprintf("Unknown command or syntax error.\n");
	}

	/* Kill server thread, this will also kill all player threads */
	struct signal signal = {
		.cnt = 0,
		.type = MSG_TERM
	};
	if (write(server.fd[1], &signal, sizeof(signal)) < 1)
		bug("%s", "server signalling fd seems closed when sending signal");
	log_printfn("main", "waiting for server to terminate");
	pthread_join(server.thread, NULL);

	/* Destroy all structures and free all memory */

	log_printfn("main", "cleaning up");
	struct list_head *p, *q;
	ptrlist_for_each_entry(s, &univ->sectors, p) {
		printf("Freeing sector %s\n", s->name);
		sector_free(s);
	}
	list_for_each_safe(p, q, &civs->list) {
		cv = list_entry(p, struct civ, list);
		list_del(p);
		civ_free(cv);
	}
	civ_free(civs);

	names_free(&univ->avail_base_names);
	names_free(&univ->avail_player_names);

	id_destroy();
	universe_free(univ);
	free(line);
	cli_tree_destroy(&cli_root);
	log_close();

	return 0;

}
