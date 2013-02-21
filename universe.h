#ifndef _HAS_UNIVERSE_H
#define _HAS_UNIVERSE_H

#include "list.h"
#include "rbtree.h"
#include "civ.h"
#include "system.h"
#include "names.h"

struct universe {
	size_t id;			/* ID of the universe (or the game?) */
	char* name;			/* The name of the universe (or the game?) */
	time_t created;			/* When the universe was created */
	unsigned long inhabited_systems;
	struct ptrlist systems;
	struct rb_root x_rbtree;
	struct list_head items;
	struct list_head ports;
	pthread_rwlock_t ports_lock;
	struct list_head port_types;
	struct list_head port_type_names;
	struct list_head planet_types;
	struct list_head ship_types;
	struct list_head ship_type_names;
	struct list_head item_names;
	struct list_head systemnames;
	pthread_rwlock_t systemnames_lock;
	struct list_head planetnames;
	pthread_rwlock_t planetnames_lock;
	struct list_head portnames;
	pthread_rwlock_t portnames_lock;
	struct list_head civs;
	struct list_head list;
	struct name_list avail_port_names;
	struct name_list avail_player_names;
};

extern struct universe univ;

void universe_free(struct universe *u);
struct universe* universe_create();
void universe_init(struct universe *u);
int universe_genesis(struct universe *univ);

unsigned long get_neighbouring_systems(struct ptrlist * const neighbours,
		const struct system * const origin, const long max_distance);

int system_move(struct system * const s, const long x, const long y);
int makeneighbours(struct system *s1, struct system *s2, unsigned long min, unsigned long max);
void linksystems(struct system *s1, struct system *s2);

#endif
