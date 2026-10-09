#include <nds.h>
#include <fat.h>
#include <filesystem.h>

#include <stdio.h>
#include <string.h>

void ZeldaHostInit(void);

#define FB_WIDTH 256
#define FB_HEIGHT 192

static u16 *framebuffer;

static void draw_idle_frame(int tick)
{
	int x, y;
	int box_x = (tick * 2) % (FB_WIDTH - 32);
	int box_y = 80;

	for (y = 0; y < FB_HEIGHT; y++) {
		u16 bg = RGB15(0, 4 + (y / 24), 8);
		u16 *row = framebuffer + y * FB_WIDTH;
		for (x = 0; x < FB_WIDTH; x++)
			row[x] = bg;
	}

	for (y = 0; y < 16; y++) {
		for (x = 0; x < 32; x++)
			framebuffer[(box_y + y) * FB_WIDTH + box_x + x] = RGB15(31, 24, 0);
	}
}

int main(int argc, char **argv)
{
	int tick = 0;

	(void)argc;
	(void)argv;

	videoSetMode(MODE_FB0);
	vramSetBankA(VRAM_A_LCD);
	framebuffer = VRAM_A;
	lcdMainOnTop();
	consoleDemoInit();

	iprintf("LttP DS  --  phase 2\n");
	iprintf("Compile + LoadAssets + init\n\n");

	if (nitroFSInit(NULL))
		iprintf("NitroFS: ok\n");
	else
		iprintf("NitroFS: FAIL\n");

	if (fatInitDefault())
		iprintf("FAT: mounted\n");
	else
		iprintf("FAT: not mounted\n");

	ZeldaHostInit();

	iprintf("\nPhase 2 reached idle loop.\n");
	iprintf("START exits.\n");

	while (pmMainLoop()) {
		swiWaitForVBlank();
		scanKeys();
		draw_idle_frame(tick++);
		if (keysDown() & KEY_START)
			break;
	}

	return 0;
}
