#include "world_rules.h"

#include <assert.h>
#include <stddef.h>

#define NO_RULES(id, cls) static const RandoRegionRules id = {cls, NULL, NULL, NULL, NULL}

NO_RULES(north_east_light_world, "Region\\Standard\\LightWorld\\NorthEast");
NO_RULES(north_west_light_world, "Region\\Standard\\LightWorld\\NorthWest");
NO_RULES(south_light_world, "Region\\Standard\\LightWorld\\South");
NO_RULES(standard_escape, "Region\\Standard\\HyruleCastleEscape");
NO_RULES(open_escape, "Region\\Open\\HyruleCastleEscape");
NO_RULES(eastern_palace, "Region\\Standard\\EasternPalace");
NO_RULES(desert_palace, "Region\\Standard\\DesertPalace");
NO_RULES(west_death_mountain, "Region\\Standard\\LightWorld\\DeathMountain\\West");
NO_RULES(east_death_mountain, "Region\\Standard\\LightWorld\\DeathMountain\\East");
NO_RULES(tower_of_hera, "Region\\Standard\\TowerOfHera");
NO_RULES(hyrule_castle_tower, "Region\\Standard\\HyruleCastleTower");
NO_RULES(east_dark_world_death_mountain, "Region\\Standard\\DarkWorld\\DeathMountain\\East");
NO_RULES(west_dark_world_death_mountain, "Region\\Standard\\DarkWorld\\DeathMountain\\West");
NO_RULES(north_east_dark_world, "Region\\Standard\\DarkWorld\\NorthEast");
NO_RULES(north_west_dark_world, "Region\\Standard\\DarkWorld\\NorthWest");
NO_RULES(south_dark_world, "Region\\Standard\\DarkWorld\\South");
NO_RULES(mire, "Region\\Standard\\DarkWorld\\Mire");
NO_RULES(palace_of_darkness, "Region\\Standard\\PalaceOfDarkness");
NO_RULES(swamp_palace, "Region\\Standard\\SwampPalace");
NO_RULES(skull_woods, "Region\\Standard\\SkullWoods");
NO_RULES(thieves_town, "Region\\Standard\\ThievesTown");
NO_RULES(ice_palace, "Region\\Standard\\IcePalace");
NO_RULES(misery_mire, "Region\\Standard\\MiseryMire");
NO_RULES(turtle_rock, "Region\\Standard\\TurtleRock");
NO_RULES(ganons_tower, "Region\\Standard\\GanonsTower");
NO_RULES(medallions, "Region\\Standard\\Medallions");
NO_RULES(fountains, "Region\\Standard\\Fountains");

#define SHARED_REGIONS \
	[RANDO_REGION_NORTH_EAST_LIGHT_WORLD] = &north_east_light_world, \
	[RANDO_REGION_NORTH_WEST_LIGHT_WORLD] = &north_west_light_world, \
	[RANDO_REGION_SOUTH_LIGHT_WORLD] = &south_light_world, \
	[RANDO_REGION_EASTERN_PALACE] = &eastern_palace, \
	[RANDO_REGION_DESERT_PALACE] = &desert_palace, \
	[RANDO_REGION_WEST_DEATH_MOUNTAIN] = &west_death_mountain, \
	[RANDO_REGION_EAST_DEATH_MOUNTAIN] = &east_death_mountain, \
	[RANDO_REGION_TOWER_OF_HERA] = &tower_of_hera, \
	[RANDO_REGION_HYRULE_CASTLE_TOWER] = &hyrule_castle_tower, \
	[RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN] = &east_dark_world_death_mountain, \
	[RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN] = &west_dark_world_death_mountain, \
	[RANDO_REGION_NORTH_EAST_DARK_WORLD] = &north_east_dark_world, \
	[RANDO_REGION_NORTH_WEST_DARK_WORLD] = &north_west_dark_world, \
	[RANDO_REGION_SOUTH_DARK_WORLD] = &south_dark_world, \
	[RANDO_REGION_MIRE] = &mire, \
	[RANDO_REGION_PALACE_OF_DARKNESS] = &palace_of_darkness, \
	[RANDO_REGION_SWAMP_PALACE] = &swamp_palace, \
	[RANDO_REGION_SKULL_WOODS] = &skull_woods, \
	[RANDO_REGION_THIEVES_TOWN] = &thieves_town, \
	[RANDO_REGION_ICE_PALACE] = &ice_palace, \
	[RANDO_REGION_MISERY_MIRE] = &misery_mire, \
	[RANDO_REGION_TURTLE_ROCK] = &turtle_rock, \
	[RANDO_REGION_GANONS_TOWER] = &ganons_tower, \
	[RANDO_REGION_MEDALLIONS] = &medallions, \
	[RANDO_REGION_FOUNTAINS] = &fountains

static const RandoRegionRules *const variants[RANDO_VARIANT_COUNT][RANDO_REGION_COUNT] = {
	[RANDO_VARIANT_STANDARD] = {SHARED_REGIONS, [RANDO_REGION_ESCAPE] = &standard_escape},
	[RANDO_VARIANT_OPEN] = {SHARED_REGIONS, [RANDO_REGION_ESCAPE] = &open_escape},
};

const RandoRegionRules *rando_region_rules(RandoVariant variant, RandoRegionId region)
{
	assert(variant < RANDO_VARIANT_COUNT && region < RANDO_REGION_COUNT);
	return variants[variant][region];
}
