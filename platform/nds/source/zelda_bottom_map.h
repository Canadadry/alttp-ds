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

/* Colors of the map, by entry, and the palette slot each is shown in. */
const u16 *ZeldaMapColors(void);
u8 ZeldaMapSlot(int entry);

#endif
