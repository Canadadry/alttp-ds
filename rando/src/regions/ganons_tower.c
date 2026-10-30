#include "../world_rules.h"

static int crystals(const RandoItems *items)
{
	return has(items, RANDO_ITEM_Crystal1) + has(items, RANDO_ITEM_Crystal2) + has(items, RANDO_ITEM_Crystal3)
		+ has(items, RANDO_ITEM_Crystal4) + has(items, RANDO_ITEM_Crystal5) + has(items, RANDO_ITEM_Crystal6)
		+ has(items, RANDO_ITEM_Crystal7);
}

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		     && ((crystals(items) >= c->crystals_tower
			  && can_enter(world, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items))
			 || (((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			      || (c->can_super_speed && has(items, RANDO_ITEM_PegasusBoots)
				  && has(items, RANDO_ITEM_Hookshot)))
			     && can_enter(world, RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN, items))))
		    || (c->can_one_frame_clip_ow
			&& (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
			    || (c->can_owyba && rando_items_has_a_bottle(items)))));
}

const RandoRegionRules rando_rules_ganons_tower = {
	"Region\\Standard\\GanonsTower", enter, NULL, NULL, NULL,
};
