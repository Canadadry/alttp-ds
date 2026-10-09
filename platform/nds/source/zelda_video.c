#include <nds.h>
#include <string.h>
#include <stdio.h>

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
static uint32 g_ppu_render_flags;
static int g_snes_width = kSnesW;
static int g_snes_height = kSnesH;
static uint8 pixel_buffer[kSnesW * kSnesH * 4] __attribute__((aligned(8)));

void ZeldaVideoInit(void)
{
	videoSetMode(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	bg_main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	lcdMainOnTop();
	bgSetPriority(bg_main, 0);

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

/* Clone of DrawPpuFrameWithPerf + SdlRenderer_BeginDraw/EndDraw.
 * DS is 256 wide, same as PC 4:3. extraLeftRight must stay 0: if it is 96,
 * ZeldaDrawPpuFrame switches the intro to a 448-wide line and this pitch shears. */
void ZeldaVideoDrawPpuFrame(void)
{
	u16 *dst_base = bgGetGfxPtr(bg_main);
	int pitch = g_snes_width * 4;
	int x, y;
	uint8 *pixels = pixel_buffer;

	g_zenv.ppu->extraLeftRight = 0;
	memset(pixel_buffer, 0, sizeof(pixel_buffer));
	ZeldaDrawPpuFrame(pixels, (size_t)pitch, g_ppu_render_flags);

	for (y = 0; y < SCREEN_HEIGHT; y++) {
		const uint8 *src = pixels + (y + kCropY) * pitch;
		u16 *dst = dst_base + y * SCREEN_WIDTH;
		for (x = 0; x < SCREEN_WIDTH; x++, src += 4) {
			/* SDL_PIXELFORMAT_ARGB8888 on LE: B, G, R, A */
			dst[x] = RGB8(src[2], src[1], src[0]) | BIT(15);
		}
	}
}
