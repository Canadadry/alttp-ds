#include "rando_pool.h"
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

/* Randomizer::__construct: three hearts, ten bombs and thirty arrows, only into an empty collection. */
static void check_start_equipment(void)
{
	RandoWorld world;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	rando_prepare_start_equipment(&world);
	CHECK(rando_items_count(&world.pre_collected) == 7);
	CHECK(world.pre_collected.counts[RANDO_ITEM_BossHeartContainer] == 3);
	CHECK(world.pre_collected.values[RANDO_ITEM_BossHeartContainer] == 3);
	CHECK(world.pre_collected.counts[RANDO_ITEM_BombUpgrade10] == 1);
	CHECK(world.pre_collected.counts[RANDO_ITEM_ArrowUpgrade10] == 3);
	CHECK(rando_items_has_health(&world.pre_collected, 3));

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	rando_items_add(&world.pre_collected, RANDO_ITEM_PegasusBoots);
	rando_prepare_start_equipment(&world);
	CHECK(rando_items_count(&world.pre_collected) == 1);
}

static void goal_world(RandoWorld *world, RandoGoal goal)
{
	RandoConfig config;

	rando_config_init(&config);
	config.goal = goal;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

/* prepareWorld's goal switch: the Triforce at Ganon, on the pedestal for pedestal, nowhere for triforce-hunt. */
static void check_goal_triforce(void)
{
	static const RandoGoal at_ganon[] = {
		RANDO_GOAL_GANON, RANDO_GOAL_FAST_GANON, RANDO_GOAL_DUNGEONS, RANDO_GOAL_GANONHUNT, RANDO_GOAL_COMPLETIONIST,
	};
	RandoWorld world;
	size_t i;

	for (i = 0; i < sizeof at_ganon / sizeof at_ganon[0]; i++) {
		goal_world(&world, at_ganon[i]);
		rando_prepare_goal(&world);
		CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_Triforce);
		CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_NONE);
	}
	goal_world(&world, RANDO_GOAL_PEDESTAL);
	rando_prepare_goal(&world);
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_Triforce);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	goal_world(&world, RANDO_GOAL_TRIFORCE_HUNT);
	rando_prepare_goal(&world);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_NONE);
}

/* completionist forces accessibility locations; other goals keep the world's value. */
static void check_goal_accessibility(void)
{
	RandoWorld world;

	goal_world(&world, RANDO_GOAL_COMPLETIONIST);
	world.config.accessibility = RANDO_ACCESSIBILITY_ITEMS;
	rando_prepare_goal(&world);
	CHECK(world.config.accessibility == RANDO_ACCESSIBILITY_LOCATIONS);
	goal_world(&world, RANDO_GOAL_GANON);
	world.config.accessibility = RANDO_ACCESSIBILITY_ITEMS;
	rando_prepare_goal(&world);
	CHECK(world.config.accessibility == RANDO_ACCESSIBILITY_ITEMS);
}

static void logic_world(RandoWorld *world, RandoVariant variant, RandoLogic logic)
{
	RandoConfig config;

	rando_config_init(&config);
	config.logic = logic;
	rando_world_init(world, variant, &config);
}

/* prepareWorld after its getters: RescueZelda outside standard, PegasusBoots under the four glitched logics. */
static void check_pre_collect(void)
{
	static const RandoLogic glitched[] = {
		RANDO_LOGIC_OVERWORLD_GLITCHES, RANDO_LOGIC_HYBRID_MAJOR_GLITCHES, RANDO_LOGIC_MAJOR_GLITCHES,
		RANDO_LOGIC_NO_LOGIC,
	};
	RandoWorld world;
	size_t i;

	logic_world(&world, RANDO_VARIANT_OPEN, RANDO_LOGIC_NO_GLITCHES);
	rando_prepare_pre_collect(&world);
	CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_RescueZelda));
	CHECK(!rando_items_has(&world.pre_collected, RANDO_ITEM_PegasusBoots));
	CHECK(rando_items_count(&world.pre_collected) == 1);

	logic_world(&world, RANDO_VARIANT_STANDARD, RANDO_LOGIC_NO_GLITCHES);
	rando_prepare_pre_collect(&world);
	CHECK(rando_items_count(&world.pre_collected) == 0);

	for (i = 0; i < sizeof glitched / sizeof glitched[0]; i++) {
		logic_world(&world, RANDO_VARIANT_STANDARD, glitched[i]);
		rando_prepare_pre_collect(&world);
		CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_PegasusBoots));
		CHECK(rando_items_count(&world.pre_collected) == 1);
	}
}

static const struct {
	RandoRegionId region;
	RandoBossSlot slot;
	RandoBossId boss;
} placed_bosses[] = {
	{RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_HELMASAUR_KING},
	{RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS},
	{RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOTHULA},
	{RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_BLIND},
	{RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE},
	{RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_VITREOUS},
	{RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_TRINEXX},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM2},
};

/* placeBosses with bossShuffle none: the baseline golden's Bosses block, whatever the slots held before. */
static void check_place_bosses(RandoWeapons weapons)
{
	RandoWorld world;
	RandoConfig config;
	size_t i;

	rando_config_init(&config);
	config.mode_weapons = weapons;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	for (i = 0; i < sizeof placed_bosses / sizeof placed_bosses[0]; i++)
		rando_world_set_boss(&world, placed_bosses[i].region, placed_bosses[i].slot, RANDO_BOSS_TRINEXX);
	rando_world_set_boss(&world, RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS);
	rando_place_bosses(&world);
	for (i = 0; i < sizeof placed_bosses / sizeof placed_bosses[0]; i++)
		CHECK(rando_world_boss(&world, placed_bosses[i].region, placed_bosses[i].slot) == placed_bosses[i].boss);
}

static const RandoItemId prizes[] = {
	RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3, RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5,
	RANDO_ITEM_Crystal6, RANDO_ITEM_Crystal7, RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfPower,
	RANDO_ITEM_PendantOfWisdom,
};

/* The generator's open world just before fillPrizes, with the given item.count entries at 0. */
static void prize_world(RandoWorld *world, RandoPrng *prng, const RandoItemId *zeroed, size_t nzeroed)
{
	RandoConfig config;
	size_t i;

	rando_config_init(&config);
	for (i = 0; i < nzeroed; i++)
		config.item_count[zeroed[i]] = 0;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
	rando_prepare_start_equipment(world);
	rando_prepare_goal(world);
	rando_prepare_pre_collect(world);
	rando_set_shops(world);
	seeded(prng, 1, 0);
	rando_set_medallions(world, prng);
	rando_place_bosses(world);
}

/* fillPrizes' assumed collection with all ten prizes still to place. */
static void assume_all(RandoWorld *world, RandoPrng *prng, RandoItems *assumed)
{
	RandoItems start;
	size_t i;

	rando_world_reset_collected(world);
	rando_items_clear(&start);
	rando_pool_add_items(world, prng, RANDO_POOL_DUNGEON, &start);
	rando_pool_add_items(world, prng, RANDO_POOL_ADVANCEMENT, &start);
	for (i = 0; i < sizeof prizes / sizeof prizes[0]; i++)
		rando_items_add(&start, prizes[i]);
	rando_world_collect_items(world, &start, assumed);
}

static void count_location_order(void *ctx, int64_t min, int64_t max, int64_t r)
{
	(void)r;
	if (min == 0 && max == 13)
		++*(int *)ctx;
}

static bool holds_prize(const RandoWorld *world, RandoLocationId location)
{
	size_t i;

	for (i = 0; i < sizeof prizes / sizeof prizes[0]; i++)
		if (rando_world_has_item(world, location, prizes[i]))
			return true;
	return false;
}

/* No candidate reaches Turtle Rock without CaneOfSomaria: six passes (VT's $attempts 5 down to 0), each
 * opened by its randomCollection draw over 14 locations, then Cannot Place Prize. */
static void check_fill_prizes_all_refused(void)
{
	static const RandoItemId zeroed[] = {RANDO_ITEM_CaneOfSomaria};
	RandoWorld world;
	RandoPrng prng;
	RandoItems assumed;
	int passes = 0;

	prize_world(&world, &prng, zeroed, 1);
	assume_all(&world, &prng, &assumed);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_PRIZE, &assumed));

	prng.observe = count_location_order;
	prng.observe_ctx = &passes;
	CHECK(!rando_fill_prizes(&world, &prng));
	CHECK(passes == 6);
	CHECK(!rando_world_has_item(&world, RANDO_LOC_TURTLE_ROCK_PRIZE, RANDO_ITEM_NONE));
}

static const RandoLocationId crystal_locations[] = {
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_PRIZE, RANDO_LOC_ICE_PALACE_PRIZE, RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_PRIZE,
};

/* Without BigKeyP2, Desert Palace (a Prize\Pendant) stays empty: each pendant-phase reset clears only the
 * pendant locations, so the crystal locations keep their prizes to the end. */
static void check_fill_prizes_pendant_failure(void)
{
	static const RandoItemId zeroed[] = {RANDO_ITEM_BigKeyP2};
	RandoWorld world;
	RandoPrng prng;
	RandoItems assumed;
	size_t i;
	int passes = 0;

	prize_world(&world, &prng, zeroed, 1);
	assume_all(&world, &prng, &assumed);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_DESERT_PALACE_PRIZE, &assumed));
	for (i = 0; i < sizeof crystal_locations / sizeof crystal_locations[0]; i++)
		CHECK(rando_world_can_access(&world, crystal_locations[i], &assumed));
	CHECK(rando_world_check_win_condition(&world, &assumed));

	prng.observe = count_location_order;
	prng.observe_ctx = &passes;
	CHECK(!rando_fill_prizes(&world, &prng));
	CHECK(passes == 6);
	for (i = 0; i < sizeof crystal_locations / sizeof crystal_locations[0]; i++)
		CHECK(holds_prize(&world, crystal_locations[i]));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_DESERT_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_TOWER_OF_HERA_PRIZE, RANDO_ITEM_NONE));
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
	check_start_equipment();
	check_goal_triforce();
	check_goal_accessibility();
	check_pre_collect();
	check_place_bosses(RANDO_WEAPONS_RANDOMIZED);
	check_place_bosses(RANDO_WEAPONS_SWORDLESS);
	check_fill_prizes_all_refused();
	check_fill_prizes_pendant_failure();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
