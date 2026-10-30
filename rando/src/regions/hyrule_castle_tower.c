#include "../world_rules.h"

#define FIRST RANDO_LOC_CASTLE_TOWER_ROOM_03
#define NLOCATIONS (RANDO_LOC_AGAHNIM - FIRST + 1)

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_can_kill_most_things(items, &world->config, 8)
		&& has(items, RANDO_ITEM_RescueZelda)
		&& (has(items, RANDO_ITEM_Cape)
		    || rando_items_has_sword(items, 2)
		    || (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)));
}

static bool dark_maze(const RandoWorld *world, const RandoItems *items)
{
	return has_lamp(world, items) && has(items, RANDO_ITEM_KeyA1);
}

static bool complete(const RandoWorld *world, const RandoItems *items)
{
	return enter(world, items) && has_n(items, RANDO_ITEM_KeyA1, 2)
		&& has_lamp(world, items)
		&& (rando_items_has_sword(items, 1)
		    || (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS
			&& (has(items, RANDO_ITEM_Hammer) || has(items, RANDO_ITEM_BugCatchingNet))));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_CASTLE_TOWER_DARK_MAZE - FIRST] = {dark_maze, NULL, NULL},
	[RANDO_LOC_AGAHNIM - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_hyrule_castle_tower = {
	"Region\\Standard\\HyruleCastleTower", enter, complete, locations, NULL,
};
