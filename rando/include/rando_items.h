#ifndef RANDO_ITEMS_H
#define RANDO_ITEMS_H

#include "rando_config.h"

#include <stdbool.h>
#include <stdint.h>

/* VT's Item::all order, the three aliases last. */
typedef enum RandoItemId {
	RANDO_ITEM_Nothing,
	RANDO_ITEM_L1Sword,
	RANDO_ITEM_L1SwordAndShield,
	RANDO_ITEM_L2Sword,
	RANDO_ITEM_MasterSword,
	RANDO_ITEM_L3Sword,
	RANDO_ITEM_L4Sword,
	RANDO_ITEM_BlueShield,
	RANDO_ITEM_RedShield,
	RANDO_ITEM_MirrorShield,
	RANDO_ITEM_FireRod,
	RANDO_ITEM_IceRod,
	RANDO_ITEM_Hammer,
	RANDO_ITEM_Hookshot,
	RANDO_ITEM_Bow,
	RANDO_ITEM_Boomerang,
	RANDO_ITEM_Powder,
	RANDO_ITEM_Bee,
	RANDO_ITEM_Bombos,
	RANDO_ITEM_Ether,
	RANDO_ITEM_Quake,
	RANDO_ITEM_Lamp,
	RANDO_ITEM_Shovel,
	RANDO_ITEM_OcarinaInactive,
	RANDO_ITEM_CaneOfSomaria,
	RANDO_ITEM_Bottle,
	RANDO_ITEM_PieceOfHeart,
	RANDO_ITEM_CaneOfByrna,
	RANDO_ITEM_Cape,
	RANDO_ITEM_MagicMirror,
	RANDO_ITEM_PowerGlove,
	RANDO_ITEM_TitansMitt,
	RANDO_ITEM_BookOfMudora,
	RANDO_ITEM_Flippers,
	RANDO_ITEM_MoonPearl,
	RANDO_ITEM_Crystal,
	RANDO_ITEM_BugCatchingNet,
	RANDO_ITEM_BlueMail,
	RANDO_ITEM_RedMail,
	RANDO_ITEM_Key,
	RANDO_ITEM_Compass,
	RANDO_ITEM_HeartContainerNoAnimation,
	RANDO_ITEM_Bomb,
	RANDO_ITEM_ThreeBombs,
	RANDO_ITEM_Mushroom,
	RANDO_ITEM_RedBoomerang,
	RANDO_ITEM_BottleWithRedPotion,
	RANDO_ITEM_BottleWithGreenPotion,
	RANDO_ITEM_BottleWithBluePotion,
	RANDO_ITEM_RedPotion,
	RANDO_ITEM_GreenPotion,
	RANDO_ITEM_BluePotion,
	RANDO_ITEM_TenBombs,
	RANDO_ITEM_BigKey,
	RANDO_ITEM_Map,
	RANDO_ITEM_OneRupee,
	RANDO_ITEM_FiveRupees,
	RANDO_ITEM_TwentyRupees,
	RANDO_ITEM_PendantOfCourage,
	RANDO_ITEM_PendantOfWisdom,
	RANDO_ITEM_PendantOfPower,
	RANDO_ITEM_BowAndArrows,
	RANDO_ITEM_BowAndSilverArrows,
	RANDO_ITEM_BottleWithBee,
	RANDO_ITEM_BottleWithFairy,
	RANDO_ITEM_BossHeartContainer,
	RANDO_ITEM_HeartContainer,
	RANDO_ITEM_OneHundredRupees,
	RANDO_ITEM_FiftyRupees,
	RANDO_ITEM_Heart,
	RANDO_ITEM_Arrow,
	RANDO_ITEM_TenArrows,
	RANDO_ITEM_SmallMagic,
	RANDO_ITEM_ThreeHundredRupees,
	RANDO_ITEM_TwentyRupees2,
	RANDO_ITEM_BottleWithGoldBee,
	RANDO_ITEM_OcarinaActive,
	RANDO_ITEM_PegasusBoots,
	RANDO_ITEM_BombUpgrade5,
	RANDO_ITEM_BombUpgrade10,
	RANDO_ITEM_BombUpgrade50,
	RANDO_ITEM_ArrowUpgrade5,
	RANDO_ITEM_ArrowUpgrade10,
	RANDO_ITEM_ArrowUpgrade70,
	RANDO_ITEM_HalfMagic,
	RANDO_ITEM_QuarterMagic,
	RANDO_ITEM_Programmable1,
	RANDO_ITEM_Programmable2,
	RANDO_ITEM_Programmable3,
	RANDO_ITEM_SilverArrowUpgrade,
	RANDO_ITEM_Rupoor,
	RANDO_ITEM_RedClock,
	RANDO_ITEM_BlueClock,
	RANDO_ITEM_GreenClock,
	RANDO_ITEM_ProgressiveSword,
	RANDO_ITEM_ProgressiveShield,
	RANDO_ITEM_ProgressiveArmor,
	RANDO_ITEM_ProgressiveGlove,
	RANDO_ITEM_singleRNG,
	RANDO_ITEM_multiRNG,
	RANDO_ITEM_ProgressiveBow,
	RANDO_ITEM_ProgressiveBowAlternate,
	RANDO_ITEM_Triforce,
	RANDO_ITEM_PowerStar,
	RANDO_ITEM_TriforcePiece,
	RANDO_ITEM_MapLW,
	RANDO_ITEM_MapDW,
	RANDO_ITEM_MapA2,
	RANDO_ITEM_MapD7,
	RANDO_ITEM_MapD4,
	RANDO_ITEM_MapP3,
	RANDO_ITEM_MapD5,
	RANDO_ITEM_MapD3,
	RANDO_ITEM_MapD6,
	RANDO_ITEM_MapD1,
	RANDO_ITEM_MapD2,
	RANDO_ITEM_MapA1,
	RANDO_ITEM_MapP2,
	RANDO_ITEM_MapP1,
	RANDO_ITEM_MapH1,
	RANDO_ITEM_MapH2,
	RANDO_ITEM_CompassA2,
	RANDO_ITEM_CompassD7,
	RANDO_ITEM_CompassD4,
	RANDO_ITEM_CompassP3,
	RANDO_ITEM_CompassD5,
	RANDO_ITEM_CompassD3,
	RANDO_ITEM_CompassD6,
	RANDO_ITEM_CompassD1,
	RANDO_ITEM_CompassD2,
	RANDO_ITEM_CompassA1,
	RANDO_ITEM_CompassP2,
	RANDO_ITEM_CompassP1,
	RANDO_ITEM_CompassH1,
	RANDO_ITEM_CompassH2,
	RANDO_ITEM_BigKeyA2,
	RANDO_ITEM_BigKeyD7,
	RANDO_ITEM_BigKeyD4,
	RANDO_ITEM_BigKeyP3,
	RANDO_ITEM_BigKeyD5,
	RANDO_ITEM_BigKeyD3,
	RANDO_ITEM_BigKeyD6,
	RANDO_ITEM_BigKeyD1,
	RANDO_ITEM_BigKeyD2,
	RANDO_ITEM_BigKeyA1,
	RANDO_ITEM_BigKeyP2,
	RANDO_ITEM_BigKeyP1,
	RANDO_ITEM_BigKeyH1,
	RANDO_ITEM_BigKeyH2,
	RANDO_ITEM_KeyH2,
	RANDO_ITEM_KeyH1,
	RANDO_ITEM_KeyP1,
	RANDO_ITEM_KeyP2,
	RANDO_ITEM_KeyA1,
	RANDO_ITEM_KeyD2,
	RANDO_ITEM_KeyD1,
	RANDO_ITEM_KeyD6,
	RANDO_ITEM_KeyD3,
	RANDO_ITEM_KeyD5,
	RANDO_ITEM_KeyP3,
	RANDO_ITEM_KeyD4,
	RANDO_ITEM_KeyD7,
	RANDO_ITEM_KeyA2,
	RANDO_ITEM_KeyGK,
	RANDO_ITEM_Crystal1,
	RANDO_ITEM_Crystal2,
	RANDO_ITEM_Crystal3,
	RANDO_ITEM_Crystal4,
	RANDO_ITEM_Crystal5,
	RANDO_ITEM_Crystal6,
	RANDO_ITEM_Crystal7,
	RANDO_ITEM_RescueZelda,
	RANDO_ITEM_DefeatAgahnim,
	RANDO_ITEM_BigRedBomb,
	RANDO_ITEM_DefeatAgahnim2,
	RANDO_ITEM_DefeatGanon,
	RANDO_ITEM_UncleSword,
	RANDO_ITEM_ShopKey,
	RANDO_ITEM_ShopArrow,
	RANDO_ITEM_COUNT,
	RANDO_ITEM_NONE = RANDO_ITEM_COUNT,
} RandoItemId;

/* One value per concrete VT item class; every VT item class extends Item directly. */
typedef enum RandoItemClass {
	RANDO_ITEM_CLASS_PLAIN,
	RANDO_ITEM_CLASS_SWORD,
	RANDO_ITEM_CLASS_SHIELD,
	RANDO_ITEM_CLASS_BOW,
	RANDO_ITEM_CLASS_BOTTLE_CONTENTS,
	RANDO_ITEM_CLASS_MEDALLION,
	RANDO_ITEM_CLASS_BOTTLE,
	RANDO_ITEM_CLASS_HEALTH,
	RANDO_ITEM_CLASS_CRYSTAL,
	RANDO_ITEM_CLASS_ARMOR,
	RANDO_ITEM_CLASS_KEY,
	RANDO_ITEM_CLASS_COMPASS,
	RANDO_ITEM_CLASS_BIG_KEY,
	RANDO_ITEM_CLASS_MAP,
	RANDO_ITEM_CLASS_PENDANT,
	RANDO_ITEM_CLASS_ARROW,
	RANDO_ITEM_CLASS_UPGRADE_BOMB,
	RANDO_ITEM_CLASS_UPGRADE_ARROW,
	RANDO_ITEM_CLASS_UPGRADE_MAGIC,
	RANDO_ITEM_CLASS_PROGRAMMABLE,
	RANDO_ITEM_CLASS_EVENT,
	RANDO_ITEM_CLASS_ALIAS,
	RANDO_ITEM_CLASS_COUNT,
} RandoItemClass;

typedef struct RandoItemInfo {
	const char *name;
	RandoItemClass item_class;
	uint8_t power_quarters;
	RandoItemId alias_target;
	bool key_prefixed;
} RandoItemInfo;

/* Asserts id < RANDO_ITEM_COUNT. */
const RandoItemInfo *rando_item_info(RandoItemId id);
/* Raw names only; RANDO_ITEM_NONE when unknown. */
RandoItemId rando_item_find(const char *name);

/* VT's ItemCollection keeps two views that can disagree: counts is item_counts (has, count),
 * values is cached_values as a multiset (every filter-based predicate). unset and diff touch
 * counts only; merge adds the argument's counts to both views. */
typedef struct RandoItems {
	uint16_t counts[RANDO_ITEM_COUNT];
	uint16_t values[RANDO_ITEM_COUNT];
} RandoItems;

void rando_items_clear(RandoItems *items);
/* Item::all(world)->copy()->manyKeys(): VT's allItems base. */
void rando_items_all(RandoItems *items);
void rando_items_add(RandoItems *items, RandoItemId id);
/* removeItem: a no-op when the item has no count. */
void rando_items_remove(RandoItems *items, RandoItemId id);
/* offsetUnset. */
void rando_items_unset(RandoItems *items, RandoItemId id);
/* out may alias a. */
void rando_items_merge(RandoItems *out, const RandoItems *a, const RandoItems *b);
/* out may alias a. */
void rando_items_diff(RandoItems *out, const RandoItems *a, const RandoItems *b);

bool rando_items_has(const RandoItems *items, RandoItemId id);
/* RANDO_CONFIG_NULL is VT's has($key, null): false. */
bool rando_items_has_n(const RandoItems *items, RandoItemId id, int at_least);
int rando_items_count(const RandoItems *items);

/* A NULL config is VT's call with no world. */
bool rando_items_can_lift_rocks(const RandoItems *items);
bool rando_items_can_lift_dark_rocks(const RandoItems *items);
bool rando_items_can_light_torches(const RandoItems *items);
bool rando_items_can_melt_things(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_fly(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_spin_speed(const RandoItems *items);
bool rando_items_can_acquire_fairy(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_bunny_revive(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_shoot_arrows(const RandoItems *items, const RandoConfig *config, int min_level);
bool rando_items_can_block_lasers(const RandoItems *items);
bool rando_items_can_extend_magic(const RandoItems *items, const RandoConfig *config, int bars);
bool rando_items_glitched_link_in_dark_world(const RandoItems *items);
bool rando_items_has_health(const RandoItems *items, int minimum_hearts);
/* In quarter hearts, initial included. */
int rando_items_heart_count(const RandoItems *items, int initial_quarters);
bool rando_items_can_kill_escape_things(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_kill_most_things(const RandoItems *items, const RandoConfig *config, int enemies);
bool rando_items_can_bomb_things(const RandoItems *items);
bool rando_items_can_get_good_bee(const RandoItems *items);
bool rando_items_has_sword(const RandoItems *items, int min_level);
bool rando_items_has_armor(const RandoItems *items, int min_level);
bool rando_items_has_bottle(const RandoItems *items, int at_least);
int rando_items_bottle_count(const RandoItems *items);
bool rando_items_has_a_bottle(const RandoItems *items);

#endif
