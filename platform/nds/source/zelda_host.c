#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "config.h"
#include "assets.h"
#include "util.h"
#include "audio.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "snes/ppu.h"
#include "zelda_video.h"
#include "zelda_save.h"
#include "zelda_audio.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

void NORETURN Die(const char *error)
{
	consoleClear();
	iprintf("FATAL:\n%s\n", error ? error : "(null)");
	iprintf("\nHalted.\n");
	for (;;)
		swiWaitForVBlank();
}

void ZeldaApuLock(void)
{
}

void ZeldaApuUnlock(void)
{
}

static uint8 *try_read_asset_file(const char *path, size_t *length)
{
	iprintf("Trying %s\n", path);
	return ReadWholeFile(path, length);
}

void ZeldaLoadAssets(void)
{
	static const char *paths[] = {
		"nitro:/zelda3_assets.dat",
		"zelda3_assets.dat",
		"fat:/zelda3_assets.dat",
		"fat:/lttp-ds/zelda3_assets.dat",
		NULL
	};
	size_t length = 0;
	uint8 *data = NULL;
	int i;

	for (i = 0; paths[i] && !data; i++)
		data = try_read_asset_file(paths[i], &length);

	if (!data)
		Die("Failed to read zelda3_assets.dat");

	{
		static const char kAssetsSig[] = { kAssets_Sig };

		if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
		    memcmp(data, kAssetsSig, 48) != 0 ||
		    *(uint32 *)(data + 80) != kNumberOfAssets)
			Die("Invalid assets file");
	}

	{
		uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(data + 84);
		size_t ai;

		for (ai = 0; ai < kNumberOfAssets; ai++) {
			uint32 size = *(uint32 *)(data + 88 + ai * 4);
			offset = (offset + 3) & ~3;
			if ((uint64)offset + size > length)
				Die("Assets file corruption");
			g_asset_sizes[ai] = size;
			g_asset_ptrs[ai] = data + offset;
			offset += size;
		}
	}

	iprintf("Assets: %u blobs, %u KB\n",
		(unsigned)kNumberOfAssets, (unsigned)(length / 1024));
}

MemBlk FindInAssetArray(int asset, int idx)
{
	return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}

/* ARM9 LDRH/LDR ignore low address bits. Packed-asset counts sit at size-2,
 * which is odd for kSprGfx, so the stock finder returns the wrong 1536 bytes. */
static uint16 read_u16(const uint8 *p)
{
	return (uint16)p[0] | ((uint16)p[1] << 8);
}

static uint32 read_u32(const uint8 *p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) |
		((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

MemBlk __wrap_FindIndexInMemblk(MemBlk data, size_t i)
{
	size_t end, mx, left_off, right_off;

	if (!data.ptr || data.size < 2)
		return (MemBlk) { 0, 0 };
	end = data.size - 2;
	mx = read_u16(data.ptr + end);
	if (mx < 8192) {
		if (i > mx || mx * 2 > end)
			return (MemBlk) { 0, 0 };
		left_off = (i == 0) ? mx * 2 : mx * 2 + read_u16(data.ptr + i * 2 - 2);
		right_off = (i == mx) ? end : mx * 2 + read_u16(data.ptr + i * 2);
	} else {
		mx -= 8192;
		if (i > mx || mx * 4 > end)
			return (MemBlk) { 0, 0 };
		left_off = (i == 0) ? mx * 4 : mx * 4 + read_u32(data.ptr + i * 4 - 4);
		right_off = (i == mx) ? end : mx * 4 + read_u32(data.ptr + i * 4);
	}
	if (left_off > right_off || right_off > end)
		return (MemBlk) { 0, 0 };
	return (MemBlk) { data.ptr + left_off, right_off - left_off };
}

/* kKeys "AXsSUDLRBY" before the 16-bit reverse in NMI_ReadJoypads.
 * bit 0 SNES B sword, 1 SNES Y item, 2 Select, 3 Start,
 * 4-7 Up Down Left Right, 8 SNES A lift/chest/dash, 9 SNES X, 10 SNES L, 11 SNES R.
 * DS B is the sword, DS A lifts, DS Y uses the equipped item.
 */
static int paused;
static int hud_dirty = 1;

static const char *item_name(unsigned id)
{
	static const char *const names[] = {
		"none", "bow", "boomerang", "hookshot", "bombs",
		"mushroom", "fire rod", "ice rod", "bombos", "ether",
		"quake", "lamp", "hammer", "flute", "net",
		"book", "bottle", "somaria", "byrna", "cape",
		"mirror", "gloves", "boots", "flippers", "pearl",
		"-", "sword", "shield", "tunic",
		"bottle", "bottle", "bottle", "bottle"
	};
	if (id >= sizeof(names) / sizeof(names[0]))
		return "?";
	return names[id];
}

static int map_keys(u32 held)
{
	int inputs = 0;

	if ((held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT))
		held &= ~(KEY_START | KEY_SELECT);
	if ((held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT))
		held &= ~(KEY_L | KEY_R | KEY_SELECT);

	if (held & KEY_B)
		inputs |= 1 << 0;
	if (held & KEY_Y)
		inputs |= 1 << 1;
	if (held & KEY_SELECT)
		inputs |= 1 << 2;
	if (held & KEY_START)
		inputs |= 1 << 3;
	if (held & KEY_UP)
		inputs |= 1 << 4;
	if (held & KEY_DOWN)
		inputs |= 1 << 5;
	if (held & KEY_LEFT)
		inputs |= 1 << 6;
	if (held & KEY_RIGHT)
		inputs |= 1 << 7;
	if (held & KEY_A)
		inputs |= 1 << 8;
	if (held & KEY_X)
		inputs |= 1 << 9;
	if (held & KEY_L)
		inputs |= 1 << 10;
	if (held & KEY_R)
		inputs |= 1 << 11;
	return inputs;
}

void ZeldaHostInit(void)
{
	ParseConfigFile(NULL);
	g_config.enable_msu = 0;
	g_config.enhanced_mode7 = false;
	g_config.extend_y = false;
	g_config.extended_aspect_ratio = 0;
	g_config.new_renderer = true;
	g_config.enable_audio = true;
	/* Hardware voices run at the SPC rate. Nothing resamples a mixed buffer. */
	g_config.audio_freq = 32000;
	g_config.audio_channels = 2;

	ZeldaLoadAssets();
	ZeldaInitialize();
	g_zenv.ppu->extraLeftRight = 0;
	g_wanted_zelda_features = 0;
	ZeldaEnableMsu(0);
	ZeldaSetLanguage(NULL);
	ZeldaVideoInit();
	ZeldaSaveMount();
	ZeldaAudioInit();
	iprintf("A lift  B sword  Y item\n");
	iprintf("Start menu\n");
}

void ZeldaHostSave(void)
{
	ZeldaWriteSram();
	ZeldaSaveFlush();
}

void ZeldaHostOnKeys(u32 held, u32 down)
{
	if ((down & (KEY_START | KEY_SELECT)) &&
	    (held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT)) {
		paused ^= 1;
		hud_dirty = 1;
	}
	if (!(held & KEY_START) &&
	    (down & (KEY_L | KEY_R | KEY_SELECT)) &&
	    (held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT)) {
		ZeldaHostSave();
		ZeldaReset(true);
		paused = 0;
		hud_dirty = 1;
	}
}

void ZeldaHostTick(u32 held)
{
	if (paused)
		return;
	ZeldaRunFrame(map_keys(held));
	/* Sprite tiles are built here, while the previous picture is still
	 * up. The vblank blit only has to DMA them. */
	ZeldaVideoComposeSprites();
}

void ZeldaHostBlit(u16 *fb)
{
	(void)fb;
	if (!paused)
		ZeldaVideoDrawPpuFrame();
}

void ZeldaHostAudio(void)
{
	if (!paused)
		ZeldaAudioFrame();
}

void ZeldaHostHud(void)
{
	unsigned hp = link_health_current;
	unsigned cap = link_health_capacity;
	unsigned magic = link_magic_power;
	unsigned bombs = link_item_bombs;
	unsigned arrows = link_num_arrows;
	unsigned keys = link_num_keys;
	unsigned item = current_item_y;
	unsigned sword = link_sword_type;
	unsigned rupees = link_rupees_actual;
	static unsigned p_hp, p_cap, p_magic, p_bombs, p_arrows, p_keys, p_item, p_sword, p_rupees;
	static int p_paused;
	const char *save = ZeldaSaveStatus();
	static const char *p_save;

	if (!hud_dirty &&
	    p_hp == hp && p_cap == cap && p_magic == magic &&
	    p_bombs == bombs && p_arrows == arrows && p_keys == keys &&
	    p_item == item && p_sword == sword && p_rupees == rupees &&
	    p_paused == paused && p_save == save)
		return;
	p_hp = hp;
	p_cap = cap;
	p_magic = magic;
	p_bombs = bombs;
	p_arrows = arrows;
	p_keys = keys;
	p_item = item;
	p_sword = sword;
	p_rupees = rupees;
	p_paused = paused;
	p_save = save;

	hud_dirty = 0;
	consoleClear();
	iprintf("%s\n", paused ? "PAUSED" : "LttP DS");
	if (hp % 8)
		iprintf("Hearts %u+%u/%u\n", hp / 8, hp % 8, cap / 8);
	else
		iprintf("Hearts %u/%u\n", hp / 8, cap / 8);
	iprintf("Rupees %u\n", rupees);
	iprintf("Bombs %u  Arrows %u\n", bombs, arrows);
	iprintf("Magic %u  Keys %u\n", magic, keys);
	iprintf("Item %s\n", item_name(item));
	iprintf("Sword %u\n", sword);
	iprintf("\nA lift    B sword\n");
	iprintf("Y item    X  L/R\n");
	iprintf("Start menu\n");
	iprintf("Sel+Start pause\n");
	iprintf("L+R+Sel reset\n");
	iprintf("L+R+Start+Sel quit\n");
	iprintf("Save %s\n", save);
}
