#pragma once

#include <nds.h>
#include "types.h"

/* SDL-style PPU present, cloned from vendor/zelda3/src/main.c. */
void ZeldaVideoInit(void);
void ZeldaVideoShowBottom(void);
/* 256x256 8-bit bitmap of the bottom screen. Valid after ZeldaVideoShowBottom. */
u16 *ZeldaVideoBottomFramebuffer(void);
/* Lid sleep powers the LCDs off. The next present has to rebuild both screens. */
void ZeldaVideoWake(void);
void ZeldaVideoComposeSprites(void);
void ZeldaVideoPrepareMode7(void);
u16 *ZeldaVideoFramebuffer(void);
void ZeldaVideoDrawPpuFrame(void);
/* 0 is the picture, 16 is black. -1 gives brightness back to the game.
 * Main engine only; the bottom screen is left alone. */
void ZeldaVideoSetFade(int level);
/* Screen-change fade laid over whatever the game does: 0 none .. 16 black. */
void ZeldaVideoSetTransition(int level);
/* Keep the brightness of the last picture shown while the game is ahead of
 * the screen and its next frame is not presented yet. */
void ZeldaVideoHoldBrightness(int on);
/* The last picture shown was already black. */
int ZeldaVideoShownBlack(void);
/* Top-screen bitmap for the settings text. Does not reconfigure the sub engine. */
void ZeldaVideoMenuBegin(void);
void ZeldaVideoMenuEnd(void);
void ZeldaVideoMenuClear(void);
void ZeldaVideoMenuText(int x, int y, const char *text);
int ZeldaVideoMenuWidth(const char *text);
uint32 ZeldaVideoRenderFlags(void);
void ZeldaVideoSetNewRenderer(int on);
int ZeldaVideoNewRenderer(void);
