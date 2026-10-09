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
#include "features.h"
#include "snes/ppu.h"
#include "snes/dma.h"
#include "zelda_video.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

#ifndef PHASE3_STEP
#define PHASE3_STEP 1
#endif

#define PPU_PAD kPpuExtraLeftRight
#define PPU_STRIDE (256 + PPU_PAD * 2)
#define PPU_H 224
#define CROP_Y 16

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

static int phase3_step = PHASE3_STEP;
static int phase3_new_renderer = 1;
static int present_dirty = 1;
static int game_chr_ready;
static int blit_frames;
static uint32 ppu_pixels[PPU_STRIDE * PPU_H];
static u16 fb_copy[SCREEN_WIDTH * SCREEN_HEIGHT];

/* 8x8 letter N, color 15 vs 0. */
static const uint8 kGlyphN[8][8] = {
	{ 15, 0,  0,  0,  0,  0,  0, 15 },
	{ 15, 15, 0,  0,  0,  0,  0, 15 },
	{ 15, 0,  15, 0,  0,  0,  0, 15 },
	{ 15, 0,  0,  15, 0,  0,  0, 15 },
	{ 15, 0,  0,  0,  15, 0,  0, 15 },
	{ 15, 0,  0,  0,  0,  15, 0, 15 },
	{ 15, 0,  0,  0,  0,  0,  15, 15 },
	{ 15, 0,  0,  0,  0,  0,  0, 15 },
};

static u16 px15(int r, int g, int b)
{
	return RGB15(r, g, b) | BIT(15);
}

static void fb_clear(u16 c)
{
	int i;
	for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
		fb_copy[i] = c;
}

static void fb_present(u16 *fb)
{
	memcpy(fb, fb_copy, sizeof(fb_copy));
}

static void encode_snes4bpp(uint16 *tile, const uint8 pix[8][8])
{
	int y, x;

	for (y = 0; y < 8; y++) {
		uint16 p01 = 0, p23 = 0;
		for (x = 0; x < 8; x++) {
			int bit = 7 - x;
			int p = pix[y][x] & 15;
			if (p & 1)
				p01 |= (uint16)(1u << bit);
			if (p & 2)
				p01 |= (uint16)(1u << (bit + 8));
			if (p & 4)
				p23 |= (uint16)(1u << bit);
			if (p & 8)
				p23 |= (uint16)(1u << (bit + 8));
		}
		tile[y] = p01;
		tile[y + 8] = p23;
	}
}

static int snes4bpp_pixel(const uint16 *tile, int x, int y)
{
	int bit = 7 - x;
	uint16 p01 = tile[y];
	uint16 p23 = tile[y + 8];

	return ((p01 >> bit) & 1)
		| (((p01 >> (bit + 8)) & 1) << 1)
		| (((p23 >> bit) & 1) << 2)
		| (((p23 >> (bit + 8)) & 1) << 3);
}

static void magnify_tile(const uint16 *tile, int ox, int oy, int mag, int ink_only)
{
	int x, y, mx, my;

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			int p = snes4bpp_pixel(tile, x, y);
			u16 c;
			if (!p && ink_only)
				continue;
			c = p ? px15(31, 31, 31) : px15(0, 0, 0);
			for (my = 0; my < mag; my++) {
				for (mx = 0; mx < mag; mx++) {
					int dx = ox + x * mag + mx;
					int dy = oy + y * mag + my;
					if (dx >= 0 && dx < SCREEN_WIDTH && dy >= 0 && dy < SCREEN_HEIGHT)
						fb_copy[dy * SCREEN_WIDTH + dx] = c;
				}
			}
		}
	}
}

/* Same 3bpp→4bpp expansion as Graphics_LoadChrHalfSlot, second half of pack 8. */
static uint16 logo_tiles[32 * 16];
static int logo_tiles_ok;

static void expand_logo_half(const uint8 *src)
{
	int tile, row;

	src += 0x300;
	for (tile = 0; tile < 32; tile++) {
		uint16 *dst = logo_tiles + tile * 16;
		const uint8 *p0 = src;
		const uint8 *p2 = src + 16;

		src += 24;
		for (row = 0; row < 8; row++) {
			uint16 t = (uint16)p0[0] | ((uint16)p0[1] << 8);
			uint8 u = p2[0];

			dst[row] = t;
			dst[row + 8] = (uint16)(((t | (t >> 8) | u) << 8) | u);
			p0 += 2;
			p2 += 1;
		}
	}
}

static void ensure_logo_tiles(void)
{
	MemBlk blk;

	if (logo_tiles_ok)
		return;
	blk = kSprGfx(8);
	if (!blk.ptr || blk.size < 0x600)
		return;
	expand_logo_half(blk.ptr);
	logo_tiles_ok = 1;
}

static int obj16_name(int base, int col, int row)
{
	return ((base & 0xf0) + (row ? 0x10 : 0)) | (((base & 0x0f) + col) & 0x0f);
}

static void blit_obj16(int name, int ox, int oy, int mag)
{
	int row, col;

	for (row = 0; row < 2; row++) {
		for (col = 0; col < 2; col++) {
			int n = obj16_name(name, col, row) - 0x60;
			if (n < 0 || n >= 32)
				continue;
			magnify_tile(&logo_tiles[n * 16], ox + col * 8 * mag, oy + row * 8 * mag, mag, 1);
		}
	}
}

/* LakeSnes/PC: 0x00RRGGBB. DS RGB15 is the same 555 packing snes9x uses on RGB555 hosts.
 * 3DS port (EstebanPdN/zelda-alttp-3ds) converts that 32-bit buffer to RGB565. */
static void present_argb_crop(u16 *fb, int stride, int x0)
{
	int x, y;

	for (y = 0; y < SCREEN_HEIGHT; y++) {
		const uint32 *src = ppu_pixels + (y + CROP_Y) * stride + x0;
		u16 *dst = fb_copy + y * SCREEN_WIDTH;
		for (x = 0; x < SCREEN_WIDTH; x++) {
			u32 c = src[x];
			dst[x] = RGB8((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff) | BIT(15);
		}
	}
	fb_present(fb);
}

static uint32 ppu_draw_flags(void)
{
	return phase3_new_renderer ? kPpuRenderFlags_NewRenderer : 0;
}

static int ZeldaHostMapKeys(u32 held)
{
	int inputs = 0;

	if (held & KEY_B)
		inputs |= 1 << 0;
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
	g_config.enable_audio = false;
	g_config.audio_freq = 22050;
	g_config.audio_channels = 1;

	ZeldaLoadAssets();
	ZeldaInitialize();
	/* PC 4:3: extraLeftRight = min(extended_aspect_ratio, 96) = 0. */
	g_zenv.ppu->extraLeftRight = 0;
	g_wanted_zelda_features = 0;
	ZeldaEnableMsu(0);
	ZeldaSetLanguage(NULL);
	ZeldaVideoInit();
	iprintf("ZeldaInitialize: ok\n");
	iprintf("video flags %lu extra %d\n",
		(unsigned long)ZeldaVideoRenderFlags(), g_zenv.ppu->extraLeftRight);
}

int ZeldaHostPhase3Step(void)
{
	return phase3_step;
}

static void restart_game_cpu(void)
{
	dma_reset(g_zenv.dma);
	ppu_reset(g_zenv.ppu);
	memset(g_zenv.ram, 0, 0x20000);
	g_zenv.ppu->extraLeftRight = 0;
	game_chr_ready = 0;
}

void ZeldaHostOnKeys(u32 held, u32 down)
{
	int prev = phase3_step;

	(void)held;

	if ((down & (KEY_X | KEY_RIGHT)) && phase3_step < 5) {
		phase3_step++;
		present_dirty = 1;
	}
	if ((down & (KEY_Y | KEY_LEFT)) && phase3_step > 1) {
		phase3_step--;
		present_dirty = 1;
	}
	if ((down & KEY_A) && (phase3_step == 3 || phase3_step == 5)) {
		phase3_new_renderer ^= 1;
		ZeldaVideoSetNewRenderer(phase3_new_renderer);
		present_dirty = 1;
		iprintf("renderer %s\n", phase3_new_renderer ? "new" : "old");
	}

	if (phase3_step != prev &&
	    ((phase3_step == 4 && prev <= 3) || phase3_step == 5)) {
		iprintf("-> 3%c restart game\n", 'a' + phase3_step - 1);
		restart_game_cpu();
	}
}

void ZeldaHostTick(u32 held)
{
	if (phase3_step == 4 && !game_chr_ready) {
		ZeldaRunFrame(0);
		if (g_zenv.vram[0x4690] != 0) {
			game_chr_ready = 1;
			present_dirty = 1;
			iprintf("3d CHR ready %04X\n", g_zenv.vram[0x4690]);
		}
	} else if (phase3_step == 5) {
		ZeldaRunFrame(ZeldaHostMapKeys(held));
	}
}

static void step_3a(u16 *fb)
{
	int x, y;

	for (y = 0; y < SCREEN_HEIGHT; y++) {
		for (x = 0; x < SCREEN_WIDTH; x++) {
			u16 c;
			if (x < 128)
				c = px15(31, 0, 0);
			else
				c = ((x / 8 + y / 8) & 1) ? px15(31, 31, 31) : px15(0, 0, 0);
			fb_copy[y * SCREEN_WIDTH + x] = c;
		}
	}
	fb_present(fb);
	if (blit_frames == 0)
		iprintf("3a FB0 red|checker\n");
}

static void step_3b(u16 *fb)
{
	uint16 tile[16];

	encode_snes4bpp(tile, kGlyphN);
	fb_clear(px15(7, 0, 7));
	magnify_tile(tile, 64, 32, 16, 0);
	fb_present(fb);
	if (blit_frames == 0)
		iprintf("3b glyph N x16  t0=%04X\n", tile[0]);
}

static void make_border_tile(uint16 *tile, int top, int left, int bot, int right)
{
	uint8 pix[8][8];
	int x, y;

	memset(pix, 0, sizeof(pix));
	for (x = 0; x < 8; x++) {
		if (top)
			pix[0][x] = 15;
		if (bot)
			pix[7][x] = 15;
	}
	for (y = 0; y < 8; y++) {
		if (left)
			pix[y][0] = 15;
		if (right)
			pix[y][7] = 15;
	}
	encode_snes4bpp(tile, pix);
}

static void step_3c(u16 *fb)
{
	Ppu *ppu = g_zenv.ppu;
	uint16 tile[16];
	int i;

	ppu_reset(ppu);
	ppu->extraLeftRight = PPU_PAD;
	ppu->extraLeftCur = 0;
	ppu->extraRightCur = 0;
	ppu->forcedBlank = false;
	ppu->brightness = 15;
	ppu->lastBrightnessMult = 0xff;
	ppu->mode = 1;
	ppu->screenEnabled[0] = 0x10;
	ppu->screenEnabled[1] = 0;
	ppu->objTileAdr1 = 0x4000;
	ppu->objSize = 0;

	make_border_tile(tile, 1, 1, 0, 0);
	memcpy(&ppu->vram[0x4690], tile, 32);
	make_border_tile(tile, 1, 0, 0, 1);
	memcpy(&ppu->vram[0x46a0], tile, 32);
	make_border_tile(tile, 0, 1, 1, 0);
	memcpy(&ppu->vram[0x4790], tile, 32);
	make_border_tile(tile, 0, 0, 1, 1);
	memcpy(&ppu->vram[0x47a0], tile, 32);

	for (i = 0x90; i < 0xa0; i++)
		ppu->cgram[i] = 0x7fff;

	memset(ppu->oam, 0, sizeof(ppu->oam));
	for (i = 0; i < 128; i++)
		ppu->oam[i * 2] = 0xf0 << 8;
	ppu->oam[0] = (104 << 8) | 96;
	ppu->oam[1] = (0x32 << 8) | 0x69;
	ppu->oam[0x100] = 2;

	memset(ppu_pixels, 0, sizeof(ppu_pixels));
	PpuBeginDrawing(ppu, (uint8 *)ppu_pixels, PPU_STRIDE * 4, ppu_draw_flags());
	for (i = 0; i <= 224; i++)
		ppu_runLine(ppu, i);

	present_argb_crop(fb, PPU_STRIDE, PPU_PAD);
	iprintf("3c spr %s pad%d v=%04X\n",
		phase3_new_renderer ? "new" : "old",
		ppu->extraLeftRight, ppu->vram[0x4690]);
}

static void step_3d(u16 *fb)
{
	static const uint8 kLogoX[4] = { 0, 16, 32, 40 };
	static const uint8 kLogoTile[4] = { 0x69, 0x6b, 0x6d, 0x6e };
	const int mag = 3;
	int i;

	ensure_logo_tiles();
	fb_clear(px15(4, 0, 8));
	if (logo_tiles_ok) {
		int ox = (SCREEN_WIDTH - 56 * mag) / 2;
		int oy = (SCREEN_HEIGHT - 16 * mag) / 2;
		for (i = 0; i < 4; i++)
			blit_obj16(kLogoTile[i], ox + kLogoX[i] * mag, oy, mag);
	}
	fb_present(fb);
	iprintf("3d logo $69=%04X vram=%04X\n",
		logo_tiles_ok ? logo_tiles[9 * 16] : 0,
		g_zenv.vram[0x4690]);
}

static void step_3e(u16 *fb)
{
	(void)fb;
	ZeldaVideoDrawPpuFrame();
	if ((blit_frames & 15) == 0) {
		iprintf("3e sdl %s extra%d blank%d %04X\n",
			ZeldaVideoNewRenderer() ? "new" : "old",
			g_zenv.ppu->extraLeftRight,
			g_zenv.ppu->forcedBlank,
			g_zenv.vram[0x4690]);
	}
}

void ZeldaHostBlit(u16 *fb)
{
	if (phase3_step <= 4 && !present_dirty) {
		fb_present(fb);
		return;
	}

	switch (phase3_step) {
	case 1:
		step_3a(fb);
		break;
	case 2:
		step_3b(fb);
		break;
	case 3:
		step_3c(fb);
		break;
	case 4:
		step_3d(fb);
		break;
	default:
		step_3e(fb);
		break;
	}
	if (phase3_step <= 4)
		present_dirty = 0;
	blit_frames++;
}
