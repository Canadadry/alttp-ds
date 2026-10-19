#include <nds.h>

#include <string.h>

#include "types.h"
#include "assets.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "load_gfx.h"
#include "zelda_bottom_map.h"

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
 * the markers. A table says which palette slot each is shown in. */

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

static void world_row(u8 *dst, int r)
{
	memcpy(dst, &world_pic[shown.oy + r - kLandMin][shown.ox - kLandMin], kMapW);
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

void ZeldaMapRow(u8 *dst, int y)
{
	int r = y - kMapY;

	init_slots();
	if (r < 0 || r >= kMapH)
		return;
	if (shown.mode == kModeWorld)
		world_row(dst, r);
	else if (shown.mode == kModeDungeon)
		memcpy(dst, dung_pic[r], kMapW);
	else
		memset(dst, slot_tab[kBlack], kMapW);

	if (shown.mode == kModeWorld) {
		if (shown.on)
			draw_mark(dst, r, shown.cx, shown.cy, kWorldMark, 7);
	} else if (shown.mode == kModeDungeon) {
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
	if (shown.mode == kModeWorld &&
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

int ZeldaMapUpdate(int *lo, int *hi)
{
	View n;
	int full = 0;

	init_slots();
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
	} else if (player_is_indoors) {
		/* A cave or house, which has no map. Draw nothing. */
		if (shown.mode != kModeNone || shown.id != -1)
			full = 1;
		n.id = -1;
	} else {
		world_view(&n);
		if (shown.mode != kModeWorld || world_dark != n.id) {
			build_world(n.id);
			full = 1;
		}
		if (shown.ox != n.ox || shown.oy != n.oy)
			full = 1;
	}

	if (full) {
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
		return *hi > *lo;
	}
	shown = n;
	return 0;
}
