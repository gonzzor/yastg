#ifndef HAS_CONSTELLATION_H
#define HAS_CONSTELLATION_H

#define CONSTELLATION_NEIGHBOUR_CHANCE 5
#define CONSTELLATION_MIN_DISTANCE 150
#define CONSTELLATION_RANDOM_DISTANCE 500
#define CONSTELLATION_PHI_RANDOM 1.0
#define CONSTELLATION_MAXNUM 3

void loadconstellations(struct universe *u);
void addconstellation(struct universe *u, char* cname);

#endif
