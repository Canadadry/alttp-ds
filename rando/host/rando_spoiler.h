#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include "rando_pool.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdio.h>

/* Writes VT's getSpoiler() for the world as it stands, in the oracle's normalized JSON (keys
 * sorted bytewise, PHP pretty-print, final newline). False on a stream error; the caller owns and
 * closes out. */
bool rando_spoiler_write_json(FILE *out, const RandoWorld *world);

/* Writes pool.txt: one `<getTarget()->getName()>:1` line per item, dungeon, advancement, nice, then
 * item pool, in list order. False on a stream error; the caller owns and closes out. */
bool rando_spoiler_write_pool(FILE *out, const RandoPools *pools);

#endif
