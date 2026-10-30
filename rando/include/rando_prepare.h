#ifndef RANDO_PREPARE_H
#define RANDO_PREPARE_H

#include "rando_prng.h"
#include "rando_world.h"

/* prepareWorld before its pool getters: completionist's accessibility switch and the goal's Triforce. */
void rando_prepare_goal(RandoWorld *world);

/* prepareWorld after its pool getters: PegasusBoots and RescueZelda into the pre-collected items. */
void rando_prepare_pre_collect(RandoWorld *world);

/* Randomizer::setShops on the draw-free path. Asserts when generic keys or the rupee bow are on:
 * that path draws and is not ported. */
void rando_set_shops(RandoWorld *world);

/* One draw per Medallions location that holds no item yet; a filled one is skipped without a draw. */
void rando_set_medallions(RandoWorld *world, RandoPrng *prng);

/* Randomizer::placeBosses with enemizer.bossShuffle none, the only path the matrix reaches: it draws nothing. */
void rando_place_bosses(RandoWorld *world);

#endif
