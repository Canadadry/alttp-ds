#include "rando_prepare.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static void place(RandoWorld *world, RandoLocationId location, RandoItemId item)
{
	bool placed = rando_world_set_item(world, location, item);

	assert(placed && "prepareWorld: a location refused its item");
	(void)placed;
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
