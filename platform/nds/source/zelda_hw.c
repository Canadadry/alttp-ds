#include <nds.h>
#include <string.h>

#include "snes/ppu.h"

/*
 * Mode 1 picture on the DS 2D engine.
 *
 * SNES mode 1 is three tiled layers plus sprites. The DS main engine has
 * four tiled backgrounds and 128 hardware sprites, so the frame is tile
 * uploads, map entries, palettes, and OAM — not a software scanline plot.
 *
 * Back to front:
 *   BG3 priority-1 tiles  HUD, in front of sprites
 *   BG1 priority-1 tiles  overhangs, in front of Link
 *   sprites priority 2    Link
 *   BG1 priority-0 tiles
 *   BG2                   ground, behind Link
 *
 * BG1 is split across two backgrounds so its per-tile priority bit still
 * puts doorways and tree tops in front of the player. BG2 keeps one scroll,
 * so its priority-1 tiles stay with the ground instead of being copied onto
 * BG1 (those layers do not share a scroll). BG3 priority-0 tiles are omitted.
 * Color math (the lamp spotlight, translucent overlays) has no DS equivalent
 * here. Mode 7 stays on the scanline renderer.
 *
 * Bank A (128KB), 16KB tile bases and 2KB map bases:
 *   0x00000 maps: HUD, BG1 high, BG1 low, BG2  (8KB each)
 *   0x08000 BG1 tiles
 *   0x10000 BG2 tiles
 *   0x18000 BG3 tiles
 * Bank E holds sprite characters, 128 bytes per OAM slot.
 */

enum {
	kCropY = 16,
	kBlankNone = -1
};

static int bg_id[4];
static uint8 bg_size[4];
static uint32 tile_hash[3][1024];
static uint32 map_sig[3];
static int map_valid[3];
static uint16 last_adr[3];
static uint8 used_bg1[1024];
static int blank_bg1 = kBlankNone;

/* 2bpp tiles are compacted. Slot 0 is a transparent tile. */
static uint16 bg3_slot[8 * 1024];
static int bg3_next;
static uint32 spr_hash[128];

static u16 map_buf[4][4096] __attribute__((aligned(4)));

static uint32 hash_words(const uint16 *vram, uint16 addr, int n) {
	uint32 h = 2166136261u;
	int i;

	for (i = 0; i < n; i++)
		h = (h ^ vram[(addr + i) & 0x7fff]) * 16777619u;
	return h ? h : 1u;
}

static void conv4(const uint16 *src, u16 *dst, int pal_off) {
	int row;

	for (row = 0; row < 8; row++) {
		uint8 plane0 = (uint8)src[row];
		uint8 plane1 = (uint8)(src[row] >> 8);
		uint8 plane2 = (uint8)src[row + 8];
		uint8 plane3 = (uint8)(src[row + 8] >> 8);
		uint8 bytes[4];
		int col;

		for (col = 0; col < 4; col++) {
			int bit0 = 7 - col * 2;
			int bit1 = bit0 - 1;
			uint32 px0 = ((plane0 >> bit0) & 1) |
				(((plane1 >> bit0) & 1) << 1) |
				(((plane2 >> bit0) & 1) << 2) |
				(((plane3 >> bit0) & 1) << 3);
			uint32 px1 = ((plane0 >> bit1) & 1) |
				(((plane1 >> bit1) & 1) << 1) |
				(((plane2 >> bit1) & 1) << 2) |
				(((plane3 >> bit1) & 1) << 3);
			if (pal_off) {
				if (px0)
					px0 += (uint32)pal_off;
				if (px1)
					px1 += (uint32)pal_off;
			}
			bytes[col] = (uint8)(px0 | (px1 << 4));
		}
		dst[row * 2] = (u16)(bytes[0] | (bytes[1] << 8));
		dst[row * 2 + 1] = (u16)(bytes[2] | (bytes[3] << 8));
	}
}

static void conv2(const uint16 *src, u16 *dst, int pal_off) {
	uint16 wide[16];
	int i;

	for (i = 0; i < 8; i++) {
		wide[i] = src[i];
		wide[i + 8] = 0;
	}
	conv4(wide, dst, pal_off);
}

static void clear_tile(u16 *gfx, int tile) {
	int i;
	u16 *dst = gfx + tile * 16;

	for (i = 0; i < 16; i++)
		dst[i] = 0;
}

static void upload4_tile(const Ppu *ppu, int bank, u16 *gfx, uint16 tile_adr, int tile) {
	uint16 addr = (uint16)((tile_adr + tile * 16) & 0x7fff);
	uint32 h = hash_words(ppu->vram, addr, 16);
	uint16 tmp[16];
	int i;

	if (tile_hash[bank][tile] == h)
		return;
	for (i = 0; i < 16; i++)
		tmp[i] = ppu->vram[(addr + i) & 0x7fff];
	conv4(tmp, gfx + tile * 16, 0);
	tile_hash[bank][tile] = h;
}

static u16 swizzle4(uint16 entry) {
	return (u16)((entry & 0x3ff) |
		((entry & 0x4000) >> 4) |
		((entry & 0x8000) >> 4) |
		(((entry >> 10) & 7) << 12));
}

static int map_words(int wider, int higher) {
	return 1024 << ((wider ? 1 : 0) + (higher ? 1 : 0));
}

static BgSize map_bg_size(int wider, int higher) {
	if (wider && higher)
		return BgSize_T_512x512;
	if (wider)
		return BgSize_T_512x256;
	if (higher)
		return BgSize_T_256x512;
	return BgSize_T_256x256;
}

static uint32 sig_map(const Ppu *ppu, uint16 adr, int words) {
	uint32 h = 2166136261u;
	int i;

	for (i = 0; i < words; i++)
		h = (h ^ ppu->vram[(adr + i) & 0x7fff]) * 16777619u;
	return h ? h : 1u;
}

static void ensure_size(int slot, int wider, int higher, int map_base, int tile_base) {
	uint8 sid = (uint8)((wider ? 1 : 0) | (higher ? 2 : 0));

	if (bg_size[slot] == sid)
		return;
	bg_id[slot] = bgInit(slot, BgType_Text4bpp, map_bg_size(wider, higher), map_base, tile_base);
	bg_size[slot] = sid;
}

static void present_bg1(Ppu *ppu) {
	BgLayer *layer = &ppu->bgLayer[0];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);
	uint32 sig = sig_map(ppu, layer->tilemapAdr, words);
	u16 *gfx;
	int tile;

	ensure_size(1, layer->tilemapWider, layer->tilemapHigher, 4, 2);
	ensure_size(2, layer->tilemapWider, layer->tilemapHigher, 8, 2);
	gfx = bgGetGfxPtr(bg_id[1]);

	if (last_adr[0] != layer->tileAdr) {
		memset(tile_hash[0], 0, sizeof tile_hash[0]);
		last_adr[0] = layer->tileAdr;
		map_valid[0] = 0;
	}

	if (!map_valid[0] || map_sig[0] != sig) {
		int i;

		memset(used_bg1, 0, sizeof used_bg1);
		for (i = 0; i < words; i++)
			used_bg1[ppu->vram[(layer->tilemapAdr + i) & 0x7fff] & 0x3ff] = 1;
		blank_bg1 = kBlankNone;
		for (i = 0; i < 1024; i++) {
			if (!used_bg1[i]) {
				blank_bg1 = i;
				break;
			}
		}
		for (i = 0; i < words; i++) {
			uint16 entry = ppu->vram[(layer->tilemapAdr + i) & 0x7fff];
			u16 blank = (u16)(blank_bg1 < 0 ? 0 : blank_bg1);

			if (entry & 0x2000) {
				map_buf[1][i] = swizzle4(entry);
				map_buf[2][i] = blank;
			} else {
				map_buf[1][i] = blank;
				map_buf[2][i] = swizzle4(entry);
			}
		}
		DC_FlushRange(map_buf[1], (uint32)words * 2);
		DC_FlushRange(map_buf[2], (uint32)words * 2);
		dmaCopyWords(3, map_buf[1], bgGetMapPtr(bg_id[1]), (uint32)words * 2);
		dmaCopyWords(3, map_buf[2], bgGetMapPtr(bg_id[2]), (uint32)words * 2);
		map_sig[0] = sig;
		map_valid[0] = 1;
	}

	for (tile = 0; tile < 1024; tile++) {
		if (!used_bg1[tile] || tile == blank_bg1)
			continue;
		upload4_tile(ppu, 0, gfx, layer->tileAdr, tile);
	}
	if (blank_bg1 >= 0 && tile_hash[0][blank_bg1] != 0) {
		clear_tile(gfx, blank_bg1);
		tile_hash[0][blank_bg1] = 0;
	}
}

static void present_bg2(Ppu *ppu) {
	BgLayer *layer = &ppu->bgLayer[1];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);
	uint32 sig = sig_map(ppu, layer->tilemapAdr, words);
	u16 *gfx;
	uint8 used[1024];
	int tile;

	ensure_size(3, layer->tilemapWider, layer->tilemapHigher, 12, 4);
	gfx = bgGetGfxPtr(bg_id[3]);

	if (last_adr[1] != layer->tileAdr) {
		memset(tile_hash[1], 0, sizeof tile_hash[1]);
		last_adr[1] = layer->tileAdr;
		map_valid[1] = 0;
	}

	memset(used, 0, sizeof used);
	if (!map_valid[1] || map_sig[1] != sig) {
		int i;

		for (i = 0; i < words; i++) {
			uint16 entry = ppu->vram[(layer->tilemapAdr + i) & 0x7fff];

			used[entry & 0x3ff] = 1;
			map_buf[3][i] = swizzle4(entry);
		}
		DC_FlushRange(map_buf[3], (uint32)words * 2);
		dmaCopyWords(3, map_buf[3], bgGetMapPtr(bg_id[3]), (uint32)words * 2);
		map_sig[1] = sig;
		map_valid[1] = 1;
	} else {
		int i;

		for (i = 0; i < words; i++)
			used[ppu->vram[(layer->tilemapAdr + i) & 0x7fff] & 0x3ff] = 1;
	}

	for (tile = 0; tile < 1024; tile++) {
		if (used[tile])
			upload4_tile(ppu, 1, gfx, layer->tileAdr, tile);
	}
}

static int bg3_lookup(int key) {
	int slot = bg3_slot[key];

	if (slot)
		return slot - 1;
	if (bg3_next >= 1024)
		return -1;
	slot = bg3_next++;
	bg3_slot[key] = (uint16)(slot + 1);
	return slot;
}

static void present_bg3(Ppu *ppu) {
	BgLayer *layer = &ppu->bgLayer[2];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);
	uint32 sig = sig_map(ppu, layer->tilemapAdr, words);
	u16 *gfx;
	int i;

	ensure_size(0, layer->tilemapWider, layer->tilemapHigher, 0, 6);
	gfx = bgGetGfxPtr(bg_id[0]);

	if (last_adr[2] != layer->tileAdr) {
		memset(tile_hash[2], 0, sizeof tile_hash[2]);
		memset(bg3_slot, 0, sizeof bg3_slot);
		bg3_next = 1;
		last_adr[2] = layer->tileAdr;
		map_valid[2] = 0;
	}

	if (!map_valid[2] || map_sig[2] != sig) {
		for (i = 0; i < words; i++) {
			uint16 entry = ppu->vram[(layer->tilemapAdr + i) & 0x7fff];
			int tile = entry & 0x3ff;
			int pal = (entry >> 10) & 7;
			int slot;
			u16 out;

			if ((entry & 0x2000) == 0) {
				map_buf[0][i] = 0;
				continue;
			}
			slot = bg3_lookup((tile << 3) | pal);
			if (slot < 0) {
				map_buf[0][i] = 0;
				continue;
			}
			out = (u16)slot;
			if (entry & 0x4000)
				out |= 1 << 10;
			if (entry & 0x8000)
				out |= 1 << 11;
			out |= (u16)((pal >> 2) << 12);
			map_buf[0][i] = out;
		}
		DC_FlushRange(map_buf[0], (uint32)words * 2);
		dmaCopyWords(3, map_buf[0], bgGetMapPtr(bg_id[0]), (uint32)words * 2);
		map_sig[2] = sig;
		map_valid[2] = 1;
	}

	for (i = 0; i < 8 * 1024; i++) {
		int slot = bg3_slot[i];
		int tile;
		int pal;
		uint16 addr;
		uint32 h;
		uint16 tmp[8];
		int w;

		if (!slot)
			continue;
		slot -= 1;
		tile = i >> 3;
		pal = i & 7;
		addr = (uint16)((layer->tileAdr + tile * 8) & 0x7fff);
		h = hash_words(ppu->vram, addr, 8) ^ (uint32)(pal + 1);
		if (tile_hash[2][slot] == h)
			continue;
		for (w = 0; w < 8; w++)
			tmp[w] = ppu->vram[(addr + w) & 0x7fff];
		conv2(tmp, gfx + slot * 16, (pal & 3) * 4);
		tile_hash[2][slot] = h;
	}
}

static u16 shade(u16 color, int brightness) {
	int r, g, b;

	if (brightness >= 15)
		return (u16)(color & 0x7fff);
	if (brightness <= 0)
		return 0;
	r = (color & 31) * brightness / 15;
	g = ((color >> 5) & 31) * brightness / 15;
	b = ((color >> 10) & 31) * brightness / 15;
	return (u16)(r | (g << 5) | (b << 10));
}

static void present_palettes(const Ppu *ppu) {
	int i;
	int brightness = ppu->brightness;

	for (i = 0; i < 128; i++)
		BG_PALETTE[i] = shade(ppu->cgram[i], brightness);
	for (i = 0; i < 128; i++)
		SPRITE_PALETTE[i] = shade(ppu->cgram[128 + i], brightness);
}

static int obj_tile(int name, int tx, int ty) {
	return (((name >> 4) + ty) << 4) | (((name & 0xf) + tx) & 0xf);
}

static int ds_sprite_priority(int snes_priority) {
	if (snes_priority >= 3)
		return 1;
	if (snes_priority == 2)
		return 2;
	return 3;
}

static void present_sprites(Ppu *ppu, int sprites_on) {
	int sprite;

	if (!sprites_on || ppu->forcedBlank) {
		oamClear(&oamMain, 0, 128);
		oamUpdate(&oamMain);
		return;
	}

	for (sprite = 0; sprite < 128; sprite++) {
		int index = sprite * 2;
		int y = ppu->oam[index] >> 8;
		int high;
		int oam1;
		int x;
		int size;
		int name;
		int base;
		u16 *gfx;
		uint32 h;
		int tx, ty, n;

		if (y == 0xf0) {
			oamSet(&oamMain, sprite, 0, 0, 0, 0, SpriteSize_8x8,
				SpriteColorFormat_16Color, SPRITE_GFX, -1, false, true,
				false, false, false);
			continue;
		}

		high = ppu->oam[0x100 + (index >> 4)] >> (index & 15);
		oam1 = ppu->oam[index + 1];
		x = (ppu->oam[index] & 0xff) + ((high & 1) ? 256 : 0);
		if (x >= 256)
			x -= 512;
		y -= kCropY;
		if (x < 0)
			x += 512;
		if (y < 0)
			y += 256;
		size = (high & 2) ? 16 : 8;
		name = oam1 & 0xff;
		base = (oam1 & 0x100) ? ppu->objTileAdr2 : ppu->objTileAdr1;
		gfx = SPRITE_GFX + sprite * 64;

		h = 2166136261u;
		for (ty = 0; ty < size; ty += 8) {
			for (tx = 0; tx < size; tx += 8)
				h = hash_words(ppu->vram,
					(uint16)((base + obj_tile(name, tx / 8, ty / 8) * 16) & 0x7fff),
					16) ^ (h * 16777619u);
		}
		if (h == 0)
			h = 1;
		if (spr_hash[sprite] != h) {
			n = 0;
			for (ty = 0; ty < size; ty += 8) {
				for (tx = 0; tx < size; tx += 8, n++) {
					uint16 addr = (uint16)((base + obj_tile(name, tx / 8, ty / 8) * 16) & 0x7fff);
					uint16 tmp[16];
					int w;

					for (w = 0; w < 16; w++)
						tmp[w] = ppu->vram[(addr + w) & 0x7fff];
					conv4(tmp, gfx + n * 16, 0);
				}
			}
			spr_hash[sprite] = h;
		}

		oamSet(&oamMain, sprite, x, y, ds_sprite_priority((oam1 >> 12) & 3),
			(oam1 >> 9) & 7,
			size == 16 ? SpriteSize_16x16 : SpriteSize_8x8,
			SpriteColorFormat_16Color, gfx, -1, false, false,
			(oam1 & 0x4000) != 0, (oam1 & 0x8000) != 0, false);
	}
	oamUpdate(&oamMain);
}

static void present_scroll(const Ppu *ppu) {
	int sy0 = (int)ppu->bgLayer[0].vScroll + kCropY;
	int sy1 = (int)ppu->bgLayer[1].vScroll + kCropY;
	int sy2 = (int)ppu->bgLayer[2].vScroll + kCropY;

	bgSetScroll(bg_id[1], ppu->bgLayer[0].hScroll, sy0);
	bgSetScroll(bg_id[2], ppu->bgLayer[0].hScroll, sy0);
	bgSetScroll(bg_id[3], ppu->bgLayer[1].hScroll, sy1);
	bgSetScroll(bg_id[0], ppu->bgLayer[2].hScroll, sy2);
	bgSetPriority(bg_id[0], 0);
	bgSetPriority(bg_id[1], 1);
	bgSetPriority(bg_id[2], 2);
	bgSetPriority(bg_id[3], 3);
	bgUpdate();
}

void ZeldaHwInit(void) {
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankE(VRAM_E_MAIN_SPRITE);
	irqEnable(IRQ_VBLANK);
	oamInit(&oamMain, SpriteMapping_1D_32, false);
	lcdMainOnTop();

	bg_size[0] = bg_size[1] = bg_size[2] = bg_size[3] = 0xff;
	bg_id[0] = bgInit(0, BgType_Text4bpp, BgSize_T_256x256, 0, 6);
	bg_id[1] = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 4, 2);
	bg_id[2] = bgInit(2, BgType_Text4bpp, BgSize_T_256x256, 8, 2);
	bg_id[3] = bgInit(3, BgType_Text4bpp, BgSize_T_256x512, 12, 4);
	bg_size[0] = 0;
	bg_size[1] = 0;
	bg_size[2] = 0;
	bg_size[3] = 2;
	bgSetPriority(bg_id[0], 0);
	bgSetPriority(bg_id[1], 1);
	bgSetPriority(bg_id[2], 2);
	bgSetPriority(bg_id[3], 3);

	memset(tile_hash, 0, sizeof tile_hash);
	memset(map_valid, 0, sizeof map_valid);
	memset(bg3_slot, 0, sizeof bg3_slot);
	memset(spr_hash, 0, sizeof spr_hash);
	bg3_next = 1;
	last_adr[0] = last_adr[1] = last_adr[2] = 0xffff;
	blank_bg1 = kBlankNone;
	dmaFillWords(0, BG_GFX, 128 * 1024);
	dmaFillWords(0, SPRITE_GFX, 16 * 1024);
	oamClear(&oamMain, 0, 128);
	oamUpdate(&oamMain);
}

void ZeldaHwPresent(Ppu *ppu) {
	int enable = ppu->forcedBlank ? 0 : ppu->screenEnabled[0];

	present_bg1(ppu);
	present_bg2(ppu);
	present_bg3(ppu);
	present_palettes(ppu);
	present_sprites(ppu, (enable & 0x10) != 0);
	present_scroll(ppu);

	if (enable & 0x04)
		bgShow(bg_id[0]);
	else
		bgHide(bg_id[0]);
	if (enable & 0x01) {
		bgShow(bg_id[1]);
		bgShow(bg_id[2]);
	} else {
		bgHide(bg_id[1]);
		bgHide(bg_id[2]);
	}
	if (enable & 0x02)
		bgShow(bg_id[3]);
	else
		bgHide(bg_id[3]);
	REG_DISPCNT |= DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D | DISPLAY_SPR_1D_SIZE_32;
}
