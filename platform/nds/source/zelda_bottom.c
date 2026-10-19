#include <nds.h>

#include "zelda_rtl.h"
#include "variables.h"
#include "zelda_bottom.h"
#include "zelda_save.h"
#include "zelda_video.h"
#include "tex_menu.h"
#include "gear-text.h"
#include "items-text.h"

enum {
	kWidth = 256,
	kHeight = 192,
	kTexW = 48,
	kTexH = 48,
	/* Slots well above the two colors in the background tile. */
	kPalText = 253,
	kPalBlack = 254,
	kPalGold = 255
};

/* A rectangle with a one-pixel border, drawn over the background. */
typedef struct {
	int x, y, w, h;
	u8 border;
	u8 fill;
} HudBox;

static const HudBox kBoxes[] = {
	{ 5, 163, 122, 24, kPalGold, kPalBlack },
	{ 130, 163, 122, 24, kPalGold, kPalBlack }
};

/* A graphic centered in one of the boxes. Palette index 0 is transparent;
 * every other pixel is drawn in color. Rows are padded to 4 bytes. */
typedef struct {
	const u8 *pix;
	int w, h;
	int box;
	u8 color;
} HudImage;

#define STRIDE(w) (((w) + 3) & ~3)

static const HudImage kImages[] = {
	{ (const u8 *)gear_textBitmap, 26, 7, 0, kPalText },
	{ (const u8 *)items_textBitmap, 34, 7, 1, kPalText }
};

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
		edge_row = y == b->y || y == b->y + b->h - 1;
		for (x = b->x; x < b->x + b->w; x++) {
			int edge_col = x == b->x || x == b->x + b->w - 1;

			line[x] = (edge_row || edge_col) ? b->border : b->fill;
		}
	}

	for (i = 0; i < sizeof kImages / sizeof kImages[0]; i++) {
		const HudImage *im = &kImages[i];
		const HudBox *b = &kBoxes[im->box];
		int top = b->y + (b->h - im->h) / 2;
		int left = b->x + (b->w - im->w) / 2;
		const u8 *src;

		if (y < top || y >= top + im->h)
			continue;
		src = im->pix + (y - top) * STRIDE(im->w);
		for (x = 0; x < im->w; x++) {
			if (src[x])
				line[left + x] = im->color;
		}
	}
}

static void draw_hud(void)
{
	/* VRAM takes 16-bit writes, so rows are built in RAM and DMA'd. */
	static u8 line[kWidth] __attribute__((aligned(4)));
	u8 *dst = (u8 *)ZeldaVideoBottomFramebuffer();
	int y;

	dmaCopy(tex_menuPal, BG_PALETTE_SUB, tex_menuPalLen);
	BG_PALETTE_SUB[kPalText] = RGB15(31, 31, 31);
	BG_PALETTE_SUB[kPalBlack] = RGB15(0, 0, 0);
	/* #e3b23d at 5 bits per channel. */
	BG_PALETTE_SUB[kPalGold] = RGB15(0xe3 >> 3, 0xb2 >> 3, 0x3d >> 3);
	for (y = 0; y < kHeight; y++) {
		compose_row(line, y);
		dmaCopy(line, dst + y * kWidth, kWidth);
	}
}

void ZeldaBottomUpdate(void)
{
	int want = hud_wanted();

	if (want == hud_up)
		return;
	if (!want) {
		ZeldaVideoShowBottom();
		return;
	}
	draw_hud();
	hud_up = 1;
}
