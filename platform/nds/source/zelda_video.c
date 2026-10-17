#include <nds.h>

#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "config.h"
#include "zelda_video.h"

enum {
	kSnesW = 256,
	kSnesH = 224,
	kCropY = 16
};

static int bg_main;
static int hw_bound;
static uint32 g_ppu_render_flags;
static int g_snes_width = kSnesW;
static int g_snes_height = kSnesH;
static u16 pixel_buffer[kSnesW * kSnesH] __attribute__((aligned(8)));

void ZeldaHwInit(void);
void ZeldaHwPresent(Ppu *ppu);
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

void ZeldaVideoInit(void)
{
	ZeldaHwInit();
	hw_bound = 1;
	bg_main = 0;

	g_snes_width = g_config.extended_aspect_ratio * 2 + 256;
	if (g_snes_width != kSnesW)
		g_snes_width = kSnesW;
	g_snes_height = g_config.extend_y ? 240 : 224;
	if (g_snes_height > kSnesH)
		g_snes_height = kSnesH;

	g_zenv.ppu->extraLeftRight = UintMin(g_config.extended_aspect_ratio, kPpuExtraLeftRight);
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

/* Mode 7 (the world map) still uses the scanline renderer. Every frame is drawn. */
static void draw_software(void)
{
	u16 *dst_base;
	int pitch = g_snes_width * (int)sizeof(u16);
	u16 *src;

	/* The scanline renderer already applied INIDISP. */
	ZeldaHwUseMasterBright(0);

	if (hw_bound) {
		videoSetMode(MODE_5_2D);
		vramSetBankA(VRAM_A_MAIN_BG);
		bg_main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
		bgSetPriority(bg_main, 0);
		hw_bound = 0;
	}

	dst_base = bgGetGfxPtr(bg_main);
	g_zenv.ppu->extraLeftRight = 0;
	ZeldaDrawPpuFrame((uint8 *)pixel_buffer, (size_t)pitch, g_ppu_render_flags);

	src = pixel_buffer + kCropY * g_snes_width;
	DC_FlushRange(src, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(u16));
	dmaCopyWords(3, src, dst_base, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(u16));
}

void ZeldaVideoComposeSprites(void)
{
	if (g_zenv.ppu && g_zenv.ppu->mode != 7)
		ZeldaHwComposeSprites();
}

void ZeldaVideoDrawPpuFrame(void)
{
	if (g_zenv.ppu->mode == 7) {
		draw_software();
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
