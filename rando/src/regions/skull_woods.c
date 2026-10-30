#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
		    || ((rando_items_has_a_bottle(items) && c->can_owyba)
			|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		&& can_enter(world, RANDO_REGION_NORTH_WEST_DARK_WORLD, items);
}

const RandoRegionRules rando_rules_skull_woods = {
	"Region\\Standard\\SkullWoods", enter, NULL, NULL, NULL,
};
