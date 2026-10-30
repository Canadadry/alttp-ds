#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (((rando_world_has_item(world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Bombos)
		      && has(items, RANDO_ITEM_Bombos))
		     || (rando_world_has_item(world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether)
			 && has(items, RANDO_ITEM_Ether))
		     || (rando_world_has_item(world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake)
			 && has(items, RANDO_ITEM_Quake)))
		    && (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1)))
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (rando_items_has_a_bottle(items)
			&& ((has(items, RANDO_ITEM_BugCatchingNet) && c->can_bunny_revive
			     && ((rando_items_can_lift_dark_rocks(items)
				  && (rando_items_can_fly(items, c)
				      || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
				 || (c->can_owyba && has(items, RANDO_ITEM_MagicMirror))
				 || c->can_one_frame_clip_ow))
			    || (c->can_owyba
				&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				    || c->can_one_frame_clip_ow
				    || rando_items_has_bottle(items, 2))))))
		&& ((c->item_placement != RANDO_PLACEMENT_BASIC && has(items, RANDO_ITEM_PegasusBoots))
		    || has(items, RANDO_ITEM_Hookshot))
		&& rando_items_can_kill_most_things(items, c, 8)
		&& can_enter(world, RANDO_REGION_MIRE, items);
}

const RandoRegionRules rando_rules_misery_mire = {
	"Region\\Standard\\MiseryMire", enter, NULL, NULL, NULL,
};
