#include "rando_world.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_regions"

/* The tests/NoGlitches files whose rows are replayed; each chunk of the region port adds its own,
 * and the last one removes the list. */
static const char *const enabled_files[] = {NULL};

static int enabled(const char *file, const char *method)
{
	const char *const *e;

	(void)method;
	for (e = enabled_files; *e; e++)
		if (strcmp(*e, file) == 0)
			return 1;
	return 0;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	int rows, total;

	printf(TEST ": no hand-written case yet\n");
	if (!fixtures)
		return 0;
	rows = fixture_replay_rows(TEST, enabled, &total);
	printf(TEST ": %d of %d NoGlitches rows replayed\n", rows, total);
	return 0;
}
