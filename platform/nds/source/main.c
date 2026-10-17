#include <nds.h>
#include <fat.h>
#include <filesystem.h>

#include <stdio.h>

#include "zelda_host.h"
#include "zelda_save.h"
#include "zelda_video.h"

int main(int argc, char **argv)
{
	u16 *framebuffer;

	(void)argc;
	(void)argv;

	lcdMainOnTop();
	consoleDemoInit();

	iprintf("LttP DS\n");

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

	while (pmMainLoop()) {
		scanKeys();
		u32 held = keysHeld();
		u32 down = keysDown();

		if ((held & (KEY_L | KEY_R | KEY_START | KEY_SELECT)) ==
		    (KEY_L | KEY_R | KEY_START | KEY_SELECT)) {
			ZeldaHostSave();
			break;
		}

		ZeldaHostOnKeys(held, down);
		ZeldaHostTick(held);
		ZeldaSaveFlush();
		ZeldaHostAudio();
		ZeldaHostHud();

		/* Present during vblank. A window or palette write during the
		 * picture shows up as a full-width bar. */
		swiWaitForVBlank();
		ZeldaHostBlit(framebuffer);
	}

	return 0;
}
