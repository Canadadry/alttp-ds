#pragma once

/* Bottom-screen HUD. Shown only while a game is being played with the
 * HUD setting on "Bottom Screen". The game over screen fades in the idle
 * picture, and continue fades the HUD back. Every other time, the idle
 * picture stays. */

/* Call once a frame, in vblank. Swaps between the idle picture and the HUD
 * when the state changes, and does nothing otherwise. */
void ZeldaBottomUpdate(void);
/* The idle picture was just put up (boot, lid wake). Forget what was shown. */
void ZeldaBottomNoteIdle(void);

/* Touches on the bottom screen's buttons, and B backing out of the gear
 * panel or item menu. May add KEY_START to held, and may take KEY_B out of
 * held and down when B was used to back out. */
void ZeldaBottomInput(u32 *held, u32 *down);

/* This frame's Start bit was added here, for the items touch or for B
 * leaving the item menu. It means SNES Start, not the DS Start key. */
int ZeldaBottomAddedStart(void);

/* Nonzero while the gear panel is up: the game stands still, the music
 * does not. */
int ZeldaBottomHoldsGame(void);
