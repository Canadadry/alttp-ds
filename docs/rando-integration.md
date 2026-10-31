# Randomizer integration — grilling notes

Where the design of "wire the C port of VT into the game" stands. Decisions are settled; open
questions are still being discussed. Not a plan yet: `plan-write` turns it into one.

## Decisions

### Scope

- Everything the generator draws must be true in game: item placement, dungeon prizes,
  medallions, crystal requirements, shops, fountains, prize packs, and the game rules each option
  changes (progressive items, Triforce pieces, keysanity, Ganon's vulnerability, Retro keys…).
- All states except **Inverted**, dropped for now (the overworld map rework is a project of its
  own).
- The generator runs on the DS, inside `lttp-ds.nds`. No patched ROM, no PC tool.

### Reference for game behavior

- The generator decides; zelda3 must know what to do with items and rules vanilla lacks
  (progressive sword, Triforce pieces, foreign dungeon keys, Ganon's crystal check, Retro keys…).
- That code is written in C in zelda3, following **z3randomizer**, the 65816 base patch an
  alttpr.com ROM carries: read its source and translate it rule by rule, so the DS game behaves
  like the randomizer players know. Pin the z3randomizer commit that matches VT `219fcafd`.

### New game

- Every new game is randomized. No Vanilla/Randomizer choice and no options menu for now; the menu
  comes later.
- Config: the `baseline` case, VT's `default` preset (Open, no glitches, advanced placement,
  standard dungeon items, items accessibility, goal ganon, crystals 7/7, randomized weapons,
  normal pool and functionality).
- Seed: fixed, 1 — the `baseline` golden, so the game can be checked against
  `tests/rando/cases/baseline/spoiler.json`.
- The other options are in scope: implemented in game one by one and covered by tests, selectable
  only once the menu exists.
- Old vanilla saves are not supported: this build is the randomizer.

### Texts and hints

- Vanilla texts everywhere for now. Hints, Sahasrahla, pedestal and tablet texts are later work
  (`todo.md`).

### zelda3 is vendored

- `vendor/zelda3` stops being a submodule of `JimmyGlasscock/zelda3` (no push access): its code is
  copied into this repository, so engine changes are committed here.

### Tests

- A headless host harness: zelda3's game logic built by gcc, no SDL, no window, no assets. Tests
  call the item-giving routines and the ported rules directly with the little data they need.
  Not "launching the game".
- No ROM in agent sessions or CI, so nothing reads `zelda3_assets.dat`. Code is shaped so that
  "which item to give" is separate from the code that reads the room.
- Rendering (sprite, text, sound) is checked by a human on the DS.
- A test must not read the same table as the code it checks (no tautology): each check needs a
  source independent of our mapping.

### Save

- The placement table is generated once on the DS when the file is created and stored in the
  slot. Loading a game costs no generation.
- Three slots stay; no mirror is dropped and the 8 KB EEPROM is unchanged (Plan 1 picked it
  because emulators assume it for an unknown game code).
- The data fits in the zelda3 slot's free bytes, already under its checksum and mirror.
- z3randomizer's own save fields keep z3randomizer's offsets (`sram.asm`), so the ASM→C
  translation stays direct; the placement table goes in the remaining holes.
- A test fails if the encoded slot data outgrows its space. The encoding (1 byte per location or
  an alphabet index) is chosen at implementation.

## Facts measured

### Generation time (`make -C tests/rando measure`, 256-seed sweep)

| | Host -O2 | DS, projected (host × 150) |
|---|---|---|
| median | 4.5 ms | ~0.7 s |
| max (sweep) | 14 ms | ~2.1 s |
| slowest case (`item_placement-basic`) | 26 ms | ~4 s |

- The ×150 factor is an assumption (A-3 of the previous plan), never measured on a DS.
- No retry on 256 seeds (every one succeeds at attempt 0).
- The generator was never profiled: speeding it up is worth doing anyway.

### Save space

- EEPROM 8 KB: three slots of 0x500 bytes and their mirrors fill `0x0000–0x1DFF`; DS settings at
  `0x1E00–0x1FFF` use ~183 of 512 bytes.
- Free inside a zelda3 slot (`variables.h`): `0x407–0x4FD` (247 bytes, after `death_var2`, before
  the checksum at `0x4FE`) and `0x38C–0x3C4` (57 bytes, unused by vanilla). About 304 bytes.

### What a slot must hold

Placement: 226 item locations in Open/Standard (from the 43 non-Inverted goldens), at most 103
distinct items per case.

| Encoding | Bytes |
|---|---|
| 1 byte per location (VT item id) | 226 |
| index into the case's item alphabet (~6.7 bits) | ~189 |
| permutation of the case's known pool (optimal, needs the pool rebuilt on load) | ~140 |

The rest of the draw:

| Data | How | Bits |
|---|---|---|
| Misery Mire and Turtle Rock medallions | 3 × 3 | 4 |
| Two fountain bottles | 7 kinds each | 6 |
| Prize packs | permutation of VT's 7 vanilla packs | 13 |
| Shops outside Retro | no draw, rebuilt from the options | 0 |
| Retro shops | 4 take-any caves of 31, the old man among the 27 left, 5 sellers of 9 shops | 27 |
| Options (no Inverted, random crystals stored resolved) | 11 options | ~25 |
| Seed | `uint32` | 32 |
| Total | | ~107 (~14 bytes) |

z3randomizer's save fields the game needs (stats excluded): ~60 bytes —
InventoryTracking, BowTracking, ItemLimitCounts (`0x38C–0x39F`); Aga2Duck, NpcFlags,
GeneralFlags, HighestSword/Shield, GoalCounter (`0x40F–0x422`); HighestMail (`0x46E`); keysanity
key counters (`0x4B0–0x4ED`); map and compass displays (`0x403`, `0x414`, `0x474`).

| Total per slot | 1 byte/location | alphabet | optimal |
|---|---|---|---|
| bytes used | 300 | 263 | 214 |
| margin out of ~304 | 4 | 41 | 90 |

### Where zelda3 gives items

- **Chests:** one table, `kDungeonRoomChests` (asset 8), copied verbatim from the ROM at
  `0x1E96E` by `assets/extract_resources.py:288`: 168 entries of 3 bytes (room word with the big
  chest bit, item). Read only at `vendor/zelda3/src/dungeon.c:5787` (`data = chest_data[2]`).
- **Everything else:** hardcoded `Link_ReceiveItem(<id>, 0)` calls (NPCs, bosses, pedestal…), 31 in
  `sprite_main.c`, `ancilla.c`, `player.c`, `sprite.c`, plus overworld items.
- zelda3 functions carry their original SNES address in a comment
  (`void Sprite_Sahasrahla(int k) { // 85f14d`), so a VT location address points at the zelda3
  function that gives that location's item.

## Open questions

- **Location ↔ game mapping and how to trust it.** The generator says "Eastern Palace - Big
  Chest → Hookshot"; the game knows "room 0xA9, chest 0". 226 lines to map. Candidates, not
  decided:
  1. by hand, checked by a human on the DS;
  2. against VT's location addresses, extracted from VT's source by a script (like
     `extract_logic.php`): independent for the ~170 chests, a weaker pointer to the zelda3
     function for the ~55 others;
  3. against vanilla contents (zelda3's chest table vs VT's vanilla item per location): needs the
     ROM's assets and a vanilla table VT lacks;
  4. invariants only: each location mapped once, big chests on big chests, chest counts per room.
- Real generation time on a DS: a boot screen printing it would settle the ×150 assumption.
- Save encoding: 1 byte per location or alphabet index.
