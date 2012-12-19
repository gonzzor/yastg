#ifndef _HAS_ITEM_H
#define _HAS_ITEM_H

#include "list.h"

struct item {
	char *name;
	int weight;
	struct list_head list;
};

int load_all_items(struct list_head * const root);
void item_free(struct item * const item);

#endif
