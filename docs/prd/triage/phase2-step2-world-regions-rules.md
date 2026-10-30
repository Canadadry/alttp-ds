---
title: "Rando world model, regions and NoGlitches location rules"
description: "The C port has no world, no items, no regions and no access rules, so nothing can be filled or checked for reachability; every later Phase 2 step needs them, held to VT's own NoGlitches location tests."
status: needs-triage
---

## Problem Statement

After Phase 2 step 1 the C port can draw VT's random numbers, but it has nothing to draw them for. There is no world:

- no item table, so the port cannot name a Hammer or count two Progressive Gloves;
- no location table, so it cannot say where an item goes;
- no regions, no bosses, no access rules, so it cannot answer "can Link reach the Big Chest of Eastern Palace with these items?" or "may the Big Key of Eastern Palace go here?".

Every stage that follows needs those answers:

- `setMedallions` writes into the two medallion locations (step 2.3.2);
- `fillPrizes` checks prize locations against region rules (step 2.3.3);
- `setFountains` and `prepareWorld` place items into locations (steps 2.3.4 and 2.3.6);
- the `RandomAssumed` fill and `isWinnable()` are nothing but location rules evaluated thousands of times (step 2.4).

VT's rules are about 4 000 lines of PHP closures spread over 26 region classes. They carry traps a straight reading misses: an item collection with two views that disagree, a location cache that never resets, a config key whose absence is a third value. A rule ported wrong does not crash. It shifts one fill decision, and the baseline golden diverges hundreds of draws later, far from the cause.

VT ships its own per-location tests, `tests/NoGlitches`: 23 PHPUnit classes, about 3 000 data-provider rows. They are the plan's named verification for this step, but they are PHP. They need PHPUnit, which the oracle's `composer install --no-dev` does not install, and they cannot run in the fast C loop.

## Solution

1. **A C world model in the `rando/` library**, a faithful port of VT's `World`, `Region`, `Location`, `Item`, `ItemCollection` and `Boss`, restricted to what the Open world (the baseline) and the Standard world (the world VT's NoGlitches tests build) need:
   - an item table in VT's `Item::all` order, with each item's VT class and heart power, and the three logical aliases;
   - an item multiset that reproduces `ItemCollection` exactly, both its count view and its value list, and every predicate the rules call (`hasSword`, `canExtendMagic`, `canKillMostThings`, …);
   - a boss table with VT's `canBeat` rules and each region's default boss assignment;
   - the region and location tables in VT's order, with each location's VT class, region, and constructor-time item;
   - every region's entry and completion rules, and every location's requirement, fill rule and always-allow rule, for the Open and Standard worlds;
   - `canAccess`, `canFill`, `fill`, `Region::canFill` and `World::collectItems`, with VT's side effects.
2. **A VT-run extractor** in the oracle. It loads each `tests/NoGlitches` class against a stub `TestCase` and runs every test method on every data-provider row inside VT. It records each row as a self-contained scenario with VT's own answer, and checks that VT reproduces the answer from the recorded scenario alone. It also dumps VT's world structure and a set of item-predicate and boss vectors. The outputs are committed fixture files, like `prng_vectors.txt`.
3. **C unit tests, one per module, in two modes**:
   - By default each runs only its few hand-written cases. This is what `make -C tests/rando test` runs, so the fast loop stays at step 2.1's ~50 ms.
   - With `--fixtures`, each also replays the committed fixtures: the world structure equals VT's dump, every predicate vector matches, and every NoGlitches row returns VT's answer. A new `make -C tests/rando logic` target runs this mode, and CI runs it next to `test`, `selftest` and `e2e`.

   Both modes read only committed oracle fixtures, never `tests/rando/cases/`, and need only gcc.

The step makes no RNG draw and claims no e2e case. `make -C tests/rando e2e` stays all `PENDING`.

## User Stories

1. As a C port developer, I want every VT item in a C table in `Item::all` order, so that item ids are stable and later stages can iterate items in VT's order.
2. As a C port developer, I want each item's VT class (Key, BigKey, Map, Compass, Bottle, Health, Sword, Pendant, Crystal, Event, Medallion, alias, plain, …), so that every `instanceof` test in VT has an exact C counterpart.
3. As a C port developer, I want the logical aliases (`UncleSword`, `ShopKey`, `ShopArrow`) to be items with their own ids and their target recorded, so that `has('UncleSword')` and `has('ProgressiveSword')` stay distinct, as in VT.
4. As a C port developer, I want a heart power on each Health item in exact integer units, so that `hasHealth` and `heartCount` compare as VT's floats do, without floating point on the ARM9.
5. As a C port developer, I want an item multiset with VT's count view (`item_counts`, read by `has`) and VT's value list (`cached_values`, read by `filter`, `bottleCount`, `hasHealth`, `heartCount`), so that a collection whose views disagree answers exactly as VT does.
6. As a C port developer, I want `add`, `remove`, `copy`, `merge` and `diff` to update each view exactly as VT does, so that later stages can build collections without re-deriving VT's quirks:
   - `merge` appends the argument's count view to the value list;
   - removing by `offsetUnset` and `diff` leave the value list untouched.
7. As a C port developer, I want `has(name, n)` to honour VT's generic-key rule (a collection holding `ShopKey` has every `Key*` item), `n == 0` always true, so that key logic in retro-style pools ports unchanged later.
8. As a C port developer, I want every `ItemCollection` predicate the ported code calls, with the same parameters, so that a rule reads like its PHP original. The predicates are `canLiftRocks`, `canLiftDarkRocks`, `canLightTorches`, `canMeltThings`, `canFly`, `canSpinSpeed`, `canAcquireFairy`, `canBunnyRevive`, `canShootArrows(1|2)`, `canBlockLasers`, `canExtendMagic`, `glitchedLinkInDarkWorld`, `hasHealth`, `heartCount`, `canKillEscapeThings`, `canKillMostThings`, `canBombThings`, `canGetGoodBee`, `hasSword(1..4)`, `hasArmor(1|2)`, `hasBottle`, `bottleCount`, `hasABottle` and `count`.
9. As a C port developer, I want `canExtendMagic` computed in exact integer arithmetic scaled by 128, so that `rom.BottleFill.Magic` values below `0x80` (Phase 3 item functionality) give VT's answers exactly.
10. As a C port developer, I want the twelve bosses and their `canBeat` rules, so that every boss location rule can call them.
11. As a C port developer, I want each region's boss slots (Ganon's Tower's four: `boss`, `top`, `middle`, `bottom`) held as boss ids in the world state, with VT's defaults, so that step 2.3.3's `placeBosses` can reassign them later.
12. As a C port developer, I want the 26 regions of the Open world in VT's order, each with its world key (`getRegion` name), its spoiler section name, its region items, its prize location and its boss slots, so that the spoiler writer and the fill can follow VT's order.
13. As a C port developer, I want every location in VT's `World::getLocations()` order, with its name, its region, its VT class and the item its constructor sets, so that location ids are VT's order and the fill visits locations as VT does. Examples of constructor items: the `Pyramid Fairy` trades, `DefeatGanon` on `Ganon`, `DefeatAgahnim` on `Agahnim`.
14. As a C port developer, I want location classes to keep VT's inheritance, so that a VT class test asks the same question in C. For example: BigChest is a Chest; Witch, Zora and BugCatchingKid are Npcs, but Uncle is not; the Bombos and Ether tablets are not Drops; HeraBasement is not a Standing.
15. As a C port developer, I want `setItem` to reproduce VT's location overrides, so that later stages placing items behave as VT's do:
    - a Pedestal turns MasterSword into L2Sword;
    - a Prize location refuses anything but a Pendant or Crystal, an Event location anything but an Event, a Medallion location anything but a Medallion.
16. As a C port developer, I want a refused `setItem` to return a failure and leave the location unchanged rather than abort, because VT throws there and the retry contract (plan § 2) turns a throw during `randomize()` into a failed attempt that the C port must reproduce.
17. As a C port developer, I want `canAccess(location, items)` to check the region's entry rule, then the location's requirement, so that access is VT's.
18. As a C port developer, I want `canFill(location, item, items, check_access)` to set the item on the location while it evaluates, then restore it, so that the rules that read their own location's item (wild maps and compasses on boss drops) see what VT's see. It evaluates `always_allow`, or `Region::canFill` and the fill rule and access.
19. As a C port developer, I want `fill(location, item, items)` to keep the item only when `canFill` succeeds, so that step 2.4's fill can call it as VT calls `Location::fill`.
20. As a C port developer, I want `Region::canFill(item)` to keep dungeon items in their dungeon unless the matching wild flag is set, and to keep the sewers key home in the Standard world, so that every dungeon-item restriction holds.
21. As a C port developer, I want `World::collectItems` with VT's memo, so that Link's Uncle's fill rule, which calls it, matches VT, and step 2.4 reuses it. The memo has two parts:
    - collected-location flags that persist across calls until reset;
    - a collectable-location set that is computed at its first use and never recomputed, even by a reset.
22. As a C port developer, I want the world to hold pre-collected items, merged into every `collectItems`, so that `prepareWorld`'s equipped items (step 2.3.6) need no new state.
23. As a C port developer, I want a world variant id (Open, Standard) that selects the escape region's rule set, so that the two Hyrule Castle Escape classes coexist without duplicating the 25 shared regions. The Inverted variant (Phase 3) is then a new table, not a rewrite.
24. As a C port developer, I want every rule written as a C function named after its VT location or region, transliterating the whole PHP closure, so that a reviewer can diff it against VT line by line. That includes the glitch-flag branches that are dead in NoGlitches.
25. As a C port developer, I want a config struct holding every world config key the ported code reads, with VT's default when the key is absent, so that each rule's `config(...)` call has one field to read.
26. As a C port developer, I want `itemPlacement` to be three-valued (unset, basic, advanced), because VT's tests leave it unset, so that `=== 'advanced'` and `!== 'basic'` are both false-and-true as VT computes them.
27. As a C port developer, I want `dungeonItems` mapped to the four wild flags exactly as VT's `World` constructor does (`mc`, `mcs`, `full` cumulative), so that Phase 3's `dungeon_items` rows only set one field.
28. As a C port developer, I want name-to-id lookup for items, locations, regions (by world key) and bosses, so that tests, the spoiler writer and the config parser can resolve VT's strings.
29. As a C port developer, I want the whole world in one fixed-size struct with no pointers between its parts (ids only), no `malloc` and no static mutable state, so that it fits the plan's C library constraints and the retry loop can reinitialize it by value.
30. As a C port developer, I want world initialization to be a pure function of the variant and the config, so that every unit test row and every retry attempt starts from the same state.
31. As a maintainer, I want an oracle extractor that runs VT's own `tests/NoGlitches` test methods inside VT, without PHPUnit, so that the fixture's answers are VT's behavior at the pinned commit, not my reading of it.
32. As a maintainer, I want the extractor to fail on any `tests/NoGlitches` test method it does not know how to encode, so that no VT test is silently dropped.
33. As a maintainer, I want each extracted row to be a self-contained scenario, so that the C test needs no knowledge of PHPUnit or of each test class's `setUp`. A scenario holds the world variant, config overrides, preset location items, the full item multiset in both views, the query and its arguments, and VT's answer.
34. As a maintainer, I want the extractor to replay every recorded scenario through VT from the scenario alone and fail if it does not reproduce the answer of the real test method, so that a lossy encoding cannot reach the fixture.
35. As a maintainer, I want the extractor to report every row where VT's answer differs from the upstream test's expected value, so that a broken upstream test is visible. VT's answer is still what the fixture records, because VT is the source of truth (CLAUDE.md).
36. As a maintainer, I want VT's world structure dumped for both variants, so that a C test can prove the C tables equal VT's: items with class, power and alias target; bosses; regions with key, spoiler name, region items, prize location and default bosses; locations with region, class and constructor item.
37. As a maintainer, I want VT's "all items" collection (both views) in the dump, so that the C test's "all items except …" base is proven equal to VT's `Item::all(...)->copy()->manyKeys()`.
38. As a maintainer, I want item-predicate and boss `canBeat` vectors computed by VT over a fixed family of multisets and three configs, so that every predicate is proven against VT, including on collections whose two views disagree and on option branches no NoGlitches row reaches. The three configs are VT's test default, the baseline, and basic placement with swordless weapons.
39. As a maintainer, I want a few `collectItems` vectors computed by VT, so that the memo quirk is proven against VT and not only described: two consecutive calls, a reset, a chain of at least three sweeps, and an item on a medallion location.
40. As a maintainer, I want the fixture files committed with a header naming what verifies them, as the PRNG vector file does, so that the fast loop needs no PHP and a reader knows which check to trust.
41. As a maintainer, I want the extractor run by `gen_golden.sh` once VT is ready, writing next to its other outputs, so that the fixtures are regenerated only when VT, its patch or the case list changes, under the same rule as the goldens.
42. As a developer, I want `test_items` to check the hand-ported behavioral cases of VT's `ItemCollectionTest` by default, and every predicate vector with `--fixtures`, so that a broken predicate is caught before any rule uses it.
43. As a developer, I want `test_bosses` to check every boss vector, so that each `canBeat` is proven in isolation.
44. As a developer, I want `test_world` to check the structure dump, the all-items base, the `setItem` overrides, `canFill`'s save and restore, `Region::canFill` (the `testRegionLockedItems` rows) and the `collectItems` vectors, so that the model is right before the rules land.
45. As a developer, I want `test_regions` to replay every NoGlitches row and print the VT test class, provider, row index, query, expected and actual on the first failure, so that a failing rule names its PHP origin.
46. As a developer, I want `test_regions` to cover the regions in chunks through an explicit list of enabled VT test classes, which the last chunk removes, so that unported regions never fail the loop and no row is skipped at the end. The last chunk also asserts the row count equals the fixture's.
47. As a developer, I want `make -C tests/rando test` to stay at step 2.1's ~50 ms, running only the hand-written cases, with each library source compiled once and not once per test binary, so that the loop stays fast (CLAUDE.md).
48. As a developer, I want every fixture replay in a separate `make -C tests/rando logic` target, run by CI and by every chunk of this step before it commits, so that the VT-derived checks are never skipped yet never slow the fast loop.
49. As a developer, I want `make -C tests/rando e2e` unchanged, all cases `PENDING`, so that this step cannot break the gate.
50. As the author of step 2.3.2, I want the two medallion locations ready to take a Medallion item, so that `setMedallions` is a pure RNG port.
51. As the author of step 2.3.3, I want prize locations, their region-completion requirements and boss slots ready, so that `fillPrizes` and `placeBosses` only add their own logic.
52. As the author of step 2.4, I want `canAccess`, `fill`, `collectItems` and the reset in place, with VT's memo, so that the fill and `isWinnable()` are ports of control flow only.
53. As a Phase 3 author, I want every config key the rules read already in the config struct with its default, so that an option row sets fields and ports its tests without touching rules. This holds for the glitch flags too.
54. As a DS integrator, I want the world state to be small (a few kilobytes) and the rule tables `const`, so that it fits the ARM9 budget Phase 4 measures.

## Implementation Decisions

All decisions below are mine. No `guideline` agent was consulted. Each one rests on the plan's Rules, on CLAUDE.md, and on VT's source at the pinned commit.

**Scope**
- The Open world (the baseline's `state`) and the Standard world (the world VT's NoGlitches tests build, except `OpenTest`). They differ only in the Hyrule Castle Escape region's rules: Open reuses Standard's region classes and locations, and swaps only that class.
- Not ported: Inverted and Retro regions (Phase 3 `state` steps), and shops, with their requirements and inventory. Shops have no NoGlitches test and belong to `setShops` (step 2.3.2), which also ports the shop requirement closures from the region files. Also not ported: ROM writes, hints, credits, and the `Equipped` and spoiler output.
- `testDungeonItemWrite` (DesertPalaceTest) writes ROM bytes. The extractor skips it by name, the only skipped method.

**Items module (new)**
- **Item table.** A `const` table in `Item::all` order, the three aliases last: raw name, VT class, heart power in quarter-hearts (0 for non-Health items), alias target id. Item ids are a C enum in that order.
- **No nice names.** The extractor resolves nice names (`Flute` is `OcarinaInactive`) to raw names, so C never needs them.
- **Multiset (`RandoItems`).** Two count arrays indexed by item id:
  - `counts` mirrors `item_counts` and is read by `has` and `count`;
  - `values` mirrors `cached_values` as a multiset and is read by every `filter`-based predicate.

  Order inside `cached_values` is not kept. No ported predicate depends on it. A later step that needs it extends the model.
- **Operations.** Each one mirrors its VT counterpart's effect on both views:
  - `add`: both views +1;
  - `remove`: VT's `removeItem`, both views −1, and the count entry dropped at 0;
  - `unset`: VT's `offsetUnset`, counts only;
  - `copy`: both views;
  - `merge(a, b)`: counts are a + b, and values are a.values + b.counts, because VT's `merge` re-adds through `each`, which walks the count view;
  - `diff`: counts only.
- **`has(name, n)`.** It applies VT's rules in this order:
  - `n == 0` is true;
  - a `ShopKey` count above 0 makes any `Key*` name true;
  - otherwise `counts >= n`.

  The `Key*` test is a name-prefix test over the table, precomputed as a per-item flag.
- **Predicates.** One C function per VT method, same parameters, reading config only from the world config passed in. Exact arithmetic:
  - `hasHealth` and `heartCount` sum quarter-hearts;
  - `canExtendMagic(bars, with_world)` compares `128·base + base·bottles·min(fill, 128)` with `128·bars`, where fill is `rom.BottleFill.Magic` with the world and `0x80` without one. Both are exact restatements of VT's float expressions for the values VT allows.
- **All items.** A builder for VT's test base: every item at count 1, then `manyKeys` (names starting with `Key` set to 10 in counts only), and every item at 1 in values. It is library code, because step 2.4's assumed-item sets reuse it; the dump proves it.

**Bosses module (new)**
- A `const` table of the 12 bosses in `Boss::all` order, each with a `canBeat` function `(world, items)`. VT passes `$locations` and no boss uses it.
- `canPlaceBoss` and boss shuffling belong to step 2.3.3.

**World module (new)**
- **Config (`RandoConfig`).** One field per world config key read by the ported code, found by grepping the ported VT files. Each field has VT's default from `World::config()`'s lookup chain at the pinned commit. Notable fields:
  - `itemPlacement`: unset, basic or advanced;
  - `mode.weapons`: unset or a value;
  - `mode.state`; `logic`; `goal`; `accessibility`;
  - `crystals.tower` and `crystals.ganon`, ints defaulting to 7;
  - `item.require.Lamp`, default 1; `item.Goal.Required`;
  - the four `region.wild*`; `region.bossNormalLocation`, default true; `region.cantTakeDamage`, `region.requireBetterBow`, `region.forceSkullWoodsKey`, `region.swordsInPool`;
  - `rom.rupeeBow`, `rom.CatchableFairies`, `rom.BottleFill.Magic`, `item.overflow.count.Shield`;
  - `enemizer.enemyHealth`; `ignoreCanKillEscapeThings`;
  - the 14 glitch flags (`canSuperBunny` … `canTransitionWrapped`, `canBunnySurf`, `canBunnyRevive`), all false in NoGlitches.

  A `dungeonItems` setter applies VT's cumulative mapping to the wild flags. Parsing a case's `config.json` into this struct is the generator driver's job (step 2.3.1), not this step's.
- **Structure tables (`const`, shared by both variants).**
  - Regions in `World\Open` order: world key, spoiler name, region-item list, prize location id or none, number of boss slots, default bosses.
  - Locations in `getLocations()` order: name, region id, VT class, constructor item or none.
  - Region and location ids are C enums. The tables may be produced once from the VT dump by a throwaway script; the committed C is the source, and the structure test checks it.
- **Location classes.** One enum value per concrete VT class, plus an `is_a(class, base)` relation that encodes VT's inheritance exactly. User story 14 lists the irregular cases.
- **Rules.**
  - One C source per VT region class, in the library's regions sub-directory, flat, since the Makefile's wildcard goes one level deep (H-3-1).
  - Each source defines a `const` region rule set: entry and completion functions, plus per location a requirement, a fill rule and an always-allow function, any of which may be NULL as in VT.
  - Signatures: entry, completion, requirement and always-allow take `(const RandoWorld *, const RandoItems *)`, and always-allow also takes the item id. The fill rule takes a non-const world, because Link's Uncle's rule calls `collectItems`, which writes the memo.
  - The prize location's requirement is the region's completion function, set by VT's `initalize`.
- **World variants.** A `const` table per variant maps each region slot to its rule set. Open and Standard differ only at the escape slot. `RandoWorld` stores the variant id, never a pointer.
- **Function pointers.** They live only in `const` tables, never in `RandoWorld`. This keeps "ids, not pointers" for all mutable state.
- **State (`RandoWorld`).**
  - The config and the variant id;
  - one item id per location (or none);
  - boss ids per region slot;
  - pre-collected items (`RandoItems`);
  - the `collectItems` memo: collected flags per location, the collectable-set flags, and whether that set has been computed.

  The struct is a few kilobytes and is reset by `rando_world_init(world, variant, config)`, which also applies the constructor items.
- **Glitch branches.** Every closure is transliterated whole, including the branches behind glitch flags, which are dead with NoGlitches config. Porting a closure partially is itself an error-prone edit, and Phase 3's glitch steps then only set flags and port their tests. The cost: those branches stay unexercised until Phase 3, whose `tests/OverworldGlitches` and `tests/MajorGlitches` ports verify them.
- **Operations**, each a port of the VT method of the same name:
  - `set_item` returns false on a refused class and leaves the location unchanged;
  - `has_item(location, item or none)`;
  - `can_access(location, items)`: region entry, then requirement;
  - `can_fill(location, item, items, check_access)`: set, evaluate, restore;
  - `fill(location, item, items)`;
  - `region_can_enter`, `region_can_complete` and `region_can_fill(item)`;
  - `collect_items(world, start or none, out)` and `reset_collected`.

  The collectable set excludes Medallion and Fountain locations and those already collected, all evaluated at its first computation. Shops' locations, merged in by VT, are absent until step 2.3.2 adds shops. In baseline-like pools they hold nothing collectable (H-3-9).
- **Names.** Linear-search lookups: `rando_item_find`, `rando_location_find`, `rando_region_find` (world key) and `rando_boss_find`. Each returns a sentinel when not found.
- **Unused parameters.** Rule functions that ignore a parameter mark it unused explicitly, so the strict flags pass.

**Comments.** A comment is kept only for the traps (CLAUDE.md § Comments, 40 words at most each):
- the two views of `RandoItems` and `merge`'s asymmetry;
- the collectable set never recomputed;
- `itemPlacement`'s unset value;
- `set_item`'s failure being an attempt failure, not an assert;
- Pedestal's MasterSword rewrite.

Rule functions carry no comment: their name is their VT origin.

**Oracle extractor (new, PHP, in the oracle directory)**
- Usage: `php <extractor> <vt_dir> <out_dir>`. It writes three fixture files, each with a header in the vector-file style (what each record means and what verifies it):
  - the world dump: items, all-items base, bosses, and regions and locations for both variants;
  - the predicate vectors: items and bosses;
  - the NoGlitches rows, including the `collectItems` vectors.
- **Bootstrap.** As `gen_case.php` does: VT autoload and Laravel kernel, the `Seed` pre-boot without bcmath, and `Item::clearCache()` / `Boss::clearCache()` with a fresh world for every row.
- **Stub `TestCase`.** A global class defined before any test file is loaded, with:
  - VT's `setUp`, `addCollected`, `allItems` and `allItemsExcept`, copied from `tests/TestCase.php` at the pinned commit;
  - `assertEquals`, `assertTrue` and `assertFalse` recording `(expected, actual)` instead of asserting.

  Each `tests/NoGlitches` file is then `require`d, and each `test*` method is found by reflection, with its `@dataProvider`.
- **Known method kinds.** `testLocation`, `testFillLocation`, `testRegionLockedItems`, `testEntry`, `testLocationWithMedallion`, and the four provider-less methods (`testNothingRequiredToEnter`, `testCanEnterWithEverything`, `testCapeOrUpgradedSwordRequiredToEnter`, `testSwordRequiredToComplete`). The region a region-scoped method queries is read from its source by reflection, and exactly one `getRegion('…')` must match. Any other method, other than the skipped `testDungeonItemWrite`, fails the run.
- **Per row:**
  1. Run the real `setUp` and test method on a fresh instance and capture the assertion's `(expected, actual)`.
  2. Record the scenario:
     - the variant;
     - the config overrides (`testSetConfig` keys);
     - the preset location items (the medallions set in `setUp` or in the method);
     - the item multiset just before the query, both views, as a delta from the empty set or from the all-items base, whichever is shorter;
     - the query (`access`, `fill`, `region_can_fill`, `enter` or `complete`) with its location, region and item arguments as raw names;
     - VT's `actual`.
  3. Rebuild the scenario in a fresh VT world from the record alone (both views set by reflection) and re-run the query. A different answer fails the run.
  4. When `actual != expected`, print the row on stderr. The run still succeeds.
- **Predicate vectors.** A fixed multiset family, built with VT operations so the two views diverge where VT's do:
  - empty;
  - each item alone;
  - each item at counts 2, 3 and 4;
  - all items;
  - all items except each single item (including the pseudo-names `AnySword`, `AnyBow`, `AnyBottle`, `Gloves`, `Flute`, `UpgradedSword`);
  - `UncleSword` plus 1 to 3 Progressive Swords;
  - Half or Quarter Magic with 0 to 4 bottles.

  Each multiset gets one line per config, holding the result of every predicate at every parameter value the ported code uses, plus `canBeat` for every boss. The three configs are VT's test default (Standard), the baseline (Open, advanced placement, randomized weapons), and basic placement with swordless weapons.
- **`collectItems` vectors.** A few scenarios fixed in the extractor's source: placements, pre-collected items, starting items, and the outputs of two consecutive calls, then of one call after a reset (both views, and the collected-location set). They must include a chain of at least three sweeps, a location behind an unmet requirement, and an item on a medallion location. They also include one `fill` of Link's Uncle per variant.
- **Format.** One record per line, tab-separated, first field the record kind, names raw (no tabs occur in VT names). The exact field order is the implementer's, documented in each header.
- **Running it.** `gen_golden.sh` runs the extractor once VT is ready, after `prng_check.php`, writing into the oracle directory, or into `<out>` with `--out` (H-3-6). The first generation is its own commit. Later runs are byte-identical while VT and its patch are unchanged, which the commit verifies with a `--out <scratch>` run. Never run to get green (CLAUDE.md).

**Makefile (modified)**
- Library sources compile once each into objects, collected in a static archive that every unit test links. Today every test binary recompiles every source, which would multiply the new ~5 000 lines by the number of tests.
- A test-support source (fixture line reader, scenario loader, name resolution, failure reporting), compiled into each unit test. It is support code, not a test, so `UNIT_TESTS` still lists only `test_<module>` entries.
- Register `test_items`, `test_bosses`, `test_world` and `test_regions` in `UNIT_TESTS`. `test` runs them with no argument, so it runs only the hand-written cases.
- **Two modes per binary.** With no argument a binary runs only its hand-written cases. With `--fixtures` it also replays the fixtures. Making the mode an argument keeps one test per module (CLAUDE.md) instead of a second binary per module.
  - `test_items`: the `ItemCollectionTest` ports.
  - `test_bosses`: one hand-written `canBeat` case per boss.
  - `test_world`: `set_item` refusals, the Pedestal rewrite, `can_fill`'s restore, `fill` keep and drop.
  - `test_regions`: the four provider-less NoGlitches methods restated by hand.
- **New `logic` target** (phony): runs every `UNIT_TESTS` binary that accepts `--fixtures` with that argument, then prints one summary line. `test_prng` is not run again. It follows step 2.1's precedent for a heavier check: `selftest` runs separately from `test` (step 2.1 note).
- **Budget.** `make test` stays at step 2.1's ~50 ms with the binaries built. Each chunk's commit message records `make test` and `make logic` times. If `make test` grows past about 100 ms, a hand-written case moves behind `--fixtures`. A hand-written case is never deleted.

**CI (modified)**
- The rando workflow gains a `make -C tests/rando logic` step after `test`, so every fixture replay runs on each push and pull request. It stays gcc only, with no PHP.

**Claims:** none. The step draws nothing. The stage table and the claims file are untouched.

## Implementation Chunks

Each chunk is green on its own: `make -C tests/rando test`, `make -C tests/rando logic` and `make -C tests/rando e2e` all pass. Every chunk runs `logic` before it commits, since `test` alone does not replay the fixtures.

1. **Extractor and fixtures.** The PHP extractor, its three committed fixtures generated from the prepared VT clone, and its call from `gen_golden.sh`. Verify:
   - the extractor's self-replay passes on every row;
   - the upstream-mismatch report is recorded in the commit message;
   - a second run is byte-identical.

   Also the Makefile split (archive and test support), the `logic` target and its CI step, with the fixture reader exercised by a minimal `test_items --fixtures` that only parses the files.
2. **Items and bosses.** Item table, `RandoItems` with both views and every operation, all predicates, the all-items builder, the boss table and `canBeat`. `test_items` checks:
   - the item part of the dump;
   - the all-items base;
   - every predicate vector;
   - the hand-ported behavioral cases of VT's `ItemCollectionTest` (counts, same-item counts, filter and values on repeated items, `canBunnyRevive`, `canAcquireFairy`, `canExtendMagic`, bottle counting).

   `test_bosses` checks every boss vector.
3. **World core.** Config struct and defaults, the `dungeonItems` mapping, structure tables, location classes and `is_a`, `RandoWorld`, init with constructor items, `set_item` overrides, `has_item`, `can_access`, `can_fill`, `fill`, `region_can_fill`, `collect_items` and the reset, name lookups, and the variant tables with every rule NULL. `test_world` checks:
   - the full structure dump for both variants;
   - the four `testRegionLockedItems` classes' rows, the only rows that need no rule;
   - `set_item` refusals and the Pedestal rewrite;
   - `can_fill` restoring the location;
   - `fill` keeping or dropping the item;
   - the `collectItems` vectors that need no rule. The rest are enabled with chunk 4.

   `test_regions` starts here with its runner and an empty enabled list.
4. **Overworld and every region entry.** Light World North East, North West and South; both escape variants; Death Mountain West and East; Dark World Death Mountain West and East, North East, North West, South and Mire. Also Medallions and Fountains (no rules), and the entry rule of every dungeon region, since overworld rules call them. Enable:
   - `LightWorldTest`, `HyruleCastleEscapeTest`, `OpenTest`;
   - `DeathMountain/*`, `DarkWorld/*`, `DarkWorld/DeathMountain/*`;
   - every `testEntry` and provider-less row, and the remaining `collectItems` and Uncle `fill` vectors.
5. **Light World dungeons.** Eastern Palace, Desert Palace, Tower of Hera, and Hyrule Castle Tower with Agahnim: location rules, fill rules, always-allow rules, completion and prize requirement. Enable their test classes.
6. **Dark World dungeons, part 1.** Palace of Darkness, Swamp Palace, Skull Woods, Thieves' Town. Enable their classes.
7. **Dark World dungeons, part 2.** Ice Palace, Misery Mire, Turtle Rock (the last two with their preset medallions). Enable their classes.
8. **Ganon's Tower and closure.** Ganon's Tower, about 530 rows and four boss slots. Then remove the enabled list from `test_regions`, assert that the rows run equal the fixture's row count, and check three things: `make -C tests/rando test` stays near ~50 ms once built, `logic` runs every fixture record, and `e2e` still prints only `PENDING`.

## Testing Decisions

- **What a good test checks:** external behavior only. That means the answers of `can_access`, `can_fill`, `fill`, `region_can_*`, the predicates and `canBeat`, the contents of the world after `set_item`, `fill` and `collect_items`, and the tables as seen through lookups. Never private layout or array sizes.
- **The oracle is VT, not my reading.** Every expected value in the fixtures was computed by VT at the pinned commit, and the extractor proves each recorded scenario reproduces it. The few hand-written cases (`ItemCollectionTest` ports, `set_item` refusals, `can_fill`'s restore) restate VT's source, and each cites the VT method it mirrors.
- **One test per module (CLAUDE.md):** `test_items`, `test_bosses`, `test_world` and `test_regions`. `test_regions` is the plan's "location tests ported from VT's `tests/NoGlitches`". The data-provider rows become fixture records, and the five test-method kinds become five query kinds in its runner.
- **Why a fixture and not hand-written C arrays:** about 3 000 rows, many built from `allItemsExcept`, whose two-view collections a hand translation would get wrong. A fixture keeps them exact, and lets the C test stay a small interpreter. It reads only the oracle's committed fixtures, never `tests/rando/cases/`, as the caller asked.
- **Failure output:** the first failure prints the VT class, provider, row index, query, arguments, expected and actual, then exits nonzero. Inside a chunk, a developer can run one test binary alone.
- **Speed:** `make test` runs only the hand-written cases and stays at step 2.1's ~50 ms. The fixture replays (about 1 MB, some 10 000 records) run in `make logic`, separate as CLAUDE.md requires for anything heavier, and in CI. Library objects are compiled once. `e2e` is untouched.
- **Prior art:** `test_prng` from step 2.1 (vector-file parsing, plain `main`, first-failure exit) (H-3-2), `prng_check.php` for an oracle script run by `gen_golden.sh`, and `gen_case.php` for bootstrapping VT without PHPUnit.
- **Not tested here:** glitch-flag branches (Phase 3), shops (step 2.3.2), boss placement (step 2.3.3), win condition, spheres and `collectOtherItems` (step 2.4), and the ROM write test.

## Hypotheses

Phase 2 step 1 is now done and pushed. Each hypothesis below has been checked against its committed code and the step 2.1 note.

- **H-3-1 — Library layout: holds, with one correction.** The library is under `rando/`. Its only public header is `rando/include/rando_prng.h`; there is no `rando.h` yet, and it stays reserved for the future `rando_generate`. Sources sit flat in `rando/src/` (`prng.c`). The test Makefile's wildcard compiles `src/*.c` and `src/*/*.c`, so the new modules add one header each (items, bosses, world) beside `rando_prng.h`, and the regions go in a flat `src/regions/` sub-directory.
- **H-3-2 — Unit-test convention: holds.** Unit tests are `unit/test_<module>.c`, registered in `UNIT_TESTS`, with a plain `main`, no framework, and a nonzero exit with a message on the first failure. They run from the rando tests directory, so oracle files open by relative path, as `test_prng` opens its vector file. *If false:* adopt step 2.1's convention. Only the support source's path handling changes.
- **H-3-3 — Build rule: holds.** Step 2.1 keeps "each test binary compiles every library source". This PRD changes it to an archive. *If step 2.1 already builds objects once:* keep its rule and only add the support source.
- **H-3-4 — Naming: holds.** Step 2.1 sets `Rando*` types and `rando_*` functions (`RandoPrng`, `rando_prng_seed`, …). This PRD follows it (`RandoWorld`, `RandoItems`, `RandoConfig`, `rando_world_init`). The PRNG API is not used: this step draws nothing, and `RandoWorld` gains its `RandoPrng` in step 2.3.1. *If the names differ:* rename to step 2.1's style. No behavior depends on it.
- **H-3-5 — `make test` and CI: failed in part.** As assumed, `make -C tests/rando test` runs every `UNIT_TESTS` binary with `-std=c99 -Wall -Wextra -Werror`. Not as assumed: after the binaries it runs only the cheap discovery self-test, about 50 ms in all. The gate fixtures run in a separate `make -C tests/rando selftest` (~2 s). CI runs `test`, `selftest` and `e2e`. *Consequence, applied above:* the fixture replays move out of `test` into a new `logic` target with its own CI step. Each unit test gets two modes: hand-written cases by default, plus fixtures with `--fixtures`. A budget keeps `make test` at about 50 ms.
- **H-3-6 — Oracle hook: holds.** `gen_golden.sh`, after step 2.1's discovery refactor, still prepares the patched VT clone in the oracle's git-ignored `vt/` (`composer install --no-dev`, PHP 8.3) and runs `prng_check.php` at one point before generating. The extractor runs at that same point. *If the script no longer has such a point:* the extractor is run by hand against the clone `gen_golden.sh` prepared, and its usage line says so.
- **H-3-7 — Gate untouched: holds.** The claims file and stage table exist and claim nothing. `e2e` prints `PENDING` for every case. This step adds no stage and no claim. *If step 2.1 already claimed something:* this step still changes no claim.
- **H-3-8 — Vector-file style: holds.** The PRNG vector file's header names what verifies its lines (VT through `prng_check.php`, C through `test_prng`). The new fixtures follow that style. *If false:* the fixtures still name the extractor and the C tests that check them.
- **H-3-9 — No shop location matters here.** In the Open and Standard baseline-like pools, shop locations (merged into the collectable set by VT) hold no item during `collectItems`, so their absence until step 2.3.2 changes no answer here. This is a claim about VT, not step 2.1. Step 2.3.2 checks it when it adds shops, and adds them to the collectable set in VT's order.

## Out of Scope

- Shops: model, requirements, inventory and the `Shops` spoiler section (step 2.3.2).
- Every RNG consumer: medallions, prizes, boss placement, fountains, prize packs, the item pool (steps 2.3.x), and the fill, win condition, `isWinnable()`, spheres, `collectOtherItems` and the retry loop (step 2.4).
- The generator driver, `config.json` parsing, `spoiler_json.c` and any claim (step 2.3.1 onward).
- Inverted and Retro worlds, and the tests of other logics (`tests/Inverted`, `tests/OverworldGlitches`, `tests/MajorGlitches`, …) (Phase 3).
- ROM writes (`writeItem`, `testDungeonItemWrite`), hints, texts, credits.
- Changing, regenerating or reading any golden in `tests/rando/cases/`, `vt.patch` or `gen_case.php`.
- Measuring the world's size on the host (Phase 2 step 5).

## Further Notes

- **Two views of `ItemCollection`.** This is the trap most likely to cost a day later. `allItemsExcept` removes items with `offsetUnset`, which leaves `cached_values` untouched, so "all items except AnyBottle" still counts its bottles in `hasBottle`, `canExtendMagic` and `hasHealth`. The predicate vectors include exactly these collections.
- **`collectItems`'s memo is sticky.** `getCollectableLocations()` caches its set on first use and `resetCollectedLocations()` does not clear the cache. Link's Uncle's fill rule calls `collectItems()` with no reset, so it also marks locations collected for later calls. The C port keeps both behaviors, and the extractor's vectors pin them.
- **VT's tests run with `itemPlacement` unset**, unlike every case of the matrix (`advanced` or `basic`). A rule that tests `=== 'advanced'` answers differently in the tests and in the baseline. The three-valued field and the three-config predicate vectors keep both honest.
- **`Randomizer::randomize()` may change config keys the rules read** (for example `item.overflow.*` for some pools) before the fill. Whichever step ports that code sets the config fields. This step only provides them with their defaults.
- The upstream-mismatch count printed by the extractor belongs in the chunk 1 commit message. A nonzero count is not a failure: VT's answer is the truth the C port must match.
