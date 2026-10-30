#include "rando.h"
#include "rando_case.h"
#include "rando_spoiler.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_spoiler_json"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define TEXT_MAX (128 * 1024)

static RandoGen gen;
static char golden[TEXT_MAX], spoiler[TEXT_MAX];
static char golden_block[TEXT_MAX], spoiler_block[TEXT_MAX];

static void read_stream(FILE *f, const char *what, char text[TEXT_MAX])
{
	size_t n = fread(text, 1, TEXT_MAX, f);

	if (ferror(f) || n == TEXT_MAX)
		test_fail(TEST, "%s: read error or longer than %d bytes", what, TEXT_MAX - 1);
	text[n] = '\0';
}

/* The generator's world for <case_dir>, stopped after its last ported stage, written by the writer. */
static void write_case(const char *case_dir)
{
	RandoCase c;
	char error[RANDO_CASE_ERROR_MAX];
	FILE *f;

	if (!rando_case_read(case_dir, &c, error))
		test_fail(TEST, "%s rejected: %s", case_dir, error);
	CHECK(rando_generate(&gen, &c.options, c.seed, 1, NULL, NULL) == RANDO_INCOMPLETE);
	f = tmpfile();
	CHECK(f != NULL);
	CHECK(rando_spoiler_write_json(f, &gen.world));
	rewind(f);
	read_stream(f, case_dir, spoiler);
	fclose(f);
}

static void read_golden(const char *case_dir)
{
	char path[512];
	FILE *f;

	snprintf(path, sizeof path, "%s/spoiler.json", case_dir);
	f = fopen(path, "rb");
	if (!f)
		test_fail(TEST, "cannot open %s", path);
	read_stream(f, path, golden);
	fclose(f);
}

/* run_e2e.sh's section_block: from `    "<name>": ` to the first line closing at four spaces, that
 * line without its trailing comma. Empty when the section is missing. */
static void section_block(const char *text, const char *name, char out[TEXT_MAX])
{
	char head[128];
	const char *p = text, *end;
	size_t len = 0, n;

	snprintf(head, sizeof head, "    \"%s\": ", name);
	out[0] = '\0';
	while (*p && strncmp(p, head, strlen(head)) != 0) {
		p = strchr(p, '\n');
		p = p ? p + 1 : "";
	}
	while (*p) {
		int closing = strncmp(p, "    }", 5) == 0 || strncmp(p, "    ]", 5) == 0;

		end = strchr(p, '\n');
		n = end ? (size_t)(end - p) : strlen(p);
		if (closing && n > 0 && p[n - 1] == ',')
			n--;
		CHECK(len + n + 2 < TEXT_MAX);
		memcpy(out + len, p, n);
		len += n;
		out[len++] = '\n';
		out[len] = '\0';
		if (closing || !end)
			return;
		p = end + 1;
	}
}

static void check_section(const char *case_dir, const char *name)
{
	section_block(golden, name, golden_block);
	section_block(spoiler, name, spoiler_block);
	CHECK(golden_block[0] != '\0');
	if (strcmp(golden_block, spoiler_block) != 0)
		test_fail(TEST, "%s: section %s\n%s\nexpected (golden)\n%s", case_dir, name, spoiler_block,
			  golden_block);
}

/* The Shops block is fully decided by setShops: it must equal the golden's. */
static void check_shops(const char *case_dir)
{
	write_case(case_dir);
	read_golden(case_dir);
	check_section(case_dir, "Shops");
}

/* Each line cut after its key: the golden's values come from stages not ported yet. */
static void keys_only(char *block)
{
	char *line = block, *w = block;

	while (*line) {
		char *end = strchr(line, '\n'), *sep = strstr(line, "\": ");
		size_t n = (sep && sep < end) ? (size_t)(sep - line) + 1 : (size_t)(end - line);

		memmove(w, line, n);
		w += n;
		*w++ = '\n';
		line = end + 1;
	}
	*w = '\0';
}

/* Every region section of the golden: same locations, in the same order, as the C file. */
static void check_region_layout(const char *case_dir)
{
	static const char *const regions[] = {
		"Castle Tower", "Dark Palace", "Dark World", "Death Mountain", "Desert Palace", "Eastern Palace",
		"Ganons Tower", "Hyrule Castle", "Ice Palace", "Light World", "Misery Mire", "Skull Woods",
		"Special", "Swamp Palace", "Thieves Town", "Tower Of Hera", "Turtle Rock",
	};
	size_t i;

	write_case(case_dir);
	read_golden(case_dir);
	for (i = 0; i < sizeof regions / sizeof regions[0]; i++) {
		section_block(golden, regions[i], golden_block);
		section_block(spoiler, regions[i], spoiler_block);
		CHECK(golden_block[0] != '\0');
		keys_only(golden_block);
		keys_only(spoiler_block);
		if (strcmp(golden_block, spoiler_block) != 0)
			test_fail(TEST, "%s: section %s keys\n%s\nexpected (golden)\n%s", case_dir, regions[i],
				  spoiler_block, golden_block);
	}
}

/* setMedallions decided both medallions (golden lines); setFountains has not run. */
static void check_special(void)
{
	write_case("cases/baseline");
	section_block(spoiler, "Special", spoiler_block);
	if (strcmp(spoiler_block, "    \"Special\": {\n"
				 "        \"Misery Mire Medallion:1\": \"Ether:1\",\n"
				 "        \"Pyramid Bottle:1\": \"Nothing\",\n"
				 "        \"Turtle Rock Medallion:1\": \"Ether:1\",\n"
				 "        \"Waterfall Bottle:1\": \"Nothing\"\n"
				 "    }\n") != 0)
		test_fail(TEST, "baseline: section Special\n%s", spoiler_block);
	write_case("cases/crystals_ganon-random");
	CHECK(strstr(spoiler, "        \"Misery Mire Medallion:1\": \"Quake:1\",\n") != NULL);
	CHECK(strstr(spoiler, "        \"Turtle Rock Medallion:1\": \"Ether:1\",\n") != NULL);
}

static void check_file_layout(void)
{
	size_t len;

	write_case("cases/baseline");
	len = strlen(spoiler);
	CHECK(strncmp(spoiler, "{\n    \"Castle Tower\": {\n", 24) == 0);
	CHECK(len >= 3 && strcmp(spoiler + len - 3, "\n}\n") == 0);
	CHECK(strstr(spoiler, "\n    \"Shops\": [\n") < strstr(spoiler, "\n    \"Skull Woods\": {\n"));
	CHECK(strstr(spoiler, "\n    \"Skull Woods\": {\n") < strstr(spoiler, "\n    \"Special\": {\n"));
	CHECK(strstr(spoiler, "Agahnim:1") == NULL);
	CHECK(strstr(spoiler, "Agahnim 2:1") == NULL);
}

static void write_world(const RandoWorld *world)
{
	FILE *f = tmpfile();

	CHECK(f != NULL);
	CHECK(rando_spoiler_write_json(f, world));
	rewind(f);
	read_stream(f, "world", spoiler);
	fclose(f);
}

/* A location writes its item's getTarget(), a shop slot its item's own name. */
static void check_alias_names(void)
{
	static RandoWorld world;

	write_case("cases/baseline");
	world = gen.world;
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_UncleSword));
	rando_world_shop_add(&world, RANDO_SHOP_CAPACITY_UPGRADE, 0, RANDO_ITEM_UncleSword, 7);
	write_world(&world);
	CHECK(strstr(spoiler, "        \"Link's Uncle:1\": \"ProgressiveSword:1\",\n") != NULL);
	CHECK(strstr(spoiler, "            \"item_0\": {\n"
			      "                \"item\": \"UncleSword:1\",\n"
			      "                \"price\": 7\n"
			      "            },\n") != NULL);
}

/* VT's constructor leaves every shop inactive: no Shops key at all. A take-any writes its own type. */
static void check_no_active_shop(void)
{
	static RandoWorld world;
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	write_world(&world);
	CHECK(strstr(spoiler, "\"Shops\"") == NULL);
	CHECK(strstr(spoiler, "        \"Turtle Rock Medallion:1\": \"Nothing\",\n") != NULL);

	CHECK(rando_shop_info(RANDO_SHOP_LONG_FAIRY_CAVE)->kind == RANDO_SHOP_KIND_TAKE_ANY);
	rando_world_shop_set_active(&world, RANDO_SHOP_LONG_FAIRY_CAVE, true);
	write_world(&world);
	CHECK(strstr(spoiler, "    \"Shops\": [\n"
			      "        {\n"
			      "            \"location\": \"Long Fairy Cave\",\n"
			      "            \"type\": \"Take Any\"\n"
			      "        }\n"
			      "    ],\n") != NULL);
}

int main(void)
{
	check_shops("cases/baseline");
	check_shops("cases/item_pool-hard");
	check_special();
	check_region_layout("cases/baseline");
	check_file_layout();
	check_alias_names();
	check_no_active_shop();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
