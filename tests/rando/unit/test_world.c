#include "rando_world.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_world"

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

/* World::__construct: the constructor items, VT's default bosses, the shops' default inventory. */
static void check_init(void)
{
	RandoWorld world;

	standard_world(&world);
	CHECK(rando_location_find("Ganon") == RANDO_LOC_GANON);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_FAIRY_SWORD) == RANDO_ITEM_L1Sword);
	CHECK(rando_world_item(&world, RANDO_LOC_LINKS_UNCLE) == RANDO_ITEM_NONE);
	CHECK(rando_world_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE) == RANDO_BOSS_LANMOLAS);
	CHECK(rando_world_boss(&world, RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS)
	      == RANDO_BOSS_ARMOS_KNIGHTS);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1) == RANDO_ITEM_Bee);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2) == RANDO_ITEM_NONE);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_BOMB_HUT, 0) == RANDO_ITEM_NONE);
	CHECK(world.config.mode_state == RANDO_STATE_STANDARD);
	CHECK(rando_items_count(&world.pre_collected) == 0);
}

/* User story 14's irregular cases of VT's location class tree. */
static void check_location_classes(void)
{
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_BIG_CHEST, RANDO_LOCATION_CLASS_CHEST));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_CHEST, RANDO_LOCATION_CLASS_BIG_CHEST));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_WITCH, RANDO_LOCATION_CLASS_NPC));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_ZORA, RANDO_LOCATION_CLASS_NPC));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID, RANDO_LOCATION_CLASS_NPC));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_NPC_UNCLE, RANDO_LOCATION_CLASS_NPC));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_DROP_BOMBOS, RANDO_LOCATION_CLASS_DROP));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_DROP_ETHER, RANDO_LOCATION_CLASS_DROP));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT, RANDO_LOCATION_CLASS_STANDING));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE, RANDO_LOCATION_CLASS_DIG));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_PRIZE_EVENT, RANDO_LOCATION_CLASS_PRIZE));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_TRADE, RANDO_LOCATION_CLASS_LOCATION));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_PRIZE_CRYSTAL, RANDO_LOCATION_CLASS_PRIZE_PENDANT));
}

/* Location::setItem and its Pedestal, Prize, Prize\Event and Medallion overrides. */
static void check_set_item(void)
{
	RandoWorld world;

	standard_world(&world);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword));
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_L2Sword);
	CHECK(rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_L2Sword));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword));
	CHECK(rando_world_set_item(&world, RANDO_LOC_KINGS_TOMB, RANDO_ITEM_MasterSword));
	CHECK(rando_world_item(&world, RANDO_LOC_KINGS_TOMB) == RANDO_ITEM_MasterSword);

	CHECK(!rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Hammer));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Crystal3));
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_PendantOfPower));
	CHECK(!rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_DefeatGanon));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE) == RANDO_ITEM_PendantOfPower);
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));

	CHECK(!rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Crystal1));
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));

	CHECK(!rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Hammer));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_NONE));
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake));
	CHECK(rando_world_has_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake));
}

/* Region::canFill: dungeon items stay home unless wild; the sewers key stays home in Standard. */
static void check_region_can_fill(void)
{
	RandoWorld world;
	RandoConfig config;

	standard_world(&world);
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP1));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_Key));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_MapP1));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_Hammer));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_ShopKey));
	world.config.region_wild_keys = true;
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyH2));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_ESCAPE, RANDO_ITEM_KeyH2));

	rando_config_init(&config);
	config.dungeon_items = RANDO_DUNGEON_ITEMS_MCS;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyH2));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_CompassP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_BigKeyP2));
	CHECK(world.config.rom_map_on_pickup && !world.config.region_wild_big_keys);
}

/* Location::canFill sets the item while it evaluates, then restores the location; Location::fill
 * keeps the item only when canFill succeeds. */
static void check_can_fill_and_fill(void)
{
	RandoWorld world;
	RandoItems none;

	standard_world(&world);
	rando_items_clear(&none);
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_Hammer));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_KeyP1, &none, true));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_KeyP2, &none, false));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_Hammer);

	CHECK(!rando_world_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyP2, &none));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_Hammer);
	CHECK(rando_world_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyP1, &none));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_BigKeyP1);

	CHECK(rando_world_can_fill(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword, &none, false));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_NONE));
}

static int collected_count(const RandoWorld *world)
{
	int n = 0, i, slot;

	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		n += rando_world_collected(world, (RandoLocationId)i);
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			n += rando_world_shop_collected(world, (RandoShopId)i, slot);
	return n;
}

static int same_items(const RandoItems *a, const RandoItems *b)
{
	return memcmp(a, b, sizeof *a) == 0;
}

/* World::collectItems: properties that hold whatever the region rules, since they are monotone. */
static void check_collect_items(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems first, again, start, out;
	int i, slot, collected;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	rando_items_add(&world.pre_collected, RANDO_ITEM_BugCatchingNet);
	CHECK(rando_world_set_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake));

	rando_world_collect_items(&world, NULL, &first);
	collected = collected_count(&world);
	CHECK(rando_items_count(&first) == 1 + collected);
	CHECK(first.counts[RANDO_ITEM_BugCatchingNet] == 1 && first.values[RANDO_ITEM_BugCatchingNet] == 1);
	CHECK(!rando_world_collected(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION));
	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		CHECK(!rando_world_collected(&world, (RandoLocationId)i)
		      || rando_world_has_item(&world, (RandoLocationId)i, RANDO_ITEM_NONE));
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			CHECK(!rando_world_shop_collected(&world, (RandoShopId)i, slot)
			      || rando_world_shop_item(&world, (RandoShopId)i, slot) != RANDO_ITEM_NONE);

	rando_world_collect_items(&world, NULL, &again);
	CHECK(same_items(&again, &world.pre_collected));
	CHECK(collected_count(&world) == collected);

	rando_world_reset_collected(&world);
	CHECK(collected_count(&world) == 0);
	rando_world_collect_items(&world, NULL, &again);
	CHECK(same_items(&again, &first));

	rando_world_reset_collected(&world);
	rando_items_clear(&start);
	start.counts[RANDO_ITEM_Hookshot] = 1;
	start.values[RANDO_ITEM_MoonPearl] = 1;
	rando_world_collect_items(&world, &start, &out);
	CHECK(out.values[RANDO_ITEM_MoonPearl] == out.counts[RANDO_ITEM_MoonPearl] + 1);
	CHECK(out.values[RANDO_ITEM_Hookshot] + 1 == out.counts[RANDO_ITEM_Hookshot]);
	CHECK(rando_items_count(&out) == 2 + collected_count(&world));
}

static const char *const location_class_names[RANDO_LOCATION_CLASS_COUNT] = {
	[RANDO_LOCATION_CLASS_LOCATION] = "Location",
	[RANDO_LOCATION_CLASS_CHEST] = "Location\\Chest",
	[RANDO_LOCATION_CLASS_BIG_CHEST] = "Location\\BigChest",
	[RANDO_LOCATION_CLASS_DASH] = "Location\\Dash",
	[RANDO_LOCATION_CLASS_DIG] = "Location\\Dig",
	[RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE] = "Location\\Dig\\HauntedGrove",
	[RANDO_LOCATION_CLASS_DROP] = "Location\\Drop",
	[RANDO_LOCATION_CLASS_DROP_BOMBOS] = "Location\\Drop\\Bombos",
	[RANDO_LOCATION_CLASS_DROP_ETHER] = "Location\\Drop\\Ether",
	[RANDO_LOCATION_CLASS_FOUNTAIN] = "Location\\Fountain",
	[RANDO_LOCATION_CLASS_MEDALLION] = "Location\\Medallion",
	[RANDO_LOCATION_CLASS_NPC] = "Location\\Npc",
	[RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID] = "Location\\Npc\\BugCatchingKid",
	[RANDO_LOCATION_CLASS_NPC_UNCLE] = "Location\\Npc\\Uncle",
	[RANDO_LOCATION_CLASS_NPC_WITCH] = "Location\\Npc\\Witch",
	[RANDO_LOCATION_CLASS_NPC_ZORA] = "Location\\Npc\\Zora",
	[RANDO_LOCATION_CLASS_PEDESTAL] = "Location\\Pedestal",
	[RANDO_LOCATION_CLASS_PRIZE] = "Location\\Prize",
	[RANDO_LOCATION_CLASS_PRIZE_CRYSTAL] = "Location\\Prize\\Crystal",
	[RANDO_LOCATION_CLASS_PRIZE_EVENT] = "Location\\Prize\\Event",
	[RANDO_LOCATION_CLASS_PRIZE_PENDANT] = "Location\\Prize\\Pendant",
	[RANDO_LOCATION_CLASS_STANDING] = "Location\\Standing",
	[RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT] = "Location\\Standing\\HeraBasement",
	[RANDO_LOCATION_CLASS_TRADE] = "Location\\Trade",
};

static const char *const slot_names[RANDO_BOSS_SLOT_COUNT] = {"boss", "top", "middle", "bottom"};

static const char *item_name(RandoItemId id)
{
	return id == RANDO_ITEM_NONE ? "-" : rando_item_info(id)->name;
}

static RandoVariant variant_of(const Fixture *fx, const char *name)
{
	if (strcmp(name, "standard") == 0)
		return RANDO_VARIANT_STANDARD;
	if (strcmp(name, "open") == 0)
		return RANDO_VARIANT_OPEN;
	fixture_fail(fx, "unknown variant");
	return RANDO_VARIANT_COUNT;
}

static void expect_field(const Fixture *fx, const char *want, const char *got, const char *what)
{
	if (strcmp(want, got) != 0) {
		fprintf(stderr, "%s: VT \"%s\", C \"%s\"\n", what, want, got);
		fixture_fail(fx, "differs from VT");
	}
}

static void check_region_record(const Fixture *fx, RandoVariant variant, int id)
{
	char buf[512];
	const RandoRegionInfo *info;
	RandoWorld world;
	RandoConfig config;
	size_t n = 0;
	int k;

	if (id >= RANDO_REGION_COUNT)
		fixture_fail(fx, "more regions in the dump than in the C table");
	info = rando_region_info((RandoRegionId)id);
	expect_field(fx, fx->field[2], info->key, "world key");
	if (rando_region_find(fx->field[2]) != (RandoRegionId)id)
		fixture_fail(fx, "rando_region_find does not return the region's id");
	expect_field(fx, fx->field[3], info->spoiler_name, "spoiler name");
	expect_field(fx, fx->field[4], rando_region_vt_class(variant, (RandoRegionId)id), "VT class");
	expect_field(fx, fx->field[5],
		     info->prize_location == RANDO_LOCATION_NONE ? "-" : rando_location_info(info->prize_location)->name,
		     "prize location");
	rando_config_init(&config);
	rando_world_init(&world, variant, &config);
	buf[0] = '\0';
	for (k = 0; k < info->nboss_slots; k++)
		n += (size_t)snprintf(buf + n, sizeof buf - n, "%s%s=%s", k ? "|" : "", slot_names[k],
				      rando_boss_name(rando_world_boss(&world, (RandoRegionId)id, (RandoBossSlot)k)));
	for (; k < RANDO_BOSS_SLOT_COUNT; k++)
		if (rando_world_boss(&world, (RandoRegionId)id, (RandoBossSlot)k) != RANDO_BOSS_NONE)
			fixture_fail(fx, "a boss in a slot past the region's boss slots");
	expect_field(fx, fx->field[6], n ? buf : "-", "boss slots");
	n = 0;
	buf[0] = '\0';
	for (k = 0; k < info->nitems; k++)
		n += (size_t)snprintf(buf + n, sizeof buf - n, "%s%s", k ? "|" : "",
				      item_name((RandoItemId)info->items[k]));
	expect_field(fx, fx->field[7], n ? buf : "-", "region items");
}

static void check_location_record(const Fixture *fx, RandoVariant variant, int id)
{
	const RandoLocationInfo *info;
	RandoWorld world;
	RandoConfig config;

	if (id >= RANDO_LOCATION_COUNT)
		fixture_fail(fx, "more locations in the dump than in the C table");
	info = rando_location_info((RandoLocationId)id);
	expect_field(fx, fx->field[2], info->name, "location name");
	if (rando_location_find(fx->field[2]) != (RandoLocationId)id)
		fixture_fail(fx, "rando_location_find does not return the location's id");
	expect_field(fx, fx->field[3], rando_region_info(info->region)->key, "location region");
	expect_field(fx, fx->field[4], location_class_names[info->location_class], "location class");
	rando_config_init(&config);
	rando_world_init(&world, variant, &config);
	expect_field(fx, fx->field[5], item_name(rando_world_item(&world, (RandoLocationId)id)), "constructor item");
}

static void check_region_spans(void)
{
	int r, l, next = 0;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const RandoRegionInfo *info = rando_region_info((RandoRegionId)r);

		if ((int)info->first_location != next)
			test_fail(TEST, "region %s does not start after the previous one's locations", info->key);
		for (l = 0; l < info->nlocations; l++)
			if (rando_location_info((RandoLocationId)(next + l))->region != (RandoRegionId)r)
				test_fail(TEST, "region %s spans a location of another region", info->key);
		next += info->nlocations;
	}
	if (next != RANDO_LOCATION_COUNT)
		test_fail(TEST, "the regions span %d locations, the table has %d", next, RANDO_LOCATION_COUNT);
}

/* Returns the number of region and location records checked. */
static int check_world_dump(void)
{
	int regions[RANDO_VARIANT_COUNT] = {0}, locations[RANDO_VARIANT_COUNT] = {0}, v, records = 0;
	Fixture fx;

	fixture_open(&fx, FIXTURE_WORLD_DUMP);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "region") == 0) {
			v = variant_of(&fx, fx.field[1]);
			check_region_record(&fx, (RandoVariant)v, regions[v]++);
		} else if (strcmp(fx.field[0], "location") == 0) {
			v = variant_of(&fx, fx.field[1]);
			check_location_record(&fx, (RandoVariant)v, locations[v]++);
		} else {
			continue;
		}
		records++;
	}
	fixture_close(&fx);
	for (v = 0; v < RANDO_VARIANT_COUNT; v++)
		if (regions[v] != RANDO_REGION_COUNT || locations[v] != RANDO_LOCATION_COUNT)
			test_fail(TEST, "variant %d: the dump has %d regions and %d locations, the C tables %d and %d",
				  v, regions[v], locations[v], RANDO_REGION_COUNT, RANDO_LOCATION_COUNT);
	check_region_spans();
	if (rando_location_find("No Such Location") != RANDO_LOCATION_NONE
	    || rando_region_find("Light World") != RANDO_REGION_NONE)
		test_fail(TEST, "a lookup of an unknown name does not return its NONE sentinel");
	return records;
}

/* The collect scenarios whose every step needs no region rule; chunk 4 enables the rest. */
static const char *const enabled_collect_scenarios[] = {NULL};

static int collect_enabled(const char *id)
{
	const char *const *e;

	for (e = enabled_collect_scenarios; *e; e++)
		if (strcmp(*e, id) == 0)
			return 1;
	return 0;
}

static void check_collected_set(const Fixture *fx, const RandoWorld *world, const char *want)
{
	char got[FIXTURE_LINE_MAX];
	size_t n = 0;
	int i, slot;

	got[0] = '\0';
	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		if (rando_world_collected(world, (RandoLocationId)i))
			n += (size_t)snprintf(got + n, sizeof got - n, "%s%s", n ? "|" : "",
					      rando_location_info((RandoLocationId)i)->name);
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			if (rando_world_shop_collected(world, (RandoShopId)i, slot))
				n += (size_t)snprintf(got + n, sizeof got - n, "%s%s - %d", n ? "|" : "",
						      rando_shop_info((RandoShopId)i)->name, slot);
	expect_field(fx, want, n ? got : "-", "collected locations");
}

static void check_items_equal(const Fixture *fx, const RandoItems *want, const RandoItems *got)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (want->counts[i] != got->counts[i] || want->values[i] != got->values[i]) {
			fprintf(stderr, "item %s: VT %u/%u, C %u/%u (counts/values)\n",
				rando_item_info((RandoItemId)i)->name, want->counts[i], want->values[i],
				got->counts[i], got->values[i]);
			fixture_fail(fx, "collected items differ from VT");
		}
}

static RandoLocationId location_of(const Fixture *fx, const char *name)
{
	RandoLocationId id = rando_location_find(name);

	if (id == RANDO_LOCATION_NONE)
		fixture_fail(fx, "unknown location");
	return id;
}

static RandoItemId item_of(const Fixture *fx, const char *name)
{
	RandoItemId id = rando_item_find(name);

	if (id == RANDO_ITEM_NONE)
		fixture_fail(fx, "unknown item");
	return id;
}

static void collect_scenario(const Fixture *fx, RandoWorld *world)
{
	char placements[FIXTURE_LINE_MAX];
	char *p = placements;
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(world, variant_of(fx, fx->field[2]), &config);
	strcpy(placements, fx->field[3]);
	while (strcmp(placements, "-") != 0 && p) {
		char *bar = strchr(p, '|'), *eq = strchr(p, '=');

		if (bar)
			*bar = '\0';
		if (!eq)
			fixture_fail(fx, "placement without '='");
		*eq = '\0';
		if (!rando_world_set_item(world, location_of(fx, p), item_of(fx, eq + 1)))
			fixture_fail(fx, "placement refused by its location");
		p = bar ? bar + 1 : NULL;
	}
	fixture_parse_items(fx, fx->field[4], fx->field[5], &world->pre_collected);
}

static void collect_step(const Fixture *fx, RandoWorld *world)
{
	const char *op = fx->field[3];

	if (strcmp(op, "collect") == 0) {
		RandoItems start, want, got;
		int has_start = strcmp(fx->field[4], "-") != 0;

		if (has_start)
			fixture_parse_items(fx, fx->field[4], fx->field[5], &start);
		rando_world_collect_items(world, has_start ? &start : NULL, &got);
		fixture_parse_items(fx, fx->field[7], fx->field[8], &want);
		check_items_equal(fx, &want, &got);
	} else if (strcmp(op, "reset") == 0) {
		rando_world_reset_collected(world);
	} else if (strcmp(op, "place") == 0) {
		if (!rando_world_set_item(world, location_of(fx, fx->field[4]), item_of(fx, fx->field[5])))
			fixture_fail(fx, "placement refused by its location");
	} else if (strcmp(op, "fill") == 0) {
		RandoItems none;

		rando_items_clear(&none);
		if (rando_world_fill(world, location_of(fx, fx->field[4]), item_of(fx, fx->field[5]), &none)
		    != atoi(fx->field[6]))
			fixture_fail(fx, "fill answer differs from VT");
	} else {
		fixture_fail(fx, "unknown collect step");
	}
	check_collected_set(fx, world, fx->field[9]);
}

/* Returns the number of collect steps replayed and stores the number of collect_step records in *total. */
static int check_collect_vectors(int *total)
{
	RandoWorld world;
	Fixture fx;
	int steps = 0, enabled = 0;

	*total = 0;
	fixture_open(&fx, FIXTURE_NOGLITCHES_ROWS);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "collect_scenario") == 0) {
			enabled = collect_enabled(fx.field[1]);
			if (enabled)
				collect_scenario(&fx, &world);
		} else if (strcmp(fx.field[0], "collect_step") == 0) {
			(*total)++;
			if (!enabled)
				continue;
			collect_step(&fx, &world);
			steps++;
		}
	}
	fixture_close(&fx);
	return steps;
}

static int region_locked_items(const char *file, const char *method)
{
	(void)file;
	return strcmp(method, "testRegionLockedItems") == 0;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	int rows, total;

	check_init();
	check_location_classes();
	check_set_item();
	check_region_can_fill();
	check_can_fill_and_fill();
	check_collect_items();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d world dump records checked\n", check_world_dump());
	rows = fixture_replay_rows(TEST, region_locked_items, &total);
	if (rows != 360)
		test_fail(TEST, "%d testRegionLockedItems rows replayed, the four classes have 360", rows);
	printf(TEST ": %d of %d NoGlitches rows replayed (testRegionLockedItems)\n", rows, total);
	rows = check_collect_vectors(&total);
	printf(TEST ": %d of %d collectItems steps replayed\n", rows, total);
	return 0;
}
