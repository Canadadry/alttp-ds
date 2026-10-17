/*
 * Homebrew has no cartridge backup. melonDS's homebrew cart is DLDI only,
 * and a flashcart stores the save as a file next to the ROM. This is that
 * file: 8192 raw bytes, the same image zelda3 keeps in g_zenv.sram.
 *
 * The engine still calls ZeldaReadSram / ZeldaWriteSram (name entry, copy,
 * erase, and the in-game save). The link wraps those onto this file.
 */
#include <nds.h>
#include <calico/nds/env.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "zelda_rtl.h"
#include "zelda_save.h"

enum { kSramBytes = 8192 };

static char g_save_path[256] = "fat:/lttp-ds.sav";
static const char *g_status = "off";

const char *ZeldaSaveStatus(void)
{
	return g_status;
}

static int is_fat_path(const char *path)
{
	return strncmp(path, "fat:", 4) == 0 || strncmp(path, "sd:", 3) == 0;
}

static int ends_with(const char *path, const char *suffix)
{
	size_t n = strlen(path);
	size_t m = strlen(suffix);

	if (n < m)
		return 0;
	return memcmp(path + n - m, suffix, m) == 0;
}

/* melonDS direct-boot writes the magic and this string only. The argc/argv
 * pointers in the newer header are not filled in, so they must not be followed. */
static int ram_ptr(const void *p)
{
	uintptr_t a = (uintptr_t)p;

	return a >= 0x02000000u && a < 0x03000000u;
}

static void set_sav_from_nds(const char *nds)
{
	size_t n = strlen(nds);

	if (n < 4 || n >= sizeof g_save_path || !ends_with(nds, ".nds"))
		return;
	memcpy(g_save_path, nds, n - 3);
	memcpy(g_save_path + n - 3, "sav", 4);
}

static void choose_path(void)
{
	const EnvNdsArgvHeader *argv = g_envNdsArgvHeader;
	char cwd[256];

	if (argv->magic == ENV_NDS_ARGV_MAGIC && ram_ptr(argv->args_str) &&
	    is_fat_path(argv->args_str)) {
		set_sav_from_nds(argv->args_str);
		return;
	}

	if (getcwd(cwd, sizeof cwd) && is_fat_path(cwd)) {
		size_t n = strlen(cwd);
		int slash = n > 0 && cwd[n - 1] == '/';

		snprintf(g_save_path, sizeof g_save_path, "%s%slttp-ds.sav", cwd, slash ? "" : "/");
	}
}

static int read_sram(const char *path)
{
	FILE *f = fopen(path, "rb");
	size_t n;
	uint8 tmp[kSramBytes];

	if (!f)
		return 0;
	n = fread(tmp, 1, kSramBytes, f);
	fclose(f);
	if (n != kSramBytes)
		return 0;
	memcpy(g_zenv.sram, tmp, kSramBytes);
	return 1;
}

void __wrap_ZeldaWriteSram(void);

static int write_sram(const char *path)
{
	FILE *f = fopen(path, "wb");
	size_t n;

	if (!f)
		return 0;
	n = fwrite(g_zenv.sram, 1, kSramBytes, f);
	if (fflush(f) != 0)
		n = 0;
	if (fclose(f) != 0)
		n = 0;
	return n == kSramBytes;
}

void __wrap_ZeldaReadSram(void)
{
	static const char *legacy[] = {
		"fat:/saves/sram.dat",
		"sd:/saves/sram.dat",
		"saves/sram.dat",
		NULL
	};
	int i;

	if (read_sram(g_save_path)) {
		g_status = "loaded";
		return;
	}
	for (i = 0; legacy[i]; i++) {
		if (!read_sram(legacy[i]))
			continue;
		g_status = "loaded";
		return;
	}
	g_status = "new";
}

void __wrap_ZeldaWriteSram(void)
{
	char tmp[sizeof g_save_path + 4];

	snprintf(tmp, sizeof tmp, "%s.tmp", g_save_path);
	if (!write_sram(tmp)) {
		g_status = "fail";
		iprintf("Save write failed:\n%s\n", g_save_path);
		return;
	}
	remove(g_save_path);
	if (rename(tmp, g_save_path) != 0) {
		if (!write_sram(g_save_path)) {
			g_status = "fail";
			iprintf("Save write failed:\n%s\n", g_save_path);
			return;
		}
		remove(tmp);
	}
	g_status = "saved";
}

void ZeldaSaveMount(void)
{
	char cwd[64];

	choose_path();
	/* Reading only. A write here never returns on melonDS: the DLDI
	 * write transfer leaves the cart busy and the top screen stays blank. */
	if (!is_fat_path(g_save_path) &&
	    !(getcwd(cwd, sizeof cwd) && is_fat_path(cwd))) {
		g_status = "off";
		iprintf("Save: no SD\n");
		return;
	}

	iprintf("Save: reading\n");
	__wrap_ZeldaReadSram();
	iprintf("Save: %s\n", g_status);
}
