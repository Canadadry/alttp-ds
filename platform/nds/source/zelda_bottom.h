#pragma once

/* Bottom-screen HUD. Shown only while a game is being played with the
 * HUD setting on "Bottom Screen"; every other time the idle picture stays. */

/* Call once a frame, in vblank. Swaps between the idle picture and the HUD
 * when the state changes, and does nothing otherwise. */
void ZeldaBottomUpdate(void);
/* The idle picture was just put up (boot, lid wake). Forget what was shown. */
void ZeldaBottomNoteIdle(void);
