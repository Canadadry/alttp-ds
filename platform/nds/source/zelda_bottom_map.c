#include <nds.h>

#include <string.h>

#include "types.h"
#include "assets.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "load_gfx.h"
#include "snes/snes_regs.h"
#include "snes/ppu.h"
#include "zelda_bottom_map.h"
#include "zelda_video.h"

uint8_t dma_read(struct Dma *dma, uint16_t adr);

/* What is on screen is a view of one of two things.
 *
 * Outdoors it is the world map, the picture the game shows when X is
 * pressed, 512 x 512 pixels of 8-bit tiles with the land in the middle 256 x
 * 256. It does not fit the box, so the box follows Link across it.
 *
 * In a dungeon it is that dungeon's map of the floor Link is on: a 5 x 5
 * grid of rooms, each four 8x8 tiles the game builds from the room's shape.
 * The tiles are 3bpp sprite graphics that are only loaded while the game's
 * own map is open, so they are unpacked from the game's data here.
 *
 * Both draw with palette entries 0 to kMapColors - 1. The last three are
 * the markers. A table says which palette slot each is shown in.
 *
 * The magic mirror stretches the top screen into horizontal bands that
 * travel downward while the colors fade through white. The same bands are
 * applied here, one per map row, and the map's colors follow that fade.
 * The world index flips at the start of the warp, while the picture on
 * top is still the world being left, so this map keeps that world until
 * the fade turns around at white.
 *
 * Rain is the overworld's BG1 overlay, the same tiles the top screen
 * blends over the ground. It is its own layer here, scrolled with that
 * one and mixed the same way: half of the rain color, or all of it while
 * the lightning is on. That mix is the rainy light. A window holds it to
 * the map, so the rest of the HUD stays solid. */

#include "zelda_map_tab.h"

enum {
	kModeNone = -1,
	kModeWorld,
	kModeDungeon,

	kWorldTiles = 64,	/* the map is 64 x 64 tiles */
	kLandMin = 128,		/* the land, in map pixels */
	kLandMax = 384,
	kLandSize = kLandMax - kLandMin,
	kWorldColors = kMapColors - 3,

	kWhite = kMapColors - 3,
	kBlack = kMapColors - 2,
	kRed = kMapColors - 1,

	kDungeons = 14,
	kRoomSize = 16,
	kGridSize = 5,
	kDungSize = kGridSize * kRoomSize,
	/* The dungeon picture's place in the box. */
	kDungX = (kMapW - kDungSize) / 2,
	kDungY = (kMapH - kDungSize) / 2,

	kTileCount = 256,
	kTileBytes = 64
};

/* Palette slots nothing else on the HUD uses: ranges of [from, to). The
 * last range is the big panel's own, free while the panel shows the map. */
static const u8 kSlotRanges[][2] = {
	{ 2, 26 }, { 96, 120 }, { 124, 126 }, { 160, 188 }, { 240, 252 },
	{ 26, 64 }
};

static u8 slot_tab[kMapColors];
static u16 colors[kMapColors];

typedef struct {
	int mode;
	int id;			/* dark world, or dungeon */
	int floor;
	int ox, oy;		/* world: the box's top left, in map pixels */
	int on;			/* Link's marker */
	int cx, cy;		/* and its center in the box */
	int boss_on;
	int bx, by;
} View;

static View shown = { kModeNone };

/* The mirror wave, one shift per map row, and how white the colors are.
 * See mirror_track. mirror_from is the world shown until the fade peaks;
 * -1 while that hold is not in use. */
static int wave_on;
static int wave_dx[kMapH];
static int wave_white;
static int mirror_from = -1;

/* The graphics: world tiles already as palette slots, or dungeon tiles as
 * their 3-bit pixel values, 64 tiles to each of the four graphics packs. */
enum { kTilesNone, kTilesWorld, kTilesDungeon };

static u8 tiles[kTileCount * kTileBytes] __attribute__((aligned(4)));
static int tiles_kind;
static int tiles_dungeon;

static u8 world_map[kWorldTiles * kWorldTiles];
static int world_dark = -1;
/* The land as pixels: palette slots, 256 x 256. */
static u8 world_pic[kLandSize][kLandSize] __attribute__((aligned(4)));
/* The same picture shifted one pixel left. The screen takes 16-bit writes
 * and the map starts on an odd pixel, so a row can be copied whole from
 * whichever of the two begins on an even address. */
static u8 world_odd[kLandSize][kLandSize] __attribute__((aligned(4)));

/* Set when the colors change; see ZeldaMapTakePalette. */
static int pal_dirty;

static u8 dung_pic[kMapH][kMapW] __attribute__((aligned(4)));
static u16 dung_ent[kGridSize * kGridSize * 4];

static void init_slots(void)
{
	unsigned i;
	int n = 0, s;

	if (slot_tab[1])
		return;
	for (i = 0; i < sizeof kSlotRanges / sizeof kSlotRanges[0]; i++)
		for (s = kSlotRanges[i][0]; s < kSlotRanges[i][1] && n < kMapColors; s++)
			slot_tab[n++] = s;
}

const u16 *ZeldaMapColors(void)
{
	return colors;
}

int ZeldaMapTakePalette(void)
{
	int d = pal_dirty;

	pal_dirty = 0;
	return d;
}

u8 ZeldaMapSlot(int entry)
{
	init_slots();
	return slot_tab[entry];
}

int ZeldaMapMirrorWhite(void)
{
	return wave_white;
}

static void marker_colors(void)
{
	colors[kWhite] = RGB15(31, 31, 31);
	colors[kBlack] = RGB15(0, 0, 0);
	colors[kRed] = RGB15(31, 6, 6);
}

/* ---- World ---- */

static void build_world(int dark)
{
	const u16 *pal = kOverworldMapPaletteData;
	int i, r, c, q;

	if (tiles_kind != kTilesWorld) {
		const u8 *src = kOverworldMapGfx;

		for (i = 0; i < kTileCount * kTileBytes; i++)
			tiles[i] = slot_tab[src[i] < kWorldColors ? src[i] : 0];
		tiles_kind = kTilesWorld;
	}

	/* Four quadrants of 32 x 32 tiles: top left, top right, bottom left,
	 * bottom right. The dark world's land replaces the middle. */
	for (q = 0; q < 4; q++)
		for (r = 0; r < 32; r++)
			for (c = 0; c < 32; c++)
				world_map[((q >> 1) * 32 + r) * kWorldTiles + (q & 1) * 32 + c] =
					kLightOverworldTilemap[q * 1024 + r * 32 + c];
	if (dark)
		for (r = 0; r < 32; r++)
			for (c = 0; c < 32; c++)
				world_map[(16 + r) * kWorldTiles + 16 + c] =
					kDarkOverworldTilemap[r * 32 + c];
	world_dark = dark;

	/* The land, drawn once as pixels, a tile row at a time in two words.
	 * Scrolling is then one copy per row. */
	for (r = 0; r < kLandSize / 8; r++) {
		for (c = 0; c < kLandSize / 8; c++) {
			const u32 *src = (const u32 *)(tiles +
				world_map[(kLandMin / 8 + r) * kWorldTiles + kLandMin / 8 + c] * kTileBytes);
			int y;

			for (y = 0; y < 8; y++) {
				u32 *dst = (u32 *)&world_pic[r * 8 + y][c * 8];

				dst[0] = src[y * 2];
				dst[1] = src[y * 2 + 1];
			}
		}
	}

	for (r = 0; r < kLandSize; r++) {
		const u32 *s = (const u32 *)world_pic[r];
		u32 *d = (u32 *)world_odd[r];

		for (c = 0; c < kLandSize / 4; c++)
			d[c] = (s[c] >> 8) | (c + 1 < kLandSize / 4 ? s[c + 1] << 24 : 0);
	}
	/* The screen is filled by DMA straight from these. */
	DC_FlushRange(world_pic, sizeof world_pic);
	DC_FlushRange(world_odd, sizeof world_odd);

	for (i = 0; i < kWorldColors; i++)
		colors[i] = pal[(dark ? 128 : 0) + i];
	marker_colors();
	pal_dirty = 1;
}

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static void world_view(View *v)
{
	u16 lx, ly;
	int mx, my;

	v->mode = kModeWorld;
	v->id = (overworld_screen_index & 0x40) ? 1 : 0;
	/* Inside a cave or house the coordinates are those of the room, so the
	 * place it was entered from stands in. */
	if (player_is_indoors) {
		lx = link_x_coord_spexit;
		ly = link_y_coord_spexit;
	} else {
		lx = link_x_coord;
		ly = link_y_coord;
	}
	mx = kLandMin + clamp(lx >> 4, 0, kLandMax - kLandMin - 1);
	my = kLandMin + clamp(ly >> 4, 0, kLandMax - kLandMin - 1);
	v->ox = clamp(mx - kMapW / 2, kLandMin, kLandMax - kMapW);
	v->oy = clamp(my - kMapH / 2, kLandMin, kLandMax - kMapH);
	v->cx = mx - v->ox;
	v->cy = my - v->oy;
	v->on = (frame_counter & 0x10) != 0;
}

/* A larger scroll shows land from further right, so the row slides left.
 * Past the land, the edge pixel is repeated. */
static void world_row(u8 *dst, int r)
{
	int yy = shown.oy + r - kLandMin;
	int x0 = shown.ox - kLandMin;
	int dx = wave_on ? wave_dx[r] : 0;
	int s0, left, n;

	if ((unsigned)yy >= (unsigned)kLandSize) {
		memset(dst, slot_tab[kBlack], kMapW);
		return;
	}
	s0 = x0 + dx;
	if (s0 >= 0 && s0 + kMapW <= kLandSize) {
		memcpy(dst, &world_pic[yy][s0], kMapW);
		return;
	}
	left = 0;
	if (s0 < 0) {
		left = -s0;
		if (left > kMapW)
			left = kMapW;
		memset(dst, world_pic[yy][0], left);
		s0 = 0;
	}
	n = kMapW - left;
	if (s0 >= kLandSize) {
		memset(dst + left, world_pic[yy][kLandSize - 1], n);
		return;
	}
	if (s0 + n > kLandSize) {
		int fit = kLandSize - s0;

		memcpy(dst + left, &world_pic[yy][s0], fit);
		memset(dst + left + fit, world_pic[yy][kLandSize - 1], n - fit);
		return;
	}
	memcpy(dst + left, &world_pic[yy][s0], n);
}

/* ---- Dungeon ---- */

/* The four graphics packs the game loads into the sprite tiles that a
 * dungeon's map uses, by dungeon. */
static const u8 kPacks[4][4] = {
	{ 97, 98, 99, 80 },
	{ 97, 86, 87, 80 },
	{ 97, 86, 99, 80 },
	{ 97, 86, 51, 80 }
};
static const u8 kPackSet[kDungeons] = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 2, 1, 3, 1 };

/* Floors: low nibble below the ground, next nibble above. */
static const u8 kFloors[kDungeons] = {
	0x21, 0x23, 0x20, 0x21, 0x70, 0x12, 0x11, 0x12, 0x02, 0x17, 0x60, 0x12, 0x13, 0x71
};
/* Where Link's room is if it is one of these, as the map knows it. */
static const u16 kRoomFrom[3] = { 137, 167, 79 };
static const u16 kRoomTo[3] = { 169, 119, 190 };
/* The boss's room, its floor, and where the icon sits in the room. */
static const u16 kBossRoom[kDungeons] = { 15, 15, 200, 51, 32, 6, 90, 144, 41, 222, 7, 172, 164, 13 };
static const short kBossFloor[kDungeons] = { -1, -1, 1, 1, 6, 0xff, 0xff, 0xff, 0xfe, 0xf9, 5, 0xff, 0xfd, 6 };
static const short kBossSpot[kDungeons] = { -1, -1, 0x808, 8, 0, 8, 0x808, 8, 0x808, 0x800, 0x404, 0x808, 8, 8 };

static u8 pack_buf[0x800] __attribute__((aligned(4)));

static void build_dungeon_tiles(int d)
{
	const u8 *packs = kPacks[kPackSet[d]];
	int s, t, r, x;

	for (s = 0; s < 4; s++) {
		Decomp_spr(pack_buf, packs[s]);
		/* Three bit planes: two interleaved by row, then the third. */
		for (t = 0; t < 64; t++) {
			const u8 *src = pack_buf + t * 24;
			u8 *dst = tiles + (s * 64 + t) * kTileBytes;

			for (r = 0; r < 8; r++)
				for (x = 0; x < 8; x++) {
					int bit = 7 - x;

					dst[r * 8 + x] = ((src[r * 2] >> bit) & 1) |
						(((src[r * 2 + 1] >> bit) & 1) << 1) |
						(((src[16 + r] >> bit) & 1) << 2);
				}
		}
	}
	tiles_kind = kTilesDungeon;
	tiles_dungeon = d;
}

static void build_dungeon_colors(void)
{
	const u16 *pal = kPalette_PalaceMapBg;
	int p, v;

	memset(colors, 0, sizeof colors);
	/* Tile palettes 2 to 7, seven drawn colors each. */
	for (p = 2; p < 8; p++)
		for (v = 1; v < 8; v++)
			colors[(p - 2) * 7 + v - 1] = pal[(p - 2) * 16 + v];
	marker_colors();
	pal_dirty = 1;
}

/* The four tile entries of every room on a floor, as the game's map
 * would have written them: what the player has seen shows, and the whole
 * layout shows with the map item. */
static void dungeon_entries(u16 *ent, int d, int index)
{
	MemBlk layout = kDungMap_FloorLayout(d);
	MemBlk shapes = kDungMap_Tiles(d);
	const u8 *curp = layout.ptr;
	int have_map = (link_dungeon_map & (0x8000u >> d)) != 0;
	int i, j, q;

	for (i = 0; i < kGridSize; i++) {
		for (j = 0; j < kGridSize; j++) {
			u8 v = curp[index * 25 + i * 5 + j];
			int yv = 0x51, vis = 0, k, count = 0;

			if (v != 0xf) {
				vis = save_dung_info[v] & 0xf;
				for (k = 0; k < (int)layout.size && curp[k] != v; k++)
					count += curp[k] != 0xf;
				yv = count < (int)shapes.size ? ((const u8 *)shapes.ptr)[count] : 0x51;
			}
			for (q = 0; q < 4; q++) {
				u16 org = kRoomPieces[yv * 4 + q], r12 = org, av;
				int bit = 8 >> q;

				if (org != 0xb00 && !(vis & bit)) {
					if (!(r12 & 0x1000)) {
						r12 = 0x400;
					} else if (have_map) {
						ent[(i * 5 + j) * 4 + q] = (org & ~0x1c00) | 0xc00;
						continue;
					} else {
						r12 = 0;
					}
				} else {
					r12 = 0;
				}
				av = (have_map || (vis & bit)) ? r12 + org : 0xb00;
				ent[(i * 5 + j) * 4 + q] = av;
			}
		}
	}
}

/* Three by five letters and numbers, a row to a byte, the high bit left. */
static const u8 kGlyph[][5] = {
	{ 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 },
	{ 5, 5, 7, 1, 1 }, { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 },
	{ 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
	{ 6, 5, 6, 5, 6 },	/* B */
	{ 7, 4, 6, 4, 4 }	/* F */
};
enum { kGlyphB = 10, kGlyphF = 11 };

static void draw_glyph(int g, int x, int y)
{
	int r, c;

	for (r = 0; r < 5; r++)
		for (c = 0; c < 3; c++)
			if (kGlyph[g][r] & (4 >> c)) {
				dung_pic[y + r * 2][x + c * 2] = slot_tab[kWhite];
				dung_pic[y + r * 2][x + c * 2 + 1] = slot_tab[kWhite];
				dung_pic[y + r * 2 + 1][x + c * 2] = slot_tab[kWhite];
				dung_pic[y + r * 2 + 1][x + c * 2 + 1] = slot_tab[kWhite];
			}
}

static void draw_dungeon(int floor)
{
	int cell, q;

	memset(dung_pic, slot_tab[kBlack], sizeof dung_pic);
	for (cell = 0; cell < kGridSize * kGridSize; cell++) {
		for (q = 0; q < 4; q++) {
			u16 av = dung_ent[cell * 4 + q];
			int t = (av & 0x3ff) - 0x300;
			int pal = (av >> 10) & 7;
			int x0 = kDungX + (cell % 5) * kRoomSize + (q & 1) * 8;
			int y0 = kDungY + (cell / 5) * kRoomSize + (q >> 1) * 8;
			int x, y;

			if (t < 0 || t >= kTileCount || pal < 2)
				continue;
			for (y = 0; y < 8; y++) {
				int sy = (av & 0x8000) ? 7 - y : y;

				for (x = 0; x < 8; x++) {
					int sx = (av & 0x4000) ? 7 - x : x;
					int v = tiles[t * kTileBytes + sy * 8 + sx];

					if (v)
						dung_pic[y0 + y][x0 + x] =
							slot_tab[(pal - 2) * 7 + v - 1];
				}
			}
		}
	}
	/* The floor, as 1F or B2. */
	if (floor >= 0) {
		draw_glyph(clamp(floor + 1, 1, 9), 4, 4);
		draw_glyph(kGlyphF, 12, 4);
	} else {
		draw_glyph(kGlyphB, 4, 4);
		draw_glyph(clamp(-floor, 1, 9), 12, 4);
	}
}

/* Where a room is on its floor's grid, or -1. */
static int find_room(const u8 *floor_row, int room)
{
	int i;

	for (i = 0; i < 25; i++)
		if (floor_row[i] == (u8)room)
			return i;
	return -1;
}

static int dungeon_view(View *v)
{
	int d = cur_palace_index_x2 >> 1;
	int below = kFloors[d] & 0xf, above = (kFloors[d] >> 4) & 0xf;
	MemBlk layout = kDungMap_FloorLayout(d);
	const u8 *curp = layout.ptr;
	int index = clamp((int8)dung_cur_floor + below, 0, below + above - 1);
	int room = dungeon_room_index, cell, i;

	if (!curp || layout.size < (unsigned)(below + above) * 25)
		return 0;
	v->mode = kModeDungeon;
	v->id = d;
	v->floor = index - below;

	for (i = 0; i < 3; i++)
		if (room == kRoomFrom[i])
			room = kRoomTo[i];
	cell = find_room(curp + index * 25, room);
	if (cell >= 0) {
		v->on = (frame_counter & 0x10) != 0;
		v->cx = kDungX + (cell % 5) * kRoomSize + ((link_x_coord & 0x1e0) >> 5);
		v->cy = kDungY + (cell / 5) * kRoomSize + ((link_y_coord & 0x1e0) >> 5);
	}

	/* The boss, once the compass says where, until it is beaten. */
	if (kBossSpot[d] >= 0 && kBossFloor[d] >= 0 &&
	    (u8)kBossFloor[d] == (u8)v->floor &&
	    (link_compass & (0x8000u >> d)) &&
	    !(save_dung_info[kBossRoom[d]] & 0x800) &&
	    (frame_counter & 0xf) < 10) {
		for (i = 24; i >= 0 && !(curp[index * 25 + i] != 0xf &&
					 curp[index * 25 + i] == kBossRoom[d]); i--)
			;
		if (i >= 0) {
			v->boss_on = 1;
			v->bx = kDungX + (i % 5) * kRoomSize + (kBossSpot[d] >> 8) + 4;
			v->by = kDungY + (i / 5) * kRoomSize + (kBossSpot[d] & 0xff) + 4;
		}
	}
	return 1;
}

/* ---- Markers ---- */

static const char *const kWorldMark[7] = {
	"...B...", "..BWB..", ".BWWWB.", "BWWWWWB", ".BWWWB.", "..BWB..", "...B..."
};
static const char *const kDungeonMark[5] = {
	"..B..", ".BWB.", "BWWWB", ".BWB.", "..B.."
};
static const char *const kBossMark[5] = {
	"BBBBB", "BRRRB", "BRRRB", "BRRRB", "BBBBB"
};

static void draw_mark(u8 *dst, int r, int cx, int cy, const char *const *pat, int n)
{
	int row = r - (cy - n / 2), c;

	if (row < 0 || row >= n)
		return;
	for (c = 0; c < n; c++) {
		int x = cx - n / 2 + c;
		int k;

		if (x < 0 || x >= kMapW)
			continue;
		switch (pat[row][c]) {
		case 'W': k = kWhite; break;
		case 'B': k = kBlack; break;
		case 'R': k = kRed; break;
		default: continue;
		}
		dst[x] = slot_tab[k];
	}
}

/* The dungeon picture is only as wide as the box, so a shift past either
 * end is the empty color. The marker is drawn afterwards, unshifted:
 * on the top screen the wave scrolls the ground and leaves Link put. */
static void blit_shifted(u8 *dst, const u8 *src, int dx, u8 fill)
{
	int x;

	if (!dx) {
		memcpy(dst, src, kMapW);
		return;
	}
	for (x = 0; x < kMapW; x++) {
		int s = x + dx;

		dst[x] = (unsigned)s < (unsigned)kMapW ? src[s] : fill;
	}
}

void ZeldaMapRow(u8 *dst, int y)
{
	int r = y - kMapY;

	init_slots();
	if (r < 0 || r >= kMapH)
		return;
	if (shown.mode == kModeWorld)
		world_row(dst, r);
	else if (shown.mode == kModeDungeon)
		blit_shifted(dst, dung_pic[r], wave_on ? wave_dx[r] : 0,
			     slot_tab[kBlack]);
	else
		memset(dst, slot_tab[kBlack], kMapW);

	if (shown.mode == kModeWorld) {
		if (shown.on)
			draw_mark(dst, r, shown.cx, shown.cy, kWorldMark, 7);
	} else 	if (shown.mode == kModeDungeon) {
		if (shown.boss_on)
			draw_mark(dst, r, shown.bx, shown.by, kBossMark, 5);
		if (shown.on)
			draw_mark(dst, r, shown.cx, shown.cy, kDungeonMark, 5);
	}
}

/* Rows of the world picture that carry no marker are copied to the screen as
 * they are. The map starts on an odd pixel, so the pixel before it and the
 * first map pixel go out as one 16-bit store, the same at the far end, and
 * the 144 between them are one DMA from a copy that starts on an even
 * address. Redrawing the whole map was most of 3 ms, a few times a second
 * while running, and all of it went on shifting bytes. */
static void put_world_row(u8 *row, int r, u8 border)
{
	int yy = shown.oy + r - kLandMin;
	int cx = shown.ox - kLandMin;
	const u8 *mid = (cx & 1) ? &world_pic[yy][cx + 1] : &world_odd[yy][cx];
	u16 *edge = (u16 *)(row + kMapX - 1);

	edge[0] = (u16)(border | (world_pic[yy][cx] << 8));
	dmaCopyHalfWords(3, mid, row + kMapX + 1, kMapW - 2);
	edge[kMapW / 2] = (u16)(world_pic[yy][cx + kMapW - 1] | (border << 8));
}

void ZeldaMapPutRow(u8 *fb, int pitch, int y, u8 border)
{
	static u8 seg[kMapW + 2] __attribute__((aligned(4)));
	int r = y - kMapY;
	u8 *row = fb + y * pitch;

	if (r < 0 || r >= kMapH)
		return;
	/* The straight copy has no per-row shift, so the wave takes the
	 * path that builds the row. */
	if (shown.mode == kModeWorld && !wave_on &&
	    !(shown.on && r >= shown.cy - 3 && r <= shown.cy + 3)) {
		put_world_row(row, r, border);
		return;
	}
	seg[0] = border;
	seg[kMapW + 1] = border;
	ZeldaMapRow(seg + 1, y);
	DC_FlushRange(seg, sizeof seg);
	dmaCopyHalfWords(3, seg, row + kMapX - 1, kMapW + 2);
}

/* ---- Update ---- */

void ZeldaMapInvalidate(void)
{
	shown.mode = kModeNone;
	world_dark = -1;
	memset(dung_ent, 0xff, sizeof dung_ent);
}

static void note_rows(int *lo, int *hi, int cy, int half)
{
	int a = kMapY + cy - half, b = kMapY + cy + half + 1;

	if (a < kMapY)
		a = kMapY;
	if (b > kMapY + kMapH)
		b = kMapY + kMapH;
	if (a < *lo)
		*lo = a;
	if (b > *hi)
		*hi = b;
}

static void mark_rows(const View *v, int *lo, int *hi)
{
	int half = v->mode == kModeWorld ? 3 : 2;

	if (v->on)
		note_rows(lo, hi, v->cy, half);
	if (v->boss_on)
		note_rows(lo, hi, v->by, 2);
}

/* Channels 6 and 7 writing both background scrolls: the mirror wave, and
 * not the door iris. The top screen reads the same registers. */
static int mirror_running(void)
{
	if ((HDMAEN_copy & 0xc0) != 0xc0 || !g_zenv.dma)
		return 0;
	return dma_read(g_zenv.dma, BBAD6) == (uint8)BG1HOFS &&
		dma_read(g_zenv.dma, BBAD7) == (uint8)BG2HOFS;
}

/* Countdown climbs to 31 on the way to white. On the way back the
 * picture stays white until the new world's colors are loaded
 * (overworld_map_state step 9), and each step after that lets one
 * level of white go, which is what the subtractive filter does once
 * it has those colors to move toward. */
static int mirror_white_of(int on)
{
	static int back = -1;
	unsigned dark = darkening_or_lightening_screen;
	unsigned c = palette_filter_countdown;
	int left;

	if (!on) {
		back = -1;
		return 0;
	}
	if (dark == 2) {
		back = -1;
		return c > 31 ? 31 : (int)c;
	}
	if (dark != 0 || c < 31)
		return 0;
	if (overworld_map_state < 9) {
		back = -1;
		return -31;
	}
	if (back < 0)
		back = (int)c;
	left = 31 - ((int)c - back);
	return left > 0 ? -left : 0;
}

/* Rows of the SNES picture the top screen actually shows. The map is
 * shorter, so each of its rows takes the line at the same fraction. */
enum { kMirrorCrop = 16 };

/* Sample the wave. Returns nonzero when the shifts changed and the map
 * picture has to be drawn again. A change in the white level is reported
 * through the palette flag instead. */
static int mirror_track(void)
{
	int on = mirror_running();
	int white = mirror_white_of(on);
	int changed = on != wave_on;
	int i;

	if (on) {
		for (i = 0; i < kMapH; i++) {
			int line = kMirrorCrop +
				(i * (SCREEN_HEIGHT - 1)) / (kMapH - 1);
			int d = (int16)(uint16)(hdma_table_dynamic[line] -
						BG2HOFS_copy2);

			if (d != wave_dx[i])
				changed = 1;
			wave_dx[i] = d;
		}
	} else if (changed) {
		memset(wave_dx, 0, sizeof wave_dx);
	}
	wave_on = on;
	if (white != wave_white) {
		wave_white = white;
		pal_dirty = 1;
	}
	return changed;
}

/* The rain layer. Tiles are the SNES 4bpp ones, expanded to 8bpp so each
 * pixel is the cgram index it already was. Color 0 stays transparent.
 * Slot 0 is that empty tile. One view of the map is about 400 tiles, and
 * the cache holds a few more than that before it starts over. */
enum {
	kRainTiles = 448,
	kRainKeys = 1024 * 8
};

static u8 rain_gfx[kRainTiles][64] __attribute__((aligned(32)));
static u8 rain_tile_dirty[kRainTiles];
static uint32 rain_hash[kRainTiles];
static short rain_slot[kRainKeys];
static int rain_used;
static int rain_cache_on;

static u16 rain_map[64 * 64] __attribute__((aligned(4)));
static int rain_map_dirty;
static int rain_seen_epoch;

static int rain_active;
/* The window is armed. Vblank holds its top at 7, and line 0 puts it
 * back on the map. See ZeldaMapRainVblank. */
static int rain_guard;
static int rain_wider;
static int rain_higher;
static int rain_scroll_x;
static int rain_scroll_y;
static int rain_half;

static void rain_cache_reset(void)
{
	int i;

	for (i = 0; i < kRainKeys; i++)
		rain_slot[i] = -1;
	memset(rain_gfx, 0, sizeof rain_gfx);
	memset(rain_tile_dirty, 0, sizeof rain_tile_dirty);
	memset(rain_map, 0, sizeof rain_map);
	rain_tile_dirty[0] = 1;
	rain_used = 1;
	rain_map_dirty = 1;
	rain_cache_on = 1;
}

/* Word offset of a tile in a SNES map and in the DS map, which is laid
 * out the same way. */
static int rain_words(const BgLayer *layer, int tx, int ty)
{
	int adr = ((ty & 31) << 5) | (tx & 31);

	if ((tx & 32) && layer->tilemapWider)
		adr += 0x400;
	if ((ty & 32) && layer->tilemapHigher)
		adr += layer->tilemapWider ? 0x800 : 0x400;
	return adr;
}

static int rain_gfx_base(const BgLayer *layer, int name)
{
	return (layer->tileAdr + name * 16) & 0x7fff;
}

static int rain_blank(const Ppu *ppu, int base)
{
	int i;

	for (i = 0; i < 16; i++)
		if (ppu->vram[(base + i) & 0x7fff])
			return 0;
	return 1;
}

static uint32 rain_tile_hash(const Ppu *ppu, int base)
{
	uint32 h = 2166136261u;
	int i;

	for (i = 0; i < 16; i++) {
		h ^= ppu->vram[(base + i) & 0x7fff];
		h *= 16777619u;
	}
	return h ? h : 1;
}

static void rain_decode(const Ppu *ppu, int base, int pal, u8 *dst)
{
	int row;

	for (row = 0; row < 8; row++) {
		uint16 plane0 = ppu->vram[(base + row) & 0x7fff];
		uint16 plane1 = ppu->vram[(base + 8 + row) & 0x7fff];
		int col;

		for (col = 0; col < 8; col++) {
			int bit = 7 - col;
			int pix = ((plane0 >> bit) & 1) |
				(((plane0 >> (8 + bit)) & 1) << 1) |
				(((plane1 >> bit) & 1) << 2) |
				(((plane1 >> (8 + bit)) & 1) << 3);

			dst[row * 8 + col] = pix ? (u8)(pal * 16 + pix) : 0;
		}
	}
}

/* The DS tile for this SNES map entry, or -1 when the cache is full.
 * A clear tile is index 0, which stays transparent. */
static int rain_ensure(const Ppu *ppu, const BgLayer *layer, uint16 entry)
{
	int name = entry & 0x3ff;
	int pal = (entry >> 10) & 7;
	int base = rain_gfx_base(layer, name);
	int key = name * 8 + pal;
	uint32 h;
	int slot;

	if (rain_blank(ppu, base))
		return 0;
	h = rain_tile_hash(ppu, base);
	slot = rain_slot[key];
	if (slot > 0 && rain_hash[slot] == h)
		return slot;
	if (slot <= 0) {
		if (rain_used >= kRainTiles)
			return -1;
		slot = rain_used++;
		rain_slot[key] = (short)slot;
	}
	rain_decode(ppu, base, pal, rain_gfx[slot]);
	rain_hash[slot] = h;
	rain_tile_dirty[slot] = 1;
	return slot;
}

static int rain_fill(const Ppu *ppu, const BgLayer *layer,
		int tx0, int tx1, int ty0, int ty1)
{
	int tx, ty;

	for (ty = ty0; ty <= ty1; ty++) {
		for (tx = tx0; tx <= tx1; tx++) {
			uint16 entry = ppu->vram[
				(layer->tilemapAdr + rain_words(layer, tx, ty)) & 0x7fff];
			int slot = rain_ensure(ppu, layer, entry);
			int at;
			u16 ds;

			if (slot < 0)
				return 0;
			/* Bits 10 and 11 are the DS flips. Bits 14 and 15
			 * are the SNES ones. */
			ds = (u16)((slot & 0x3ff) |
				((entry & 0x4000) ? 0x400 : 0) |
				((entry & 0x8000) ? 0x800 : 0));
			at = rain_words(layer, tx, ty);
			if (rain_map[at] != ds) {
				rain_map[at] = ds;
				rain_map_dirty = 1;
			}
		}
	}
	return 1;
}

void ZeldaMapRainPrepare(int show)
{
	const Ppu *ppu;
	const BgLayer *layer;
	int sx, sy, tx0, tx1, ty0, ty1;

	rain_active = 0;
	if (!show || player_is_indoors || BYTE(overlay_index) != 0x9f)
		return;
	ppu = g_zenv.ppu;
	if (!ppu)
		return;
	if (!rain_cache_on)
		rain_cache_reset();
	layer = &ppu->bgLayer[0];
	/* The top screen drops the first 16 SNES lines, and its rain scroll
	 * is shifted by that much. The same shift puts the same streaks on
	 * the same pixels here. */
	sx = (int)layer->hScroll;
	sy = (int)layer->vScroll + kMirrorCrop;
	tx0 = (kMapX + sx) >> 3;
	tx1 = (kMapX + kMapW - 1 + sx) >> 3;
	ty0 = (kMapY + sy) >> 3;
	ty1 = (kMapY + kMapH - 1 + sy) >> 3;
	if (!rain_fill(ppu, layer, tx0, tx1, ty0, ty1)) {
		rain_cache_reset();
		if (!rain_fill(ppu, layer, tx0, tx1, ty0, ty1))
			return;
	}
	rain_wider = layer->tilemapWider;
	rain_higher = layer->tilemapHigher;
	rain_scroll_x = sx;
	rain_scroll_y = sy;
	rain_half = ppu->halfColor;
	rain_active = 1;
}

static u16 rain_dim_color(u16 c, int level)
{
	int keep;

	if (level <= 0)
		return c;
	if (level >= 16)
		return 0;
	keep = 16 - level;
	return RGB15(((c >> 0) & 31) * keep / 16,
		     ((c >> 5) & 31) * keep / 16,
		     ((c >> 10) & 31) * keep / 16);
}

/* Line 0 and scanline 256 both match a Y trigger of 0. Only line 0 is
 * past the vblank match, so the top can move back onto the map. */
static void rain_line0(void)
{
	if (!rain_guard || REG_VCOUNT != 0)
		return;
	SUB_WIN0_Y0 = (u8)kMapY;
}

static void rain_guard_on(void)
{
	if (rain_guard)
		return;
	rain_guard = 1;
	SetYtrigger(0);
	irqSet(IRQ_VCOUNT, rain_line0);
	irqEnable(IRQ_VCOUNT);
	REG_DISPSTAT |= DISP_YTRIGGER_IRQ;
}

static void rain_guard_off(void)
{
	if (!rain_guard)
		return;
	rain_guard = 0;
	irqDisable(IRQ_VCOUNT);
	REG_DISPSTAT &= ~DISP_YTRIGGER_IRQ;
}

void ZeldaMapRainVblank(void)
{
	if (!rain_guard)
		return;
	/* 6 also compares equal to scanline 262. Leaving it there turns the
	 * window on in vblank, and it stays on from the top of the screen. */
	SUB_WIN0_Y0 = (u8)(kMapY + 1);
}

static void rain_hide(void)
{
	int bg = ZeldaVideoRainBg();

	rain_guard_off();
	if (bg >= 0)
		bgHide(bg);
	REG_BLDCNT_SUB = 0;
	windowDisableSub(WINDOW_0);
}

/* The panel fade dims its own colors. Dim the rain by the same amount and
 * the blend stays the rainy color, only darker, instead of mixing black
 * back onto the map. */
static void rain_palette(const Ppu *ppu, int level)
{
	int i;

	vramSetBankH(VRAM_H_LCD);
	for (i = 0; i < 128; i++)
		VRAM_H_EXT_PALETTE[0][0][i] =
			rain_dim_color((u16)(ppu->cgram[i] & 0x7fff), level);
	vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
}

static void rain_upload(int bg)
{
	u16 *gfx = bgGetGfxPtr(bg);
	u16 *map = bgGetMapPtr(bg);
	int words = 1024 << ((rain_wider ? 1 : 0) + (rain_higher ? 1 : 0));
	int i;

	for (i = 0; i < rain_used; i++) {
		if (!rain_tile_dirty[i])
			continue;
		DC_FlushRange(rain_gfx[i], 64);
		dmaCopy(rain_gfx[i], gfx + i * 32, 64);
		rain_tile_dirty[i] = 0;
	}
	if (!rain_map_dirty)
		return;
	DC_FlushRange(rain_map, (u32)words * 2);
	dmaCopy(rain_map, map, (u32)words * 2);
	rain_map_dirty = 0;
}

static void rain_show(int bg)
{
	int coef = rain_half ? 8 : 16;

	/* In vblank the top stays at 7. Line 0 moves it to the map, which
	 * is line 6, after scanline 262 has already gone by. */
	windowSetBoundsSub(WINDOW_0, kMapX,
		(u8)(REG_VCOUNT >= (unsigned)SCREEN_HEIGHT ? kMapY + 1 : kMapY),
		(u8)(kMapX + kMapW), (u8)(kMapY + kMapH));
	rain_guard_on();
	/* Inside the map: the rain, the picture, and the blend. Outside:
	 * the picture only, so hearts and counters stay solid. */
	SUB_WIN_IN = (u16)(BIT(0) | BIT(3) | BIT(5));
	SUB_WIN_OUT = (u16)BIT(3);
	windowEnableSub(WINDOW_0);
	REG_BLDCNT_SUB = BLEND_ALPHA | BLEND_SRC_BG0 | BLEND_DST_BG3 |
		BLEND_DST_BACKDROP;
	REG_BLDALPHA_SUB = (u16)(coef | (coef << 8));
	REG_BG0HOFS_SUB = (u16)rain_scroll_x;
	REG_BG0VOFS_SUB = (u16)rain_scroll_y;
	bgSetScroll(bg, rain_scroll_x, rain_scroll_y);
	bgSetPriority(bg, 0);
	bgExtPaletteEnableSub();
	bgShow(bg);
}

void ZeldaMapRainCommit(int dim)
{
	int bg, ep, i;

	if (!rain_active || dim < 0 || !g_zenv.ppu) {
		rain_hide();
		return;
	}
	ZeldaVideoRainConfigure(rain_wider, rain_higher);
	bg = ZeldaVideoRainBg();
	if (bg < 0) {
		rain_hide();
		return;
	}
	ep = ZeldaVideoRainEpoch();
	if (ep != rain_seen_epoch) {
		rain_seen_epoch = ep;
		for (i = 0; i < rain_used; i++)
			rain_tile_dirty[i] = 1;
		rain_map_dirty = 1;
	}
	rain_upload(bg);
	rain_palette(g_zenv.ppu, dim);
	rain_show(bg);
}

/* The index already names the destination. Keep drawing the world we
 * left until the fade reaches white and turns around. */
static void mirror_hold_world(View *n)
{
	if (wave_on && darkening_or_lightening_screen == 2) {
		if (mirror_from < 0)
			mirror_from = n->id ^ 1;
		n->id = mirror_from;
	} else {
		mirror_from = -1;
	}
}

int ZeldaMapUpdate(int *lo, int *hi)
{
	View n;
	int full = 0;
	int wave_rows;

	init_slots();
	wave_rows = mirror_track();
	memset(&n, 0, sizeof n);
	n.mode = kModeNone;

	if (player_is_indoors && cur_palace_index_x2 < kDungeons * 2 &&
	    dungeon_view(&n)) {
		u16 ent[kGridSize * kGridSize * 4];

		if (shown.mode != kModeDungeon || shown.id != n.id ||
		    tiles_kind != kTilesDungeon || tiles_dungeon != n.id) {
			if (tiles_kind != kTilesDungeon || tiles_dungeon != n.id)
				build_dungeon_tiles(n.id);
			build_dungeon_colors();
			memset(dung_ent, 0xff, sizeof dung_ent);
			full = 1;
		}
		/* Rooms are seen a few at a time, so look every few frames. */
		if (full || shown.floor != n.floor || (frame_counter & 7) == 0) {
			dungeon_entries(ent, n.id, n.floor + (kFloors[n.id] & 0xf));
			if (full || shown.floor != n.floor ||
			    memcmp(ent, dung_ent, sizeof ent)) {
				memcpy(dung_ent, ent, sizeof ent);
				draw_dungeon(n.floor);
				full = 1;
			}
		}
		mirror_from = -1;
	} else if (player_is_indoors) {
		/* A cave or house, which has no map. Draw nothing. */
		if (shown.mode != kModeNone || shown.id != -1)
			full = 1;
		n.id = -1;
		mirror_from = -1;
	} else {
		world_view(&n);
		mirror_hold_world(&n);
		if (shown.mode != kModeWorld || world_dark != n.id) {
			build_world(n.id);
			full = 1;
		}
		if (shown.ox != n.ox || shown.oy != n.oy)
			full = 1;
	}

	if (full || wave_rows) {
		*lo = kMapY;
		*hi = kMapY + kMapH;
		shown = n;
		return 1;
	}
	if (shown.on != n.on || shown.boss_on != n.boss_on ||
	    (n.on && (shown.cx != n.cx || shown.cy != n.cy)) ||
	    (n.boss_on && (shown.bx != n.bx || shown.by != n.by))) {
		*lo = kMapY + kMapH;
		*hi = kMapY;
		mark_rows(&shown, lo, hi);
		mark_rows(&n, lo, hi);
		shown = n;
		if (*hi > *lo)
			return 1;
	} else {
		shown = n;
	}
	/* The fade moved and the picture did not. */
	if (pal_dirty) {
		*lo = *hi = kMapY;
		return 1;
	}
	return 0;
}
