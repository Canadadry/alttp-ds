#include <nds.h>
#include <string.h>

#include "zelda_rtl.h"
#include "variables.h"
#include "snes/ppu.h"

/*
 * Mode 1 picture on the DS 2D engine.
 *
 * SNES mode 1 is three tiled layers plus sprites. The DS main engine has
 * four tiled backgrounds and 128 hardware sprites, so the frame is tile
 * uploads, map entries, palettes, and OAM — not a software scanline plot.
 *
 * Back to front, matching PpuDrawBackgrounds in mode 1 with BG3 priority
 * (the only mode-1 setting this game writes, BGMODE 9):
 *   BG3 priority 1        HUD, in front of every sprite
 *   sprites priority 3
 *   BG1 priority 1        overhangs, and the file-select wallpaper
 *   BG2 priority 1        doorways and other tiles that cover Link
 *   sprites priority 2    Link
 *   BG1 priority 0
 *   BG2 priority 0        ground, in front of sprites 0 and 1
 *   sprites priority 1
 *   sprites priority 0
 *   BG3 priority 0        name letters. Behind the wallpaper.
 *   backdrop
 *
 * The DS has four backgrounds, so BG2 priority 1 stays on the ground layer
 * (it has its own scroll and cannot move onto BG1) and BG3 priority 0 shares
 * that hardware layer: the letters use it while BG2 is off. A subscreen-only
 * picture with nothing behind it (the title art) stays at priority 3. A
 * subscreen BG1 over an opaque BG2 is the rain and the other overworld
 * overlays: it is drawn in front of every sprite and alpha-blended, which
 * is what SNES half-add color math does. A ledge jump raises Link to sprite
 * priority 3 (sideways, south, and diagonal), and that still has to sit
 * under the rain. A fixed color
 * add or subtract is applied to the palettes, which is the blue wash in
 * the opening and the dark rooms. Mosaic uses the DS mosaic register, so
 * the pixelated fade when leaving the house is visible. The door iris is
 * the SNES window HDMA table: outside the circle the picture is black, and
 * the circle closes on Link, then opens again outside. Mode 7 stays on
 * the scanline renderer.
 *
 * Naming a file scrolls the letter grid without moving the name. An IRQ at
 * SNES line 128 switches BG3 from the name scroll to selectfile_var8. That
 * scanline split is done here with an HBlank interrupt; the rest of the
 * picture keeps one scroll per background.
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
static uint8 bg_tilebase[4];
static uint32 tile_hash[3][1024];
static uint32 map_sig[3];
static int map_valid[3];
static uint16 last_adr[3];
static uint8 used_bg1[1024];
static int blank_bg1 = kBlankNone;

/* 2bpp tiles are compacted. Slot 0 is a transparent tile.
 * bg3_lo is the priority-0 map (name letters), behind the wallpaper. */
static uint16 bg3_slot[8 * 1024];
static int bg3_next;
static u16 bg3_lo[4096] __attribute__((aligned(4)));
static int back_mode;
static uint32 spr_hash[128];

static u16 map_buf[4][4096] __attribute__((aligned(4)));
/* BG2 tiles with the priority bit. Doorways live here, in front of Link. */
static u16 bg2_hi[4096] __attribute__((aligned(4)));
static int bg2_blank;
static int bg2_lift_mode = -1;
static int bg2_front_on_slot1;

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

	if (bg_size[slot] == sid && bg_tilebase[slot] == (uint8)tile_base)
		return;
	bg_id[slot] = bgInit(slot, BgType_Text4bpp, map_bg_size(wider, higher), map_base, tile_base);
	bg_size[slot] = sid;
	bg_tilebase[slot] = (uint8)tile_base;
}

static int bg1_split_mode = -1;

static void present_bg1(Ppu *ppu, int split) {
	BgLayer *layer = &ppu->bgLayer[0];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);
	uint32 sig = sig_map(ppu, layer->tilemapAdr, words);
	u16 *gfx;
	int tile;

	/* Slot 1 stays on BG2 graphics while it is showing doorways. BG1 tiles
	 * always live with slot 2. */
	if (split)
		ensure_size(1, layer->tilemapWider, layer->tilemapHigher, 4, 2);
	ensure_size(2, layer->tilemapWider, layer->tilemapHigher, 8, 2);
	gfx = bgGetGfxPtr(bg_id[2]);

	if (last_adr[0] != layer->tileAdr) {
		memset(tile_hash[0], 0, sizeof tile_hash[0]);
		last_adr[0] = layer->tileAdr;
		map_valid[0] = 0;
	}
	if (bg1_split_mode != split) {
		bg1_split_mode = split;
		map_valid[0] = 0;
	}

	memset(used_bg1, 0, sizeof used_bg1);
	{
		int i;

		for (i = 0; i < words; i++)
			used_bg1[ppu->vram[(layer->tilemapAdr + i) & 0x7fff] & 0x3ff] = 1;
	}
	if (!map_valid[0] || map_sig[0] != sig) {
		int i;

		blank_bg1 = kBlankNone;
		for (i = 0; i < 1024; i++) {
			if (!used_bg1[i]) {
				blank_bg1 = i;
				break;
			}
		}
		if (!split)
			blank_bg1 = kBlankNone;
		for (i = 0; i < words; i++) {
			uint16 entry = ppu->vram[(layer->tilemapAdr + i) & 0x7fff];
			u16 blank = (u16)(blank_bg1 < 0 ? 0 : blank_bg1);

			if (!split) {
				map_buf[1][i] = 0;
				map_buf[2][i] = swizzle4(entry);
			} else if (entry & 0x2000) {
				map_buf[1][i] = swizzle4(entry);
				map_buf[2][i] = blank;
			} else {
				map_buf[1][i] = blank;
				map_buf[2][i] = swizzle4(entry);
			}
		}
		DC_FlushRange(map_buf[2], (uint32)words * 2);
		dmaCopyWords(3, map_buf[2], bgGetMapPtr(bg_id[2]), (uint32)words * 2);
		if (split) {
			DC_FlushRange(map_buf[1], (uint32)words * 2);
			dmaCopyWords(3, map_buf[1], bgGetMapPtr(bg_id[1]), (uint32)words * 2);
		}
		map_sig[0] = sig;
		map_valid[0] = 1;
	}

	for (tile = 0; tile < 1024; tile++) {
		if (!used_bg1[tile] || tile == blank_bg1)
			continue;
		upload4_tile(ppu, 0, gfx, layer->tileAdr, tile);
	}
	/* The unused index fills the other priority layer. It has to stay
	 * transparent even when a previous upload left pixels there and the
	 * hash was cleared, or those pixels paint over the logo and sprites. */
	if (blank_bg1 >= 0 && !used_bg1[blank_bg1]) {
		clear_tile(gfx, blank_bg1);
		tile_hash[0][blank_bg1] = 0;
	}
}

static void present_bg2(Ppu *ppu, int lift) {
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
	if (bg2_lift_mode != lift) {
		bg2_lift_mode = lift;
		map_valid[1] = 0;
	}

	memset(used, 0, sizeof used);
	if (!map_valid[1] || map_sig[1] != sig) {
		int i;

		for (i = 0; i < words; i++)
			used[ppu->vram[(layer->tilemapAdr + i) & 0x7fff] & 0x3ff] = 1;
		bg2_blank = 0;
		for (i = 1; i < 1024; i++) {
			if (!used[i]) {
				bg2_blank = i;
				break;
			}
		}
		for (i = 0; i < words; i++) {
			uint16 entry = ppu->vram[(layer->tilemapAdr + i) & 0x7fff];

			/* Same split as PpuDrawBackground: 0xb100 is in front of Link,
			 * 0x7100 stays on the ground behind him. */
			if (lift && (entry & 0x2000)) {
				bg2_hi[i] = swizzle4(entry);
				map_buf[3][i] = (u16)bg2_blank;
			} else {
				bg2_hi[i] = (u16)bg2_blank;
				map_buf[3][i] = swizzle4(entry);
			}
		}
		DC_FlushRange(map_buf[3], (uint32)words * 2);
		dmaCopyWords(3, map_buf[3], bgGetMapPtr(bg_id[3]), (uint32)words * 2);
		map_sig[1] = sig;
		map_valid[1] = 1;
		if (!used[bg2_blank]) {
			clear_tile(gfx, bg2_blank);
			tile_hash[1][bg2_blank] = 0;
		}
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

static void present_bg3(Ppu *ppu, int show_low) {
	BgLayer *layer = &ppu->bgLayer[2];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);
	uint32 sig = sig_map(ppu, layer->tilemapAdr, words);
	u16 *gfx;
	int i;

	ensure_size(0, layer->tilemapWider, layer->tilemapHigher, 0, 6);
	if (show_low)
		ensure_size(3, layer->tilemapWider, layer->tilemapHigher, 12, 6);
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

			slot = bg3_lookup((tile << 3) | pal);
			if (slot < 0) {
				map_buf[0][i] = 0;
				bg3_lo[i] = 0;
				continue;
			}
			out = (u16)slot;
			if (entry & 0x4000)
				out |= 1 << 10;
			if (entry & 0x8000)
				out |= 1 << 11;
			out |= (u16)((pal >> 2) << 12);
			/* Priority 1 is the HUD, in front of sprites. Priority 0 is
			 * the name text, which the wallpaper has to cover. */
			if (entry & 0x2000) {
				map_buf[0][i] = out;
				bg3_lo[i] = 0;
			} else {
				map_buf[0][i] = 0;
				bg3_lo[i] = out;
			}
		}
		DC_FlushRange(map_buf[0], (uint32)words * 2);
		dmaCopyWords(3, map_buf[0], bgGetMapPtr(bg_id[0]), (uint32)words * 2);
		if (show_low) {
			DC_FlushRange(bg3_lo, (uint32)words * 2);
			dmaCopyWords(3, bg3_lo, bgGetMapPtr(bg_id[3]), (uint32)words * 2);
		}
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

static int clamp5(int v) {
	if (v < 0)
		return 0;
	if (v > 31)
		return 31;
	return v;
}

/* Fixed-color math. A transparent subscreen keeps the fixed color and
 * skips the half; half only applies when the subscreen itself is off. */
static u16 math_color(u16 color, const Ppu *ppu) {
	int r = color & 31;
	int g = (color >> 5) & 31;
	int b = (color >> 10) & 31;

	if (ppu->subtractColor) {
		r -= ppu->fixedColorR;
		g -= ppu->fixedColorG;
		b -= ppu->fixedColorB;
	} else {
		r += ppu->fixedColorR;
		g += ppu->fixedColorG;
		b += ppu->fixedColorB;
	}
	if (ppu->halfColor && !ppu->addSubscreen) {
		r >>= 1;
		g >>= 1;
		b >>= 1;
	}
	return (u16)(clamp5(r) | (clamp5(g) << 5) | (clamp5(b) << 10));
}

static int math_active(const Ppu *ppu) {
	if (ppu->preventMathMode == 3 || (ppu->mathEnabled & 0x3f) == 0)
		return 0;
	if (ppu->fixedColorR | ppu->fixedColorG | ppu->fixedColorB)
		return 1;
	return ppu->halfColor && !ppu->addSubscreen;
}

static void present_palettes(const Ppu *ppu) {
	int i;
	int brightness = ppu->brightness;
	int on = math_active(ppu);
	int bg = on && (ppu->mathEnabled & 0x07);
	int obj = on && (ppu->mathEnabled & 0x10);
	int back = on && (ppu->mathEnabled & 0x20);
	u16 color;

	color = back ? math_color(ppu->cgram[0], ppu) : ppu->cgram[0];
	BG_PALETTE[0] = shade(color, brightness);
	for (i = 1; i < 128; i++) {
		color = bg ? math_color(ppu->cgram[i], ppu) : ppu->cgram[i];
		BG_PALETTE[i] = shade(color, brightness);
	}
	for (i = 0; i < 128; i++) {
		color = obj ? math_color(ppu->cgram[128 + i], ppu) : ppu->cgram[128 + i];
		SPRITE_PALETTE[i] = shade(color, brightness);
	}
}

/* SNES mosaic size 1 is one pixel. The DS register is that size minus one. */
static void present_mosaic(const Ppu *ppu) {
	int i;
	int size;

	if (ppu->forcedBlank || ppu->mosaicSize <= 1 || (ppu->mosaicEnabled & 7) == 0) {
		for (i = 0; i < 4; i++)
			bgMosaicDisable(bg_id[i]);
		bgSetMosaic(0, 0);
		return;
	}
	size = ppu->mosaicSize - 1;
	if (size > 15)
		size = 15;
	bgSetMosaic((unsigned)size, (unsigned)size);
	for (i = 0; i < 4; i++)
		bgMosaicEnable(bg_id[i]);
}

/* Half-add is (front + back) / 2. A full add clamps the sum. */
static void present_overlay_blend(const Ppu *ppu, int overlay) {
	int coef;

	if (!overlay || ppu->subtractColor) {
		REG_BLDCNT = 0;
		return;
	}
	coef = ppu->halfColor ? 8 : 16;
	REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG2 |
		BLEND_DST_BG0 | BLEND_DST_BG1 | BLEND_DST_BG3 |
		BLEND_DST_SPRITE | BLEND_DST_BACKDROP;
	REG_BLDALPHA = (u16)(coef | (coef << 8));
}

static int obj_tile(int name, int tx, int ty) {
	return (((name >> 4) + ty) << 4) | (((name & 0xf) + tx) & 0xf);
}

static int ds_sprite_priority(int snes_priority) {
	/* Same interleave as PpuDrawBackgrounds: 3 above BG1-high, 2 above
	 * BG1-low, 0 and 1 above only the back layer. */
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

/* First DS line of the letter grid. SNES row 127 is the first row drawn
 * with the post-IRQ scroll, and the top 16 SNES rows are cropped. */
enum { kBg3SplitLine = 128 - 1 - kCropY };

/* Inside the window: every layer, sprites, and the blender. Outside: nothing. */
enum { kWinInside = 0x3f };

static int bg3_split_on;
static int iris_on;
static int scanline_irq_on;
static u16 bg3_top_x, bg3_top_y, bg3_bot_x, bg3_bot_y;

/* hdma_table_dynamic[line] is WH0 then WH1: left, then inclusive right.
 * The DS right edge is stored plus one, and 0 means 256. */
static void iris_apply_line(unsigned ds_line) {
	unsigned left, right;
	uint16 span;

	if (ds_line >= SCREEN_HEIGHT)
		return;
	span = hdma_table_dynamic[ds_line + kCropY];
	left = span & 0xff;
	right = span >> 8;
	if (left > right) {
		WIN0_X0 = 255;
		WIN0_X1 = 255;
		return;
	}
	WIN0_X0 = (u8)left;
	WIN0_X1 = (u8)(right + 1);
}

/* Fires at the end of REG_VCOUNT, so the change applies to the next line. */
static void scanline_hblank(void) {
	unsigned line = REG_VCOUNT;

	if (bg3_split_on) {
		if (line >= (unsigned)(kBg3SplitLine - 1) && line < SCREEN_HEIGHT - 1) {
			REG_BG3HOFS = bg3_bot_x;
			REG_BG3VOFS = bg3_bot_y;
		} else {
			REG_BG3HOFS = bg3_top_x;
			REG_BG3VOFS = bg3_top_y;
		}
	}
	if (iris_on)
		iris_apply_line(line + 1);
}

static void scanline_irq_sync(void) {
	int on = bg3_split_on || iris_on;

	if (scanline_irq_on == on)
		return;
	scanline_irq_on = on;
	if (on) {
		irqSet(IRQ_HBLANK, scanline_hblank);
		irqEnable(IRQ_HBLANK);
		REG_DISPSTAT |= DISP_HBLANK_IRQ;
	} else {
		irqDisable(IRQ_HBLANK);
		REG_DISPSTAT &= ~DISP_HBLANK_IRQ;
	}
}

static void bg3_split_set(int on) {
	bg3_split_on = on;
	scanline_irq_sync();
}

/* Spotlight sets this window and fills hdma_table_dynamic for the circle. */
static void present_iris(void) {
	int on = (HDMAEN_copy & 0x80) && W12SEL_copy == 0x33 && WOBJSEL_copy == 0x33;

	iris_on = on;
	scanline_irq_sync();
	if (!on) {
		REG_DISPCNT &= ~DISPLAY_WIN0_ON;
		return;
	}
	WIN0_Y0 = 0;
	WIN0_Y1 = SCREEN_HEIGHT;
	WIN_IN = kWinInside;
	WIN_OUT = 0;
	BG_PALETTE[0] = 0;
	iris_apply_line(0);
	REG_DISPCNT |= DISPLAY_WIN0_ON;
}

/* Overworld: BG1 is not on the main screen, so this layer can show the
 * priority-1 ground tiles with BG2's own graphics. */
static void show_bg2_front(const Ppu *ppu) {
	const BgLayer *layer = &ppu->bgLayer[1];
	int words = map_words(layer->tilemapWider, layer->tilemapHigher);

	ensure_size(1, layer->tilemapWider, layer->tilemapHigher, 4, 4);
	DC_FlushRange(bg2_hi, (uint32)words * 2);
	dmaCopyWords(3, bg2_hi, bgGetMapPtr(bg_id[1]), (uint32)words * 2);
	bg2_front_on_slot1 = 1;
}

static void upload4_reloc(const Ppu *ppu, u16 *gfx, uint16 tile_adr, int src, int dst) {
	uint16 addr = (uint16)((tile_adr + src * 16) & 0x7fff);
	uint32 h = hash_words(ppu->vram, addr, 16) ^ 0x9e3779b9u;
	uint16 tmp[16];
	int i;

	if (!h)
		h = 1;
	if (tile_hash[0][dst] == h)
		return;
	for (i = 0; i < 16; i++)
		tmp[i] = ppu->vram[(addr + i) & 0x7fff];
	conv4(tmp, gfx + dst * 16, 0);
	tile_hash[0][dst] = h;
}

static u16 swizzle_as(uint16 entry, int tile) {
	return swizzle4((uint16)((entry & ~0x3ff) | (tile & 0x3ff)));
}

/* Dungeon BG1 already uses both spare layers. Copy doorway tiles into free
 * slots of that tileset and put them on the priority-1 map, which is the
 * layer zelda3 draws above Link and below sprite priority 3. */
static void bake_bg2_doors(Ppu *ppu) {
	BgLayer *top = &ppu->bgLayer[0];
	BgLayer *ground = &ppu->bgLayer[1];
	int words = map_words(top->tilemapWider, top->tilemapHigher);
	static int remap[1024];
	static int free_tile[1024];
	int free_n = 0;
	int free_i = 0;
	int patched = 0;
	u16 *gfx;
	u16 blank;
	int i;

	if (top->hScroll != ground->hScroll || top->vScroll != ground->vScroll ||
	    top->tilemapWider != ground->tilemapWider ||
	    top->tilemapHigher != ground->tilemapHigher)
		return;
	gfx = bgGetGfxPtr(bg_id[1]);
	blank = (u16)(blank_bg1 < 0 ? 0 : blank_bg1);
	for (i = 0; i < 1024; i++)
		remap[i] = -1;
	for (i = 1; i < 1024; i++) {
		if (!used_bg1[i] && i != blank_bg1)
			free_tile[free_n++] = i;
	}
	for (i = 0; i < words; i++) {
		uint16 above = ppu->vram[(top->tilemapAdr + i) & 0x7fff];
		uint16 door;
		int src, dst;

		if (above & 0x2000) {
			map_buf[1][i] = swizzle4(above);
			continue;
		}
		door = ppu->vram[(ground->tilemapAdr + i) & 0x7fff];
		if (!(door & 0x2000)) {
			map_buf[1][i] = blank;
			continue;
		}
		src = door & 0x3ff;
		dst = remap[src];
		if (dst < 0) {
			if (free_i >= free_n) {
				map_buf[1][i] = blank;
				map_buf[3][i] = swizzle4(door);
				patched = 1;
				continue;
			}
			dst = free_tile[free_i++];
			remap[src] = dst;
			upload4_reloc(ppu, gfx, ground->tileAdr, src, dst);
		}
		map_buf[1][i] = swizzle_as(door, dst);
	}
	DC_FlushRange(map_buf[1], (uint32)words * 2);
	dmaCopyWords(3, map_buf[1], bgGetMapPtr(bg_id[1]), (uint32)words * 2);
	if (patched) {
		DC_FlushRange(map_buf[3], (uint32)words * 2);
		dmaCopyWords(3, map_buf[3], bgGetMapPtr(bg_id[3]), (uint32)words * 2);
	}
}

static void present_scroll(const Ppu *ppu, int back_bg3) {
	int sy0 = (int)ppu->bgLayer[0].vScroll + kCropY;
	int sy1 = (int)ppu->bgLayer[1].vScroll + kCropY;
	int sy2 = (int)ppu->bgLayer[2].vScroll + kCropY;
	int split = back_bg3 && irq_flag != 0;
	int ime = enterCriticalSection();

	if (bg2_front_on_slot1)
		bgSetScroll(bg_id[1], ppu->bgLayer[1].hScroll, sy1);
	else
		bgSetScroll(bg_id[1], ppu->bgLayer[0].hScroll, sy0);
	bgSetScroll(bg_id[2], ppu->bgLayer[0].hScroll, sy0);
	bgSetScroll(bg_id[0], ppu->bgLayer[2].hScroll, sy2);
	if (back_bg3)
		bgSetScroll(bg_id[3], ppu->bgLayer[2].hScroll, sy2);
	else
		bgSetScroll(bg_id[3], ppu->bgLayer[1].hScroll, sy1);

	if (split) {
		/* Above the IRQ the name stays on the normal BG3 scroll. Below it
		 * the grid follows selectfile_var8 and BG3 V is reset to 0. */
		bg3_top_x = (u16)ppu->bgLayer[2].hScroll;
		bg3_top_y = (u16)sy2;
		bg3_bot_x = (u16)(selectfile_var8 & 0x1ff);
		bg3_bot_y = (u16)kCropY;
	}
	bg3_split_set(split);
	if (irq_flag & 0x80)
		irq_flag = 0;
	leaveCriticalSection(ime);
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
	bg_tilebase[0] = 6;
	bg_tilebase[1] = 2;
	bg_tilebase[2] = 2;
	bg_tilebase[3] = 4;
	back_mode = -1;
	bgSetPriority(bg_id[0], 0);
	bgSetPriority(bg_id[1], 1);
	bgSetPriority(bg_id[2], 2);
	bgSetPriority(bg_id[3], 3);
	bg3_split_set(0);

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
	int main_en = ppu->forcedBlank ? 0 : ppu->screenEnabled[0];
	int sub_en = ppu->forcedBlank ? 0 : ppu->screenEnabled[1];
	int bg1_main = (main_en & 0x01) != 0;
	int bg1_sub = (sub_en & 0x01) != 0;
	/* Rain and the other overworld covers: BG1 is subscreen-only, BG2 is
	 * the opaque ground, and color math is on. The title picture has no
	 * ground layer, so it stays behind the sprites. */
	int overlay = bg1_sub && !bg1_main && (main_en & 0x02) &&
		ppu->addSubscreen && ppu->preventMathMode != 3 &&
		(ppu->mathEnabled & 0x3f) != 0;
	int bg2_main = (main_en & 0x02) != 0;
	int bg2_on = ((main_en | sub_en) & 0x02) != 0;
	/* Door frames are BG2 priority tiles. Lift them whenever the ground is
	 * on the main screen. In a dungeon that also needs its own BG1, the
	 * scrolls have to match so the tiles can share BG1's front layer. */
	int lift_bg2 = bg2_main && (!bg1_main ||
		(ppu->bgLayer[0].hScroll == ppu->bgLayer[1].hScroll &&
		 ppu->bgLayer[0].vScroll == ppu->bgLayer[1].vScroll &&
		 ppu->bgLayer[0].tilemapWider == ppu->bgLayer[1].tilemapWider &&
		 ppu->bgLayer[0].tilemapHigher == ppu->bgLayer[1].tilemapHigher));
	int bg3_on = ((main_en | sub_en) & 0x04) != 0;
	int show_bg3_lo = bg3_on && !bg2_on;

	/* BG3 priority 0 and BG2 share the back background. Switching which
	 * one owns that map has to upload it again. */
	if (bg2_on) {
		if (back_mode != 0) {
			back_mode = 0;
			map_valid[1] = 0;
		}
	} else if (show_bg3_lo) {
		if (back_mode != 1) {
			back_mode = 1;
			map_valid[2] = 0;
		}
	}

	/* Main-screen BG1 keeps the priority split. A subscreen-only BG1 is the
	 * title art, which is added behind the sprites instead of covering them. */
	bg2_front_on_slot1 = 0;
	present_bg1(ppu, bg1_main);
	if (bg2_on)
		present_bg2(ppu, lift_bg2);
	if (lift_bg2 && bg1_main)
		bake_bg2_doors(ppu);
	else if (lift_bg2)
		show_bg2_front(ppu);
	present_bg3(ppu, show_bg3_lo);
	present_palettes(ppu);
	present_sprites(ppu, (main_en & 0x10) != 0);
	present_scroll(ppu, show_bg3_lo);

	if (main_en & 0x04) {
		bgSetPriority(bg_id[0], 0);
		bgShow(bg_id[0]);
	} else if (sub_en & 0x04) {
		bgSetPriority(bg_id[0], 3);
		bgShow(bg_id[0]);
	} else {
		bgHide(bg_id[0]);
	}

	if (bg1_main) {
		bgSetPriority(bg_id[1], 1);
		bgSetPriority(bg_id[2], 2);
		bgShow(bg_id[1]);
		bgShow(bg_id[2]);
	} else if (overlay) {
		/* In front of every sprite, including a priority-3 ledge jump,
		 * and behind the HUD. Doorways sit between the rain and Link. */
		bgSetPriority(bg_id[2], 0);
		bgShow(bg_id[2]);
		if (bg2_front_on_slot1) {
			bgSetPriority(bg_id[1], 1);
			bgShow(bg_id[1]);
		} else {
			bgHide(bg_id[1]);
		}
	} else if (bg1_sub) {
		bgSetPriority(bg_id[2], 3);
		bgHide(bg_id[1]);
		bgShow(bg_id[2]);
	} else if (bg2_front_on_slot1) {
		bgSetPriority(bg_id[1], 1);
		bgShow(bg_id[1]);
		bgHide(bg_id[2]);
	} else {
		bgHide(bg_id[1]);
		bgHide(bg_id[2]);
	}

	if (bg2_on) {
		/* Main-screen BG2 is in the same band as BG1 priority 0: layer 2
		 * (BG1) is in front of layer 3, Link is in front of both, and
		 * sprites 0/1 are behind both. Subscreen-only BG2 is the title
		 * picture, added behind every sprite. */
		bgSetPriority(bg_id[3], (main_en & 0x02) ? 2 : 3);
		bgShow(bg_id[3]);
	} else if (show_bg3_lo) {
		bgSetPriority(bg_id[3], 3);
		bgShow(bg_id[3]);
	} else {
		bgHide(bg_id[3]);
	}
	REG_DISPCNT |= DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D | DISPLAY_SPR_1D_SIZE_32;
	present_mosaic(ppu);
	present_overlay_blend(ppu, overlay);
	present_iris();
}
