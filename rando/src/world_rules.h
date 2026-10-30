#ifndef RANDO_WORLD_RULES_H
#define RANDO_WORLD_RULES_H

#include "rando_world.h"

#include <stddef.h>

typedef bool (*RandoRule)(const RandoWorld *world, const RandoItems *items);
typedef bool (*RandoAlwaysAllowRule)(const RandoWorld *world, RandoItemId item, const RandoItems *items);
/* Non-const world: Link's Uncle's fill rule calls collect_items, which writes the memo. */
typedef bool (*RandoFillRule)(RandoWorld *world, RandoItemId item, const RandoItems *items);

typedef struct RandoLocationRules {
	RandoRule requirement;
	RandoFillRule fill;
	RandoAlwaysAllowRule always_allow;
} RandoLocationRules;

/* Any rule may be NULL, as in VT. locations is indexed from the region's first_location and shops
 * from its first_shop; either may be NULL when every entry would be. */
typedef struct RandoRegionRules {
	const char *vt_class;
	RandoRule can_enter;
	RandoRule can_complete;
	const RandoLocationRules *locations;
	const RandoRule *shops;
} RandoRegionRules;

const RandoRegionRules *rando_region_rules(RandoVariant variant, RandoRegionId region);

extern const RandoRegionRules rando_rules_north_east_light_world;
extern const RandoRegionRules rando_rules_north_west_light_world;
extern const RandoRegionRules rando_rules_south_light_world;
extern const RandoRegionRules rando_rules_standard_escape;
extern const RandoRegionRules rando_rules_open_escape;
extern const RandoRegionRules rando_rules_eastern_palace;
extern const RandoRegionRules rando_rules_desert_palace;
extern const RandoRegionRules rando_rules_west_death_mountain;
extern const RandoRegionRules rando_rules_east_death_mountain;
extern const RandoRegionRules rando_rules_tower_of_hera;
extern const RandoRegionRules rando_rules_hyrule_castle_tower;
extern const RandoRegionRules rando_rules_east_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_west_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_north_east_dark_world;
extern const RandoRegionRules rando_rules_north_west_dark_world;
extern const RandoRegionRules rando_rules_south_dark_world;
extern const RandoRegionRules rando_rules_mire;
extern const RandoRegionRules rando_rules_palace_of_darkness;
extern const RandoRegionRules rando_rules_swamp_palace;
extern const RandoRegionRules rando_rules_skull_woods;
extern const RandoRegionRules rando_rules_thieves_town;
extern const RandoRegionRules rando_rules_ice_palace;
extern const RandoRegionRules rando_rules_misery_mire;
extern const RandoRegionRules rando_rules_turtle_rock;
extern const RandoRegionRules rando_rules_ganons_tower;

static inline bool has(const RandoItems *items, RandoItemId id)
{
	return rando_items_has(items, id);
}

static inline bool has_n(const RandoItems *items, RandoItemId id, int at_least)
{
	return rando_items_has_n(items, id, at_least);
}

static inline bool can_enter(const RandoWorld *world, RandoRegionId region, const RandoItems *items)
{
	return rando_world_region_can_enter(world, region, items);
}

static inline bool has_lamp(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_has_n(items, RANDO_ITEM_Lamp, world->config.item_require_lamp);
}

static inline bool wild_or_not_dungeon_item(const RandoConfig *c, RandoItemId item)
{
	RandoItemClass k = rando_item_info(item)->item_class;

	return !((!c->region_wild_keys && k == RANDO_ITEM_CLASS_KEY)
		 || (!c->region_wild_big_keys && k == RANDO_ITEM_CLASS_BIG_KEY)
		 || (!c->region_wild_maps && k == RANDO_ITEM_CLASS_MAP)
		 || (!c->region_wild_compasses && k == RANDO_ITEM_CLASS_COMPASS));
}

#endif
