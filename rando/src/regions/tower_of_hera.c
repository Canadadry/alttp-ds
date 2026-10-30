#include "../world_rules.h"

static bool main_entrance(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip)
		|| (c->can_super_speed && rando_items_can_spin_speed(items)))
		|| c->can_one_frame_clip_ow
		|| ((has(items, RANDO_ITEM_MagicMirror) || (has(items, RANDO_ITEM_Hookshot) && has(items, RANDO_ITEM_Hammer)))
		    && can_enter(world, RANDO_REGION_WEST_DEATH_MOUNTAIN, items));
}

static bool mire(const RandoWorld *world, const RandoItems *items)
{
	return world->config.can_one_frame_clip_uw
		&& (((rando_world_has_item(world, RANDO_LOC_MISERY_MIRE_COMPASS_CHEST, RANDO_ITEM_BigKeyD6)
		      || rando_world_has_item(world, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD6))
		     && has_n(items, RANDO_ITEM_KeyD6, 2))
		    || has_n(items, RANDO_ITEM_KeyD6, 3))
		&& can_enter(world, RANDO_REGION_MISERY_MIRE, items);
}

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	return has(items, RANDO_ITEM_RescueZelda) && (main_entrance(world, items) || mire(world, items));
}

const RandoRegionRules rando_rules_tower_of_hera = {
	"Region\\Standard\\TowerOfHera", enter, NULL, NULL, NULL,
};
