#include "rando.h"
#include "rando_prepare.h"

#include <assert.h>
#include <stdbool.h>

static const char *const stage_names[RANDO_STAGE_COUNT] = {
	"crystals",
	"getAdvancementItems",
	"setShops",
	"setMedallions",
	"placeBosses",
	"fillPrizes",
	"setFountains",
	"shufflePrizePacks",
	"prepareWorld",
	"fill",
};

const char *rando_stage_name(RandoStage stage)
{
	assert(stage < RANDO_STAGE_COUNT);
	return stage_names[stage];
}

static void observe_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	RandoGen *gen = ctx;

	assert(gen->stage != RANDO_STAGE_NONE && "draw outside any stage: label it with its VT stage");
	gen->draw++;
	if (gen->trace)
		gen->trace(gen->trace_ctx, gen->attempt, gen->draw, stage_names[gen->stage], min, max, r);
}

static int resolve_crystals(RandoGen *gen, RandoCrystals crystals)
{
	if (crystals == RANDO_CRYSTALS_RANDOM)
		return (int)rando_random_int(&gen->prng, 0, 7);
	return (int)crystals;
}

/* Returns false when the attempt failed and the loop must retry; otherwise stores the final status. */
static bool run_attempt(RandoGen *gen, RandoStatus *status)
{
	RandoConfig config;
	int crystals_ganon, crystals_tower;
	RandoVariant variant;

	gen->stage = RANDO_STAGE_NONE;
	gen->draw = 0;
	rando_prng_seed(&gen->prng, rando_splitmix32((uint32_t)(gen->seed + gen->attempt)));

	gen->stage = RANDO_STAGE_CRYSTALS;
	crystals_ganon = resolve_crystals(gen, gen->options.crystals_ganon);
	crystals_tower = resolve_crystals(gen, gen->options.crystals_tower);

	gen->stage = RANDO_STAGE_NONE;

	if (gen->options.state == RANDO_OPTION_STATE_INVERTED || gen->options.state == RANDO_OPTION_STATE_RETRO) {
		*status = RANDO_UNSUPPORTED;
		return true;
	}
	variant = gen->options.state == RANDO_OPTION_STATE_STANDARD ? RANDO_VARIANT_STANDARD : RANDO_VARIANT_OPEN;
	rando_config_from_options(&gen->options, crystals_ganon, crystals_tower, &config);
	rando_world_init(&gen->world, variant, &config);

	gen->stage = RANDO_STAGE_GET_ADVANCEMENT_ITEMS;
	rando_pool_build(&gen->world, &gen->prng, &gen->pools);

	/* Neither stage may call collectItems: VT caches the collectable shop items on the first call. */
	gen->stage = RANDO_STAGE_SET_SHOPS;
	rando_set_shops(&gen->world);
	gen->stage = RANDO_STAGE_SET_MEDALLIONS;
	rando_set_medallions(&gen->world, &gen->prng);
	gen->stage = RANDO_STAGE_NONE;

	*status = RANDO_INCOMPLETE;
	return true;
}

RandoStatus rando_generate(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
			   RandoTraceFn trace, void *trace_ctx)
{
	RandoStatus status;

	gen->options = *options;
	gen->seed = seed;
	gen->trace = trace;
	gen->trace_ctx = trace_ctx;
	gen->prng.observe = observe_draw;
	gen->prng.observe_ctx = gen;
	for (gen->attempt = 0;; gen->attempt++) {
		if (run_attempt(gen, &status))
			return status;
		if (max_attempts != 0 && gen->attempt + 1 >= max_attempts)
			return RANDO_GAVE_UP;
	}
}
