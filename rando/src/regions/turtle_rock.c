#include "../world_rules.h"

static bool upper(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((rando_world_has_item(world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Bombos)
		 && has(items, RANDO_ITEM_Bombos))
		|| (rando_world_has_item(world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Ether)
		    && has(items, RANDO_ITEM_Ether))
		|| (rando_world_has_item(world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake)
		    && has(items, RANDO_ITEM_Quake)))
		&& (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (c->can_owyba && rando_items_has_a_bottle(items)
			&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)))
		&& has(items, RANDO_ITEM_CaneOfSomaria)
		&& ((has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_dark_rocks(items)
		     && can_enter(world, RANDO_REGION_EAST_DEATH_MOUNTAIN, items))
		    || ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow));
}

static bool middle(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (((c->can_mirror_clip && has(items, RANDO_ITEM_MagicMirror))
		 && (has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive))
		|| ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    && ((c->can_owyba && rando_items_has_a_bottle(items)) || has(items, RANDO_ITEM_MoonPearl)))
		|| (c->can_super_speed && has(items, RANDO_ITEM_MoonPearl) && rando_items_can_spin_speed(items))
		|| (c->can_one_frame_clip_ow
		    && (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
			|| (c->can_owyba && rando_items_has_a_bottle(items)))))
		&& (has(items, RANDO_ITEM_PegasusBoots) || has(items, RANDO_ITEM_CaneOfSomaria)
		    || has(items, RANDO_ITEM_Hookshot) || !c->region_cant_take_damage
		    || has(items, RANDO_ITEM_Cape) || has(items, RANDO_ITEM_CaneOfByrna))
		&& can_enter(world, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items);
}

static bool lower(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
		&& (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& ((((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)
		     && can_enter(world, RANDO_REGION_WEST_DEATH_MOUNTAIN, items))
		    || ((c->can_super_speed && rando_items_can_spin_speed(items))
			&& can_enter(world, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items)));
}

static bool enter(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (lower(world, items) || middle(world, items) || upper(world, items));
}

const RandoRegionRules rando_rules_turtle_rock = {
	"Region\\Standard\\TurtleRock", enter, NULL, NULL, NULL,
};
