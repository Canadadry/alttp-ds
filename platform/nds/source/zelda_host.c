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
static int save_on_exit;

enum {
	kRowHud = 0,
	kRowBeep = 1,
	kRowSave = 2,
	kRowCancel = 3,
	kCaretX = 16,
	kLabelX = 36,
	kValueX = 120,
	kTextX = 36,
	kTitleY = 22,
	kHudY = 64,
	kBeepY = 88,
	kSaveY = 150,
	kCancelY = 170
};

static const char *const kHudChoice[] = { "Top Screen", "Bottom Screen" };
static const char *const kOnOff[] = { "Off", "On" };

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

static void settings_draw(void)
{
	const char *title = "Settings";
	int value_x = settings_value_x();
	int caret_y = kCancelY;

	ZeldaVideoMenuClear();
	ZeldaVideoMenuText((256 - ZeldaVideoMenuWidth(title)) / 2, kTitleY, title);
	ZeldaVideoMenuText(kLabelX, kHudY, "HUD");
	ZeldaVideoMenuText(value_x, kHudY, kHudChoice[hud_pick]);
	ZeldaVideoMenuText(kLabelX, kBeepY, "Low HP Indicator");
	ZeldaVideoMenuText(value_x, kBeepY, kOnOff[beep_on]);
	ZeldaVideoMenuText(kTextX, kSaveY, "Save");
	ZeldaVideoMenuText(kTextX, kCancelY, "Cancel");
	if (cursor == kRowHud)
		caret_y = kHudY;
	else if (cursor == kRowBeep)
		caret_y = kBeepY;
	else if (cursor == kRowSave)
		caret_y = kSaveY;
	ZeldaVideoMenuText(kCaretX, caret_y, ">");
}

static void settings_move(int dir)
{
	if (dir < 0) {
		if (cursor > kRowHud)
			cursor--;
	} else if (cursor < kRowCancel) {
		cursor++;
	}
	if (settings_menu)
		settings_draw();
}

static void settings_scroll(int dir)
{
	(void)dir;
	if (cursor == kRowHud)
		hud_pick ^= 1;
	else if (cursor == kRowBeep)
		beep_on ^= 1;
	else
		return;
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
	cursor = kRowHud;
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
