#include "rando_prepare.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_prepare"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void open_world(RandoWorld *world, RandoItemPool pool)
{
	RandoConfig config;

	rando_config_init(&config);
	config.item_pool = pool;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

static void check_same_inventory(const RandoWorld *a, const RandoWorld *b, RandoShopId shop)
{
	int s;

	for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
		CHECK(rando_world_shop_item(a, shop, s) == rando_world_shop_item(b, shop, s));
		if (rando_world_shop_item(a, shop, s) != RANDO_ITEM_NONE)
			CHECK(rando_world_shop_price(a, shop, s) == rando_world_shop_price(b, shop, s));
	}
}

/* setShops activates every shop that is not a Shop\TakeAny and, in baseline, changes no inventory. */
static void check_shops_baseline(void)
{
	RandoWorld world, fresh;
	int i, active = 0;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	fresh = world;
	rando_set_shops(&world);
	for (i = 0; i < RANDO_SHOP_COUNT; i++) {
		RandoShopId shop = (RandoShopId)i;

		CHECK(rando_world_shop_active(&world, shop) == (rando_shop_info(shop)->kind != RANDO_SHOP_KIND_TAKE_ANY));
		active += rando_world_shop_active(&world, shop);
		check_same_inventory(&world, &fresh, shop);
	}
	CHECK(active == 10);
}

/* shops.HardMode clears Capacity Upgrade, which stays active: VT lists it item-less (item_pool-hard golden). */
static void check_shops_hard(void)
{
	RandoWorld world, fresh;
	int s;

	open_world(&world, RANDO_ITEM_POOL_HARD);
	fresh = world;
	rando_set_shops(&world);
	CHECK(rando_world_shop_active(&world, RANDO_SHOP_CAPACITY_UPGRADE));
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_FOREST_SHOP);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_POTION_SHOP);
}

static void check_slot(const RandoWorld *world, RandoShopId shop, int slot, RandoItemId item, int price)
{
	CHECK(rando_world_shop_item(world, shop, slot) == item);
	CHECK(rando_world_shop_price(world, shop, slot) == price);
}

/* Shield overflow count 1, below 2: Dark World Forest Shop slot 0 at 500 (item_pool-expert golden). */
static void check_shops_expert(void)
{
	RandoWorld world, fresh;
	int s;

	open_world(&world, RANDO_ITEM_POOL_EXPERT);
	fresh = world;
	rando_set_shops(&world);
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, RANDO_ITEM_TwentyRupees2, 500);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1, RANDO_ITEM_Bee, 10);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_POTION_SHOP);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP);
}

static const RandoShopId below_one_shops[] = {
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
};

/* Shield overflow count 0: also slot 1 of four shops at 50; no HardMode (item_pool-crowd_control golden). */
static void check_shops_crowd_control(RandoItemId replacement)
{
	RandoWorld world, fresh;
	RandoConfig config;
	size_t i;

	rando_config_init(&config);
	config.item_pool = RANDO_ITEM_POOL_CROWD_CONTROL;
	config.item_overflow_replacement_shield = replacement;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	fresh = world;
	rando_set_shops(&world);
	check_same_inventory(&world, &fresh, RANDO_SHOP_CAPACITY_UPGRADE);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, replacement, 500);
	for (i = 0; i < sizeof below_one_shops / sizeof below_one_shops[0]; i++) {
		check_slot(&world, below_one_shops[i], 0, RANDO_ITEM_RedPotion, 150);
		check_slot(&world, below_one_shops[i], 1, replacement, 50);
	}
}

static void seeded(RandoPrng *prng, uint32_t seed, int skip)
{
	memset(prng, 0, sizeof *prng);
	rando_prng_seed(prng, rando_splitmix32(seed));
	while (skip-- > 0)
		rando_random_int(prng, 0, 0);
}

/* cases/crystals_ganon-random seed 1, attempt 0: draws 6 and 7 are 0 then 2; Turtle Rock comes first. */
static void check_medallions_golden(void)
{
	RandoWorld world;
	RandoPrng prng;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	seeded(&prng, 1, 5);
	rando_set_medallions(&world, &prng);
	CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == RANDO_ITEM_Ether);
	CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == RANDO_ITEM_Quake);
}

static void count_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	(void)min;
	(void)max;
	(void)r;
	++*(int *)ctx;
}

/* VT's $medallions, indexed by get_random_int(0, 2). */
static const RandoItemId by_index[] = {RANDO_ITEM_Ether, RANDO_ITEM_Bombos, RANDO_ITEM_Quake};

static void check_medallion_index(void)
{
	int seen[3] = {0, 0, 0};
	uint32_t seed;

	for (seed = 0; seed < 64; seed++) {
		RandoWorld world;
		RandoPrng prng, twin;
		int64_t tr, mm;

		open_world(&world, RANDO_ITEM_POOL_NORMAL);
		seeded(&prng, seed, 0);
		seeded(&twin, seed, 0);
		rando_set_medallions(&world, &prng);
		tr = rando_random_int(&twin, 0, 2);
		mm = rando_random_int(&twin, 0, 2);
		CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == by_index[tr]);
		CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == by_index[mm]);
		seen[tr]++;
		seen[mm]++;
	}
	CHECK(seen[0] && seen[1] && seen[2]);
}

/* A location that already holds an item is skipped without a draw. */
static void check_medallion_preset(void)
{
	RandoWorld world;
	RandoPrng prng, twin;
	int draws = 0;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Bombos));
	seeded(&prng, 7, 0);
	seeded(&twin, 7, 0);
	prng.observe = count_draw;
	prng.observe_ctx = &draws;
	rando_set_medallions(&world, &prng);
	CHECK(draws == 1);
	CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == RANDO_ITEM_Bombos);
	CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == by_index[rando_random_int(&twin, 0, 2)]);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_shops_baseline();
	check_shops_hard();
	check_shops_expert();
	check_shops_crowd_control(RANDO_ITEM_TwentyRupees2);
	check_shops_crowd_control(RANDO_ITEM_Arrow);
	check_medallions_golden();
	check_medallion_index();
	check_medallion_preset();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
