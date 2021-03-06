#ifndef _HAS_SYSTEM_H
#define _HAS_SYSTEM_H

#include "civ.h"
#include "list.h"
#include "rbtree.h"
#include "ptrlist.h"
#include "universe.h"

struct system {
	char *name;
	struct civ *owner;
	char *gname;
	long x, y;
	struct rb_node x_rbtree;
	unsigned long r;
	double phi;
	int hab;
	unsigned int hablow, habhigh;
	struct ptrlist stars;
	struct ptrlist planets;
	struct ptrlist ports;
	struct ptrlist links;
	struct list_head list;
};

void system_init(struct system *s);
int system_create(struct system *s, char *name);
void system_free(struct system *s);

unsigned long system_distance(const struct system * const a, const struct system * const b);

#endif
