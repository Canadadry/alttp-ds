#ifndef RANDO_H
#define RANDO_H

#include "rando_options.h"
#include "rando_pool.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdint.h>

/* tests/rando/stages.txt's `stage` lines, in order. */
typedef enum RandoStage {
	RANDO_STAGE_CRYSTALS,
	RANDO_STAGE_GET_ADVANCEMENT_ITEMS,
	RANDO_STAGE_SET_SHOPS,
	RANDO_STAGE_SET_MEDALLIONS,
	RANDO_STAGE_PLACE_BOSSES,
	RANDO_STAGE_FILL_PRIZES,
	RANDO_STAGE_SET_FOUNTAINS,
	RANDO_STAGE_SHUFFLE_PRIZE_PACKS,
	RANDO_STAGE_PREPARE_WORLD,
	RANDO_STAGE_FILL,
	RANDO_STAGE_COUNT,
	RANDO_STAGE_NONE = RANDO_STAGE_COUNT,
} RandoStage;

/* Asserts stage < RANDO_STAGE_COUNT. */
const char *rando_stage_name(RandoStage stage);

typedef enum RandoStatus {
	RANDO_COMPLETE,
	/* Every ported stage ran; the next one is not ported yet. */
	RANDO_INCOMPLETE,
	/* The options need a world variant that is not ported. */
	RANDO_UNSUPPORTED,
	RANDO_GAVE_UP,
} RandoStatus;

/* One call per draw; draw restarts at 1 on every attempt. */
typedef void (*RandoTraceFn)(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min,
			     int64_t max, int64_t r);

/* Ids, never pointers, except the caller's trace hook and the PRNG observer rando_generate points
 * back at this struct. */
typedef struct RandoGen {
	RandoOptions options;
	uint32_t seed;
	uint32_t attempt;
	RandoStage stage;
	uint32_t draw;
	RandoTraceFn trace;
	void *trace_ctx;
	RandoPrng prng;
	RandoPools pools;
	RandoWorld world;
} RandoGen;

/* The retry contract: attempt N reseeds with splitmix32(seed + N). max_attempts 0 is unbounded;
 * otherwise RANDO_GAVE_UP once max_attempts attempts have failed. trace may be NULL. gen->attempt
 * is the last attempt run. */
RandoStatus rando_generate(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
			   RandoTraceFn trace, void *trace_ctx);

#endif
