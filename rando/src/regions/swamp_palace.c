#include "../world_rules.h"

static bool mire(const RandoWorld *world, const RandoItems *items)
{
	return world->config.can_one_frame_clip_uw
		&& has_n(items, RANDO_ITEM_KeyD6, 3)
		&& can_enter(world, RANDO_REGION_MISERY_MIRE, items);
}

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& has(items, RANDO_ITEM_Flippers)
		&& can_enter(world, RANDO_REGION_SOUTH_DARK_WORLD, items)
		&& ((has(items, RANDO_ITEM_MoonPearl) && has(items, RANDO_ITEM_MagicMirror))
		    || (c->can_one_frame_clip_uw
			&& (has(items, RANDO_ITEM_BigKeyP3) || has(items, RANDO_ITEM_BigKeyD6)) && mire(world, items)
			&& rando_world_can_access(world, RANDO_LOC_OLD_MAN, items)
			&& ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip)
			    || (c->can_super_speed && rando_items_can_spin_speed(items))
			    || c->can_one_frame_clip_ow)));
}

const RandoRegionRules rando_rules_swamp_palace = {
	"Region\\Standard\\SwampPalace", enter, NULL, NULL, NULL,
};
