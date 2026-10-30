#include "rando_prng.h"

#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTORS "oracle/prng_vectors.txt"
#define CASES "cases"

static void fail(const char *what, const char *line)
{
	fprintf(stderr, "test_prng: FAIL %s: %s\n", what, line);
	exit(1);
}

/* Fills out with the numbers after the kind word and returns their count; exits past max. */
static int parse_fields(const char *line, int64_t *out, int max)
{
	const char *p = strchr(line, ' ');
	int n = 0;

	while (p && *p) {
		char *end;

		if (n == max)
			fail("too many fields", line);
		out[n++] = strtoll(p, &end, 10);
		if (end == p)
			fail("malformed field", line);
		p = end;
	}
	return n;
}

static void check_next(const char *line)
{
	int64_t v[16];
	int n = parse_fields(line, v, 16);
	RandoPrng prng = {0};
	int i;

	if (n < 2)
		fail("too few fields", line);
	rando_prng_seed(&prng, (uint32_t)v[0]);
	for (i = 1; i < n; i++)
		if (rando_prng_next(&prng) != (uint32_t)v[i])
			fail("vector", line);
}

static void check_range(const char *line)
{
	int64_t v[16];
	int n = parse_fields(line, v, 16);
	RandoPrng prng = {0};
	int i;

	if (n < 4)
		fail("too few fields", line);
	rando_prng_seed(&prng, (uint32_t)v[0]);
	for (i = 3; i < n; i++)
		if (rando_random_int(&prng, v[1], v[2]) != v[i])
			fail("vector", line);
}

static int check_vectors(void)
{
	FILE *f = fopen(VECTORS, "r");
	char line[512];
	int checked = 0;

	if (!f)
		fail("cannot open", VECTORS);
	while (fgets(line, sizeof line, f)) {
		uint32_t x, want;

		line[strcspn(line, "\n")] = '\0';
		if (line[0] == '#' || line[0] == '\0')
			continue;
		if (sscanf(line, "splitmix32 %" SCNu32 " %" SCNu32, &x, &want) == 2) {
			if (rando_splitmix32(x) != want)
				fail("vector", line);
		} else if (strncmp(line, "next ", 5) == 0) {
			check_next(line);
		} else if (strncmp(line, "range ", 6) == 0) {
			check_range(line);
		} else {
			fail("unknown vector kind", line);
		}
		checked++;
	}
	fclose(f);
	return checked;
}

/* Returns the number of draws replayed; 0 when the case has no trace.txt. */
static long replay_case(const char *name)
{
	char path[512], line[512];
	FILE *f;
	uint32_t seed;
	RandoPrng prng = {0};
	long attempt = -1, draws = 0;

	snprintf(path, sizeof path, CASES "/%s/seed", name);
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (fscanf(f, "%" SCNu32, &seed) != 1)
		fail("malformed seed", path);
	fclose(f);

	snprintf(path, sizeof path, CASES "/%s/trace.txt", name);
	f = fopen(path, "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof line, f)) {
		long a, draw, want_draw = 0;
		int64_t min, max, r;

		line[strcspn(line, "\n")] = '\0';
		if (sscanf(line, "%ld %ld %*s %" SCNd64 " %" SCNd64 " -> %" SCNd64,
			   &a, &draw, &min, &max, &r) != 5)
			fail("malformed trace line", line);
		if (a != attempt) {
			attempt = a;
			rando_prng_seed(&prng, rando_splitmix32(seed + (uint32_t)a));
			want_draw = 1;
		} else {
			want_draw = draws + 1;
		}
		if (draw != want_draw)
			fail("unexpected draw number", line);
		draws = draw;
		if (rando_random_int(&prng, min, max) != r) {
			fprintf(stderr, "test_prng: case %s\n", name);
			fail("golden draw", line);
		}
	}
	fclose(f);
	return draws;
}

static long replay_goldens(int *cases)
{
	DIR *dir = opendir(CASES);
	struct dirent *e;
	long total = 0;

	if (!dir)
		fail("cannot open", CASES);
	*cases = 0;
	while ((e = readdir(dir))) {
		long n;

		if (e->d_name[0] == '.')
			continue;
		n = replay_case(e->d_name);
		if (n > 0) {
			total += n;
			(*cases)++;
		}
	}
	closedir(dir);
	if (*cases == 0)
		fail("no golden trace under", CASES);
	return total;
}

typedef struct Seen {
	int count;
	int64_t draw[8][3];
} Seen;

static void record(void *ctx, int64_t min, int64_t max, int64_t r)
{
	Seen *seen = ctx;

	if (seen->count == 8)
		fail("observer", "more draws than made");
	seen->draw[seen->count][0] = min;
	seen->draw[seen->count][1] = max;
	seen->draw[seen->count][2] = r;
	seen->count++;
}

static void check_observer(void)
{
	static const int64_t range[4][2] = {{0, 6}, {3, 3}, {-5, 5}, {0, 4294967295}};
	Seen seen = {0};
	RandoPrng prng = {0};
	int64_t got[4];
	int i;

	prng.observe = record;
	prng.observe_ctx = &seen;
	rando_prng_seed(&prng, 1);
	for (i = 0; i < 4; i++)
		got[i] = rando_random_int(&prng, range[i][0], range[i][1]);
	if (seen.count != 4)
		fail("observer", "not called once per draw");
	for (i = 0; i < 4; i++)
		if (seen.draw[i][0] != range[i][0] || seen.draw[i][1] != range[i][1] ||
		    seen.draw[i][2] != got[i])
			fail("observer", "wrong (min, max, r)");
}

static void check_reseed(void)
{
	Seen seen = {0};
	RandoPrng prng = {0};
	uint32_t first[3];
	int i;

	prng.observe = record;
	prng.observe_ctx = &seen;
	rando_prng_seed(&prng, 3735928559u);
	for (i = 0; i < 3; i++)
		first[i] = rando_prng_next(&prng);
	rando_random_int(&prng, 0, 9);
	rando_prng_seed(&prng, 3735928559u);
	for (i = 0; i < 3; i++)
		if (rando_prng_next(&prng) != first[i])
			fail("reseed", "sequence not reset");
	rando_random_int(&prng, 0, 9);
	if (seen.count != 2 || seen.draw[0][2] != seen.draw[1][2])
		fail("reseed", "observer lost or draw differs");
}

int main(void)
{
	int vectors = check_vectors();
	int cases;
	long draws = replay_goldens(&cases);

	check_observer();
	check_reseed();

	printf("test_prng: %d vectors, %ld golden draws in %d cases ok\n", vectors, draws, cases);
	return 0;
}
