#include "rando_prepare.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static const RandoShopId shield_shops[] = {
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
};

void rando_set_shops(RandoWorld *world)
{
	const RandoConfig *c = &world->config;
	RandoItemId shield = c->item_overflow_replacement_shield;
	size_t j;
	int i;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind != RANDO_SHOP_KIND_TAKE_ANY)
			rando_world_shop_set_active(world, (RandoShopId)i, true);

	/* Capacity Upgrade stays active: VT lists it in Shops with no item. */
	if (c->shops_hard_mode)
		rando_world_shop_clear(world, RANDO_SHOP_CAPACITY_UPGRADE);
	if (c->item_overflow_count_shield < 2)
		rando_world_shop_add(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, shield, 500);
	if (c->item_overflow_count_shield < 1)
		for (j = 0; j < sizeof shield_shops / sizeof shield_shops[0]; j++)
			rando_world_shop_add(world, shield_shops[j], 1, shield, 50);

	assert(!c->rom_generic_keys && !c->rom_rupee_bow &&
	       "setShops: the retro shops' random path (generic keys, rupee bow, take-anys) is not ported");
}

void rando_set_medallions(RandoWorld *world, RandoPrng *prng)
{
	static const RandoItemId medallions[] = {RANDO_ITEM_Ether, RANDO_ITEM_Bombos, RANDO_ITEM_Quake};
	const RandoRegionInfo *region = rando_region_info(RANDO_REGION_MEDALLIONS);
	int i;

	/* Table order, Turtle Rock first: baseline's two equal draws would hide a swap. */
	for (i = 0; i < region->nlocations; i++) {
		RandoLocationId location = (RandoLocationId)(region->first_location + i);
		bool placed;

		if (rando_world_has_item(world, location, RANDO_ITEM_NONE))
			continue;
		placed = rando_world_set_item(world, location, medallions[rando_random_int(prng, 0, 2)]);
		assert(placed && "setMedallions: a Medallion location refused a medallion");
		(void)placed;
	}
}
