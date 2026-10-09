#include <nds.h>
#include <fat.h>
#include <filesystem.h>

#include <stdio.h>
#include <string.h>

#define FB_WIDTH 256
#define FB_HEIGHT 192

static u16 *framebuffer;

static void draw_frame(int tick, u32 held)
{
	int x, y;
	int box_x = (tick * 2) % (FB_WIDTH - 32);
	int box_y = 80 + ((tick / 2) % 40) - 20;

	for (y = 0; y < FB_HEIGHT; y++) {
		u16 *row = framebuffer + y * FB_WIDTH;
		u16 bg = RGB15(0, 8 + (y / 16), 4);
		for (x = 0; x < FB_WIDTH; x++)
			row[x] = bg;
	}

	for (y = 0; y < 24; y++) {
		u16 *row = framebuffer + y * FB_WIDTH;
		for (x = 0; x < FB_WIDTH; x++)
			row[x] = RGB15(2, 12, 4);
	}

	for (y = 0; y < 24; y++) {
		for (x = 0; x < 32; x++) {
			int px = box_x + x;
			int py = box_y + y;
			if (px >= 0 && px < FB_WIDTH && py >= 0 && py < FB_HEIGHT)
				framebuffer[py * FB_WIDTH + px] = RGB15(31, 24, 0);
		}
	}

	if (held & KEY_UP)
		for (x = 120; x < 136; x++)
			framebuffer[40 * FB_WIDTH + x] = RGB15(31, 31, 31);
	if (held & KEY_DOWN)
		for (x = 120; x < 136; x++)
			framebuffer[56 * FB_WIDTH + x] = RGB15(31, 31, 31);
	if (held & KEY_LEFT)
		for (y = 40; y < 56; y++)
			framebuffer[y * FB_WIDTH + 112] = RGB15(31, 31, 31);
	if (held & KEY_RIGHT)
		for (y = 40; y < 56; y++)
			framebuffer[y * FB_WIDTH + 144] = RGB15(31, 31, 31);
}

static void print_keys(u32 held)
{
	iprintf("\x1b[12;0H");
	iprintf("Held: %s%s%s%s %s%s%s%s %s%s%s%s    \n",
		(held & KEY_UP) ? "U" : "-",
		(held & KEY_DOWN) ? "D" : "-",
		(held & KEY_LEFT) ? "L" : "-",
		(held & KEY_RIGHT) ? "R" : "-",
		(held & KEY_A) ? "A" : "-",
		(held & KEY_B) ? "B" : "-",
		(held & KEY_X) ? "X" : "-",
		(held & KEY_Y) ? "Y" : "-",
		(held & KEY_L) ? "l" : "-",
		(held & KEY_R) ? "r" : "-",
		(held & KEY_START) ? "S" : "-",
		(held & KEY_SELECT) ? "s" : "-");
}

static void try_read_nitro(void)
{
	FILE *fp = fopen("nitro:/hello.txt", "rb");
	char buf[64];
	size_t n;

	if (!fp) {
		iprintf("NitroFS file: FAIL\n");
		return;
	}

	memset(buf, 0, sizeof(buf));
	n = fread(buf, 1, sizeof(buf) - 1, fp);
	fclose(fp);

	if (n == 0) {
		iprintf("NitroFS file: empty\n");
		return;
	}

	iprintf("NitroFS: %s\n", buf);
}

static void try_read_fat(void)
{
	static const char *paths[] = {
		"zelda3_assets.dat",
		"fat:/zelda3_assets.dat",
		"fat:/lttp-ds/zelda3_assets.dat",
		NULL
	};
	int i;

	if (!fatInitDefault()) {
		iprintf("FAT: not mounted (need DLDI)\n");
		return;
	}

	iprintf("FAT: mounted\n");
	for (i = 0; paths[i]; i++) {
		FILE *fp = fopen(paths[i], "rb");
		if (fp) {
			long size;
			fseek(fp, 0, SEEK_END);
			size = ftell(fp);
			fclose(fp);
			iprintf("Assets: %s (%ld bytes)\n", paths[i], size);
			return;
		}
	}

	iprintf("Assets: zelda3_assets.dat not found\n");
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

	iprintf("LttP DS  --  phase 1\n");
	iprintf("DS Phat / DS Lite bring-up\n");
	iprintf("Top: framebuffer  Bottom: log\n\n");

	if (nitroFSInit(NULL)) {
		iprintf("NitroFS: ok\n");
		try_read_nitro();
	} else {
		iprintf("NitroFS: FAIL\n");
	}

	try_read_fat();

	iprintf("\nD-pad moves the marker.\n");
	iprintf("START exits to loader.\n");

	while (pmMainLoop()) {
		swiWaitForVBlank();
		scanKeys();

		u32 held = keysHeld();
		u32 down = keysDown();

		draw_frame(tick++, held);
		print_keys(held);

		if (down & KEY_START)
			break;
	}

	return 0;
}
