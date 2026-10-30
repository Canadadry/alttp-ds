#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (((has(items, RANDO_ITEM_MoonPearl)
		      || ((c->can_owyba && rando_items_has_a_bottle(items))
			  || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		     && can_enter(world, RANDO_REGION_NORTH_EAST_DARK_WORLD, items))
		    || (c->can_one_frame_clip_uw && c->can_dungeon_revive
			&& can_enter(world, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)));
}

const RandoRegionRules rando_rules_palace_of_darkness = {
	"Region\\Standard\\PalaceOfDarkness", enter, NULL, NULL, NULL,
};
