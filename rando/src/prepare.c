#include "rando_prepare.h"
#include "rando_pool.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static void place(RandoWorld *world, RandoLocationId location, RandoItemId item)
{
	bool placed = rando_world_set_item(world, location, item);

	assert(placed && "prepareWorld: a location refused its item");
	(void)placed;
}

void rando_prepare_start_equipment(RandoWorld *world)
{
	static const RandoItemId equipment[] = {
		RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer,
		RANDO_ITEM_BombUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10,
	};
	size_t i;

	if (rando_items_count(&world->pre_collected) != 0)
		return;
	for (i = 0; i < sizeof equipment / sizeof equipment[0]; i++)
		rando_items_add(&world->pre_collected, equipment[i]);
}

void rando_prepare_goal(RandoWorld *world)
{
	RandoConfig *c = &world->config;

	if (c->goal == RANDO_GOAL_COMPLETIONIST)
		c->accessibility = RANDO_ACCESSIBILITY_LOCATIONS;

	switch (c->goal) {
	case RANDO_GOAL_PEDESTAL:
		place(world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_Triforce);
		break;
	case RANDO_GOAL_GANON:
	case RANDO_GOAL_FAST_GANON:
	case RANDO_GOAL_DUNGEONS:
	case RANDO_GOAL_GANONHUNT:
	case RANDO_GOAL_COMPLETIONIST:
		place(world, RANDO_LOC_GANON, RANDO_ITEM_Triforce);
		break;
	default:
		break;
	}
}

void rando_prepare_pre_collect(RandoWorld *world)
{
	RandoLogic logic = world->config.logic;

	/* TODO(step 2.3.6): VT also drops the first PegasusBoots from the advancement list and appends
	 * TwentyRupees to the item pool here; test_generate --fixtures still compares the raw getter lists. */
	if (logic == RANDO_LOGIC_MAJOR_GLITCHES || logic == RANDO_LOGIC_HYBRID_MAJOR_GLITCHES
	    || logic == RANDO_LOGIC_OVERWORLD_GLITCHES || logic == RANDO_LOGIC_NO_LOGIC)
		rando_items_add(&world->pre_collected, RANDO_ITEM_PegasusBoots);

	if (world->config.mode_state != RANDO_STATE_STANDARD)
		rando_items_add(&world->pre_collected, RANDO_ITEM_RescueZelda);
}

static const RandoShopId shield_shops[] = {
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
};

void rando_set_shops(RandoWorld *world)
{
	const RandoConfig *c = &world->config;
	RandoItemId shield = c->item_overflow_replacement_shield;
	size_t j;
	int i;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind != RANDO_SHOP_KIND_TAKE_ANY)
			rando_world_shop_set_active(world, (RandoShopId)i, true);

	/* Capacity Upgrade stays active: VT lists it in Shops with no item. */
	if (c->shops_hard_mode)
		rando_world_shop_clear(world, RANDO_SHOP_CAPACITY_UPGRADE);
	if (c->item_overflow_count_shield < 2)
		rando_world_shop_add(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, shield, 500);
	if (c->item_overflow_count_shield < 1)
		for (j = 0; j < sizeof shield_shops / sizeof shield_shops[0]; j++)
			rando_world_shop_add(world, shield_shops[j], 1, shield, 50);

	assert(!c->rom_generic_keys && !c->rom_rupee_bow &&
	       "setShops: the retro shops' random path (generic keys, rupee bow, take-anys) is not ported");
}

void rando_set_medallions(RandoWorld *world, RandoPrng *prng)
{
	static const RandoItemId medallions[] = {RANDO_ITEM_Ether, RANDO_ITEM_Bombos, RANDO_ITEM_Quake};
	const RandoRegionInfo *region = rando_region_info(RANDO_REGION_MEDALLIONS);
	int i;

	/* Table order, Turtle Rock first: baseline's two equal draws would hide a swap. */
	for (i = 0; i < region->nlocations; i++) {
		RandoLocationId location = (RandoLocationId)(region->first_location + i);
		bool placed;

		if (rando_world_has_item(world, location, RANDO_ITEM_NONE))
			continue;
		placed = rando_world_set_item(world, location, medallions[rando_random_int(prng, 0, 2)]);
		assert(placed && "setMedallions: a Medallion location refused a medallion");
		(void)placed;
	}
}

void rando_set_fountains(RandoWorld *world, RandoPrng *prng)
{
	const RandoRegionInfo *region = rando_region_info(RANDO_REGION_FOUNTAINS);
	int i;

	for (i = 0; i < region->nlocations; i++) {
		RandoLocationId location = (RandoLocationId)(region->first_location + i);

		if (rando_world_has_item(world, location, RANDO_ITEM_NONE))
			continue;
		place(world, location, rando_get_bottle(world, prng, true));
	}
}

static const struct {
	RandoRegionId region;
	RandoBossSlot slot;
	RandoBossId boss;
} unshuffled_bosses[] = {
	{RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_HELMASAUR_KING},
	{RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS},
	{RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOTHULA},
	{RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_BLIND},
	{RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE},
	{RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_VITREOUS},
	{RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_TRINEXX},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_MOLDORM},
};

void rando_place_bosses(RandoWorld *world)
{
	size_t i;

	if (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS)
		rando_world_set_boss(world, RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE);

	for (i = 0; i < sizeof unshuffled_bosses / sizeof unshuffled_bosses[0]; i++)
		rando_world_set_boss(world, unshuffled_bosses[i].region, unshuffled_bosses[i].slot,
				     unshuffled_bosses[i].boss);

	rando_world_set_boss(world, RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM);
	rando_world_set_boss(world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM2);
}

#define PRIZE_LOCATIONS 14
#define PRIZES 10
#define PRIZE_PASSES 6

static const RandoItemId prize_items[PRIZES] = {
	RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3, RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5,
	RANDO_ITEM_Crystal6, RANDO_ITEM_Crystal7, RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfPower,
	RANDO_ITEM_PendantOfWisdom,
};

static void assume_items(RandoWorld *world, RandoPrng *prng, const RandoItemId *prizes, int nprizes,
			 RandoItems *assumed)
{
	RandoItems start;
	int i;

	rando_world_reset_collected(world);
	rando_items_clear(&start);
	rando_pool_add_items(world, prng, RANDO_POOL_DUNGEON, &start);
	rando_pool_add_items(world, prng, RANDO_POOL_ADVANCEMENT, &start);
	for (i = 0; i < nprizes; i++)
		rando_items_add(&start, prizes[i]);
	rando_world_collect_items(world, &start, assumed);
}

static void clear_locations(RandoWorld *world, const RandoLocationId *locations, int n)
{
	int i;

	for (i = 0; i < n; i++)
		rando_world_set_item(world, locations[i], RANDO_ITEM_NONE);
}

/* One phase of fillPrizes over its locations. False when a location stays empty or a win check fails;
 * the phase's locations that were empty on entry are then empty again. */
static bool place_prize_phase(RandoWorld *world, RandoPrng *prng, const RandoLocationId *locations, int nlocations,
			      RandoItemId *prizes, int *nprizes)
{
	RandoLocationId empty[PRIZE_LOCATIONS];
	int nempty = 0, e, i;

	for (i = 0; i < nlocations; i++)
		if (!rando_world_has_item(world, locations[i], RANDO_ITEM_NONE))
			empty[nempty++] = locations[i];

	for (e = 0; e < nempty; e++) {
		RandoItemId prize = RANDO_ITEM_NONE;
		RandoItems assumed;
		int total = *nprizes;
		bool accessible = false;

		/* array_pop takes the candidate from the end; a refused one goes back at the front (array_unshift).
		 * Every candidate makes its four bottle draws before the access check. */
		for (i = 0; i < total; i++) {
			prize = prizes[--*nprizes];
			assume_items(world, prng, prizes, *nprizes, &assumed);
			if (rando_world_can_access(world, empty[e], &assumed)) {
				accessible = true;
				break;
			}
			memmove(prizes + 1, prizes, (size_t)*nprizes * sizeof prizes[0]);
			prizes[0] = prize;
			++*nprizes;
		}
		if (!accessible)
			continue;
		place(world, empty[e], prize);
		/* checkWinCondition collects on top of the candidate's collected locations: no reset here. */
		if (!rando_world_check_win_condition(world, &assumed)) {
			clear_locations(world, empty, nempty);
			return false;
		}
	}

	for (e = 0; e < nempty; e++)
		if (!rando_world_has_item(world, empty[e], RANDO_ITEM_NONE)) {
			clear_locations(world, empty, nempty);
			return false;
		}
	return true;
}

bool rando_fill_prizes(RandoWorld *world, RandoPrng *prng)
{
	int pass;

	for (pass = 0; pass < PRIZE_PASSES; pass++) {
		RandoLocationId pool[PRIZE_LOCATIONS], order[PRIZE_LOCATIONS];
		RandoLocationId crystals[PRIZE_LOCATIONS], pendants[PRIZE_LOCATIONS];
		RandoItemId prizes[PRIZES];
		int npool = 0, norder = 0, ncrystals = 0, npendants = 0, nprizes = 0, i, j;

		for (i = 0; i < RANDO_LOCATION_COUNT; i++)
			if (rando_location_is_a(rando_location_info((RandoLocationId)i)->location_class,
						RANDO_LOCATION_CLASS_PRIZE)) {
				assert(npool < PRIZE_LOCATIONS && "fillPrizes: more Prize locations than VT's 14");
				pool[npool++] = (RandoLocationId)i;
			}
		/* randomCollection: splice one draw per remaining location, the last one 0..0. */
		while (npool > 0) {
			int r = (int)rando_random_int(prng, 0, npool - 1);

			order[norder++] = pool[r];
			memmove(pool + r, pool + r + 1, (size_t)(npool - r - 1) * sizeof pool[0]);
			npool--;
		}
		for (j = 0; j < norder; j++) {
			RandoLocationClass location_class = rando_location_info(order[j])->location_class;

			if (location_class == RANDO_LOCATION_CLASS_PRIZE_CRYSTAL)
				crystals[ncrystals++] = order[j];
			else if (location_class == RANDO_LOCATION_CLASS_PRIZE_PENDANT)
				pendants[npendants++] = order[j];
		}

		for (j = 0; j < PRIZES; j++) {
			bool placed = false;
			int k;

			for (k = 0; k < norder && !placed; k++)
				placed = rando_world_has_item(world, order[k], prize_items[j]);
			if (!placed)
				prizes[nprizes++] = prize_items[j];
		}
		rando_fy_shuffle(prng, prizes, (size_t)nprizes, sizeof prizes[0]);

		/* crossWorld: the pendant phase takes what the crystal phase left, so pendants land in crystal
		 * locations and crystals in pendant ones. */
		if (place_prize_phase(world, prng, crystals, ncrystals, prizes, &nprizes)
		    && place_prize_phase(world, prng, pendants, npendants, prizes, &nprizes))
			return true;
	}
	return false;
}
