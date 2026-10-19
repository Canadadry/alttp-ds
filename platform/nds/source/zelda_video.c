#include <nds.h>

#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "config.h"
#include "util.h"
#include "zelda_video.h"
#include "bottom-screen-idle.h"

enum {
	kCropY = 16
};

static int bg_main;
static int hw_bound;
static uint32 g_ppu_render_flags;

void ZeldaHwInit(void);
void ZeldaHwWake(void);
void ZeldaHwConsoleSuspend(void);
void ZeldaHwSetFade(int level);
void ZeldaHwPresent(Ppu *ppu);
void ZeldaHwPresentMode7(Ppu *ppu);
void ZeldaHwPrepareMode7(const Ppu *ppu);
void ZeldaHwComposeSprites(void);
void ZeldaHwUseMasterBright(int on);

/* Rows 0..15 and 208..223 are cropped. ppu_runLine(n) draws row n-1. */
void __real_ppu_runLine(Ppu *ppu, int line);
void __wrap_ppu_runLine(Ppu *ppu, int line)
{
	if (line < (kCropY + 1) || line > (kCropY + SCREEN_HEIGHT))
		return;
	__real_ppu_runLine(ppu, line);
}

void ZeldaVideoShowBottom(void)
{
	int bg;

	videoSetModeSub(MODE_5_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	bg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	dmaCopy(bottom_screen_idleBitmap, bgGetGfxPtr(bg), bottom_screen_idleBitmapLen);
	dmaCopy(bottom_screen_idlePal, BG_PALETTE_SUB, bottom_screen_idlePalLen);
}

void ZeldaVideoWake(void)
{
	/* Registers and the upload cache, not a graphics wipe. oamInit cleared
	 * the sprite copy composed while the lid was still shut. */
	ZeldaHwWake();
	ZeldaVideoShowBottom();
	hw_bound = 1;
	bg_main = 0;
	ZeldaVideoComposeSprites();
}

void ZeldaVideoInit(void)
{
	ZeldaHwInit();
	hw_bound = 1;
	bg_main = 0;

	g_zenv.ppu->extraLeftRight = 0;
	g_ppu_render_flags =
		(g_config.new_renderer ? kPpuRenderFlags_NewRenderer : 0) |
		(g_config.enhanced_mode7 ? kPpuRenderFlags_4x4Mode7 : 0) |
		(g_config.extend_y ? kPpuRenderFlags_Height240 : 0) |
		(g_config.no_sprite_limits ? kPpuRenderFlags_NoSpriteLimits : 0);
}

u16 *ZeldaVideoFramebuffer(void)
{
	return bgGetGfxPtr(bg_main);
}

uint32 ZeldaVideoRenderFlags(void)
{
	return g_ppu_render_flags;
}

int ZeldaVideoNewRenderer(void)
{
	return (g_ppu_render_flags & kPpuRenderFlags_NewRenderer) != 0;
}

void ZeldaVideoSetNewRenderer(int on)
{
	if (on)
		g_ppu_render_flags |= kPpuRenderFlags_NewRenderer;
	else
		g_ppu_render_flags &= ~kPpuRenderFlags_NewRenderer;
}

void ZeldaVideoComposeSprites(void)
{
	if (g_zenv.ppu)
		ZeldaHwComposeSprites();
}

void ZeldaVideoPrepareMode7(void)
{
	if (g_zenv.ppu && g_zenv.ppu->mode == 7)
		ZeldaHwPrepareMode7(g_zenv.ppu);
}

void ZeldaVideoSetFade(int level)
{
	ZeldaHwSetFade(level);
}

/* Dialogue glyphs are 2bpp, 8 wide and 16 tall: one tile, then the tile
 * 16 slots later. The width table is the advance, including the gap. */
static int menu_bg;
static u16 *menu_fb;

static int font_code(int ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A';
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 26;
	if (ch >= '0' && ch <= '9')
		return ch - '0' + 52;
	if (ch == '>')
		return 0x44;
	return 0x59;
}

static void menu_plot(int x, int y, u16 color)
{
	if ((unsigned)x >= 256 || (unsigned)y >= SCREEN_HEIGHT)
		return;
	menu_fb[y * 256 + x] = color;
}

static int menu_advance(int code)
{
	MemBlk widths = FindIndexInMemblk(g_zenv.dialogue_font_blk, 1);
	int width;

	if (!widths.ptr || code < 0 || (size_t)code >= widths.size)
		return 4;
	width = widths.ptr[code];
	if (width <= 0 || width > 8)
		return 4;
	return width;
}

static int menu_glyph(int x, int y, int code)
{
	static const u16 kInk[4] = {
		0,
		RGB15(6, 6, 12) | BIT(15),
		RGB15(31, 31, 31) | BIT(15),
		RGB15(31, 31, 27) | BIT(15)
	};
	MemBlk font = FindIndexInMemblk(g_zenv.dialogue_font_blk, 0);
	int tile, half, row, col, width;
	const u8 *src;

	if (!menu_fb || !font.ptr)
		return menu_advance(code);
	width = menu_advance(code);
	tile = ((code & 0x70) * 2) + (code & 0x0f);
	for (half = 0; half < 2; half++) {
		src = font.ptr + (tile + half * 16) * 16;
		if (src + 16 > font.ptr + font.size)
			break;
		for (row = 0; row < 8; row++) {
			u8 p0 = src[row * 2];
			u8 p1 = src[row * 2 + 1];

			for (col = 0; col < width; col++) {
				int bit = 0x80 >> col;
				int pix = ((p0 & bit) ? 1 : 0) | ((p1 & bit) ? 2 : 0);

				if (pix)
					menu_plot(x + col, y + half * 8 + row, kInk[pix]);
			}
		}
	}
	return width;
}

void ZeldaVideoMenuBegin(void)
{
	ZeldaHwConsoleSuspend();
	/* Main engine only. Bank C and the sub display stay on the idle picture. */
	videoSetMode(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	menu_bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	menu_fb = bgGetGfxPtr(menu_bg);
	ZeldaVideoMenuClear();
}

void ZeldaVideoMenuEnd(void)
{
	menu_fb = NULL;
	ZeldaHwInit();
	hw_bound = 1;
	bg_main = 0;
}

void ZeldaVideoMenuClear(void)
{
	if (!menu_fb)
		return;
	dmaFillHalfWords(BIT(15), menu_fb, 256 * SCREEN_HEIGHT * sizeof(u16));
}

void ZeldaVideoMenuText(int x, int y, const char *text)
{
	if (!menu_fb || !text)
		return;
	while (*text)
		x += menu_glyph(x, y, font_code(*text++));
}

int ZeldaVideoMenuWidth(const char *text)
{
	int w = 0;

	if (!text)
		return 0;
	while (*text)
		w += menu_advance(font_code(*text++));
	return w;
}

void ZeldaVideoDrawPpuFrame(void)
{
	if (g_zenv.ppu->mode == 7) {
		ZeldaHwPresentMode7(g_zenv.ppu);
		hw_bound = 0;
		return;
	}
	if (!hw_bound) {
		ZeldaHwInit();
		hw_bound = 1;
		bg_main = 0;
		/* oamInit just cleared the shadow OAM composed before this blank. */
		ZeldaHwComposeSprites();
	}
	ZeldaHwPresent(g_zenv.ppu);
}
