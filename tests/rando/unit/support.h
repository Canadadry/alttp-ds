#ifndef RANDO_TEST_SUPPORT_H
#define RANDO_TEST_SUPPORT_H

#include <stdio.h>

#define FIXTURE_WORLD_DUMP "oracle/world_dump.txt"
#define FIXTURE_PREDICATE_VECTORS "oracle/predicate_vectors.txt"
#define FIXTURE_NOGLITCHES_ROWS "oracle/noglitches_rows.txt"

#define FIXTURE_LINE_MAX 8192
#define FIXTURE_FIELDS_MAX 32

typedef struct Fixture {
	FILE *file;
	const char *path;
	long line_no;
	char line[FIXTURE_LINE_MAX];
	char record[FIXTURE_LINE_MAX];
	char *field[FIXTURE_FIELDS_MAX];
	int nfields;
} Fixture;

/* Returns 1 for `--fixtures`, 0 with no argument; exits 2 on anything else. */
int test_fixtures_mode(const char *test, int argc, char **argv);

void test_fail(const char *test, const char *fmt, ...);

/* Exits 1 when the file cannot be opened. */
void fixture_open(Fixture *fx, const char *path);

/* Returns 0 at the end of the file, else 1 with field[0..nfields) pointing into fx (valid until the next call).
 * Skips empty lines and `#` comments; exits 1 on a line longer than FIXTURE_LINE_MAX. */
int fixture_next(Fixture *fx);

/* Prints the file, line number and the record as read, then exits 1. */
void fixture_fail(const Fixture *fx, const char *why);

void fixture_close(Fixture *fx);

#endif
