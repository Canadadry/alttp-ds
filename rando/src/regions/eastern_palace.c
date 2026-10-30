#include "../world_rules.h"

#define FIRST RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST
#define NLOCATIONS (RANDO_LOC_EASTERN_PALACE_PRIZE - FIRST + 1)

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	(void)world;
	return has(items, RANDO_ITEM_RescueZelda);
}

static bool big_chest(const RandoWorld *world, const RandoItems *items)
{
	(void)world;
	return has(items, RANDO_ITEM_BigKeyP1);
}

static bool big_key_chest(const RandoWorld *world, const RandoItems *items)
{
	return has_lamp(world, items);
}

static bool boss(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_can_shoot_arrows(items, &world->config, 1)
		&& (has_lamp(world, items)
		    || (world->config.item_placement == RANDO_PLACEMENT_ADVANCED && has(items, RANDO_ITEM_FireRod)))
		&& has(items, RANDO_ITEM_BigKeyP1)
		&& boss_can_beat(world, RANDO_REGION_EASTERN_PALACE, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_CompassP1,
						 RANDO_ITEM_MapP1, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassP1, RANDO_ITEM_MapP1);
}

static bool complete(const RandoWorld *world, const RandoItems *items)
{
	return rando_world_can_access(world, RANDO_LOC_EASTERN_PALACE_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_EASTERN_PALACE_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST - FIRST] = {big_key_chest, NULL, NULL},
	[RANDO_LOC_EASTERN_PALACE_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_EASTERN_PALACE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_eastern_palace = {
	"Region\\Standard\\EasternPalace", enter, complete, locations, NULL,
};
