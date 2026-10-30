#include "rando.h"
#include "rando_case.h"
#include "rando_out.h"

#include <stdio.h>

enum {
	EXIT_COMPLETE = 0,
	EXIT_INPUT = 1,
	EXIT_USAGE = 2,
	EXIT_INCOMPLETE = 3,
	EXIT_UNSUPPORTED = 4,
	EXIT_GAVE_UP = 5,
};

#define ORACLE_WATCHDOG 1000u

static RandoGen gen;

int main(int argc, char **argv)
{
	RandoCase c;
	RandoTraceFile trace;
	RandoStatus status;
	char case_error[RANDO_CASE_ERROR_MAX], out_error[RANDO_OUT_ERROR_MAX];
	uint32_t cap;

	if (argc != 3) {
		fprintf(stderr, "usage: rando_gen <case_dir> <out_dir>\n");
		return EXIT_USAGE;
	}
	if (!rando_case_read(argv[1], &c, case_error)) {
		fprintf(stderr, "rando_gen: %s\n", case_error);
		return EXIT_INPUT;
	}
	/* The golden's attempts file holds the 0-based index of its winning attempt. */
	cap = !c.has_attempts ? ORACLE_WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
	if (!rando_trace_open(&trace, argv[2], out_error)) {
		fprintf(stderr, "rando_gen: %s\n", out_error);
		return EXIT_INPUT;
	}
	status = rando_generate(&gen, &c.options, c.seed, cap, rando_trace_write, &trace);
	if (!rando_trace_close(&trace, out_error)) {
		fprintf(stderr, "rando_gen: %s\n", out_error);
		return EXIT_INPUT;
	}
	switch (status) {
	case RANDO_COMPLETE:
		if (!rando_attempts_write(argv[2], gen.attempt, out_error)) {
			fprintf(stderr, "rando_gen: %s\n", out_error);
			return EXIT_INPUT;
		}
		return EXIT_COMPLETE;
	case RANDO_INCOMPLETE:
		return EXIT_INCOMPLETE;
	case RANDO_UNSUPPORTED:
		return EXIT_UNSUPPORTED;
	case RANDO_GAVE_UP:
		fprintf(stderr, "rando_gen: gave up after %u attempts\n", (unsigned)cap);
		return EXIT_GAVE_UP;
	}
	return EXIT_INPUT;
}
