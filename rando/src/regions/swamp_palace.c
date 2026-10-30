#include "../world_rules.h"

#define FIRST RANDO_LOC_SWAMP_PALACE_ENTRANCE
#define NLOCATIONS (RANDO_LOC_SWAMP_PALACE_PRIZE - FIRST + 1)

static bool mire(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return world->config.can_one_frame_clip_uw
		&& has_n(items, RANDO_ITEM_KeyD6, 3)
		&& can_enter(world, locations, RANDO_REGION_MISERY_MIRE, items);
}

static bool hera(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return world->config.can_one_frame_clip_uw
		&& can_enter(world, locations, RANDO_REGION_TOWER_OF_HERA, items)
		&& has(items, RANDO_ITEM_BigKeyP3);
}

static bool past_first_key(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (has(items, RANDO_ITEM_KeyD2) || mire(world, locations, items))
		&& (has(items, RANDO_ITEM_Hammer) || mire(world, locations, items) || hera(world, locations, items));
}

static bool entrance_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.can_one_frame_clip_uw || world->config.region_wild_keys || item == RANDO_ITEM_KeyD2;
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return past_first_key(world, locations, items)
		&& (has(items, RANDO_ITEM_BigKeyD2)
		    || (mire(world, locations, items) && has(items, RANDO_ITEM_BigKeyD6))
		    || (hera(world, locations, items) && has(items, RANDO_ITEM_BigKeyP3)));
}

static bool big_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_BigKeyD2;
}

static bool big_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_BigKeyD2;
}

static bool map_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_bomb_things(items) && (has(items, RANDO_ITEM_KeyD2) || mire(world, locations, items));
}

static bool flooded(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hookshot) && past_first_key(world, locations, items);
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return flooded(world, locations, items)
		&& boss_can_beat(world, RANDO_REGION_SWAMP_PALACE, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_SWAMP_PALACE_BOSS, RANDO_ITEM_CompassD2,
						 RANDO_ITEM_MapD2, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD2, RANDO_ITEM_MapD2);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_SWAMP_PALACE_BOSS, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& has(items, RANDO_ITEM_Flippers)
		&& can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items)
		&& ((has(items, RANDO_ITEM_MoonPearl) && has(items, RANDO_ITEM_MagicMirror))
		    || (c->can_one_frame_clip_uw
			&& (has(items, RANDO_ITEM_BigKeyP3) || has(items, RANDO_ITEM_BigKeyD6)) && mire(world, locations, items)
			&& rando_world_can_access(locations, RANDO_LOC_OLD_MAN, items)
			&& ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip)
			    || (c->can_super_speed && rando_items_can_spin_speed(items))
			    || c->can_one_frame_clip_ow)));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SWAMP_PALACE_ENTRANCE - FIRST] = {NULL, entrance_fill, NULL},
	[RANDO_LOC_SWAMP_PALACE_BIG_CHEST - FIRST] = {big_chest, big_chest_fill, big_chest_always_allow},
	[RANDO_LOC_SWAMP_PALACE_BIG_KEY_CHEST - FIRST] = {past_first_key, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_MAP_CHEST - FIRST] = {map_chest, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_WEST_CHEST - FIRST] = {past_first_key, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_COMPASS_CHEST - FIRST] = {past_first_key, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_LEFT - FIRST] = {flooded, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_RIGHT - FIRST] = {flooded, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_WATERFALL_ROOM - FIRST] = {flooded, NULL, NULL},
	[RANDO_LOC_SWAMP_PALACE_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_SWAMP_PALACE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_swamp_palace = {
	"Region\\Standard\\SwampPalace", enter, complete, locations, NULL,
};
