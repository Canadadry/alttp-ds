#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	(void)world;
	return has(items, RANDO_ITEM_RescueZelda);
}

const RandoRegionRules rando_rules_eastern_palace = {
	"Region\\Standard\\EasternPalace", enter, NULL, NULL, NULL,
};
