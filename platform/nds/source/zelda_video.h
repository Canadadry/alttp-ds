#pragma once

#include <nds.h>
#include "types.h"

/* SDL-style PPU present, cloned from vendor/zelda3/src/main.c. */
void ZeldaVideoInit(void);
void ZeldaVideoShowBottom(void);
/* Lid sleep powers the LCDs off. The next present has to rebuild both screens. */
void ZeldaVideoWake(void);
void ZeldaVideoComposeSprites(void);
void ZeldaVideoPrepareMode7(void);
u16 *ZeldaVideoFramebuffer(void);
void ZeldaVideoDrawPpuFrame(void);
/* 0 is the picture, 16 is black. -1 gives brightness back to the game.
 * Main engine only; the bottom screen is left alone. */
void ZeldaVideoSetFade(int level);
/* Top-screen bitmap for the settings text. Does not reconfigure the sub engine. */
void ZeldaVideoMenuBegin(void);
void ZeldaVideoMenuEnd(void);
void ZeldaVideoMenuClear(void);
void ZeldaVideoMenuText(int x, int y, const char *text);
int ZeldaVideoMenuWidth(const char *text);
uint32 ZeldaVideoRenderFlags(void);
void ZeldaVideoSetNewRenderer(int on);
int ZeldaVideoNewRenderer(void);
