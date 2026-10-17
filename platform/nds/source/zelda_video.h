#pragma once

#include <nds.h>
#include "types.h"

/* SDL-style PPU present, cloned from vendor/zelda3/src/main.c. */
void ZeldaVideoInit(void);
void ZeldaVideoComposeSprites(void);
u16 *ZeldaVideoFramebuffer(void);
void ZeldaVideoDrawPpuFrame(void);
uint32 ZeldaVideoRenderFlags(void);
void ZeldaVideoSetNewRenderer(int on);
int ZeldaVideoNewRenderer(void);
