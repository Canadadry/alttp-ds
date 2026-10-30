#include "../world_rules.h"

/* Region\Open\HyruleCastleEscape::initalize does not call its parent's: Sanctuary, the Map Chest
 * and Zelda have no requirement, and the region no completion rule. */

#define FIRST RANDO_LOC_SANCTUARY
#define NLOCATIONS (RANDO_LOC_ZELDA - FIRST + 1)

static bool lamp_or_advanced_fire_rod(const RandoWorld *world, const RandoItems *items)
{
	return has_lamp(world, items)
		|| (world->config.item_placement == RANDO_PLACEMENT_ADVANCED && has(items, RANDO_ITEM_FireRod));
}

static bool sewers_secret_room(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_can_lift_rocks(items)
		|| (lamp_or_advanced_fire_rod(world, items)
		    && has(items, RANDO_ITEM_KeyH2) && rando_items_can_kill_most_things(items, &world->config, 5));
}

static bool key_and_kill_most_things(const RandoWorld *world, const RandoItems *items)
{
	return has(items, RANDO_ITEM_KeyH2) && rando_items_can_kill_most_things(items, &world->config, 5);
}

static bool secret_passage_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return wild_or_not_dungeon_item(&world->config, item);
}

static bool links_uncle_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	RandoItems collected;

	(void)items;
	rando_world_collect_items(world, NULL, &collected);
	return rando_world_can_access(world, RANDO_LOC_SANCTUARY, &collected)
		&& wild_or_not_dungeon_item(&world->config, item);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_DARK_CROSS - FIRST] = {lamp_or_advanced_fire_rod, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST - FIRST] = {key_and_kill_most_things, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL - FIRST] = {key_and_kill_most_things, NULL, NULL},
	[RANDO_LOC_LINKS_UNCLE - FIRST] = {NULL, links_uncle_fill, NULL},
	[RANDO_LOC_SECRET_PASSAGE - FIRST] = {NULL, secret_passage_fill, NULL},
};

const RandoRegionRules rando_rules_open_escape = {
	"Region\\Open\\HyruleCastleEscape", NULL, NULL, locations, NULL,
};
