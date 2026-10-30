#ifndef RANDO_POOL_H
#define RANDO_POOL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_POOL_CAPACITY 256

/* prepareWorld's getter order: getDungeonPool, getAdvancementItems, getNiceItems, getItemPool. */
typedef enum RandoPoolList {
	RANDO_POOL_DUNGEON,
	RANDO_POOL_ADVANCEMENT,
	RANDO_POOL_NICE,
	RANDO_POOL_ITEMS,
	RANDO_POOL_LIST_COUNT,
} RandoPoolList;

typedef struct RandoPools {
	uint8_t items[RANDO_POOL_LIST_COUNT][RANDO_POOL_CAPACITY];
	uint16_t count[RANDO_POOL_LIST_COUNT];
} RandoPools;

/* The four lists from the initialized world's derived config, in VT's item order. Asserts a list
 * stays under RANDO_POOL_CAPACITY and that a random bottle only occurs in the advancement list. */
void rando_pool_build(const RandoWorld *world, RandoPrng *prng, RandoPools *pools);

/* VT's World::getBottle: one draw. */
RandoItemId rando_get_bottle(const RandoWorld *world, RandoPrng *prng, bool filled);

#endif
