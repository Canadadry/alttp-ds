#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (has(items, RANDO_ITEM_BookOfMudora)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || c->can_one_frame_clip_ow
		    || (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, RANDO_REGION_MIRE, items)));
}

const RandoRegionRules rando_rules_desert_palace = {
	"Region\\Standard\\DesertPalace", enter, NULL, NULL, NULL,
};
