#include <nds.h>

#include <string.h>

#include "zelda_rtl.h"
#include "variables.h"
#include "snes/ppu.h"
#include "zelda_bottom.h"
#include "zelda_save.h"
#include "zelda_video.h"
#include "tex_menu.h"
#include "gear-text.h"
#include "items-text.h"
#include "selected-item-border.h"

enum {
	kWidth = 256,
	kHeight = 192,
	kTexW = 48,
	kTexH = 48,
	/* Slots well above the two colors in the background tile. */
	/* Sixteen slots for the game's own HUD colors: rupee, bomb, arrow,
	 * digits, four apiece. Slot 0 of each group is never drawn. */
	kPalHud = 224,
	/* Thirty-two slots for the equipped item: eight BG3 palettes, four each. */
	kPalItem = 192,
	/* The selected-item border's own colors; index 0 is clear. */
	kPalBorder = 188,
	kPalGreen = 252,
	kPalText = 253,
	kPalBlack = 254,
	kPalGold = 255
};

/* A filled rectangle with a solid border inside its w x h, drawn over the
 * background. Boxes are drawn in order. */
typedef struct {
	int x, y, w, h;
	int border_w;
	u8 border;
	u8 fill;
} HudBox;

/* The first two are the buttons; kImages refers to them by index. */
static const HudBox kBoxes[] = {
	{ 5, 163, 122, 24, 1, kPalGold, kPalBlack },
	{ 130, 163, 122, 24, 1, kPalGold, kPalBlack },
	{ 5, 4, 150, 153, 2, kPalGreen, kPalBlack },
	{ 167, 4, 78, 41, 2, kPalGreen, kPalBlack }
};

/* A graphic, centered in one of the boxes, or at (x, y) when box is -1.
 * Palette index 0 is transparent. Every other pixel is drawn in color, or
 * when color is 0, in the slot pal_base plus its own index. Rows are padded
 * to 4 bytes. */
typedef struct {
	const u8 *pix;
	int w, h;
	int box;
	int x, y;
	u8 color;
	u8 pal_base;
} HudImage;

#define STRIDE(w) (((w) + 3) & ~3)

static const HudImage kImages[] = {
	{ (const u8 *)gear_textBitmap, 26, 7, 0, 0, 0, kPalText, 0 },
	{ (const u8 *)items_textBitmap, 34, 7, 1, 0, 0, kPalText, 0 },
	{ (const u8 *)selected_item_borderBitmap, 50, 50, -1, 183, 52, 0, kPalBorder }
};

/* The rupee, bomb and arrow counters. The icons and digits are the game's
 * own 2bpp HUD tiles, read out of BG3 in the PPU's VRAM with the HUD
 * palettes from CGRAM, so they follow whatever the game has loaded. */
enum {
	kIconX = 189,
	kArrowX = 186,
	kRupeeY = 10,
	kBombY = 21,
	kArrowY = 32,
	/* Digits start 3 pixels past the 8-wide rupee and bomb icons. */
	kValueX = kIconX + 8 + 3,
	kTileH = 8,
	kMaxSpriteW = 32,
	/* Rows the counters can touch, so a change redraws only these. */
	kDynTop = kRupeeY,
	kDynBottom = kArrowY + kTileH,

	/* HUD tile numbers in the BG3 character set. */
	kTileRupee = 0xa8,
	kTileBomb = 0x88,
	kTileArrow = 0xa7,
	kTileDigit0 = 0x90,

	kGroupRupee = 0,
	kGroupBomb = 1,
	kGroupArrow = 2,
	kGroupDigit = 3
};

/* BG3 palette numbers the game draws each group with. */
static const u8 kGroupPal[4] = { 7, 3, 0, 1 };

typedef struct {
	int x, y, w, h;
	u8 px[kTileH][kMaxSpriteW];	/* palette slot, 0 is clear */
} HudSprite;

enum { kRupeeIcon, kBombIcon, kArrowIcon, kRupeeNum, kBombNum, kArrowNum, kSprites };

typedef struct {
	HudSprite spr[kSprites];
	u16 pal[16];
} HudCounters;

static HudCounters shown;
static HudCounters next;

static void read_tile(const Ppu *ppu, int tile, u8 out[kTileH][kTileH])
{
	int base = ppu->bgLayer[2].tileAdr + tile * 8;
	int r, x;

	for (r = 0; r < 8; r++) {
		u16 w = ppu->vram[(base + r) & 0x7fff];

		for (x = 0; x < 8; x++)
			out[r][x] = ((w >> (7 - x)) & 1) | (((w >> (15 - x)) & 1) << 1);
	}
}

/* An icon of one or two tiles side by side (right < 0 for one), cropped to
 * the pixels it draws. (x, y) is where the top-left drawn pixel lands. */
static void build_icon(HudSprite *s, const Ppu *ppu, int left, int right,
		       int group, int x, int y)
{
	u8 strip[kTileH][2 * kTileH];
	int n = right < 0 ? 1 : 2;
	int minx = 99, maxx = -1, miny = 99, maxy = -1;
	int t, r, c;

	for (t = 0; t < n; t++) {
		u8 one[kTileH][kTileH];

		read_tile(ppu, t ? right : left, one);
		for (r = 0; r < kTileH; r++)
			memcpy(&strip[r][t * kTileH], one[r], kTileH);
	}
	for (r = 0; r < kTileH; r++) {
		for (c = 0; c < n * kTileH; c++) {
			if (!strip[r][c])
				continue;
			if (c < minx) minx = c;
			if (c > maxx) maxx = c;
			if (r < miny) miny = r;
			if (r > maxy) maxy = r;
		}
	}
	s->x = x;
	s->y = y;
	if (maxx < 0)
		return;
	s->w = maxx - minx + 1;
	s->h = maxy - miny + 1;
	for (r = 0; r < s->h; r++) {
		for (c = 0; c < s->w; c++) {
			u8 p = strip[miny + r][minx + c];

			s->px[r][c] = p ? kPalHud + group * 4 + p : 0;
		}
	}
}

/* A count as digit cells, left aligned at kValueX. Leading zeros stay,
 * as on the game's own bar. */
static void build_number(HudSprite *s, const Ppu *ppu, int value, int digits,
			 int y)
{
	int i, r, c;

	s->x = kValueX;
	s->y = y;
	s->w = digits * kTileH;
	s->h = kTileH;
	for (i = digits - 1; i >= 0; i--) {
		u8 cell[kTileH][kTileH];

		read_tile(ppu, kTileDigit0 + value % 10, cell);
		value /= 10;
		for (r = 0; r < kTileH; r++) {
			for (c = 0; c < kTileH; c++) {
				u8 p = cell[r][c];

				s->px[r][i * kTileH + c] =
					p ? kPalHud + kGroupDigit * 4 + p : 0;
			}
		}
	}
}

static void build_counters(HudCounters *d)
{
	const Ppu *ppu = g_zenv.ppu;
	int rupees = link_rupees_actual;
	int g, p;

	memset(d, 0, sizeof *d);
	for (g = 0; g < 4; g++)
		for (p = 1; p < 4; p++)
			d->pal[g * 4 + p] = ppu->cgram[kGroupPal[g] * 4 + p] & 0x7fff;

	build_icon(&d->spr[kRupeeIcon], ppu, kTileRupee, -1, kGroupRupee, kIconX, kRupeeY);
	build_icon(&d->spr[kBombIcon], ppu, kTileBomb, kTileBomb + 1, kGroupBomb, kIconX, kBombY);
	build_icon(&d->spr[kArrowIcon], ppu, kTileArrow, kTileArrow + 2, kGroupArrow, kArrowX, kArrowY);
	/* Three digits, four once the wallet holds a thousand. */
	build_number(&d->spr[kRupeeNum], ppu, rupees, rupees > 999 ? 4 : 3, kRupeeY);
	build_number(&d->spr[kBombNum], ppu, link_item_bombs % 100, 2, kBombY);
	build_number(&d->spr[kArrowNum], ppu, link_num_arrows % 100, 2, kArrowY);
}

static void set_counter_palette(const HudCounters *d)
{
	int i;

	for (i = 1; i < 16; i++)
		BG_PALETTE_SUB[kPalHud + i] = d->pal[i];
}

/* The equipped item. The game keeps the item box as a 2x2 block of BG3 tile
 * entries in its HUD buffer, so whatever it has equipped, bottles and the
 * flute included, is already there. Each pixel is drawn twice across and
 * twice down, with nothing smoothed. */
enum {
	kItemX = 193,
	kItemY = 62,
	kItemSize = 32,
	/* HUDXY(5, 1) in the 32-word-wide HUD buffer. */
	kItemBoxWord = 1 * 32 + 5
};

typedef struct {
	u8 px[kItemSize][kItemSize];	/* palette slot, 0 is clear */
	u16 pal[32];
} HudItem;

static HudItem shown_item;
static HudItem next_item;

static void build_item(HudItem *it)
{
	const Ppu *ppu = g_zenv.ppu;
	const u16 *box = &hud_tile_indices_buffer[kItemBoxWord];
	int t, r, c, p;

	memset(it, 0, sizeof *it);
	for (t = 0; t < 4; t++) {
		u16 entry = box[(t >> 1) * 32 + (t & 1)];
		int pal = (entry >> 10) & 7;
		u8 tile[kTileH][kTileH];

		read_tile(ppu, entry & 0x3ff, tile);
		for (p = 1; p < 4; p++)
			it->pal[pal * 4 + p] = ppu->cgram[pal * 4 + p] & 0x7fff;
		for (r = 0; r < kTileH; r++) {
			for (c = 0; c < kTileH; c++) {
				int sr = (entry & 0x8000) ? kTileH - 1 - r : r;
				int sc = (entry & 0x4000) ? kTileH - 1 - c : c;
				u8 v = tile[sr][sc];
				int x = ((t & 1) * kTileH + c) * 2;
				int y = ((t >> 1) * kTileH + r) * 2;

				if (!v)
					continue;
				v += kPalItem + pal * 4;
				it->px[y][x] = v;
				it->px[y][x + 1] = v;
				it->px[y + 1][x] = v;
				it->px[y + 1][x + 1] = v;
			}
		}
	}
}

static void set_item_palette(const HudItem *it)
{
	int i;

	for (i = 0; i < 32; i++)
		if (i & 3)
			BG_PALETTE_SUB[kPalItem + i] = it->pal[i];
}

static int hud_up;

void ZeldaBottomNoteIdle(void)
{
	hud_up = 0;
}

/* Modules 6 through 19 are the underworld and overworld loads, play, the
 * item menu, the iris, the transitions, and death. The title, the file
 * screens, attract, and the ending are outside that. */
static int in_game(void)
{
	return main_module_index >= 6 && main_module_index <= 19;
}

static int hud_wanted(void)
{
	return !ZeldaHudOnTop() && in_game();
}

/* One screen row: the tiled background, then whatever sits on that row. */
static void compose_row(u8 *line, int y)
{
	const u8 *tex = (const u8 *)tex_menuBitmap + (y % kTexH) * kTexW;
	unsigned i;
	int x;

	for (x = 0; x < kWidth; x++)
		line[x] = tex[x % kTexW];

	for (i = 0; i < sizeof kBoxes / sizeof kBoxes[0]; i++) {
		const HudBox *b = &kBoxes[i];
		int edge_row;

		if (y < b->y || y >= b->y + b->h)
			continue;
		edge_row = y < b->y + b->border_w ||
			y >= b->y + b->h - b->border_w;
		for (x = b->x; x < b->x + b->w; x++) {
			int edge_col = x < b->x + b->border_w ||
				x >= b->x + b->w - b->border_w;

			line[x] = (edge_row || edge_col) ? b->border : b->fill;
		}
	}

	for (i = 0; i < sizeof kImages / sizeof kImages[0]; i++) {
		const HudImage *im = &kImages[i];
		int top = im->y;
		int left = im->x;
		const u8 *src;

		if (im->box >= 0) {
			const HudBox *b = &kBoxes[im->box];

			top = b->y + (b->h - im->h) / 2;
			left = b->x + (b->w - im->w) / 2;
		}
		if (y < top || y >= top + im->h)
			continue;
		src = im->pix + (y - top) * STRIDE(im->w);
		for (x = 0; x < im->w; x++) {
			if (src[x])
				line[left + x] = im->color ? im->color : im->pal_base + src[x];
		}
	}

	if (y >= kItemY && y < kItemY + kItemSize) {
		const u8 *row = shown_item.px[y - kItemY];

		for (x = 0; x < kItemSize; x++) {
			if (row[x])
				line[kItemX + x] = row[x];
		}
	}

	for (i = 0; i < kSprites; i++) {
		const HudSprite *s = &shown.spr[i];

		if (y < s->y || y >= s->y + s->h)
			continue;
		for (x = 0; x < s->w; x++) {
			u8 p = s->px[y - s->y][x];

			if (p)
				line[s->x + x] = p;
		}
	}
}

/* VRAM takes 16-bit writes, so rows are built in RAM and DMA'd. */
static void put_rows(int from, int to)
{
	static u8 line[kWidth] __attribute__((aligned(4)));
	u8 *dst = (u8 *)ZeldaVideoBottomFramebuffer();
	int y;

	for (y = from; y < to; y++) {
		compose_row(line, y);
		dmaCopy(line, dst + y * kWidth, kWidth);
	}
}

static void draw_hud(void)
{
	int i;

	build_counters(&shown);
	build_item(&shown_item);
	dmaCopy(tex_menuPal, BG_PALETTE_SUB, tex_menuPalLen);
	for (i = 1; i < 4; i++)
		BG_PALETTE_SUB[kPalBorder + i] = selected_item_borderPal[i];
	/* #50a865 at 5 bits per channel. */
	BG_PALETTE_SUB[kPalGreen] = RGB15(0x50 >> 3, 0xa8 >> 3, 0x65 >> 3);
	BG_PALETTE_SUB[kPalText] = RGB15(31, 31, 31);
	BG_PALETTE_SUB[kPalBlack] = RGB15(0, 0, 0);
	/* #e3b23d at 5 bits per channel. */
	BG_PALETTE_SUB[kPalGold] = RGB15(0xe3 >> 3, 0xb2 >> 3, 0x3d >> 3);
	set_counter_palette(&shown);
	set_item_palette(&shown_item);
	put_rows(0, kHeight);
}

/* Counts tick up a point per frame while they fill, the player swaps items,
 * and the game can reload its tiles, so look every frame and touch the
 * screen only on a change, and only the rows that changed. */
static void refresh_dynamic(void)
{
	build_counters(&next);
	if (memcmp(&next, &shown, sizeof shown)) {
		memcpy(&shown, &next, sizeof shown);
		set_counter_palette(&shown);
		put_rows(kDynTop, kDynBottom);
	}
	build_item(&next_item);
	if (memcmp(&next_item, &shown_item, sizeof shown_item)) {
		memcpy(&shown_item, &next_item, sizeof shown_item);
		set_item_palette(&shown_item);
		put_rows(kItemY, kItemY + kItemSize);
	}
}

void ZeldaBottomUpdate(void)
{
	int want = hud_wanted();

	if (want && hud_up) {
		refresh_dynamic();
		return;
	}
	if (want == hud_up)
		return;
	if (!want) {
		ZeldaVideoShowBottom();
		return;
	}
	draw_hud();
	hud_up = 1;
}
