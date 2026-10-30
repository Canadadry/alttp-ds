#include "rando_playthrough.h"

#include <assert.h>
#include <string.h>

/* A pseudo-location is a world location id, or RANDO_LOCATION_COUNT + shop * RANDO_SHOP_SLOTS + slot.
 * VT's equipment slots only ever add the pre-collected items, so they get no id. */
#define PSEUDO_COUNT RANDO_PLAYTHROUGH_ENTRY_MAX

static RandoWorld shadow;
static uint8_t shadow_shop_items[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
static uint16_t sphere_locations[PSEUDO_COUNT];
static int sphere_start[PSEUDO_COUNT + 2];
static int nspheres;
static bool in_sphere[PSEUDO_COUNT];
static bool required[PSEUDO_COUNT];
static uint16_t required_location[PSEUDO_COUNT], required_sphere[PSEUDO_COUNT];
static int nrequired;
static uint16_t required_keys[PSEUDO_COUNT];
static int nrequired_keys;
static bool in_order[PSEUDO_COUNT];

static const RandoItemId junk_items[] = {
	RANDO_ITEM_BlueShield, RANDO_ITEM_Boomerang, RANDO_ITEM_MirrorShield, RANDO_ITEM_RedBoomerang,
	RANDO_ITEM_RedShield, RANDO_ITEM_BombUpgrade5, RANDO_ITEM_BombUpgrade10, RANDO_ITEM_BombUpgrade50,
	RANDO_ITEM_ArrowUpgrade5, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade70, RANDO_ITEM_RedPotion,
	RANDO_ITEM_Bee, RANDO_ITEM_TenArrows, RANDO_ITEM_Bomb, RANDO_ITEM_ThreeBombs,
	RANDO_ITEM_OneRupee, RANDO_ITEM_FiveRupees, RANDO_ITEM_TwentyRupees, RANDO_ITEM_FiftyRupees,
	RANDO_ITEM_OneHundredRupees, RANDO_ITEM_ThreeHundredRupees, RANDO_ITEM_Heart, RANDO_ITEM_Rupoor,
};

static bool is_junk(RandoItemId item)
{
	size_t i;

	for (i = 0; i < sizeof junk_items / sizeof junk_items[0]; i++)
		if (junk_items[i] == item)
			return true;
	return false;
}

static bool is_shop(int p)
{
	return p >= RANDO_LOCATION_COUNT;
}

static RandoShopId shop_of(int p)
{
	return (RandoShopId)((p - RANDO_LOCATION_COUNT) / RANDO_SHOP_SLOTS);
}

static int slot_of(int p)
{
	return (p - RANDO_LOCATION_COUNT) % RANDO_SHOP_SLOTS;
}

static RandoItemId shadow_item(int p)
{
	return (RandoItemId)(is_shop(p) ? shadow_shop_items[shop_of(p)][slot_of(p)] : shadow.location_items[p]);
}

static void set_shadow_item(int p, RandoItemId item)
{
	if (is_shop(p))
		shadow_shop_items[shop_of(p)][slot_of(p)] = (uint8_t)item;
	else
		shadow.location_items[p] = (uint8_t)item;
}

/* $world->getCollectableLocations()[name]->getItem(): a shop slot reads the real world's cached slot. */
static RandoItemId real_item(const RandoWorld *world, int p)
{
	if (!is_shop(p))
		return rando_world_item(world, (RandoLocationId)p);
	return (RandoItemId)(world->collectable_computed ? world->collectable_shop_items[shop_of(p)][slot_of(p)]
							 : world->shop_items[shop_of(p)][slot_of(p)]);
}

/* A shop slot's region is the real world's, so its access always reads the real world.
 * TODO: with location_world the real world, a world location diverges from VT, whose $this->locations
 * reads and nested canAccess($items) read the shadow (dungeon_items-mcs, Ice Palace - Big Key Chest). */
static bool can_access(const RandoWorld *location_world, const RandoWorld *world, int p, const RandoItems *items)
{
	if (is_shop(p))
		return rando_world_can_access_shop(world, shop_of(p), items);
	return rando_world_can_access(location_world, (RandoLocationId)p, items);
}

/* The shadow's getCollectableLocations(); its collected state is always empty. */
static bool is_collectable(int p)
{
	RandoLocationClass c;

	if (is_shop(p))
		return true;
	c = rando_location_info((RandoLocationId)p)->location_class;
	return c != RANDO_LOCATION_CLASS_MEDALLION && c != RANDO_LOCATION_CLASS_FOUNTAIN;
}

/* The equipment slots, then every sphere. */
static void collectable_items(RandoItems *items)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < rando_world_pre_collected_count(&shadow); i++)
		rando_items_add(items, rando_world_pre_collected_item(&shadow, i));
	for (i = 0; i < sphere_start[nspheres]; i++)
		if (shadow_item(sphere_locations[i]) != RANDO_ITEM_NONE)
			rando_items_add(items, shadow_item(sphere_locations[i]));
}

/* World::getLocationSpheres on the shadow; sphere 0 holds only the equipment slots. */
static void build_spheres(const RandoWorld *world)
{
	RandoItems my_items = shadow.pre_collected, found;
	int p, n, start;

	memset(in_sphere, 0, sizeof in_sphere);
	nspheres = 1;
	sphere_start[0] = sphere_start[1] = 0;
	do {
		start = n = sphere_start[nspheres];
		for (p = 0; p < PSEUDO_COUNT; p++) {
			if (!is_collectable(p) || shadow_item(p) == RANDO_ITEM_NONE || in_sphere[p]
			    || !can_access(&shadow, world, p, &my_items))
				continue;
			sphere_locations[n++] = (uint16_t)p;
		}
		rando_items_clear(&found);
		for (p = start; p < n; p++) {
			in_sphere[sphere_locations[p]] = true;
			rando_items_add(&found, shadow_item(sphere_locations[p]));
		}
		rando_items_merge(&my_items, &my_items, &found);
		assert(nspheres + 1 < PSEUDO_COUNT + 2);
		sphere_start[++nspheres] = n;
	} while (n > start);
}

static void add_required(int p, int sphere)
{
	int k;

	assert(!required[p]);
	required[p] = true;
	required_location[nrequired] = (uint16_t)p;
	required_sphere[nrequired++] = (uint16_t)sphere;
	for (k = 0; k < nrequired_keys && required_keys[k] != sphere; k++)
		;
	if (k == nrequired_keys)
		required_keys[nrequired_keys++] = (uint16_t)sphere;
}

/* VT's `break 2` leaves only the two inner loops: "put all higher items back" still runs for that
 * check sphere, and the later check spheres are skipped because p is now required. Keep it so. */
static void bubble(const RandoWorld *world, int p, int sphere)
{
	RandoItems items;
	int nkeys = nrequired_keys, k, r, nr;

	for (k = nkeys - 1; k >= 0; k--) {
		int check = required_keys[k];

		if (check == sphere || required[p])
			continue;
		for (r = 0; r < nrequired; r++)
			if (required_sphere[r] >= check)
				set_shadow_item(required_location[r], RANDO_ITEM_NONE);
		nr = nrequired;
		for (r = 0; r < nr; r++) {
			int q = required_location[r];
			RandoItemId temp;

			if (required_sphere[r] != check)
				continue;
			temp = shadow_item(q);
			set_shadow_item(q, RANDO_ITEM_NONE);
			collectable_items(&items);
			if (!can_access(world, world, q, &items)) {
				set_shadow_item(p, real_item(world, p));
				add_required(p, sphere);
				break;
			}
			set_shadow_item(q, temp);
		}
		for (r = 0; r < nrequired; r++)
			set_shadow_item(required_location[r], real_item(world, required_location[r]));
	}
}

static void prune(const RandoWorld *world)
{
	const RandoConfig *c = &world->config;
	RandoItems items;
	int sphere, i;

	memset(required, 0, sizeof required);
	nrequired = nrequired_keys = 0;
	for (sphere = nspheres - 1; sphere >= 1; sphere--) {
		for (i = sphere_start[sphere]; i < sphere_start[sphere + 1]; i++) {
			int p = sphere_locations[i];
			RandoItemId pulled = shadow_item(p);
			RandoItemClass item_class;

			if (pulled == RANDO_ITEM_NONE)
				continue;
			set_shadow_item(p, RANDO_ITEM_NONE);
			item_class = rando_item_info(pulled)->item_class;
			if (((item_class == RANDO_ITEM_CLASS_MAP || item_class == RANDO_ITEM_CLASS_COMPASS)
			     && rando_dungeon_item_confined(c, item_class))
			    || is_junk(pulled))
				continue;
			collectable_items(&items);
			if (!rando_world_win_condition(&shadow, &items)) {
				set_shadow_item(p, real_item(world, p));
				add_required(p, sphere);
				continue;
			}
			bubble(world, p, sphere);
		}
	}
}

static bool round_skips(const RandoConfig *c, RandoItemId item)
{
	RandoItemClass item_class = rando_item_info(item)->item_class;

	return ((c->rom_generic_keys || !c->region_wild_keys) && item_class == RANDO_ITEM_CLASS_KEY)
		|| item_class == RANDO_ITEM_CLASS_MAP || item_class == RANDO_ITEM_CLASS_COMPASS
		|| item == RANDO_ITEM_RescueZelda;
}

static bool is_trade(int p)
{
	return !is_shop(p) && rando_location_info((RandoLocationId)p)->location_class == RANDO_LOCATION_CLASS_TRADE;
}

static void push_entry(RandoPlaythrough *out, int round, int p)
{
	RandoPlaythroughEntry *e;

	assert(out->nentries < RANDO_PLAYTHROUGH_ENTRY_MAX);
	e = &out->entries[out->nentries++];
	e->round = (uint16_t)round;
	e->location = is_shop(p) ? RANDO_LOCATION_NONE : (RandoLocationId)p;
	e->shop = is_shop(p) ? shop_of(p) : RANDO_SHOP_NONE;
	e->slot = (uint8_t)(is_shop(p) ? slot_of(p) : 0);
	e->item = shadow_item(p);
}

const char *rando_playthrough_entry_region(const RandoPlaythroughEntry *e)
{
	RandoRegionId region = e->location == RANDO_LOCATION_NONE ? rando_shop_info(e->shop)->region
								  : rando_location_info(e->location)->region;

	return rando_region_info(region)->spoiler_name;
}

/* VT counts the rounds, then takes one off for each round left with no entry once Trade locations
 * are filtered: that is the number of rounds holding an entry. */
static void count_rounds_and_regions(RandoPlaythrough *out)
{
	int i, j;

	out->longest_item_chain = 0;
	out->regions_visited = out->nequipped > 0;
	for (i = 0; i < out->nentries; i++) {
		const RandoPlaythroughEntry *e = &out->entries[i];

		if (i == 0 || out->entries[i - 1].round != e->round)
			out->longest_item_chain++;
		for (j = i - 1; j >= 0 && out->entries[j].round == e->round; j--)
			if (strcmp(rando_playthrough_entry_region(&out->entries[j]), rando_playthrough_entry_region(e)) == 0)
				break;
		out->regions_visited += j < 0 || out->entries[j].round != e->round;
	}
}

static void replay(const RandoWorld *world, RandoPlaythrough *out)
{
	const RandoConfig *c = &world->config;
	RandoItems my_items = shadow.pre_collected, found;
	int longest = 1, round_size = 0, p, nfound;
	bool available[PSEUDO_COUNT];

	memset(in_order, 0, sizeof in_order);
	out->nentries = 0;
	do {
		if (round_size > 0)
			longest++;
		round_size = 0;
		nfound = 0;
		rando_items_clear(&found);
		for (p = 0; p < PSEUDO_COUNT; p++) {
			available[p] = is_collectable(p) && shadow_item(p) != RANDO_ITEM_NONE
				       && !in_order[p] && can_access(world, world, p, &my_items);
			if (available[p]) {
				rando_items_add(&found, shadow_item(p));
				nfound++;
			}
		}
		for (p = 0; p < PSEUDO_COUNT; p++) {
			if (!available[p])
				continue;
			in_order[p] = true;
			if (round_skips(c, shadow_item(p)))
				continue;
			round_size++;
			if (!is_trade(p))
				push_entry(out, longest, p);
		}
		rando_items_merge(&my_items, &my_items, &found);
	} while (nfound > 0);
	count_rounds_and_regions(out);
}

void rando_playthrough(const RandoWorld *world, RandoPlaythrough *out)
{
	int i, p;

	rando_world_copy(&shadow, world);
	memcpy(shadow_shop_items, shadow.shop_items, sizeof shadow_shop_items);
	for (p = 0; p < RANDO_LOCATION_COUNT; p++)
		if (is_junk(shadow_item(p)))
			set_shadow_item(p, RANDO_ITEM_NONE);
	build_spheres(world);
	prune(world);

	out->nequipped = 0;
	for (i = 0; i < rando_world_pre_collected_count(&shadow); i++) {
		RandoItemId item = rando_world_pre_collected_item(&shadow, i);
		RandoItemClass c = rando_item_info(item)->item_class;

		if (c != RANDO_ITEM_CLASS_UPGRADE_ARROW && c != RANDO_ITEM_CLASS_UPGRADE_BOMB
		    && c != RANDO_ITEM_CLASS_HEALTH && c != RANDO_ITEM_CLASS_EVENT)
			out->equipped[out->nequipped++] = item;
	}
	replay(world, out);
}
