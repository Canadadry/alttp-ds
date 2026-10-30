#include "rando_spoiler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Equipped, meta and playthrough (step 2.4). */

#define KEY_MAX 96
#define WORLD_SUFFIX ":1"

typedef struct Entry {
	char key[KEY_MAX];
	RandoItemId item;
} Entry;

static int entry_cmp(const void *a, const void *b)
{
	return strcmp(((const Entry *)a)->key, ((const Entry *)b)->key);
}

static int name_cmp(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void indent(FILE *out, int level)
{
	fprintf(out, "%*s", 4 * level, "");
}

/* json_encode with JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE; suffix is appended inside the quotes. */
static void string(FILE *out, const char *s, const char *suffix)
{
	fputc('"', out);
	for (; *s; s++) {
		assert((unsigned char)*s >= 0x20 && "spoiler: control characters in names are not encoded");
		if (*s == '"' || *s == '\\')
			fputc('\\', out);
		fputc(*s, out);
	}
	fputs(suffix, out);
	fputc('"', out);
}

static void key(FILE *out, int level, const char *s)
{
	indent(out, level);
	string(out, s, "");
	fputs(": ", out);
}

static bool spoiler_skips(RandoLocationClass location_class)
{
	return rando_location_is_a(location_class, RANDO_LOCATION_CLASS_PRIZE_EVENT) ||
	       rando_location_is_a(location_class, RANDO_LOCATION_CLASS_TRADE);
}

/* VT's getTarget()->getName(). */
static const char *target_name(RandoItemId item)
{
	const RandoItemInfo *info = rando_item_info(item);

	return info->item_class == RANDO_ITEM_CLASS_ALIAS ? rando_item_info(info->alias_target)->name : info->name;
}

/* Every region named spoiler_name merges into one object. TODO: rom.genericKeys writes `Key` for a
 * key (state-retro). */
static void write_region_section(FILE *out, const RandoWorld *world, const char *spoiler_name)
{
	static Entry entries[RANDO_LOCATION_COUNT];
	int n = 0, r, i;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const RandoRegionInfo *region = rando_region_info((RandoRegionId)r);

		if (strcmp(region->spoiler_name, spoiler_name) != 0)
			continue;
		for (i = 0; i < region->nlocations; i++) {
			RandoLocationId id = (RandoLocationId)(region->first_location + i);
			const RandoLocationInfo *location = rando_location_info(id);
			int len;

			if (spoiler_skips(location->location_class))
				continue;
			len = snprintf(entries[n].key, KEY_MAX, "%s" WORLD_SUFFIX, location->name);
			assert(len > 0 && len < KEY_MAX && "spoiler: raise KEY_MAX for this location name");
			entries[n++].item = rando_world_item(world, id);
		}
	}
	if (n == 0) {
		fputs("[]", out);
		return;
	}
	qsort(entries, (size_t)n, sizeof entries[0], entry_cmp);
	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, entries[i].key);
		if (entries[i].item == RANDO_ITEM_NONE)
			string(out, "Nothing", "");
		else
			string(out, target_name(entries[i].item), WORLD_SUFFIX);
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

static bool any_shop_active(const RandoWorld *world)
{
	int s;

	for (s = 0; s < RANDO_SHOP_COUNT; s++)
		if (rando_world_shop_active(world, (RandoShopId)s))
			return true;
	return false;
}

/* Keys item_<slot>, location, type are already in strcmp order. Shop items are written with
 * getName(), not getTarget(), as VT's shop loop does. */
static void write_shops(FILE *out, const RandoWorld *world)
{
	bool first = true;
	int s, slot;

	fputs("[\n", out);
	for (s = 0; s < RANDO_SHOP_COUNT; s++) {
		RandoShopId id = (RandoShopId)s;
		const RandoShopInfo *shop = rando_shop_info(id);

		if (!rando_world_shop_active(world, id))
			continue;
		if (!first)
			fputs(",\n", out);
		first = false;
		indent(out, 2);
		fputs("{\n", out);
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++) {
			RandoItemId item = rando_world_shop_item(world, id, slot);
			char name[16];

			if (item == RANDO_ITEM_NONE)
				continue;
			snprintf(name, sizeof name, "item_%d", slot);
			key(out, 3, name);
			fputs("{\n", out);
			key(out, 4, "item");
			string(out, rando_item_info(item)->name, WORLD_SUFFIX);
			fputs(",\n", out);
			key(out, 4, "price");
			fprintf(out, "%d\n", rando_world_shop_price(world, id, slot));
			indent(out, 3);
			fputs("},\n", out);
		}
		key(out, 3, "location");
		string(out, shop->name, "");
		fputs(",\n", out);
		key(out, 3, "type");
		string(out, shop->kind == RANDO_SHOP_KIND_TAKE_ANY ? "Take Any" : "Shop", "");
		fputc('\n', out);
		indent(out, 2);
		fputc('}', out);
	}
	fputc('\n', out);
	indent(out, 1);
	fputc(']', out);
}

typedef struct BossKey {
	const char *key;
	RandoRegionId region;
	RandoBossSlot slot;
	const char *literal;
} BossKey;

/* VT's getSpoiler Bosses keys, already in strcmp order. Hyrule Castle, Ganons Tower and Ganon are
 * literals in VT, not world reads. */
static const BossKey boss_keys[] = {
	{"Desert Palace", RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Eastern Palace", RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Ganon", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Ganon"},
	{"Ganons Tower", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Agahnim 2"},
	{"Ganons Tower Basement", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, NULL},
	{"Ganons Tower Middle", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, NULL},
	{"Ganons Tower Top", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, NULL},
	{"Hyrule Castle", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Agahnim"},
	{"Ice Palace", RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Misery Mire", RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Palace Of Darkness", RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Skull Woods", RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Swamp Palace", RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Thieves Town", RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Tower Of Hera", RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Turtle Rock", RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, NULL},
};

static const char *boss_key_name(const RandoWorld *world, const BossKey *k)
{
	RandoBossId boss;

	if (k->region == RANDO_REGION_NONE)
		return k->literal;
	boss = rando_world_boss(world, k->region, k->slot);
	assert(boss != RANDO_BOSS_NONE && "spoiler: Bosses written before placeBosses filled every slot");
	return rando_boss_name(boss);
}

static void write_bosses(FILE *out, const RandoWorld *world)
{
	size_t i, n = sizeof boss_keys / sizeof boss_keys[0];

	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, boss_keys[i].key);
		string(out, boss_key_name(world, &boss_keys[i]), "");
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

bool rando_spoiler_write_json(FILE *out, const RandoWorld *world)
{
	static const char shops[] = "Shops", bosses[] = "Bosses";
	const char *sections[RANDO_REGION_COUNT + 2];
	int n = 0, r, i;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const char *name = rando_region_info((RandoRegionId)r)->spoiler_name;

		for (i = 0; i < n && strcmp(sections[i], name) != 0; i++)
			;
		if (i == n)
			sections[n++] = name;
	}
	if (any_shop_active(world))
		sections[n++] = shops;
	sections[n++] = bosses;
	qsort(sections, (size_t)n, sizeof sections[0], name_cmp);

	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 1, sections[i]);
		if (sections[i] == shops)
			write_shops(out, world);
		else if (sections[i] == bosses)
			write_bosses(out, world);
		else
			write_region_section(out, world, sections[i]);
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	fputs("}\n", out);
	return fflush(out) == 0 && !ferror(out);
}
