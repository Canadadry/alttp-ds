#include <nds.h>

#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "config.h"
#include "zelda_video.h"
#include "bottom-screen-idle.h"

enum {
	kCropY = 16
};

static int bg_main;
static int hw_bound;
static uint32 g_ppu_render_flags;

void ZeldaHwInit(void);
void ZeldaHwPresent(Ppu *ppu);
void ZeldaHwPresentMode7(Ppu *ppu);
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
