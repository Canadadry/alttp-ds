#include "rando_world.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_regions"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void standard_world(RandoWorld *world)
{
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(world, RANDO_VARIANT_STANDARD, &config);
}

/* TestCase::allItemsExcept: allItems, then offsetUnset of each name, which keeps the values view. */
static void all_items_except(RandoItems *items, const RandoItemId *except, int n)
{
	int i;

	rando_items_all(items);
	rando_items_unset(items, RANDO_ITEM_Key);
	rando_items_unset(items, RANDO_ITEM_BigKey);
	rando_items_unset(items, RANDO_ITEM_ShopKey);
	for (i = 0; i < n; i++)
		rando_items_unset(items, except[i]);
}

/* EasternPalaceTest::testNothingRequiredToEnter. */
static void check_nothing_required_to_enter_eastern_palace(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	rando_items_clear(&items);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_EASTERN_PALACE, &items));
}

/* TowerOfHeraTest::testCanEnterWithEverything. */
static void check_can_enter_tower_of_hera_with_everything(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, NULL, 0);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_TOWER_OF_HERA, &items));
}

/* HyruleCastleTowerTest::testCapeOrUpgradedSwordRequiredToEnter. */
static void check_cape_or_upgraded_sword_required_to_enter_castle_tower(void)
{
	static const RandoItemId except[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_L2Sword, RANDO_ITEM_MasterSword, RANDO_ITEM_L3Sword, RANDO_ITEM_L4Sword,
		RANDO_ITEM_UncleSword,
	};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, except, (int)(sizeof except / sizeof except[0]));
	CHECK(!rando_world_region_can_enter(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, &items));
}

/* HyruleCastleTowerTest::testSwordRequiredToComplete. */
static void check_sword_required_to_complete_castle_tower(void)
{
	static const RandoItemId except[] = {
		RANDO_ITEM_L1Sword, RANDO_ITEM_L1SwordAndShield, RANDO_ITEM_L2Sword, RANDO_ITEM_MasterSword,
		RANDO_ITEM_L3Sword, RANDO_ITEM_L4Sword, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_UncleSword,
	};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, except, (int)(sizeof except / sizeof except[0]));
	CHECK(!rando_world_region_can_complete(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, &items));
}

/* Region::initalize's bossNormalLocation fill rule, which no NoGlitches row varies. */
static void check_boss_normal_location_decides_dungeon_items_on_bosses(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_MapP1, &items, true));
	config.region_boss_normal_location = false;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_MapP1, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BOSS, RANDO_ITEM_KeyP3, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_DESERT_PALACE_BOSS, RANDO_ITEM_Hammer, &items, false));
}

/* The Tower of Hera Big Key Chest takes its own key unreachable unless accessibility is "locations". */
static void check_hera_big_key_chest_follows_accessibility(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST, RANDO_ITEM_KeyP3, &items, true));
	config.accessibility = RANDO_ACCESSIBILITY_LOCATIONS;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST, RANDO_ITEM_KeyP3, &items, false));
}

/* The tests/NoGlitches files whose rows are replayed whole; each chunk of the region port adds its
 * own, and the last one removes the list. */
static const char *const enabled_files[] = {
	"LightWorldTest.php", "HyruleCastleEscapeTest.php", "OpenTest.php",
	"DeathMountain/EastTest.php", "DeathMountain/WestTest.php",
	"DarkWorld/NorthEastTest.php", "DarkWorld/NorthWestTest.php", "DarkWorld/SouthTest.php", "DarkWorld/MireTest.php",
	"DarkWorld/DeathMountain/EastTest.php", "DarkWorld/DeathMountain/WestTest.php",
	"EasternPalaceTest.php", "DesertPalaceTest.php", "TowerOfHeraTest.php", "HyruleCastleTowerTest.php", NULL,
};

/* Methods replayed in every file: their region rules are all ported. */
static const char *const enabled_methods[] = {
	"testEntry", "testNothingRequiredToEnter", "testCanEnterWithEverything",
	"testCapeOrUpgradedSwordRequiredToEnter", "testSwordRequiredToComplete", NULL,
};

static int listed(const char *const *list, const char *name)
{
	for (; *list; list++)
		if (strcmp(*list, name) == 0)
			return 1;
	return 0;
}

static int enabled(const char *file, const char *method)
{
	return listed(enabled_files, file) || listed(enabled_methods, method);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	int rows, total;

	check_nothing_required_to_enter_eastern_palace();
	check_can_enter_tower_of_hera_with_everything();
	check_cape_or_upgraded_sword_required_to_enter_castle_tower();
	check_sword_required_to_complete_castle_tower();
	check_boss_normal_location_decides_dungeon_items_on_bosses();
	check_hera_big_key_chest_follows_accessibility();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	rows = fixture_replay_rows(TEST, enabled, &total);
	printf(TEST ": %d of %d NoGlitches rows replayed\n", rows, total);
	return 0;
}
