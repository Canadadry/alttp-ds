#include "rando_config.h"

void rando_config_init(RandoConfig *config)
{
	config->mode_state = RANDO_STATE_UNSET;
	config->mode_weapons = RANDO_WEAPONS_UNSET;
	config->item_placement = RANDO_PLACEMENT_UNSET;
	config->enemizer_enemy_health = RANDO_ENEMY_HEALTH_DEFAULT;
	config->ignore_can_kill_escape_things = false;
	config->rom_catchable_fairies = true;
	config->rom_rupee_bow = false;
	config->rom_bottle_fill_magic = 0x80;
}
