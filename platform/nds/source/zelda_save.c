/*
 * Homebrew has no cartridge backup. The save is an 8 KB file named
 * lttp-ds.sav in the root of the DLDI volume (FAT16, 8.3 name LTTP-DS.SAV).
 *
 * The engine calls ZeldaWriteSram from inside a frame (name entry, copy,
 * erase, in-game save). On melonDS that must not call the DLDI write
 * routine: libnds runs it on the ARM7, and a sector write there never
 * clears the cart busy bit, so the frame never returns. The wrap only
 * marks the image dirty. ZeldaSaveFlush, from the main loop, writes the
 * sectors itself and gives up if the cart stays busy.
 */
#include <nds.h>
#include <calico/dev/blk.h>
#include <calico/nds/env.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "zelda_rtl.h"
#include "zelda_save.h"

enum { kSramBytes = 8192, kSector = 512, kMelonIo = 0x4E4C454D };

static const char kSaveName[11] = "LTTP-DS SAV";

static u8 g_sec[kSector] __attribute__((aligned(32)));
static const char *g_status = "off";
static int g_melon;
static int g_has_fat;
static int g_dirty;
static int g_tries;
static int g_have_dir;
static u32 g_spc;
static u32 g_fat_lba;
static u32 g_root_lba;
static u32 g_root_secs;
static u32 g_data_lba;
static u32 g_clusters;
static u32 g_dir_lba;
static u32 g_dir_index;
static u32 g_cluster;

const char *ZeldaSaveStatus(void)
{
	return g_status;
}

static int read_sec(u32 lba)
{
	return blkDevReadSectors(BlkDevice_Dldi, g_sec, lba, 1);
}

static int melon_write_sector(u32 lba, const u8 *data)
{
	vu8 *cmd = (vu8 *)0x040001A8;
	vu16 *aux = (vu16 *)0x040001A0;
	vu32 *romctrl = (vu32 *)0x040001A4;
	vu32 *romdata = (vu32 *)0x04100010;
	const u32 *word = (const u32 *)data;
	u16 saved = REG_EXMEMCNT;
	int ime = enterCriticalSection();
	int ok = 0;
	int i, spins;

	REG_EXMEMCNT = saved & ~(1u << 11);

	spins = 0;
	while (*romctrl & (1u << 31)) {
		if (++spins > 100000)
			goto out;
	}

	*aux = 0x8000;
	cmd[0] = 0xC1;
	cmd[1] = (u8)(lba >> 24);
	cmd[2] = (u8)(lba >> 16);
	cmd[3] = (u8)(lba >> 8);
	cmd[4] = (u8)lba;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	/* Start, write, 512-byte block, KEY2. Same shape as melonDLDI. */
	*romctrl = 0xE1400000u;

	for (i = 0; i < 128; i++) {
		spins = 0;
		for (;;) {
			u32 ctrl = *romctrl;

			if (ctrl & (1u << 23))
				break;
			if ((ctrl & (1u << 31)) == 0 || ++spins > 100000)
				goto out;
		}
		*romdata = word[i];
	}

	spins = 0;
	while (*romctrl & (1u << 31)) {
		if (++spins > 100000)
			goto out;
	}
	ok = 1;
out:
	if (!ok)
		*romctrl = 0;
	REG_EXMEMCNT = saved;
	leaveCriticalSection(ime);
	return ok;
}

static int write_sec(u32 lba, const void *data)
{
	if (data != g_sec)
		memcpy(g_sec, data, kSector);
	if (g_melon)
		return melon_write_sector(lba, g_sec);
	return blkDevWriteSectors(BlkDevice_Dldi, g_sec, lba, 1);
}

static u16 rd16(const u8 *p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static int load_geometry(void)
{
	u32 part, tot, reserved, fats, root_ents, spf;

	if (!read_sec(0) || g_sec[510] != 0x55 || g_sec[511] != 0xAA)
		return 0;
	if (g_sec[0] == 0xE9 || g_sec[0] == 0xEB) {
		part = 0;
	} else {
		part = rd32(g_sec + 0x1C6);
		if (!part || !read_sec(part))
			return 0;
	}
	if (rd16(g_sec + 11) != 512 || g_sec[13] == 0)
		return 0;
	g_spc = g_sec[13];
	reserved = rd16(g_sec + 14);
	fats = g_sec[16];
	root_ents = rd16(g_sec + 17);
	tot = rd16(g_sec + 19);
	spf = rd16(g_sec + 22);
	if (!tot)
		tot = rd32(g_sec + 32);
	/* FAT32 has a zero 16-bit FAT size. This image is FAT16. */
	if (!spf || !fats || !root_ents || !tot)
		return 0;

	g_fat_lba = part + reserved;
	g_root_lba = g_fat_lba + fats * spf;
	g_root_secs = (root_ents * 32u + 511u) >> 9;
	g_data_lba = g_root_lba + g_root_secs;
	if (g_data_lba < part || tot <= g_data_lba - part)
		return 0;
	g_clusters = (tot - (g_data_lba - part)) / g_spc;
	g_has_fat = 1;
	return 1;
}

static u16 fat_get(u32 cluster)
{
	u32 off = (cluster * 2u) & 511u;

	if (!read_sec(g_fat_lba + ((cluster * 2u) >> 9)))
		return 0xFFFF;
	return rd16(g_sec + off);
}

static int fat_put(u32 cluster, u16 value)
{
	u32 lba = g_fat_lba + ((cluster * 2u) >> 9);
	u32 off = (cluster * 2u) & 511u;

	if (!read_sec(lba))
		return 0;
	g_sec[off] = (u8)value;
	g_sec[off + 1] = (u8)(value >> 8);
	return write_sec(lba, g_sec);
}

static int name_is_save(const u8 *ent)
{
	int i;

	if (ent[0] == 0 || ent[0] == 0xE5 || ent[11] == 0x0F)
		return 0;
	if (ent[11] & 0x18)
		return 0;
	for (i = 0; i < 11; i++) {
		u8 c = ent[i];

		if (c >= 'a' && c <= 'z')
			c = (u8)(c - 32);
		if (c != (u8)kSaveName[i])
			return 0;
	}
	return 1;
}

static void remember_dir(u32 lba, u32 index, u32 cluster)
{
	g_have_dir = 1;
	g_dir_lba = lba;
	g_dir_index = index;
	g_cluster = cluster;
}

static int find_dir(void)
{
	u32 s, i;

	g_have_dir = 0;
	g_cluster = 0;
	for (s = 0; s < g_root_secs; s++) {
		if (!read_sec(g_root_lba + s))
			return 0;
		for (i = 0; i < 16; i++) {
			u8 *ent = g_sec + i * 32;

			if (ent[0] == 0)
				return 1;
			if (!name_is_save(ent))
				continue;
			remember_dir(g_root_lba + s, i, rd16(ent + 26));
			return 1;
		}
	}
	return 1;
}

static u32 alloc_cluster(void)
{
	u32 sector, max, i;

	max = g_clusters + 2;
	if (max > 2048)
		max = 2048;
	for (sector = 0; sector < ((max * 2u + 511u) >> 9); sector++) {
		if (!read_sec(g_fat_lba + sector))
			return 0;
		for (i = 0; i < 512; i += 2) {
			u32 cluster = sector * 256u + i / 2u;
			u16 link = rd16(g_sec + i);

			if (cluster < 2 || cluster >= max || link != 0)
				continue;
			g_sec[i] = 0xFF;
			g_sec[i + 1] = 0xFF;
			if (!write_sec(g_fat_lba + sector, g_sec))
				return 0;
			return cluster;
		}
	}
	return 0;
}

static int write_dir_entry(u32 cluster)
{
	u8 *ent;

	if (!g_have_dir)
		return 0;
	if (!read_sec(g_dir_lba))
		return 0;
	ent = g_sec + g_dir_index * 32;
	memset(ent, 0, 32);
	memcpy(ent, kSaveName, 11);
	ent[11] = 0x20;
	ent[26] = (u8)cluster;
	ent[27] = (u8)(cluster >> 8);
	ent[28] = (u8)(kSramBytes & 0xFF);
	ent[29] = (u8)((kSramBytes >> 8) & 0xFF);
	ent[30] = (u8)((kSramBytes >> 16) & 0xFF);
	ent[31] = (u8)(kSramBytes >> 24);
	g_cluster = cluster;
	return write_sec(g_dir_lba, g_sec);
}

static int claim_dir_slot(void)
{
	u32 s, i;

	if (g_have_dir)
		return 1;
	for (s = 0; s < g_root_secs; s++) {
		if (!read_sec(g_root_lba + s))
			return 0;
		for (i = 0; i < 16; i++) {
			u8 mark = g_sec[i * 32];

			if (mark != 0 && mark != 0xE5)
				continue;
			remember_dir(g_root_lba + s, i, 0);
			return 1;
		}
	}
	return 0;
}

static int ensure_cluster(void)
{
	u32 cluster;

	if (!g_have_dir && !claim_dir_slot())
		return 0;
	if (g_cluster >= 2 && g_cluster < 0xFFF8)
		return 1;
	cluster = alloc_cluster();
	if (!cluster)
		return 0;
	return write_dir_entry(cluster);
}

static int write_sram_chain(u32 cluster)
{
	u32 left = kSramBytes;
	u32 off = 0;

	while (left) {
		u32 base, s, next, n;

		if (cluster < 2 || cluster >= 0xFFF8)
			return 0;
		base = g_data_lba + (cluster - 2) * g_spc;
		for (s = 0; s < g_spc && left; s++) {
			n = left > kSector ? kSector : left;
			memset(g_sec, 0, kSector);
			memcpy(g_sec, g_zenv.sram + off, n);
			if (!write_sec(base + s, g_sec))
				return 0;
			off += n;
			left -= n;
		}
		if (!left)
			return 1;
		next = fat_get(cluster);
		if (next < 2 || next >= 0xFFF8) {
			next = alloc_cluster();
			if (!next || !fat_put(cluster, (u16)next))
				return 0;
		}
		cluster = next;
	}
	return 1;
}

static int read_sram_chain(u32 cluster)
{
	u32 left = kSramBytes;
	u32 off = 0;
	int any = 0;

	while (left && cluster >= 2 && cluster < 0xFFF8) {
		u32 base = g_data_lba + (cluster - 2) * g_spc;
		u32 s, n;

		for (s = 0; s < g_spc && left; s++) {
			u32 i;

			if (!read_sec(base + s))
				return 0;
			n = left > kSector ? kSector : left;
			memcpy(g_zenv.sram + off, g_sec, n);
			for (i = 0; i < n; i++) {
				if (g_sec[i])
					any = 1;
			}
			off += n;
			left -= n;
		}
		if (!left)
			break;
		cluster = fat_get(cluster);
	}
	if (left)
		memset(g_zenv.sram + off, 0, left);
	g_status = any ? "loaded" : "new";
	return 1;
}

static int read_legacy(void)
{
	static const char *paths[] = {
		"fat:/saves/sram.dat",
		"sd:/saves/sram.dat",
		"saves/sram.dat",
		NULL
	};
	int i;

	for (i = 0; paths[i]; i++) {
		FILE *f = fopen(paths[i], "rb");
		size_t n;

		if (!f)
			continue;
		n = fread(g_zenv.sram, 1, kSramBytes, f);
		fclose(f);
		if (n == kSramBytes) {
			g_status = "loaded";
			return 1;
		}
	}
	return 0;
}

void ZeldaSaveFlush(void)
{
	if (!g_dirty)
		return;
	if (!g_has_fat || !g_zenv.sram) {
		g_dirty = 0;
		g_status = "off";
		return;
	}
	if (!ensure_cluster() || !write_sram_chain(g_cluster)) {
		if (++g_tries >= 2) {
			g_dirty = 0;
			g_status = "fail";
		}
		return;
	}
	g_tries = 0;
	g_dirty = 0;
	g_status = "saved";
}

void __wrap_ZeldaWriteSram(void)
{
	g_dirty = 1;
	g_tries = 0;
}

void ZeldaSaveMount(void)
{
	g_melon = g_envExtraInfo->dldi_io_type == kMelonIo;
	if (!g_zenv.sram || !load_geometry() || !find_dir()) {
		g_status = "off";
		iprintf("Save: no SD\n");
		return;
	}

	iprintf("Save: reading\n");
	if (g_have_dir && g_cluster >= 2 && read_sram_chain(g_cluster)) {
		iprintf("Save: %s\n", g_status);
		return;
	}
	if (read_legacy()) {
		iprintf("Save: %s\n", g_status);
		return;
	}
	g_status = "new";
	iprintf("Save: %s\n", g_status);
}
