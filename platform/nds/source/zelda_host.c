#include <nds.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "config.h"
#include "assets.h"
#include "util.h"
#include "audio.h"
#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "zelda_video.h"
#include "zelda_save.h"
#include "zelda_audio.h"
#include "messaging.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

void NORETURN Die(const char *error)
{
	(void)error;
	for (;;)
		swiWaitForVBlank();
}

void ZeldaApuLock(void)
{
}

void ZeldaApuUnlock(void)
{
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
		data = ReadWholeFile(paths[i], &length);

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
static int settings_open;
static int settings_menu;
/* Two frames after the game is back: one samples the closing press, one eats it. */
static int settings_eat;

enum {
	kUiOff = 0,
	kUiGameOut,
	kUiMenuIn,
	kUiMenu,
	kUiMenuOut,
	kUiGameIn
};

static int ui;
static int fade;
static int cursor;
static int hud_pick;
static int beep_on;
static int bomb_99;
static int arrow_99;
static int run_speed;
static int dark_bright;
static int hair_blonde;
static int spawn_dungeon;
static int scroll_row;
static int save_on_exit;

enum {
	kRowHud = 0,
	kRowBeep,
	kRowSpawn,
	kRowBomb,
	kRowArrow,
	kRowSpeed,
	kRowDark,
	kRowHair,
	kRowSave,
	kRowCancel,
	kSettingCount = kRowSave,
	kVisible = 5,
	kCaretX = 16,
	kLabelX = 36,
	kValueX = 120,
	kTextX = 36,
	kTitleY = 22,
	kSaveY = 146,
	kCancelY = 166
};

static const int kSlotY[kVisible] = { 42, 60, 78, 96, 114 };
static const char *const kHudChoice[] = { "Top Screen", "Bottom Screen" };
static const char *const kOnOff[] = { "Off", "On" };
static const char *const kRunChoice[] = { "Normal", "Double", "Triple" };

static int settings_value_x(void)
{
	int label = ZeldaVideoMenuWidth("Low HP Indicator");
	int value = ZeldaVideoMenuWidth("Bottom Screen");
	int x = kLabelX + label + 12;

	if (x < kValueX)
		x = kValueX;
	if (x + value > 248)
		x = 248 - value;
	return x;
}

static const char *settings_label(int row)
{
	switch (row) {
	case kRowHud:
		return "HUD";
	case kRowBeep:
		return "Low HP Indicator";
	case kRowSpawn:
		return "Spawn";
	case kRowBomb:
		return "Bomb Capacity";
	case kRowArrow:
		return "Arrow Capacity";
	case kRowSpeed:
		return "Run Speed";
	case kRowDark:
		return "Dark Rooms";
	case kRowHair:
		return "Hair Color";
	default:
		return "";
	}
}

static const char *settings_value(int row)
{
	switch (row) {
	case kRowHud:
		return kHudChoice[hud_pick];
	case kRowBeep:
		return kOnOff[beep_on];
	case kRowSpawn:
		return spawn_dungeon ? "Dungeon" : "Normal";
	case kRowBomb:
		return bomb_99 ? "99" : "10";
	case kRowArrow:
		return arrow_99 ? "99" : "30";
	case kRowSpeed:
		return kRunChoice[run_speed];
	case kRowDark:
		return dark_bright ? "Bright" : "Normal";
	case kRowHair:
		return hair_blonde ? "Blonde" : "Pink";
	default:
		return "";
	}
}

static void settings_follow(void)
{
	if (cursor >= kSettingCount)
		return;
	if (cursor < scroll_row)
		scroll_row = cursor;
	if (cursor >= scroll_row + kVisible)
		scroll_row = cursor - kVisible + 1;
}

static int settings_caret_y(void)
{
	if (cursor == kRowSave)
		return kSaveY;
	if (cursor == kRowCancel)
		return kCancelY;
	return kSlotY[cursor - scroll_row];
}

static void settings_draw(void)
{
	const char *title = "Settings";
	int value_x = settings_value_x();
	int i;

	ZeldaVideoMenuClear();
	ZeldaVideoMenuText((256 - ZeldaVideoMenuWidth(title)) / 2, kTitleY, title);
	for (i = 0; i < kVisible; i++) {
		int row = scroll_row + i;

		if (row >= kSettingCount)
			break;
		ZeldaVideoMenuText(kLabelX, kSlotY[i], settings_label(row));
		ZeldaVideoMenuText(value_x, kSlotY[i], settings_value(row));
	}
	ZeldaVideoMenuText(kTextX, kSaveY, "Save");
	ZeldaVideoMenuText(kTextX, kCancelY, "Cancel");
	ZeldaVideoMenuText(kCaretX, settings_caret_y(), ">");
}

static void settings_move(int dir)
{
	if (dir < 0) {
		if (cursor > kRowHud)
			cursor--;
	} else if (cursor < kRowCancel) {
		cursor++;
	}
	settings_follow();
	if (settings_menu)
		settings_draw();
}

static void settings_scroll(int dir)
{
	int n = 2;
	int *v = NULL;

	if (cursor == kRowHud)
		v = &hud_pick;
	else if (cursor == kRowBeep)
		v = &beep_on;
	else if (cursor == kRowSpawn)
		v = &spawn_dungeon;
	else if (cursor == kRowBomb)
		v = &bomb_99;
	else if (cursor == kRowArrow)
		v = &arrow_99;
	else if (cursor == kRowSpeed) {
		v = &run_speed;
		n = 3;
	} else if (cursor == kRowDark) {
		v = &dark_bright;
	} else if (cursor == kRowHair) {
		v = &hair_blonde;
	} else {
		return;
	}
	if (dir < 0)
		*v = (*v + n - 1) % n;
	else
		*v = (*v + 1) % n;
	if (settings_menu)
		settings_draw();
}

static void settings_close(int save)
{
	if (ui != kUiMenu)
		return;
	save_on_exit = save;
	ui = kUiMenuOut;
}

static void ZeldaSettingsEnter(void)
{
	settings_open = 1;
	settings_menu = 0;
	ui = kUiGameOut;
	fade = 0;
	hud_pick = ZeldaHudOnTop() ? 0 : 1;
	beep_on = ZeldaLowHpBeep() ? 1 : 0;
	bomb_99 = ZeldaBombCap() == 99;
	arrow_99 = ZeldaArrowCap() == 99;
	run_speed = ZeldaRunSpeed() - 1;
	if (run_speed < 0)
		run_speed = 0;
	if (run_speed > 2)
		run_speed = 2;
	dark_bright = ZeldaDarkRooms() ? 1 : 0;
	hair_blonde = ZeldaHairBlonde() ? 1 : 0;
	spawn_dungeon = ZeldaSpawnDungeon() ? 1 : 0;
	cursor = kRowHud;
	scroll_row = 0;
	save_on_exit = 0;
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

static void note_dungeon(int slot, int entrance)
{
	ZeldaSpawnRemember(slot, entrance);
}

static int file_dungeon_entrance(int slot)
{
	return ZeldaSpawnPrepare(slot);
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
	SelectMenu_OnSettings = ZeldaSettingsEnter;
	SaveGame_NoteDungeon = note_dungeon;
	LoadFile_DungeonEntrance = file_dungeon_entrance;
}

void ZeldaHostSave(void)
{
	ZeldaWriteSram();
	ZeldaSaveFlush();
}

void ZeldaHostOnKeys(u32 held, u32 down)
{
	if (!(held & KEY_START) &&
	    (down & (KEY_L | KEY_R | KEY_SELECT)) &&
	    (held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT)) {
		if (settings_menu)
			ZeldaVideoMenuEnd();
		ZeldaVideoSetFade(-1);
		settings_open = 0;
		settings_menu = 0;
		settings_eat = 0;
		ui = kUiOff;
		SelectMenu_LockInput = 0;
		ZeldaHostSave();
		ZeldaReset(true);
		paused = 0;
		return;
	}
	if (settings_open) {
		if (ui != kUiMenu)
			return;
		if (down & KEY_UP)
			settings_move(-1);
		else if (down & KEY_DOWN)
			settings_move(1);
		else if (down & KEY_LEFT)
			settings_scroll(-1);
		else if (down & KEY_RIGHT)
			settings_scroll(1);
		else if (down & KEY_B)
			settings_close(0);
		else if ((down & (KEY_A | KEY_START)) && cursor >= kRowSave)
			settings_close(cursor == kRowSave);
		return;
	}
	if ((down & (KEY_START | KEY_SELECT)) &&
	    (held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT)) {
		paused ^= 1;
	}
}

void ZeldaHostTick(u32 held)
{
	int spun;
	int catching;

	if (settings_open || paused)
		return;
	if (settings_eat) {
		SelectMenu_LockInput = 1;
		settings_eat--;
	}
	/* A press before the sword drop runs the rest of the intro here.
	 * The sword stays on its cue. The half-second of music before that cue
	 * plays while the old picture is still up. */
	spun = 0;
	catching = 0;
	for (;;) {
		ZeldaRunFrame(map_keys(held));
		if (!ZeldaIntroCatchingUp())
			break;
		if (!catching) {
			ZeldaAudioDropLeadIn();
			catching = 1;
		}
		if (++spun >= 1000) {
			ZeldaIntroCancelCatchup();
			break;
		}
		ZeldaAudioCatchupFrame();
	}
	if (ZeldaIntroCatchupLanded())
		ZeldaAudioPlayLeadIn();
	else if (catching)
		ZeldaAudioDropLeadIn();
	SelectMenu_LockInput = 0;
	/* Sprite tiles are built here, while the previous picture is still
	 * up. The vblank blit only has to DMA them. */
	ZeldaVideoComposeSprites();
}

void ZeldaHostBlit(u16 *fb)
{
	(void)fb;
	switch (ui) {
	case kUiGameOut:
		if (!paused)
			ZeldaVideoDrawPpuFrame();
		ZeldaVideoSetFade(fade);
		if (fade < 16) {
			fade += 2;
			break;
		}
		ZeldaVideoMenuBegin();
		settings_menu = 1;
		settings_draw();
		ui = kUiMenuIn;
		break;
	case kUiMenuIn:
		ZeldaVideoSetFade(fade);
		if (fade > 0)
			fade -= 2;
		else
			ui = kUiMenu;
		break;
	case kUiMenu:
		ZeldaVideoSetFade(0);
		break;
	case kUiMenuOut:
		if (fade < 16) {
			fade += 2;
			ZeldaVideoSetFade(fade);
			break;
		}
		if (save_on_exit) {
			ZeldaHudSetOnTop(hud_pick == 0);
			ZeldaLowHpBeepSet(beep_on);
			ZeldaBombCapSet(bomb_99 ? 99 : 10);
			ZeldaArrowCapSet(arrow_99 ? 99 : 30);
			ZeldaRunSpeedSet(run_speed + 1);
			ZeldaDarkRoomsSet(dark_bright);
			ZeldaHairSet(hair_blonde);
			ZeldaSpawnSet(spawn_dungeon);
		}
		ZeldaVideoMenuEnd();
		settings_menu = 0;
		ui = kUiGameIn;
		__attribute__((fallthrough));
	case kUiGameIn:
		if (!paused)
			ZeldaVideoDrawPpuFrame();
		ZeldaVideoSetFade(fade);
		if (fade > 0) {
			fade -= 2;
			break;
		}
		ZeldaVideoSetFade(-1);
		settings_open = 0;
		settings_eat = 2;
		ui = kUiOff;
		break;
	default:
		if (!paused)
			ZeldaVideoDrawPpuFrame();
		break;
	}
}

void ZeldaHostAudio(void)
{
	if (!paused)
		ZeldaAudioFrame();
}
