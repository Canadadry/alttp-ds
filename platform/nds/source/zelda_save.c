/*
 * Homebrew has no cartridge backup. The save is an 8 KB file named
 * lttp-ds.sav in the root of the DLDI volume (8.3 name LTTP-DS.SAV).
 *
 * ZeldaWriteSram only marks the image dirty. ZeldaSaveFlush writes it
 * after the frame. On melonDS the libfat write never returns: the ARM7
 * DLDI driver waits forever for the cart busy bit. The flush instead
 * borrows the cart on the ARM9, which is the interface melonDS stores,
 * and gives it back before the next frame.
 */
#include <nds.h>
#include <calico/dev/blk.h>
#include <calico/nds/env.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "zelda_rtl.h"
#include "features.h"
#include "hud.h"
#include "player.h"
#include "dungeon.h"
#include "load_gfx.h"
#include "select_file.h"
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
static u32 g_spf;
static u32 g_nfats;
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

/* Past the three slots and their mirrors (last byte 0x1DFF). A display
 * choice, not part of a slot checksum.
 * Version 1: hud, beep, checksum at byte 7.
 * Version 2 adds bomb and arrow caps and stores the checksum at byte 9.
 * Version 3 adds run speed and stores the checksum at byte 11.
 * Version 4 adds dark rooms and stores the checksum at byte 13.
 * Version 5 adds hair color and stores the checksum at byte 15. */
enum { kSetOff = 0x1E00, kSetVersion = 5 };

static int g_hud_top = 1;
static int g_low_hp_beep = 1;
static int g_bomb_cap = 10;
static int g_arrow_cap = 30;
static int g_run_mul = 1;
static int g_rooms_bright;
static int g_hair_blonde;

static uint8 settings_sum(const uint8 *p, int n)
{
	uint8 s = 0;
	int i;

	for (i = 0; i < n; i++)
		s ^= p[i];
	return s;
}

static void apply_low_hp_beep(void)
{
	if (g_low_hp_beep)
		g_wanted_zelda_features &= ~kFeatures0_DisableLowHealthBeep;
	else
		g_wanted_zelda_features |= kFeatures0_DisableLowHealthBeep;
}

static void apply_carry_caps(void)
{
	Hud_SetCarryCaps((uint8)g_bomb_cap, (uint8)g_arrow_cap);
}

static void apply_run_speed(void)
{
	Player_SetRunSpeed((uint8)g_run_mul);
}

static void apply_rooms_bright(void)
{
	Dungeon_SetRoomsBright(g_rooms_bright);
}

static void apply_hair(void)
{
	Palette_SetHairBlonde(g_hair_blonde);
}

static void settings_store(void)
{
	uint8 *p;

	if (!g_zenv.sram)
		return;
	p = g_zenv.sram + kSetOff;
	p[0] = 'L';
	p[1] = 'T';
	p[2] = 'D';
	p[3] = 'S';
	p[4] = kSetVersion;
	p[5] = (uint8)(g_hud_top ? 0 : 1);
	p[6] = (uint8)(g_low_hp_beep ? 0 : 1);
	p[7] = (uint8)(g_bomb_cap == 99);
	p[8] = (uint8)(g_arrow_cap == 99);
	p[9] = 0;
	p[10] = (uint8)(g_run_mul - 1);
	p[11] = 0;
	p[12] = (uint8)(g_rooms_bright ? 1 : 0);
	p[13] = 0;
	p[14] = (uint8)(g_hair_blonde ? 1 : 0);
	p[15] = settings_sum(p, 15);
	ZeldaWriteSram();
}

static void settings_load(void)
{
	const uint8 *p = g_zenv.sram + kSetOff;

	g_hud_top = 1;
	g_low_hp_beep = 1;
	g_bomb_cap = 10;
	g_arrow_cap = 30;
	g_run_mul = 1;
	g_rooms_bright = 0;
	g_hair_blonde = 0;
	if (p[0] == 'L' && p[1] == 'T' && p[2] == 'D' && p[3] == 'S' &&
	    p[5] <= 1 && p[6] <= 1) {
		if (p[4] == 1 && p[7] == settings_sum(p, 7)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
		} else if (p[4] == 2 && p[7] <= 1 && p[8] <= 1 &&
			   p[9] == settings_sum(p, 9)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
		} else if (p[4] == 3 && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[11] == settings_sum(p, 11)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
		} else if (p[4] == 4 && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[13] == settings_sum(p, 13)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
		} else if (p[4] == kSetVersion && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[14] <= 1 &&
			   p[15] == settings_sum(p, 15)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
			g_hair_blonde = p[14] != 0;
		}
	}
	apply_low_hp_beep();
	apply_carry_caps();
	apply_run_speed();
	apply_rooms_bright();
	apply_hair();
}

int ZeldaHudOnTop(void)
{
	return g_hud_top;
}

void ZeldaHudSetOnTop(int on)
{
	g_hud_top = on != 0;
	settings_store();
}

int ZeldaLowHpBeep(void)
{
	return g_low_hp_beep;
}

void ZeldaLowHpBeepSet(int on)
{
	g_low_hp_beep = on != 0;
	apply_low_hp_beep();
	settings_store();
}

int ZeldaBombCap(void)
{
	return g_bomb_cap;
}

void ZeldaBombCapSet(int cap)
{
	g_bomb_cap = cap == 99 ? 99 : 10;
	apply_carry_caps();
	settings_store();
}

int ZeldaArrowCap(void)
{
	return g_arrow_cap;
}

void ZeldaArrowCapSet(int cap)
{
	g_arrow_cap = cap == 99 ? 99 : 30;
	apply_carry_caps();
	settings_store();
}

int ZeldaRunSpeed(void)
{
	return g_run_mul;
}

void ZeldaRunSpeedSet(int mul)
{
	if (mul < 1)
		mul = 1;
	if (mul > 3)
		mul = 3;
	g_run_mul = mul;
	apply_run_speed();
	settings_store();
}

int ZeldaDarkRooms(void)
{
	return g_rooms_bright;
}

void ZeldaDarkRoomsSet(int bright)
{
	g_rooms_bright = bright != 0;
	apply_rooms_bright();
	settings_store();
}

int ZeldaHairBlonde(void)
{
	return g_hair_blonde;
}

void ZeldaHairSet(int blonde)
{
	g_hair_blonde = blonde != 0;
	apply_hair();
	settings_store();
}

static int read_sec(u32 lba)
{
	return blkDevReadSectors(BlkDevice_Dldi, g_sec, lba, 1);
}

/* melonDS only commits a cart transfer from the CPU that owns the slot.
 * DLDI leaves that as the ARM7, and its write routine never sees busy
 * clear. Hand the slot to the ARM9 for one sector, then hand it back. */
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

	REG_EXMEMCNT = saved & ~EXMEMCNT_NDS_SLOT_ARM7;
	if (REG_EXMEMCNT & EXMEMCNT_NDS_SLOT_ARM7)
		goto out;

	spins = 0;
	while (*romctrl & (1u << 31)) {
		if (++spins > 200000)
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
	*romctrl = 0xE1400000u;
	if ((*romctrl & (1u << 31)) == 0)
		goto out;

	for (i = 0; i < 128; i++) {
		spins = 0;
		for (;;) {
			u32 ctrl = *romctrl;

			if (ctrl & (1u << 23))
				break;
			if ((ctrl & (1u << 31)) == 0 || ++spins > 200000)
				goto out;
		}
		*romdata = word[i];
	}

	spins = 0;
	while (*romctrl & (1u << 31)) {
		if (++spins > 200000)
			goto out;
	}
	ok = 1;
out:
	REG_EXMEMCNT = saved;
	leaveCriticalSection(ime);
	return ok;
}

static int write_sec(u32 lba, const void *data)
{
	static u8 check[kSector] __attribute__((aligned(32)));

	if (data != g_sec)
		memcpy(g_sec, data, kSector);
	if (!g_melon)
		return blkDevWriteSectors(BlkDevice_Dldi, g_sec, lba, 1);
	if (!melon_write_sector(lba, g_sec))
		return 0;
	/* The protocol can finish on the ARM9 interface without the
	 * emulator storing the sector. A DLDI read is the real image. */
	if (!blkDevReadSectors(BlkDevice_Dldi, check, lba, 1))
		return 0;
	return memcmp(check, g_sec, kSector) == 0;
}

/* File bytes live past the root directory. A data write into the FAT or
 * the directory is what made folder sync drop lttp-ds.sav. */
static int write_data_sec(u32 lba)
{
	if (!g_data_lba || lba < g_data_lba)
		return 0;
	return write_sec(lba, g_sec);
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
	g_nfats = fats;
	root_ents = rd16(g_sec + 17);
	tot = rd16(g_sec + 19);
	spf = rd16(g_sec + 22);
	if (!tot)
		tot = rd32(g_sec + 32);
	/* FAT32 has a zero 16-bit FAT size. This image is FAT16. */
	if (!spf || !fats || !root_ents || !tot)
		return 0;

	g_spf = spf;
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

static int fat_read(u32 cluster, u16 *out)
{
	u32 sector = (cluster * 2u) >> 9;
	u32 off = (cluster * 2u) & 511u;
	u32 fat;

	if (cluster < 2)
		return 0;
	for (fat = 0; fat < g_nfats; fat++) {
		if (!read_sec(g_fat_lba + fat * g_spf + sector))
			continue;
		*out = rd16(g_sec + off);
		return 1;
	}
	return 0;
}

static int fat_put(u32 cluster, u16 value)
{
	u32 sector = (cluster * 2u) >> 9;
	u32 off = (cluster * 2u) & 511u;
	u32 fat;

	if (cluster < 2)
		return 0;
	for (fat = 0; fat < g_nfats; fat++) {
		u32 lba = g_fat_lba + fat * g_spf + sector;

		if (!read_sec(lba))
			return 0;
		g_sec[off] = (u8)value;
		g_sec[off + 1] = (u8)(value >> 8);
		if (!write_sec(lba, g_sec))
			return 0;
	}
	return 1;
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
			if (!fat_put(cluster, 0xFFFF))
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
	/* 1980-01-01 00:00:02. A zero timestamp is ignored by folder sync,
	 * so a rewrite never replaces the copy on the host. */
	ent[22] = 1;
	ent[24] = 0x21;
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
		u32 base, s, n;

		if (cluster < 2 || cluster >= 0xFFF8)
			return 0;
		base = g_data_lba + (cluster - 2) * g_spc;
		for (s = 0; s < g_spc && left; s++) {
			n = left > kSector ? kSector : left;
			memset(g_sec, 0, kSector);
			memcpy(g_sec, g_zenv.sram + off, n);
			if (!write_data_sec(base + s))
				return 0;
			off += n;
			left -= n;
		}
		if (!left)
			return 1;
		{
			u16 link;

			if (!fat_read(cluster, &link))
				return 0;
			if (link < 2 || link >= 0xFFF8) {
				u32 grown = alloc_cluster();

				if (!grown || !fat_put(cluster, (u16)grown))
					return 0;
				link = (u16)grown;
			}
			cluster = link;
		}
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
		{
			u16 link;

			if (!fat_read(cluster, &link))
				return 0;
			cluster = link;
		}
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

/* Bump the directory time without rewriting the name. melonDS folder
 * sync only copies a file when that time changes, and it deletes the
 * host file when the directory entry disappears. */
static int touch_dir(void)
{
	u8 *ent;
	u16 t;

	if (!g_have_dir || !read_sec(g_dir_lba))
		return 0;
	ent = g_sec + g_dir_index * 32;
	if (ent[0] == 0 || ent[0] == 0xE5)
		return 0;
	t = (u16)(rd16(ent + 22) + 1);
	if (t == 0)
		t = 1;
	ent[22] = (u8)t;
	ent[23] = (u8)(t >> 8);
	if (rd16(ent + 24) == 0) {
		ent[24] = 0x21;
		ent[25] = 0;
	}
	return write_sec(g_dir_lba, g_sec);
}

static const char *const kSavePaths[] = {
	"fat:/lttp-ds.sav",
	"sd:/lttp-ds.sav",
	"lttp-ds.sav",
	NULL
};

static int read_stdio(void)
{
	int i;

	for (i = 0; kSavePaths[i]; i++) {
		FILE *f = fopen(kSavePaths[i], "rb");
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

/* In-place update. "wb" is only used when the file is not there yet;
 * opening an existing file with "wb" truncates it before the bytes land.
 * The length flips between 8192 and 8193 so melonDS folder sync copies
 * the file even when the FAT clock is still in the same 2-second slot. */
static int write_stdio(void)
{
	int i;

	for (i = 0; kSavePaths[i]; i++) {
		FILE *f = fopen(kSavePaths[i], "r+b");
		long sz;
		int wrote;

		if (!f) {
			FILE *probe = fopen(kSavePaths[i], "rb");

			if (probe) {
				fclose(probe);
				continue;
			}
			f = fopen(kSavePaths[i], "wb");
		}
		if (!f)
			continue;
		wrote = fwrite(g_zenv.sram, 1, kSramBytes, f) == kSramBytes;
		if (wrote && fflush(f) == 0 && fseek(f, 0, SEEK_END) == 0) {
			sz = ftell(f);
			if (sz >= 0 && sz <= (long)kSramBytes)
				ftruncate(fileno(f), (off_t)kSramBytes + 1);
			else if (sz > (long)kSramBytes)
				ftruncate(fileno(f), (off_t)kSramBytes);
		}
		fsync(fileno(f));
		if (fclose(f) == 0 && wrote)
			return 1;
	}
	return 0;
}

void ZeldaSaveFlush(void)
{
	if (!g_dirty)
		return;
	if (!g_zenv.sram) {
		g_dirty = 0;
		g_status = "off";
		return;
	}
	/* Heal a slot whose existence mark or checksum was clobbered in
	 * the same frame as the save, before those bytes reach the card. */
	Sram_RepairSlots(g_zenv.sram);
	/* libfat's write on melonDS waits on the ARM7 driver, which never
	 * returns. Hardware still uses it. */
	if (!g_melon && write_stdio()) {
		g_tries = 0;
		g_dirty = 0;
		g_status = "saved";
		return;
	}
	if (!g_has_fat) {
		g_dirty = 0;
		g_status = "off";
		return;
	}
	if (!ensure_cluster() || !write_sram_chain(g_cluster) || !touch_dir()) {
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
	if (!g_zenv.sram) {
		g_status = "off";
		return;
	}
	g_melon = g_envExtraInfo->dldi_io_type == kMelonIo;
	load_geometry();
	find_dir();
	if (read_stdio())
		;
	else if (g_has_fat && g_have_dir && g_cluster >= 2 && read_sram_chain(g_cluster))
		;
	else if (read_legacy())
		;
	else
		g_status = "new";
	settings_load();
}
