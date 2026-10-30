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
