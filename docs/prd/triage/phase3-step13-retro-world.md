---
title: "Retro: VT's Retro world, its random shops and its bow test"
description: "The C generator returns RANDO_UNSUPPORTED for state retro before world init, so state-retro is the last pending case and VT's RetroBowTest is not ported."
status: needs-triage
---

## Problem Statement

The `state: retro` row (plan Phase 3, step 13) is the last world the C port does not generate. A
player who picks Retro on the DS gets no seed: `rando_generate` stops with `RANDO_UNSUPPORTED`
right after the crystal draws, before world init. `state-retro` is the only case no step has
claimed (relative to `H-17-2`), and the row owes `RetroBowTest::testLocation` (32 rows, step 3.0
note).

Retro is small in VT. `World\Retro` extends `World\Open` and has no region class of its own; its
constructor only:

- **forces four config keys** before the parent constructor runs (`array_merge($config, …)`, so
  they win over the caller's config): `rom.rupeeBow`, `rom.genericKeys`, `region.takeAnys`,
  `region.wildKeys`, all true. `World::factory` sets `mode.state` to `retro`.
- **changes the item counts** after the parent constructor, unless `difficulty` is `custom`:
  normal and crowd control pools get `KeyD1` and `KeyA2` at 0 and `TwentyRupees2` at 10 plus its
  current count (0 when unset, 4 under crowd control); hard and expert pools also get `KeyD7` and
  `KeyD2` at 0, and `TwentyRupees2` at 15 plus its current count.

Most of what those keys switch on is already ported and simply never ran on a generated world:
`canShootArrows`' rupee-bow branch, `has()`'s `ShopKey` rule, `prepareWorld`'s retro branches
(take-any sword removal, arrows replaced by `FiveRupees`, wild keys released; step 2.3.6), the
wild-key rules of the region files and the host playthrough's generic-key round skip. What is not
ported:

- **`setShops`' random path** (`Randomizer::setShops` after the draw-free part). The C function
  asserts on `rom_generic_keys`/`rom_rupee_bow` (step 2.3.2). In `state-retro` it draws 10 times
  (step 1.2 note: `0/4/10/2/0/64/2/7/182/621` per stage): `randomCollection(4)` over the
  Take-Any shops (4 draws), each activated with `BluePotion` and `BossHeartContainer` at 0; one
  `random()` over the Take-Any shops still inactive (1 draw), activated with `ProgressiveSword`
  (or `ThreeHundredRupees` under swordless or vanilla weapons) at 0; `randomCollection(5)` over the
  shops that are neither Take-Any nor Upgrade, excluding the Dark World Lake Hylia Shop in an
  Inverted world (5 draws), each activated with `ShopArrow` at 80 in slot 0 (rupee bow),
  `ShopKey` at 100 in slot 1 (generic keys) and `TenBombs` at 50 in slot 2; then, under rupee
  bow, the Dark World Forest Shop activated with every `Item\Arrow` slot repriced as `ShopArrow`
  at 80, and the Capacity Upgrade cleared, then given `BombUpgrade5` at 100 (max 7) unless the
  shops are in hard mode.
- **The spoiler's generic-key naming** (`World::getSpoiler`): under `rom.genericKeys`, a region
  section names an `Item\Key` (not a big key, not the `ShopKey` alias) `Key`, with no world
  suffix. The golden has 19 such entries. The host writer has a `TODO` for it, and the gate's
  `pool` check, which compares `pool.txt` against the golden's placed items, needs the same naming.
- **The Retro world itself**: no variant, no derivation, and the extractor knows neither the Retro
  world nor the top-level `tests/RetroBowTest.php` (namespace `NoGlitches`).

## Solution

The C port gets VT's Retro world as a third classic-layout variant that shares Open's region
rules, with Retro's forced keys and item counts derived at world init; `setShops`' random path;
and the spoiler's `Key` naming. `RetroBowTest::testLocation` is extracted into the NoGlitches row
fixture and replayed by `make logic`, the Retro world's structure joins the world dump, and a
Retro predicate vector config pins `canShootArrows` under the rupee bow. `state-retro` is claimed
at `all`, and every case of the matrix is then claimed (relative to `H-17-2`).

A scratchpad prototype from committed HEAD (before step 3.12) already does all of it: about 80
lines of library code and 3 of host code, and `state-retro` matches VT at `all` (trace, attempts,
spoiler with playthrough). All 32 RetroBowTest rows, the Retro world dump and the Retro predicate
vectors replay, and every other claim and fixture is unchanged.

## User Stories

1. As a player choosing Retro, I want a seed generated on the DS, so that the Retro mode is
   playable without a PC tool.
2. As a player choosing Retro, I want the same item placement alttpr.com gives for my seed and
   settings, so that I can compare my spoiler with VT's.
3. As a player choosing Retro, I want five shops chosen as VT chooses them to sell arrows, a key
   and bombs, so that my shops match alttpr.com's.
4. As a player choosing Retro, I want four Take-Any caves with a Blue Potion and a heart
   container, and one old man with a sword, so that the caves match VT's.
5. As a player choosing Retro with swordless or vanilla weapons, I want the old man to give 300
   rupees, as VT does, so that no extra sword enters the game.
6. As a player choosing Retro, I want my bow to need bought arrows (or silver arrows) in logic, so
   that the generator never expects me to shoot without them.
7. As a player choosing Retro, I want small keys to be generic and placed anywhere, so that the
   placement follows VT's wild-key logic.
8. As a player choosing Retro, I want the Capacity Upgrade fairy to sell only bomb upgrades, as VT
   does, so that the shop matches alttpr.com's.
9. As a player choosing Retro, I want the pool to have no Palace of Darkness and Ganon's Tower
   small keys and ten more 20-rupee items, so that the pool matches VT's Retro pool.
10. As a player choosing any other mode, I want my seeds unchanged, so that adding Retro breaks
    nothing I already have.
11. As a spoiler reader, I want a key placed in a Retro world to read `Key`, as VT's spoiler does,
    so that the C spoiler and alttpr.com's are the same document.
12. As a maintainer, I want `state-retro` claimed at `all`, so that the e2e gate protects the
    whole Retro generation, playthrough included.
13. As a maintainer, I want every other claim unchanged after every chunk, so that Retro is proven
    not to leak into the other worlds.
14. As a maintainer, I want `RetroBowTest::testLocation`'s 32 rows replayed by `make logic`, each
    replayed by VT from its record before it is written, so that the rupee-bow rules are pinned by
    VT's own test.
15. As a maintainer, I want the Retro world's regions and locations in the world dump, so that a
    Retro world is checked record by record (VT classes, prizes, locations, constructor items)
    against VT's.
16. As a maintainer, I want a Retro predicate vector config, so that `canShootArrows` under the
    rupee bow is checked against VT's values on every item multiset.
17. As a maintainer, I want a hand-written test pinning Retro's derived config (state, the four
    forced flags, the item counts per pool), so that `make test` catches a missing derived value
    the fixtures cannot see.
18. As a maintainer, I want hand-written tests of `setShops`' random path (draw count, which shops
    and slots, prices, the old man's item, the Forest Shop repricing, the Capacity Upgrade, the
    Inverted Lake Hylia exclusion), so that the branches no golden reaches are still pinned.
19. As a maintainer, I want the draw-free `setShops` path unchanged and still drawing nothing, so
    that the 43 other cases keep their traces.
20. As a maintainer, I want the `pool` check to work on a Retro claim, so that the claim can stop
    at `prepareWorld` while the spoiler's key naming is checked.
21. As a maintainer, I want `make test` as fast as it is, with only hand-written cases added, so
    that the loop stays fast (CLAUDE.md).
22. As a maintainer, I want `make e2e` under 0.5 s with the new claim, so that the gate's budget
    holds.
23. As a maintainer, I want `make measure`'s figures recorded, so that the step's size and time are
    weighed against the DS budget.
24. As a maintainer, I want the step note to say whether every matrix case is claimed, so that the
    plan's "none pending" condition is visibly met or not.
25. As a DS integrator (roadmap item 2), I want the Retro world as a variant of the existing const
    tables, with no writable static and no growth of `RandoWorld`, so that A-2 is unaffected.
26. As a DS integrator, I want the generic-key and rupee-bow flags in the world config, so that the
    item-give code can later give a generic key and a rupee bow as the ROM would.

## Implementation Decisions

### The Retro world

- **A new variant, `RANDO_VARIANT_RETRO`**, appended after the Inverted variant step 3.12 adds
  (`H-17-1`). Rejected: generating Retro as an Open world with the flags set by the generator —
  `mode_state` is derived from the variant at world init (VT's `World::factory` sets `mode.state`
  to the type), the spoiler's `meta.mode` reads it, and the fixture replay builds worlds from a
  variant name; a variant keeps "which world is this" in one place, as VT's class does.
- **Layout and rules:** Retro uses the classic layout (`H-17-1`) and Open's region rules: its
  row of the variant rules table is Open's (the shared regions plus Open's escape), as VT's Retro
  has no region class of its own. `rando_region_vt_class` therefore returns Open's classes for it,
  which the world dump checks.
- **Derivation at world init**, transliterating VT's constructor order:
  1. first, before anything reads them, `rom_rupee_bow`, `rom_generic_keys`, `region_take_anys`
     and `region_wild_keys` are set true, overriding the caller's config (VT's `array_merge`
     puts them last); `mode_state` is `RANDO_STATE_RETRO`;
  2. the rest of the derivation runs as for Open (dungeon items, `rom.vanilla*` — false for keys,
     since wild keys are on — item pool and functionality values, hunt goals);
  3. last, Retro's counts: `KeyD1` and `KeyA2` at 0; for the hard and expert pools also `KeyD7`
     and `KeyD2` at 0 and `TwentyRupees2` at 15 plus its current count; for the normal and crowd
     control pools `TwentyRupees2` at 10 plus its current count. An unset count
     (`RANDO_CONFIG_NULL`) counts as 0 there, as VT's `config('item.count.TwentyRupees2', 0)`.
- VT skips the counts when `difficulty` is `custom`. The C port has no `difficulty` (no preset,
  case or VT test sets `custom`); the counts always apply. Recorded, not modelled.
- **The generator** maps `state: retro` to the Retro variant; only `inverted` would still return
  `RANDO_UNSUPPORTED`, and after step 3.12 nothing does (`H-17-2`). `rando_config_from_options`
  is unchanged (it already maps the state; world init re-derives it).
- `region.takeAnys` is not added to the extractor's config keys: no ported rule or predicate reads
  it, only `setShops` and `prepareWorld`, and the hand-written derivation test pins it.

### `setShops`' random path

- `rando_set_shops` takes the generator's PRNG (the draw-free path never draws; its unit tests may
  pass none). The generator already labels the stage `setShops`, so the 10 draws trace as VT's.
- Transliteration of VT, in VT's order, after the existing draw-free part:
  - return when none of generic keys, rupee bow and take-anys is on (every non-Retro case);
  - take-anys: the Take-Any shops in shop-table order (VT's `getShops()` order); 4 chosen by
    `rando_random_collection`, in draw order, each active with slot 0 `BluePotion` and slot 1
    `BossHeartContainer`, both at 0; then the still-inactive Take-Any shops in table order, one
    chosen by one `rando_random_int(0, n - 1)` draw (`Collection::random`), active with slot 0
    `ProgressiveSword` at 0, or `ThreeHundredRupees` under swordless or vanilla weapons;
  - the shops whose kind is plain shop (neither Take-Any nor Upgrade), in table order, less the
    Dark World Lake Hylia Shop when the world is the Inverted variant (VT tests
    `instanceof World\Inverted`, so the C test is on the variant, not on `mode_state`); 5 chosen
    by `rando_random_collection`, each active, slot 0 `ShopArrow` at 80 under rupee bow, slot 1
    `ShopKey` at 100 under generic keys, slot 2 `TenBombs` at 50;
  - under rupee bow: the Dark World Forest Shop active, every filled slot whose item has the
    arrow class (VT's `Item\Arrow`: `Arrow`, `TenArrows`) replaced by `ShopArrow` at 80; the
    Capacity Upgrade cleared, then, unless the shops are in hard mode, slot 0 `BombUpgrade5` at
    100.
- **Not modelled, ROM-only:** the shopkeeper (`setShopkeeper('old_man')`) and the Capacity
  Upgrade's `max` of 7. Neither is in the spoiler or read by logic.
- **Slot order is VT's insertion order** in every Retro path: VT keys inventory by slot and
  appends a new key at the end, and every slot the random path writes is either already present
  (default inventories fill slots 0–2) or written in increasing slot order into an empty or
  cleared inventory. The C slot-indexed arrays therefore list shop items in VT's order, which the
  collectable shop items and the playthrough walk. The prototype's byte-identical spoiler confirms
  it for `state-retro`.
- The existing assert goes. Shop collection needs no change: the C search already walks every
  shop's slots, active or not, as VT's `ShopCollection::getLocations` does, so the Take-Any items
  (the old man's sword included) and the bought `ShopArrow`/`ShopKey` are collected in logic, and
  the shop requirements ported in step 2.2 already cover the Take-Any shops.

### Spoiler and `pool.txt` naming (host only)

- One helper in the host spoiler writer names a placed item as VT's region sections do: `Key`,
  with no world suffix, for an item of the key class when the world has generic keys; otherwise
  the target name with the world suffix. The region sections use it, and so does `pool.txt`: the
  gate's `pool` check compares `pool.txt` with the golden's region-section items, so both must
  use the same names. `rando_spoiler_write_pool` therefore takes the world (its contract comment
  says the names are the region sections'). The `TODO` goes.
- Big keys and the `ShopKey` alias keep their names (VT: `Item\BigKey` does not extend
  `Item\Key`; `ShopKey` is an `ItemAlias`), as do the `Shops` section (VT uses `getName()` there:
  `ShopKey:1`) and the playthrough (also `getName()`, and keys are skipped from its rounds under
  generic keys, already ported).
- Prototype finding: without the naming, a claim at `prepareWorld` is `DIVERGENT` (`pool: golden:
  Key x19, c: Key x0`), while `shufflePrizePacks` passes; this sets the chunk order below.

### Oracle and fixtures

- The extractor: `RetroBowTest.php` added to its top-level test files with `testLocation` (its
  rows go to the NoGlitches row fixture, after the other top-level files' rows, variant field
  `retro`, no config override since the reference world is a Retro world); `retro` appended to its
  world-dump variants (after `inverted`, `H-17-3`); a `retro` predicate vector config (the test
  config on a Retro world) appended after the existing ones, so existing lines stay in place.
- Prototype counts at HEAD (before 3.12): +32 rows (NoGlitches 3 067 → 3 099), +262 world-dump
  records (26 regions, 236 locations, all equal to Open's but the variant), +964 predicate-file
  lines (161 216 predicate values from 120 912); VT agrees with every RetroBowTest row; every
  existing line byte-identical; the extractor ran in ~28 s.
- The C fixture readers accept the variant name `retro` wherever they accept `standard` and
  `open` (row replay, world dump; through the single variant-name parser if 3.12 left one,
  `H-17-4`).
- `gen_golden.sh` is run in the oracle chunk, in its own commit, because the extractor changes on
  purpose; a `--fresh --out` run must leave every golden, the pool fixtures and every existing
  fixture line byte-identical.

### Gate, claims, measurements

- After the `setShops` chunk, `state-retro` is claimed at `shufflePrizePacks` (Shops, medallions,
  bosses, prizes and fountains checked); the naming chunk raises it to `all`. Expected final
  report, relative to `H-17-2`: 44 pass, 0 divergent, 0 pending.
- `test_generate`: its `state-retro` skip goes (both pool fixtures replay Retro's lists: the
  prototype replays all 43 supported cases); `check_states` generates Retro to `prepareWorld`
  (271 trace lines: 0/4/10/2/0/64/2/7/182) with variant and `mode_state` Retro, and matches the
  golden prefix, instead of expecting `RANDO_UNSUPPORTED`.
- Times (prototype, noisy host): `make test` 75–92 ms (unchanged tree: 76–94 ms), `make logic`
  ~0.22 s, `make e2e` 0.17–0.21 s (unchanged tree: ~0.16 s). `make measure`: ARM proxy code+rodata 81 804 B against
  step 3.4's 80.6 KB (+~1.2 KB for the random path), stack peak 6 328 B unchanged, `state-retro`
  3.9 ms on the host with the golden's attempts, draws and per-stage counts. The step note records
  the figures measured after 3.12 (`H-17-5`).
- **Plan bookkeeping:** a `### Step 3.13` note records: Retro as a classic-layout variant on
  Open's rules; the claim; `setShops`' 10 draws; the 32 RetroBowTest rows and the dump/vector
  records added, "VT agrees with every row"; the decisions not modelled (shopkeeper, Capacity
  Upgrade `max`, `difficulty: custom`); whether every matrix case is now claimed at `all`; `make
  test`/`logic`/`e2e` times and `make measure`'s figures against 3.12's. The 3.0 table is not
  edited.

### Hypotheses about step 3.12 (inverted), in flight while this PRD is written

Checked after 3.12 landed: all six hold. Layouts are the `layouts[]` table in
`rando/src/layouts.c` (Bomb Merchant is `RANDO_REGION_NONE` in the classic layout), rules rows are
in `rando/src/variants.c`, `mode_state` is a per-variant table in `rando/src/world.c`, the
Inverted Lake Hylia default is a `shop_overrides` table applied at world init; 3.12's note
records ARM 112.5 KB, stack 6 328 B, max case 18 ms.

- `H-17-1`: 3.12 lands as its PRD says: `RandoVariant` gains `RANDO_VARIANT_INVERTED`; locations
  are reached through a per-variant layout (classic for Standard and Open, inverted), selected
  from the variant inside the world module; `RandoRegionInfo.first_location`/`nlocations`/
  `prize_location` and `RandoLocationInfo.region` are gone; the variant rules table has an
  Inverted row. Retro is added as one more variant mapped to the classic layout and to Open's
  rules row, wherever 3.12 put those mappings (a table entry or a switch case). If 3.12 kept
  per-variant data this step cannot reach that way, Retro's mapping goes next to Open's in
  whatever form 3.12 chose.
- `H-17-2`: 3.12 claims `state-inverted` at `all` and leaves only `state-retro` pending: 43 pass,
  1 pending before this step. This step's e2e expectations are relative to that.
- `H-17-3`: 3.12's extractor has `VARIANTS = standard, open, inverted`, an `inverted` vector
  config, its own row trees and a `testComplete` kind; the Retro additions are appended after
  them. If its lists are shaped differently, `retro` is appended to whatever lists hold the world
  variants and the vector configs.
- `H-17-4`: 3.12 teaches the C fixture readers the variant name `inverted`; if it centralized the
  name-to-variant parsing, `retro` is added there once, otherwise next to each `inverted`.
- `H-17-5`: 3.12's note records `make measure` (its PRD expects ~105–113 KB ARM code+rodata);
  this step compares against it, or against step 3.4's (80.6 KB, 6 328 B, max case 22 ms) if 3.12
  recorded none.
- `H-17-6`: 3.12 does not touch `rando_set_shops` beyond its world init's Inverted Lake Hylia
  default inventory; the draw-free path and its tests are as at HEAD.

## Implementation Chunks

In dependency order; each ends with `make test`, `make logic`, `make e2e` green and one commit.

1. **Oracle (test commit).** Extractor: `RetroBowTest.php`'s `testLocation`, the `retro`
   world-dump variant, the `retro` vector config; `gen_golden.sh` run; goldens, pool fixtures and
   existing fixture lines byte-identical. C side, only to stay green: the world-dump check and the
   row replay skip `retro` records by name (one named skip each, removed by chunk 2). The `retro`
   predicate vectors already replay (the prototype passes them: `canShootArrows`' rupee-bow branch
   is ported). Red/green: none (fixtures only); the extractor's self-replay is the check.
2. **Retro world.** `RANDO_VARIANT_RETRO` (classic layout, Open's rules row), its derivation, the
   fixture readers' `retro` name; the chunk-1 skips go, so the Retro world dump and the 32
   RetroBowTest rows replay. The generator still returns `RANDO_UNSUPPORTED` for Retro (the
   `setShops` assert would fire). Test first: `test_world` cases on fresh Retro worlds — state
   Retro; the four flags true even when the input config set them false; `rom_vanilla_keys` false
   under Major Glitches; `KeyD1`/`KeyA2` 0 and `TwentyRupees2` 10 (normal), 14 (crowd control),
   `KeyD7`/`KeyD2` 0 and `TwentyRupees2` 15 (hard, expert); the Escape's VT class Open's; an Open
   world unchanged.
3. **`setShops`' random path and the generator.** The random path, the PRNG parameter, the
   generator's Retro mapping, the `test_generate` changes; claim `state-retro shufflePrizePacks`.
   Test first: `test_prepare` cases — a Retro world draws exactly 10 times (counting observer)
   and ends with 5 active Take-Any shops (4 with the potion and heart container at 0, 1 with the
   sword), 5 plain shops with the arrow, key and bombs at 80/100/50, no Upgrade shop chosen,
   Capacity Upgrade holding only `BombUpgrade5` at 100 (empty in hard mode); the old man's item
   is `ThreeHundredRupees` under swordless and vanilla weapons; for a seed where the Forest Shop
   is not chosen, its `TenArrows` slot becomes `ShopArrow` at 80 and its other slots keep their
   items; an Inverted world with the retro flags set never offers arrows in the Dark World Lake
   Hylia Shop over a seed range where an Open world does; rupee bow alone draws 5 times; the
   baseline config still draws nothing.
4. **Generic-key naming and the claim.** The host naming helper for the region sections and
   `pool.txt`, the pool writer's world parameter, the `TODO` removed; `state-retro` raised to
   `all`; `make test`, `logic`, `e2e`, `selftest`, `measure`; the step note. Test first:
   `test_spoiler_json` — under generic keys a `KeyD1` reads `Key` (no suffix), a `BigKeyD1`
   keeps `BigKeyD1:1`, a `ShopKey` in a shop keeps `ShopKey:1`, and `pool.txt` names keys the
   same way; without generic keys nothing changes. A divergence found here is fixed in this chunk
   (first differing draw or entry, then the cause), never by regenerating a golden.

Size estimates (lines, tests included): 1 ≈ 10 PHP + 15 C (+~1 260 fixture lines); 2 ≈ 50 C +
80 test; 3 ≈ 70 C + 150 test; 4 ≈ 25 C + 60 test, plus the note.

## Testing Decisions

- A good test drives the public world, prepare and generator API (or the host writer) and checks
  VT-observable behaviour — the derived config, which shops sell what at which price, how many
  draws a stage makes, what a location can be reached with, what the spoiler says — never how a
  table or a slot array is laid out.
- **VT's tests are fixtures**, replayed by `make logic`, not by `make test`: RetroBowTest's rows by
  `test_regions --fixtures`; the Retro world dump by `test_world`/`test_items`/`test_bosses
  --fixtures`; the Retro vectors by `test_items`/`test_bosses --fixtures`; Retro's pool lists
  (`pool_lists.txt`, `prepared_pools.txt`) by `test_generate --fixtures` once its skip goes.
- **Hand-written cases (in `make test`, cheap):** `test_world` pins Retro's derivation (chunk 2);
  `test_prepare` pins every branch of `setShops`' random path, including the ones no golden
  reaches (swordless/vanilla old man, the Forest Shop repricing when not chosen, hard-mode
  Capacity Upgrade, the Inverted exclusion, rupee bow alone) (chunk 3); `test_generate` generates
  Retro to `prepareWorld` against the golden prefix (chunk 3); `test_spoiler_json` pins the naming
  (chunk 4). The derivation test exists because the fixtures replay rows on a C Retro world
  without comparing its derived config with VT's (steps 3.1 and 3.3 found the same gap).
- **e2e:** `state-retro` at `shufflePrizePacks` (chunk 3), then `all` (chunk 4). Every other claim
  stays green after every chunk; no golden is regenerated (chunk 1's `gen_golden.sh` run must leave
  them byte-identical).
- **Order-sensitive behaviour is proven by e2e only:** which shops the 10 draws pick depends on
  the shop-table order and is pinned by the golden's `Shops` section and trace; the hand-written
  `setShops` tests check counts, slots and prices, not which shop a given seed picks (except the
  one seed chosen for the Forest Shop case, which the test names as such).
- Prior art: step 2.3.2's `setShops` and `setMedallions` tests and its counting observer in
  `test_prepare`; steps 3.1 and 3.3's extractor-then-replay pairs and their world-init flag
  cases; step 2.2's `TOP_LEVEL_METHODS` (`RegionTest`, `WorldTest`); `test_generate`'s
  `check_states` and named `skipped_cases`; step 2.4's `test_spoiler_json` naming cases.

## Out of Scope

- Retro combined with other non-default options (the plan's second pass): Retro with hard or
  expert pools, swordless weapons, Inverted or glitched logic get VT's code paths transliterated
  and unit-tested here, but no golden.
- ROM-side Retro: the shopkeeper sprite, the Capacity Upgrade's `max`, `setGenericKeys`,
  `setRupeeArrow`, `InitialSram`'s rupee-bow equipment, generic-key hints (`KeyGK` in
  `getHint`/`writeItem`), the entrance randomizer's `--retro` flag. Roadmap item 2 must give a
  generic key and a rupee bow as the ROM would (step 3.3 noted the same for `rom.vanilla*`).
- `difficulty: custom` and the `item.count.*` overrides a customizer would send.
- Reducing code size or time (the `item_placement-basic` time over A-3, A-2 after 3.12): recorded,
  not fixed here.
- Step 3.14's `weapons` tests.

## Further Notes

- Scratchpad artefacts (not committed): the prototype tree (Retro variant, derivation, random
  `setShops`, spoiler naming, `state-retro all` claim, the extractor changes and regenerated
  fixtures) and its `make measure` report.
- The golden's `Shops` section, for reference: Take-Any `Lake Hylia Fairy`, `Bush Covered House`,
  `50 Rupee Cave`, `Fortune Teller (Dark)` (potion and heart), `Archery Game` (the old man's
  `ProgressiveSword`); arrow/key/bomb shops `Light World Lake Hylia Shop`, `Light World Death
  Mountain Shop`, `Dark World Forest Shop`, `Dark World Lumberjack Hut Shop`, `Dark World Outcasts
  Shop`; `Capacity Upgrade` with only `BombUpgrade5` at 100.
- `RetroBowTest`'s rows pin eight locations (the Eastern and Palace of Darkness bosses, two Palace
  of Darkness chests, four Ganon's Tower chests) with a bow alone, two bows, bow and silvers, and
  bow and `ShopArrow`: only the last two give access.
