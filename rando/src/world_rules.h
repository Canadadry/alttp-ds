#ifndef RANDO_WORLD_RULES_H
#define RANDO_WORLD_RULES_H

#include "rando_world.h"

typedef bool (*RandoRule)(const RandoWorld *world, const RandoItems *items);
typedef bool (*RandoAlwaysAllowRule)(const RandoWorld *world, RandoItemId item, const RandoItems *items);
/* Non-const world: Link's Uncle's fill rule calls collect_items, which writes the memo. */
typedef bool (*RandoFillRule)(RandoWorld *world, RandoItemId item, const RandoItems *items);

typedef struct RandoLocationRules {
	RandoRule requirement;
	RandoFillRule fill;
	RandoAlwaysAllowRule always_allow;
} RandoLocationRules;

/* Any rule may be NULL, as in VT. locations is indexed from the region's first_location and shops
 * from its first_shop; either may be NULL when every entry would be. */
typedef struct RandoRegionRules {
	const char *vt_class;
	RandoRule can_enter;
	RandoRule can_complete;
	const RandoLocationRules *locations;
	const RandoRule *shops;
} RandoRegionRules;

const RandoRegionRules *rando_region_rules(RandoVariant variant, RandoRegionId region);

#endif
