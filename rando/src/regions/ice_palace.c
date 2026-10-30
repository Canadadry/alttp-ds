#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (rando_items_can_melt_things(items, c) || c->can_one_frame_clip_uw)
		&& (((has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive)
		     && (has(items, RANDO_ITEM_Flippers) || c->can_fake_flipper)
		     && rando_items_can_lift_dark_rocks(items))
		    || (can_enter(world, RANDO_REGION_SOUTH_DARK_WORLD, items)
			&& (((has(items, RANDO_ITEM_MoonPearl)
			      || (rando_items_has_a_bottle(items) && c->can_owyba)
			      || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c)))
			     && ((c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
				  && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				      || (c->can_super_speed && rando_items_can_spin_speed(items))))
				 || (has(items, RANDO_ITEM_Flippers) && c->can_transition_wrapped
				     && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
					 || c->can_one_frame_clip_ow))))
			    || (c->can_one_frame_clip_ow
				&& c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)))));
}

const RandoRegionRules rando_rules_ice_palace = {
	"Region\\Standard\\IcePalace", enter, NULL, NULL, NULL,
};
