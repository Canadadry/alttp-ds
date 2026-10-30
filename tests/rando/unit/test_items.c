#include "support.h"

#include <stdio.h>
#include <string.h>

typedef struct Shape {
	const char *kind;
	int fields;
} Shape;

static const Shape world_dump[] = {
	{"item", 5}, {"all_items", 3}, {"boss", 2}, {"region", 8}, {"location", 6}, {NULL, 0},
};
static const Shape predicate_vectors[] = {
	{"vector_config", 3}, {"config", 4}, {"columns", 2}, {"multiset", 5}, {"pred", 4}, {NULL, 0},
};
static const Shape noglitches_rows[] = {
	{"row", 15}, {"collect_scenario", 6}, {"collect_step", 10}, {NULL, 0},
};

static int count_items(const char *s, char sep)
{
	int n = 1;

	for (; *s; s++)
		n += *s == sep;
	return n;
}

/* Returns the number of records; every record must have a known kind and its field count. */
static int check_shapes(const char *path, const Shape *shapes)
{
	Fixture fx;
	int records = 0, columns = -1;

	fixture_open(&fx, path);
	while (fixture_next(&fx)) {
		const Shape *s = shapes;

		while (s->kind && strcmp(s->kind, fx.field[0]) != 0)
			s++;
		if (!s->kind)
			fixture_fail(&fx, "unknown record kind");
		if (fx.nfields != s->fields)
			fixture_fail(&fx, "wrong field count");
		if (strcmp(s->kind, "columns") == 0)
			columns = count_items(fx.field[1], '|');
		if (strcmp(s->kind, "pred") == 0 && count_items(fx.field[3], ' ') != columns)
			fixture_fail(&fx, "value count differs from the columns record");
		records++;
	}
	fixture_close(&fx);
	if (records == 0)
		test_fail("test_items", "%s has no record", path);
	return records;
}

int main(int argc, char **argv)
{
	int records = 0;

	if (!test_fixtures_mode("test_items", argc, argv)) {
		printf("test_items: no hand-written case yet\n");
		return 0;
	}
	records += check_shapes(FIXTURE_WORLD_DUMP, world_dump);
	records += check_shapes(FIXTURE_PREDICATE_VECTORS, predicate_vectors);
	records += check_shapes(FIXTURE_NOGLITCHES_ROWS, noglitches_rows);
	printf("test_items: %d fixture records parsed\n", records);
	return 0;
}
