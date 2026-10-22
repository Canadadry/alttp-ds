#ifndef ZELDA_BOTTOM_MAP_H
#define ZELDA_BOTTOM_MAP_H

#include <nds.h>

/* The map drawn on the bottom screen's HUD: the world map outdoors, the
 * dungeon map inside one. It fills a rectangle of the big panel. */
enum {
	kMapX = 7,
	kMapY = 6,
	kMapW = 146,
	kMapH = 149,
	/* Palette entries the map uses, and where they live; see ZeldaMapSlot. */
	kMapColors = 120
};

/* Forget what is drawn, so the next update starts over. */
void ZeldaMapInvalidate(void);

/* Look at the game and bring the map up to date. Returns nonzero when screen
 * rows lo up to hi need drawing again. */
int ZeldaMapUpdate(int *lo, int *hi);

/* One screen row of the map: kMapW palette slots, for screen row y. */
void ZeldaMapRow(u8 *dst, int y);

/* One screen row of the map drawn into the bottom screen's 256-wide bitmap,
 * one palette slot per byte. The pixel on each side of the map is set to
 * border. Outdoors this is a straight copy of the world picture. */
void ZeldaMapPutRow(u8 *fb, int pitch, int y, u8 border);

/* Nonzero once after the map's colors change. */
int ZeldaMapTakePalette(void);

/* The magic mirror's fade, for the map's own colors. Positive: add that
 * much white to each channel. Negative: each channel is at least that far
 * toward white, which is the fade back down. Zero when the mirror is not
 * running. */
int ZeldaMapMirrorWhite(void);

/* Build this frame's rain, before vblank. show is nonzero while the world
 * map is actually on the bottom screen. */
void ZeldaMapRainPrepare(int show);
/* Start of vblank, before line 262. The map's top line is 6, and the DS
 * also matches that number in vblank, which would open the rain window
 * from the top of the screen. */
void ZeldaMapRainVblank(void);
/* Put that rain up, in vblank. dim is the panel fade, 0 clear through 16
 * black; negative leaves the rain off. */
void ZeldaMapRainCommit(int dim);

/* Colors of the map, by entry, and the palette slot each is shown in. */
const u16 *ZeldaMapColors(void);
u8 ZeldaMapSlot(int entry);

#endif
