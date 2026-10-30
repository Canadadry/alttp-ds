#ifndef RANDO_CONFIG_H
#define RANDO_CONFIG_H

#include <stdbool.h>

/* UNSET is VT's absent key (null): `=== 'basic'` and `=== 'advanced'` are then both false. */
typedef enum RandoItemPlacement {
	RANDO_PLACEMENT_UNSET,
	RANDO_PLACEMENT_BASIC,
	RANDO_PLACEMENT_ADVANCED,
} RandoItemPlacement;

typedef enum RandoWeapons {
	RANDO_WEAPONS_UNSET,
	RANDO_WEAPONS_RANDOMIZED,
	RANDO_WEAPONS_ASSURED,
	RANDO_WEAPONS_VANILLA,
	RANDO_WEAPONS_SWORDLESS,
} RandoWeapons;

typedef enum RandoState {
	RANDO_STATE_UNSET,
	RANDO_STATE_STANDARD,
	RANDO_STATE_OPEN,
	RANDO_STATE_INVERTED,
	RANDO_STATE_RETRO,
} RandoState;

typedef enum RandoEnemyHealth {
	RANDO_ENEMY_HEALTH_DEFAULT,
	RANDO_ENEMY_HEALTH_EASY,
	RANDO_ENEMY_HEALTH_HARD,
	RANDO_ENEMY_HEALTH_EXPERT,
} RandoEnemyHealth;

/* One field per VT world config key the ported code reads, holding VT's value or, when the key
 * is absent, the default the reading code passes to config(). */
typedef struct RandoConfig {
	RandoState mode_state;
	RandoWeapons mode_weapons;
	RandoItemPlacement item_placement;
	RandoEnemyHealth enemizer_enemy_health;
	bool ignore_can_kill_escape_things;
	bool rom_catchable_fairies;
	bool rom_rupee_bow;
	int rom_bottle_fill_magic;
} RandoConfig;

/* Every key absent, as in a World built with no config. */
void rando_config_init(RandoConfig *config);

#endif
