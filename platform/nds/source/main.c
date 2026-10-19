#include <nds.h>
#include <fat.h>
#include <filesystem.h>

#include "zelda_bottom.h"
#include "zelda_host.h"
#include "zelda_save.h"
#include "zelda_video.h"

int main(int argc, char **argv)
{
	u16 *framebuffer;

	(void)argc;
	(void)argv;

	lcdMainOnTop();
	ZeldaVideoShowBottom();
	nitroFSInit(NULL);
	fatInitDefault();

	ZeldaHostInit();
	framebuffer = ZeldaVideoFramebuffer();

	while (pmMainLoop()) {
		scanKeys();
		u32 held = keysHeld();
		u32 down = keysDown();

		ZeldaBottomInput(&held, down);

		if ((held & (KEY_L | KEY_R | KEY_START | KEY_SELECT)) ==
		    (KEY_L | KEY_R | KEY_START | KEY_SELECT)) {
			ZeldaHostSave();
			break;
		}

		ZeldaHostOnKeys(held, down);
		ZeldaHostTick(held);
		ZeldaSaveFlush();
		ZeldaHostAudio();
		/* Matrix for the map, before the blank, so vblank only stores
		 * line 0. Building it in the blank was running into that line. */
		ZeldaVideoPrepareMode7();

		/* Present during vblank. A window or palette write during the
		 * picture shows up as a full-width bar. */
		swiWaitForVBlank();
		ZeldaHostBlit(framebuffer);
		ZeldaBottomUpdate();
	}

	return 0;
}
