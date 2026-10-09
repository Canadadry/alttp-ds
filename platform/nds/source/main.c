#include <nds.h>
#include <fat.h>
#include <filesystem.h>

#include <stdio.h>

#include "zelda_host.h"
#include "zelda_video.h"

int main(int argc, char **argv)
{
	u16 *framebuffer;
	int last_step = -1;

	(void)argc;
	(void)argv;

	lcdMainOnTop();
	consoleDemoInit();

	iprintf("LttP DS  --  phase 3\n");
	iprintf("asset index + 256 present\n");
	iprintf("Y/Left=prev  X/Right=next\n");
	iprintf("A=old/new renderer\n\n");

	if (nitroFSInit(NULL))
		iprintf("NitroFS: ok\n");
	else
		iprintf("NitroFS: FAIL\n");

	if (fatInitDefault())
		iprintf("FAT: mounted\n");
	else
		iprintf("FAT: not mounted\n");

	ZeldaHostInit();
	framebuffer = ZeldaVideoFramebuffer();
	iprintf("L+R+START+SELECT quits\n");

	while (pmMainLoop()) {
		scanKeys();
		u32 held = keysHeld();
		u32 down = keysDown();

		if ((held & (KEY_L | KEY_R | KEY_START | KEY_SELECT)) ==
		    (KEY_L | KEY_R | KEY_START | KEY_SELECT))
			break;

		ZeldaHostOnKeys(held, down);
		ZeldaHostTick(held);
		ZeldaHostBlit(framebuffer);

		scanKeys();
		held = keysHeld();
		down = keysDown();
		if (down)
			ZeldaHostOnKeys(held, down);

		if (ZeldaHostPhase3Step() != last_step) {
			last_step = ZeldaHostPhase3Step();
			iprintf("step 3%c\n", 'a' + last_step - 1);
		}

		swiWaitForVBlank();
	}

	return 0;
}
