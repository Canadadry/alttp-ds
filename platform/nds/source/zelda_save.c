/*
 * No backing store yet: Mount and Flush report off and write nothing.
 */
#include <nds.h>

#include <stdint.h>
#include <string.h>

#include "types.h"
#include "zelda_rtl.h"
#include "features.h"
#include "hud.h"
#include "player.h"
#include "dungeon.h"
#include "load_gfx.h"
#include "assets.h"
#include "select_file.h"
#include "zelda_save.h"

static const char *g_status = "off";
static int g_dirty;

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
 * Version 5 adds hair color and stores the checksum at byte 15.
 * Version 6 adds spawn and stores the checksum after the exit snapshots.
 * Version 7 adds the magic mirror in the spare byte before spawn.
 * Version 8 adds one DS key id per SNES control after those snapshots. */
enum { kSetOff = 0x1E00, kSetVersion = 8, kExitBytes = 0x32, kExitRam = 0xC140 };

extern uint8 g_ram[];

static int g_hud_top;
static int g_low_hp_beep = 1;
static int g_bomb_cap = 10;
static int g_arrow_cap = 30;
static int g_run_mul = 1;
static int g_rooms_bright;
static int g_hair_blonde;
static int g_spawn_dungeon;
static int g_mirror_free;
/* Joypad-bit order. The matching DS key, until the player saves another. */
static const uint8 kDefaultMap[kSnesButtonCount] = {
	kDsB, kDsY, kDsSelect, kDsStart,
	kDsPadUp, kDsPadDown, kDsPadLeft, kDsPadRight,
	kDsA, kDsX, kDsL, kDsR
};
static uint8 g_button_map[kSnesButtonCount];
static uint8 g_spawn_entrance[3] = { 0xff, 0xff, 0xff };
static uint8 g_spawn_exit[3][kExitBytes];

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

static void apply_mirror_free(void)
{
	if (g_mirror_free)
		g_wanted_zelda_features |= kFeatures0_MirrorToDarkworld;
	else
		g_wanted_zelda_features &= ~kFeatures0_MirrorToDarkworld;
}

static void map_reset(void)
{
	memcpy(g_button_map, kDefaultMap, sizeof g_button_map);
}

static int map_at(void)
{
	return 20 + (int)sizeof(g_spawn_exit);
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
	p[15] = (uint8)(g_mirror_free ? 1 : 0);
	p[16] = (uint8)(g_spawn_dungeon ? 1 : 0);
	p[17] = g_spawn_entrance[0];
	p[18] = g_spawn_entrance[1];
	p[19] = g_spawn_entrance[2];
	memcpy(p + 20, g_spawn_exit, sizeof(g_spawn_exit));
	memcpy(p + map_at(), g_button_map, sizeof g_button_map);
	p[map_at() + kSnesButtonCount] =
		settings_sum(p, map_at() + kSnesButtonCount);
	ZeldaWriteSram();
}

static void settings_load(void)
{
	const uint8 *p = g_zenv.sram + kSetOff;

	g_hud_top = 0;
	g_low_hp_beep = 1;
	g_bomb_cap = 10;
	g_arrow_cap = 30;
	g_run_mul = 1;
	g_rooms_bright = 0;
	g_hair_blonde = 0;
	g_spawn_dungeon = 0;
	g_mirror_free = 0;
	map_reset();
	g_spawn_entrance[0] = g_spawn_entrance[1] = g_spawn_entrance[2] = 0xff;
	memset(g_spawn_exit, 0, sizeof(g_spawn_exit));
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
		} else if (p[4] == 5 && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[14] <= 1 &&
			   p[15] == settings_sum(p, 15)) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
			g_hair_blonde = p[14] != 0;
		} else if (p[4] == 6 && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[14] <= 1 && p[16] <= 1 &&
			   p[20 + sizeof(g_spawn_exit)] ==
			   settings_sum(p, 20 + (int)sizeof(g_spawn_exit))) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
			g_hair_blonde = p[14] != 0;
			g_spawn_dungeon = p[16] != 0;
			g_spawn_entrance[0] = p[17];
			g_spawn_entrance[1] = p[18];
			g_spawn_entrance[2] = p[19];
			memcpy(g_spawn_exit, p + 20, sizeof(g_spawn_exit));
		} else if (p[4] == 7 && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[14] <= 1 && p[15] <= 1 &&
			   p[16] <= 1 &&
			   p[20 + sizeof(g_spawn_exit)] ==
			   settings_sum(p, 20 + (int)sizeof(g_spawn_exit))) {
			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
			g_hair_blonde = p[14] != 0;
			g_mirror_free = p[15] != 0;
			g_spawn_dungeon = p[16] != 0;
			g_spawn_entrance[0] = p[17];
			g_spawn_entrance[1] = p[18];
			g_spawn_entrance[2] = p[19];
			memcpy(g_spawn_exit, p + 20, sizeof(g_spawn_exit));
		} else if (p[4] == kSetVersion && p[7] <= 1 && p[8] <= 1 &&
			   p[10] <= 2 && p[12] <= 1 && p[14] <= 1 && p[15] <= 1 &&
			   p[16] <= 1 &&
			   p[map_at() + kSnesButtonCount] ==
			   settings_sum(p, map_at() + kSnesButtonCount)) {
			int i;

			g_hud_top = p[5] == 0;
			g_low_hp_beep = p[6] == 0;
			g_bomb_cap = p[7] ? 99 : 10;
			g_arrow_cap = p[8] ? 99 : 30;
			g_run_mul = p[10] + 1;
			g_rooms_bright = p[12] != 0;
			g_hair_blonde = p[14] != 0;
			g_mirror_free = p[15] != 0;
			g_spawn_dungeon = p[16] != 0;
			g_spawn_entrance[0] = p[17];
			g_spawn_entrance[1] = p[18];
			g_spawn_entrance[2] = p[19];
			memcpy(g_spawn_exit, p + 20, sizeof(g_spawn_exit));
			for (i = 0; i < kSnesButtonCount; i++) {
				uint8 v = p[map_at() + i];

				if (v < kDsButtonCount)
					g_button_map[i] = v;
			}
		}
	}
	apply_low_hp_beep();
	apply_carry_caps();
	apply_run_speed();
	apply_rooms_bright();
	apply_hair();
	apply_mirror_free();
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

int ZeldaSpawnDungeon(void)
{
	return g_spawn_dungeon;
}

void ZeldaSpawnSet(int dungeon)
{
	g_spawn_dungeon = dungeon != 0;
	settings_store();
}

int ZeldaMirrorFree(void)
{
	return g_mirror_free;
}

void ZeldaMirrorSet(int free)
{
	g_mirror_free = free != 0;
	apply_mirror_free();
	settings_store();
}

int ZeldaButtonMap(int snes)
{
	if (snes < 0 || snes >= kSnesButtonCount)
		return kDsA;
	return g_button_map[snes];
}

void ZeldaButtonMapGet(int map[kSnesButtonCount])
{
	int i;

	for (i = 0; i < kSnesButtonCount; i++)
		map[i] = g_button_map[i];
}

void ZeldaButtonMapSet(const int map[kSnesButtonCount])
{
	int i;

	for (i = 0; i < kSnesButtonCount; i++) {
		int v = map[i];

		if ((unsigned)v >= kDsButtonCount)
			v = kDefaultMap[i];
		g_button_map[i] = (uint8)v;
	}
	settings_store();
}

void ZeldaSpawnRemember(int slot, int entrance)
{
	if (slot < 0 || slot > 2)
		return;
	if (entrance < 0 || entrance > 254) {
		g_spawn_entrance[slot] = 0xff;
	} else {
		g_spawn_entrance[slot] = (uint8)entrance;
		memcpy(g_spawn_exit[slot], g_ram + kExitRam, kExitBytes);
	}
	settings_store();
}

/* Erasing a file, or starting a new one in its place, leaves no dungeon to
 * spawn in. Without this the next game opens in the old file's dungeon. */
void ZeldaSpawnForget(int slot)
{
	if (slot < 0 || slot > 2)
		return;
	g_spawn_entrance[slot] = 0xff;
	memset(g_spawn_exit[slot], 0, kExitBytes);
	settings_store();
}

/* A copied file loads where the original does. */
void ZeldaSpawnCopy(int from, int to)
{
	if (from < 0 || from > 2 || to < 0 || to > 2 || from == to)
		return;
	g_spawn_entrance[to] = g_spawn_entrance[from];
	memcpy(g_spawn_exit[to], g_spawn_exit[from], kExitBytes);
	settings_store();
}

int ZeldaSpawnPrepare(int slot)
{
	int entrance;

	if (!g_spawn_dungeon || slot < 0 || slot > 2)
		return -1;
	entrance = g_spawn_entrance[slot];
	if (entrance == 0xff)
		return -1;
	if ((unsigned)entrance >= kEntranceData_rooms_SIZE / 2)
		return -1;
	memcpy(g_ram + kExitRam, g_spawn_exit[slot], kExitBytes);
	return entrance;
}

void ZeldaSaveFlush(void)
{
	if (!g_dirty)
		return;
	g_dirty = 0;
	g_status = "off";
}

void __wrap_ZeldaWriteSram(void)
{
	g_dirty = 1;
}

void ZeldaSaveMount(void)
{
	map_reset();
	g_status = "off";
	if (!g_zenv.sram)
		return;
	settings_load();
}
