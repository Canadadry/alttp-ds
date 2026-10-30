#ifndef RANDO_PREPARE_H
#define RANDO_PREPARE_H

#include "rando_prng.h"
#include "rando_world.h"

/* Randomizer::setShops on the draw-free path. Asserts when generic keys or the rupee bow are on:
 * that path draws and is not ported. */
void rando_set_shops(RandoWorld *world);

/* One draw per Medallions location that holds no item yet; a filled one is skipped without a draw. */
void rando_set_medallions(RandoWorld *world, RandoPrng *prng);

#endif
