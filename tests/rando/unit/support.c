#include "support.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int test_fixtures_mode(const char *test, int argc, char **argv)
{
	if (argc == 1)
		return 0;
	if (argc == 2 && strcmp(argv[1], "--fixtures") == 0)
		return 1;
	fprintf(stderr, "usage: %s [--fixtures]\n", test);
	exit(2);
}

void test_fail(const char *test, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: FAIL ", test);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void fixture_open(Fixture *fx, const char *path)
{
	memset(fx, 0, sizeof *fx);
	fx->path = path;
	fx->file = fopen(path, "r");
	if (!fx->file)
		test_fail("fixture", "cannot open %s", path);
}

int fixture_next(Fixture *fx)
{
	while (fgets(fx->line, sizeof fx->line, fx->file)) {
		size_t len = strlen(fx->line);
		char *p;

		fx->line_no++;
		if (len > 0 && fx->line[len - 1] == '\n')
			fx->line[--len] = '\0';
		else if (!feof(fx->file))
			test_fail("fixture", "%s:%ld: line longer than %d bytes", fx->path, fx->line_no,
				  FIXTURE_LINE_MAX - 2);
		if (fx->line[0] == '\0' || fx->line[0] == '#')
			continue;
		memcpy(fx->record, fx->line, len + 1);
		fx->nfields = 0;
		for (p = fx->line;;) {
			if (fx->nfields == FIXTURE_FIELDS_MAX)
				fixture_fail(fx, "too many fields");
			fx->field[fx->nfields++] = p;
			p = strchr(p, '\t');
			if (!p)
				break;
			*p++ = '\0';
		}
		return 1;
	}
	if (ferror(fx->file))
		test_fail("fixture", "%s: read error", fx->path);
	return 0;
}

void fixture_fail(const Fixture *fx, const char *why)
{
	fprintf(stderr, "fixture: FAIL %s:%ld: %s\n  %s\n", fx->path, fx->line_no, why, fx->record);
	exit(1);
}

void fixture_close(Fixture *fx)
{
	if (fx->file)
		fclose(fx->file);
	fx->file = NULL;
}

void fixture_parse_view(const Fixture *fx, const char *view, uint16_t out[RANDO_ITEM_COUNT],
			const uint16_t all[RANDO_ITEM_COUNT])
{
	char name[64];
	const char *p = view + 1;

	if (view[0] == 'E')
		memset(out, 0, RANDO_ITEM_COUNT * sizeof out[0]);
	else if (view[0] == 'A' && all)
		memcpy(out, all, RANDO_ITEM_COUNT * sizeof out[0]);
	else
		fixture_fail(fx, "view base is neither E nor a usable A");
	if (*p == '\0')
		return;
	if (*p != ':')
		fixture_fail(fx, "view base not followed by ':'");
	while (*p) {
		const char *eq = strchr(++p, '=');
		char *end;
		long count;
		RandoItemId id;

		if (!eq || (size_t)(eq - p) >= sizeof name)
			fixture_fail(fx, "malformed view entry");
		memcpy(name, p, (size_t)(eq - p));
		name[eq - p] = '\0';
		id = rando_item_find(name);
		if (id == RANDO_ITEM_NONE)
			fixture_fail(fx, "unknown item in view");
		count = strtol(eq + 1, &end, 10);
		if (end == eq + 1 || count < 0 || count > UINT16_MAX || (*end != '|' && *end != '\0'))
			fixture_fail(fx, "malformed view count");
		out[id] = (uint16_t)count;
		p = end;
	}
}

void fixture_parse_items(const Fixture *fx, const char *counts, const char *values, RandoItems *out)
{
	RandoItems all;

	rando_items_all(&all);
	fixture_parse_view(fx, counts, out->counts, all.counts);
	fixture_parse_view(fx, values, out->values, all.values);
}

#define PRED_COLUMNS_MAX 128
#define PRED_CONFIGS_MAX 8
#define PRED_MULTISETS_MAX 2048

typedef struct NamedConfig {
	char name[32];
	RandoConfig config;
} NamedConfig;

typedef struct EnumValue {
	const char *json;
	int value;
} EnumValue;

static const EnumValue states[] = {
	{"\"standard\"", RANDO_STATE_STANDARD}, {"\"open\"", RANDO_STATE_OPEN},
	{"\"inverted\"", RANDO_STATE_INVERTED}, {"\"retro\"", RANDO_STATE_RETRO}, {NULL, 0},
};
static const EnumValue weapons[] = {
	{"\"randomized\"", RANDO_WEAPONS_RANDOMIZED}, {"\"assured\"", RANDO_WEAPONS_ASSURED},
	{"\"vanilla\"", RANDO_WEAPONS_VANILLA}, {"\"swordless\"", RANDO_WEAPONS_SWORDLESS}, {NULL, 0},
};
static const EnumValue placements[] = {
	{"\"basic\"", RANDO_PLACEMENT_BASIC}, {"\"advanced\"", RANDO_PLACEMENT_ADVANCED}, {NULL, 0},
};
static const EnumValue enemy_healths[] = {
	{"\"default\"", RANDO_ENEMY_HEALTH_DEFAULT}, {"\"easy\"", RANDO_ENEMY_HEALTH_EASY},
	{"\"hard\"", RANDO_ENEMY_HEALTH_HARD}, {"\"expert\"", RANDO_ENEMY_HEALTH_EXPERT}, {NULL, 0},
};
static const EnumValue booleans[] = {{"true", 1}, {"false", 0}, {NULL, 0}};

/* Keys the predicate vectors carry that no item predicate or boss reads. */
static const char *const unread_keys[] = {
	"accessibility", "canBootsClip", "canBunnyRevive", "canBunnySurf", "canDungeonRevive",
	"canFakeFlipper", "canMirrorClip", "canMirrorWrap", "canOWYBA", "canOneFrameClipOW",
	"canOneFrameClipUW", "canSuperBunny", "canSuperSpeed", "canTransitionWrapped", "canWaterWalk",
	"crystals.ganon", "crystals.tower", "dungeonItems", "goal", "item.Goal.Required",
	"item.overflow.count.Shield", "item.require.Lamp", "logic", "region.bossNormalLocation",
	"region.cantTakeDamage", "region.forceSkullWoodsKey", "region.requireBetterBow",
	"region.requireBetterSword", "region.swordsInPool", "region.wildBigKeys", "region.wildCompasses",
	"region.wildKeys", "region.wildMaps", "rom.genericKeys", "rom.mapOnPickup", "rom.vanillaBigKeys",
	"rom.vanillaCompasses", "rom.vanillaKeys", "rom.vanillaMaps", NULL,
};

static int enum_value(const Fixture *fx, const EnumValue *values, const char *json)
{
	for (; values->json; values++)
		if (strcmp(values->json, json) == 0)
			return values->value;
	fixture_fail(fx, "unknown config value");
	return 0;
}

static void apply_config_key(const Fixture *fx, RandoConfig *c, const char *key, const char *json)
{
	const char *const *k;

	if (strcmp(json, "null") == 0)
		return;
	if (strcmp(key, "mode.state") == 0)
		c->mode_state = (RandoState)enum_value(fx, states, json);
	else if (strcmp(key, "mode.weapons") == 0)
		c->mode_weapons = (RandoWeapons)enum_value(fx, weapons, json);
	else if (strcmp(key, "itemPlacement") == 0)
		c->item_placement = (RandoItemPlacement)enum_value(fx, placements, json);
	else if (strcmp(key, "enemizer.enemyHealth") == 0)
		c->enemizer_enemy_health = (RandoEnemyHealth)enum_value(fx, enemy_healths, json);
	else if (strcmp(key, "ignoreCanKillEscapeThings") == 0)
		c->ignore_can_kill_escape_things = enum_value(fx, booleans, json);
	else if (strcmp(key, "rom.CatchableFairies") == 0)
		c->rom_catchable_fairies = enum_value(fx, booleans, json);
	else if (strcmp(key, "rom.rupeeBow") == 0)
		c->rom_rupee_bow = enum_value(fx, booleans, json);
	else if (strcmp(key, "rom.BottleFill.Magic") == 0)
		c->rom_bottle_fill_magic = atoi(json);
	else {
		for (k = unread_keys; *k && strcmp(*k, key) != 0; k++)
			;
		if (!*k)
			fixture_fail(fx, "config key neither read nor known as unread");
	}
}

static const RandoConfig *find_config(const Fixture *fx, const NamedConfig *configs, int n, const char *name)
{
	int i;

	for (i = 0; i < n; i++)
		if (strcmp(configs[i].name, name) == 0)
			return &configs[i].config;
	fixture_fail(fx, "unknown config name");
	return NULL;
}

int fixture_replay_predicates(const char *test, const PredicateColumn *columns, int ncolumns, long *values)
{
	static RandoItems multisets[PRED_MULTISETS_MAX];
	NamedConfig configs[PRED_CONFIGS_MAX];
	int column_of[PRED_COLUMNS_MAX];
	int nconfigs = 0, nmultisets = 0, ncols = -1, records = 0, i;
	Fixture fx;

	*values = 0;
	fixture_open(&fx, FIXTURE_PREDICATE_VECTORS);
	while (fixture_next(&fx)) {
		const char *kind = fx.field[0];

		if (strcmp(kind, "vector_config") == 0) {
			if (nconfigs == PRED_CONFIGS_MAX || strlen(fx.field[1]) >= sizeof configs[0].name)
				fixture_fail(&fx, "too many configs or name too long");
			strcpy(configs[nconfigs].name, fx.field[1]);
			rando_config_init(&configs[nconfigs++].config);
		} else if (strcmp(kind, "config") == 0) {
			RandoConfig *c = (RandoConfig *)find_config(&fx, configs, nconfigs, fx.field[1]);

			apply_config_key(&fx, c, fx.field[2], fx.field[3]);
		} else if (strcmp(kind, "columns") == 0) {
			char *p = fx.field[1];
			int found = 0;

			for (ncols = 0; p; ncols++) {
				char *bar = strchr(p, '|');

				if (ncols == PRED_COLUMNS_MAX)
					fixture_fail(&fx, "too many columns");
				if (bar)
					*bar = '\0';
				column_of[ncols] = -1;
				for (i = 0; i < ncolumns; i++)
					if (strcmp(columns[i].name, p) == 0) {
						column_of[ncols] = i;
						found++;
					}
				p = bar ? bar + 1 : NULL;
			}
			if (found != ncolumns)
				test_fail(test, "%d of the %d columns tested are missing from the fixture",
					  ncolumns - found, ncolumns);
		} else if (strcmp(kind, "multiset") == 0) {
			if (atoi(fx.field[1]) != nmultisets || nmultisets == PRED_MULTISETS_MAX)
				fixture_fail(&fx, "multiset index out of order or too many multisets");
			fixture_parse_items(&fx, fx.field[3], fx.field[4], &multisets[nmultisets++]);
		} else if (strcmp(kind, "pred") == 0) {
			int m = atoi(fx.field[1]);
			const RandoConfig *c = find_config(&fx, configs, nconfigs, fx.field[2]);
			char *p = fx.field[3];

			if (m < 0 || m >= nmultisets || ncols < 0)
				fixture_fail(&fx, "pred before its multiset or the columns record");
			for (i = 0; i < ncols; i++) {
				char *end;
				long want = strtol(p, &end, 10);
				int got;

				if (end == p)
					fixture_fail(&fx, "malformed value");
				p = end;
				if (column_of[i] < 0)
					continue;
				got = columns[column_of[i]].eval(&multisets[m], c);
				if (got != want) {
					fprintf(stderr, "%s: %s: VT %ld, C %d\n", test, columns[column_of[i]].name,
						want, got);
					fixture_fail(&fx, "predicate differs from VT");
				}
				(*values)++;
			}
			if (*p != '\0')
				fixture_fail(&fx, "more values than columns");
			records++;
		} else {
			fixture_fail(&fx, "unknown record kind");
		}
	}
	fixture_close(&fx);
	if (records == 0)
		test_fail(test, "no pred record replayed");
	return records;
}
