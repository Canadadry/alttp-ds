#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include "rando_world.h"

#include <stdbool.h>
#include <stdio.h>

/* Writes VT's getSpoiler() for the world as it stands, in the oracle's normalized JSON (keys
 * sorted bytewise, PHP pretty-print, final newline). False on a stream error; the caller owns and
 * closes out. */
bool rando_spoiler_write_json(FILE *out, const RandoWorld *world);

#endif
