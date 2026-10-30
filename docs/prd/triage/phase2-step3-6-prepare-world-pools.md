---
title: "Rando prepareWorld's own work: the prepared item pool, its pre-fills and its three shuffles"
description: "After step 2.3.5 the C generator stops after shufflePrizePacks with VT's raw getter lists, so no claim can reach prepareWorld, the fill has no prepared pool to start from, and the gate's pool check has never run."
status: needs-triage
---

## Problem Statement

After Phase 2 step 3.5 (assumed in H-9-1 to H-9-8), the C generator replays VT's draws through `shufflePrizePacks` and stops. It still holds the four lists exactly as VT's getters returned them. VT's `prepareWorld` does a lot more with those lists before `randomize()` hands them to the fill, and that work is missing from the C port:

- **List edits.** Under the four glitched logics, the first `PegasusBoots` leaves the advancement list and a `TwentyRupees` joins the item pool. This is the `TODO(step 2.3.6)` that step 2.3.3 left in `rando_prepare_pre_collect`. `SilverArrowUpgrade` moves to the nice list. Swords and bottles leave the advancement list. Swords, health and armor leave the nice list, and health leaves the item pool. Then some of them come back, depending on `itemPlacement`, `mode.weapons`, `region.requireBetterSword` and `region.takeAnys`: the uncle sword, the master sword, a third sword, one bottle, `TwentyRupees2` per sword when swordless, `FiftyRupees` when assured. `rom.rupeeBow` swaps arrows for rupees. The `region.wild*` flags move dungeon items into the advancement list.
- **Pre-fills.** `Pyramid Fairy - Bow` gets `BowAndArrows` (or `BowAndSilverArrows`). Boss hearts go in when the config asks for them. Vanilla weapons put swords on Link's Uncle, `Pyramid Fairy - Left`, `Blacksmith` and the pedestal. `region.forceSkullWoodsKey` puts a `KeyD3` on `Skull Woods - Pinball Room`. `weapons: assured` pre-collects the uncle sword. Swordless calls `collectItems()`, the world's first call to it.
- **Three `fy_shuffle`s**: advancement, then nice, then the item pool. That is 153 draws in baseline (step 1.1 note: 43 + 40 + 70). The dungeon list is not shuffled.

So no claim can go past `shufflePrizePacks`. Step 2.4 (the fill) has no prepared pool to start from. And the gate's `pool` check, built in step 2.1 for this step, has never run.

The plan says this step checks "that the pool's items equal those the golden spoiler places, minus the pre-filled ones". Step 2.1's PRD added hypothesis H-2-10: the golden's placed items are the entry values outside `Bosses`, `Equipped`, `Shops`, `Special`, `meta` and `playthrough`, and this step would declare every pre-filled location with a `keys` check. Running VT itself on every case (see Implementation Decisions) shows that the first half holds, with two exceptions, and the second half cannot work:

- A `check` line applies to every claim that reaches its stage. The pre-filled locations differ by option. For a location VT has not pre-filled, the C partial spoiler writes `"Nothing"` where the golden has the item the fill put there later. So a `keys Skull Woods - Pinball Room:1` check would fail baseline (`Nothing` against `BigKeyD3:1`).
- In `goal-pedestal`, `goal-triforce-hunt` and `goal-ganonhunt` the pool holds more items than there are free locations: 1, 30 and 50 more. VT's fill leaves the extra item-pool items unplaced. Which ones depends on the fill's own shuffle, so the golden cannot show them. The first two of these cases are claimed today.

## Solution

1. **A new oracle fixture.** `dump_pools.php` also writes `prepared_pools.txt`: for every case, VT's four lists as `prepareWorld` leaves them in the `Randomizer` (final order, raw names). Before writing, it checks its draws against the golden trace. `make logic` replays it against the C lists. This fixture is the only check that sees the order before each shuffle, and the only one that sees the surplus cases.
2. **A gate refinement: pre-fills are what the C spoiler already fills.** The `pool` check also compares every in-scope spoiler entry that the C partial spoiler fills (any value but `"Nothing"`) against the golden by value, and leaves those entries out of the golden multiset. This replaces H-2-10's per-location `keys` checks. The existing verdicts don't change (`make selftest`).
3. **A stop stage for tests.** `rando_generate_until` stops after a named stage. `rando_generate` keeps its signature and runs every ported stage. The raw-list fixture is now replayed by stopping after `getAdvancementItems`, before the Boots edit changes the lists.
4. **The Boots edit where VT makes it.** `rando_prepare_pre_collect` takes the pools and makes VT's list edit. This resolves the step 2.3.3 `TODO`.
5. **`rando_prepare_pools`**, ported line for line from VT's `prepareWorld` after `shufflePrizePacks`: every pre-fill, every list move, and the three shuffles. It runs as stage `prepareWorld` right after `shufflePrizePacks`. The generator then stops with `RANDO_INCOMPLETE`, just before the fill.
6. **`pool.txt`.** `rando_gen` writes the prepared pool next to the partial spoiler (step 2.1's contract).
7. **Gate files.** `stages.txt` gains `check prepareWorld pool`. Eleven claims move up to `prepareWorld`. `goal-pedestal` and `goal-triforce-hunt` stay at `shufflePrizePacks` because of the surplus. Six new claims cover the paths no claim takes yet: `item_placement-basic`, `weapons-assured`, `weapons-vanilla`, `dungeon_items-mc`, `-mcs` and `-full`.

`make -C tests/rando e2e` then prints 17 `PASS … (prepareWorld)`, 2 `PASS … (shufflePrizePacks)` and 25 `PENDING` lines, in about 0.2 s.

## User Stories

1. As the e2e gate, I want the C trace to carry `prepareWorld`'s three shuffles right after `shufflePrizePacks`' seven draws, labelled `prepareWorld`, so that a `prepareWorld` claim compares 232 lines in baseline (233 in the `crystals_*-random` cases).
2. As the e2e gate, I want `stages.txt` to declare `check prepareWorld pool`, so that every `prepareWorld` claim compares the C pool with what the golden places.
3. As the e2e gate, I want the `pool` check to treat every in-scope entry the C partial spoiler fills as a pre-fill, compared by value and left out of the multiset, so that pre-filled locations that differ by option need no per-location check line.
4. As the e2e gate, I want a pre-fill whose C value differs from the golden's reported as `golden: [Section] "Key": "V"` / `c: [Section] "Key": "W"`, the bytewise first such entry, before the multiset comparison, so that a misplaced pre-fill reads as a location and not as an item count.
5. As the e2e gate, I want a C pre-fill on a key the golden lacks reported as `golden: <none>`, so that a pre-fill on a non-existent or wrongly named location fails.
6. As the e2e gate, I want C entries holding `"Nothing"` ignored by the pre-fill pass, so that an empty location never counts as a pre-fill.
7. As the e2e gate, I want every existing `pool` verdict and message unchanged, so that `make selftest` proves the refinement changes nothing that already held.
8. As a C port developer, I want `rando_prepare_pre_collect` to drop the first `PegasusBoots` from the advancement list and append a `TwentyRupees` to the item pool under the four glitched logics, exactly when VT does, so that the list edit sits where VT makes it and the step 2.3.3 `TODO` goes away.
9. As a C port developer, I want `rando_prepare_pools` to set `Pyramid Fairy - Bow` to `BowAndArrows`, or to `BowAndSilverArrows` under `region.pyramidBowUpgrade`, so that the fill and `collectItems` see VT's trade item even though the spoiler hides it.
10. As a C port developer, I want the boss-heart pre-fill (`region.bossesHaveItem`, `region.bossHeartsInPool`) ported over VT's ten boss locations in VT's order, so that the function is VT's even though no matrix case sets those keys.
11. As a C port developer, I want `SilverArrowUpgrade` appended to the nice list during the advancement walk, before the nice walk, so that the nice list has VT's order.
12. As a C port developer, I want swords and bottles taken out of the advancement list in order, then swords, health and armor out of the nice list, then health out of the item pool, so that the side lists hold VT's order.
13. As a C port developer, I want `itemPlacement: basic` to append health then armor to the advancement list, and every other placement to append them to the nice list, so that `item_placement-basic`'s 71/12 list sizes come out right.
14. As a C port developer, I want swordless to append one `TwentyRupees2` to the nice list per removed sword, call `collectItems()` once, and add `SilverArrowUpgrade` to the advancement list only when VT's two-bow check fails, so that the swordless branch is VT's, including its side effect on the collectable cache.
15. As a C port developer, I want vanilla weapons to pre-fill Link's Uncle with `UncleSword`, then `Pyramid Fairy - Left`, `Blacksmith` and the pedestal with the next swords popped, or to drop the pedestal sword for a `TwentyRupees` when the pedestal holds the Triforce, so that `weapons-vanilla`'s four pre-fills match.
16. As a C port developer, I want randomized and assured weapons to put the uncle sword back (pre-collected with a `FiftyRupees` in the item pool when assured, otherwise pushed onto the advancement list), then the master sword, then a third sword under `region.requireBetterSword`, then apply `region.takeAnys`, so that the sword order is VT's.
17. As a C port developer, I want one bottle popped back onto the advancement list and the rest appended to the nice list, so that the bottle split is VT's.
18. As a C port developer, I want `rom.rupeeBow` to replace every arrow and arrow upgrade in the item pool with `FiveRupees`, appended after the kept items, so that retro's pool is ready for Phase 3.
19. As a C port developer, I want `region.forceSkullWoodsKey` to move the first `KeyD3` from the dungeon list to `Skull Woods - Pinball Room`, so that `item_placement-basic`'s pre-fill matches.
20. As a C port developer, I want the `region.wildBigKeys`, `wildKeys`, `wildMaps` and `wildCompasses` walks in VT's order, with VT's `KeyH2` exception for standard without NoLogic, so that the `dungeon_items-*` advancement lists have VT's order.
21. As a C port developer, I want the three `fy_shuffle`s in VT's order (advancement, nice, item pool) and the dungeon list left unshuffled, so that the 153 baseline draws match.
22. As a C port developer, I want every list append to assert room under `RANDO_POOL_CAPACITY`, so that an overflow fails loudly.
23. As a C port developer, I want the vanilla and randomized branches to assert that the uncle sword's popped sword is `ProgressiveSword`, so that a future pool where `UncleSword`'s fixed C alias target would be wrong fails loudly instead of placing the wrong sword.
24. As a test author, I want `rando_generate_until` to stop with `RANDO_INCOMPLETE` right after a named stage, so that a test can read the world and the pools as any stage leaves them, now and after step 2.4.
25. As a test author, I want `rando_generate` unchanged for its callers, so that `rando_gen` and every existing test keep compiling.
26. As a maintainer, I want `prepared_pools.txt` dumped from VT itself, with the dump's draws checked against the golden trace, so that the C lists are compared with VT's real lists, order included, for all 42 supported cases.
27. As a maintainer, I want `make logic` to replay `prepared_pools.txt` and the existing `pool_lists.txt`, so that the order before each shuffle and the surplus cases are checked without slowing `make test`.
28. As a maintainer, I want `pool_lists.txt` byte-identical after the dump script change and no case file touched, so that the new fixture is a pure addition (CLAUDE.md: never regenerate a golden).
29. As a maintainer, I want `rando_gen` to write `pool.txt` whenever it stops incomplete, one `<target name>:1` line per item of the four lists in list order, so that the gate reads the pool in the spoiler's own notation (`UncleSword` reads `ProgressiveSword:1`).
30. As a maintainer, I want the eleven claims that hold raised to `prepareWorld`, and `goal-pedestal` and `goal-triforce-hunt` kept at `shufflePrizePacks` with the reason in the step note, so that the ratchet holds and no claim rests on a check that can't pass.
31. As a maintainer, I want new claims only for paths no claim covers yet, so that the e2e loop stays fast.
32. As a maintainer, I want four deliberate breaks run and their outcomes recorded, so that the step note says which guard owns which mistake.
33. As a maintainer, I want the e2e time measured (median of five runs) and recorded against the 0.5 s budget, so that the step note carries it like every earlier one.
34. As a developer, I want `make -C tests/rando test` to gain only hand-written cases of microseconds each, so that the fast loop stays fast.
35. As the author of step 2.4, I want the generator to stop just before the fill with the prepared pool in `RandoGen`, so that the fill is one more stage call on VT's exact inputs.
36. As the author of the Phase 3 `state-retro` step, I want `takeAnys`, `rupeeBow` and `wildKeys` already ported here, so that retro only needs its world and its shops.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. They rest on the plan's Rules (claim ratchet, the 0.5 s e2e budget, H-2-9's "a sub-step that needs another check kind adds it, with fixtures"), step notes 1.1–2.3.5, CLAUDE.md, the committed harness and library, and VT's source at the pinned commit. Every assumption about step 3.5's outcome is labelled `H-9-n` (see Hypotheses).

**Facts read from VT, the goldens and committed code (not hypotheses)**
- VT's `prepareWorld`, after `shufflePrizePacks`, in this order: take the locations that are neither `Prize` nor `Medallion`. Set `Pyramid Fairy - Bow`. Then the boss hearts. Then the advancement walk (`SilverArrowUpgrade` → nice, `Sword` → swords, `Bottle` → bottles, each with `continue`). Then the nice walk (`Sword`, `Upgrade\Health`, `Armor`, three separate `if`s). Then the item-pool walk (`Upgrade\Health`). Then the basic/other merge of health then armors. Then the weapons branch (swordless / vanilla / else). Then the bottle split, `rom.rupeeBow`, `region.forceSkullWoodsKey`, and the four wild walks (big keys, keys, maps, compasses). Finally `fy_shuffle` of advancement, then nice, then the item pool. The dungeon list is only merged.
- Under the four glitched logics, VT's Boots edit happens before `setShops`. Nothing from `setShops` to `shufflePrizePacks` reads the lists.
- In the matrix the only `region.*` keys set are `wild*` (from `dungeon_items`), `requireBetterSword`, `requireBetterBow` and `forceSkullWoodsKey` (from `itemPlacement: basic`), and `takeAnys` and `wildKeys` (retro). `bossesHaveItem`, `bossHeartsInPool` and `pyramidBowUpgrade` keep their defaults (false, true, false). `difficulty` is always `normal`, so the `!== 'custom'` guards always pass.
- `getSpoiler` skips `Location\Prize\Event` (`Ganon`) and `Location\Trade` (`Pyramid Fairy - Bow`, `Pyramid Fairy - Sword`). No golden holds a `"Nothing"` value.
- `UncleSword` is an `ItemAlias` whose target `setTarget` replaces. In every matrix case the popped sword is `ProgressiveSword` (`pool_lists.txt`: 4 `ProgressiveSword` per case, no other sword). The C alias target is fixed at `ProgressiveSword`.
- `ItemCollection::merge` accepts a plain array. The swordless check reads `merge($advancement_items)->has('ProgressiveBow', 2)`, then `in_array` on the collected items' `values()`.
- **VT run over every case** (an ad hoc PHP script in a scratch directory, not committed). For each case it calls `oracle_attempt_world(case, 0)`, `new Randomizer([$world])`, `prepareWorld($world)`, then reads the four lists by reflection. The draws match the golden prefix in all 44 cases, and the next golden stage is `fill`. List sizes (dungeon/advancement/nice/item pool): baseline and 28 others (state-inverted included) 63/43/40/70. `dungeon_items-mc` 40/66/40/70, `-mcs` 11/95/40/70, `-full` 0/106/40/70. The four `glitches-*` 63/42/40/71. `goal-triforce-hunt` 63/73/40/70, `goal-ganonhunt` 63/93/40/70. `item_placement-basic` 62/71/12/70. `item_pool-crowd_control` 63/39/40/74. `weapons-assured` 63/42/40/71, `-swordless` 63/41/42/70, `-vanilla` 63/41/38/70. `state-retro` 34/62/39/81.
- Same run, the golden's in-scope entries (outside the six sections, minus `*- Prize:1`) against the lists' target names plus VT's spoiler-visible pre-fills:
  - Pre-fills: `goal-pedestal` `Master Sword Pedestal` = `Triforce` (step 2.3.3's prologue). `item_placement-basic` `Skull Woods - Pinball Room` = `KeyD3`. `weapons-vanilla` `Link's Uncle`, `Pyramid Fairy - Left`, `Blacksmith` and `Master Sword Pedestal` = `ProgressiveSword`. No other case has a visible pre-fill. Each one equals the golden value.
  - Golden multiset minus pre-fills equals the pool multiset in 40 cases. The exceptions: `goal-pedestal` (one `Arrow` too many in the pool), `goal-triforce-hunt` (30 junk items too many), `goal-ganonhunt` (50), all item-pool items that VT's last `fastFillItemsInLocations` leaves unplaced. And `state-retro` (`rom.genericKeys` writes `Key`; Phase 3).
- The committed harness at HEAD (step 3.5 done) passes all 42 supported cases at `setFountains` when every one is claimed: 42 pass in 0.35–0.39 s on this container. The thirteen real claims take 0.14–0.16 s here (the 2.3.5 note's ~0.11 s was on another machine). So a claim costs about 7 ms here.

**H-2-10, corrected**
- **First half confirmed:** the golden's placed items are exactly the entry values outside `Bosses`, `Equipped`, `Shops`, `Special`, `meta` and `playthrough`, minus the `*- Prize:1` keys. The pre-filled `Ganon` and `Pyramid Fairy - Bow` are not in the spoiler, and their items (`Triforce`, `BowAndArrows`) are not in any list either.
- **But "pool = placed − pre-filled" holds only when the pool fits the free locations.** It fails in `goal-pedestal`, `goal-triforce-hunt` and `goal-ganonhunt`, and the golden cannot show which items the fill left out. Those cases are not claimed at `prepareWorld`. Their lists are checked exactly by `prepared_pools.txt`.
- **Second half replaced:** a per-location `keys` check cannot express option-dependent pre-fills. The `pool` check instead treats what the C spoiler fills as the pre-fills and checks each one by value.

**Gate (`run_e2e.sh` `pool` check, refined)**
- The domain is unchanged: golden entries in every section except `Bosses`, `Equipped`, `Shops`, `Special`, `meta` and `playthrough`, minus the keys matched by a due `keys` check.
- New pre-fill pass, before the multiset. Walk the C partial spoiler's entries under the same domain rules (same excluded sections, same due-key exclusion), keeping only values other than `"Nothing"`. For each such C entry: if the golden has the same section and key with the same value, the entry leaves the golden multiset. If the value differs, or the golden lacks the key, the check fails. It reports the bytewise first offending `[Section] "Key"` as `    golden: [Section] "Key": "V"` (or `    golden: <none>`) and `    c: [Section] "Key": "W"`.
- Then the existing multiset comparison of what is left of the golden against `pool.txt`, with the same messages. A missing `pool.txt` is still `DIVERGENT`.
- Not a new check kind: H-2-9 allows one, but this is what `pool` was meant to mean ("minus the pre-filled ones"). The `stages.txt` grammar is unchanged.
- Cost: one more pass over entries already loaded by `load_entries`. Nothing else is read.

**Oracle fixture (`dump_pools.php`, extended; `prepared_pools.txt`, new)**
- For each case: a fresh `oracle_attempt_world(case, 0)`, `new Randomizer([$world])`, `prepareWorld($world)`. VT's patch labels the stages itself, as in `gen_case.php`. The dump fails, writing nothing, unless its draws are the golden trace's first lines and the next golden line's stage is `fill`.
- Format: `pool_lists.txt`'s. A header naming what writes and what replays it, then per case `case <name> <seed> <key>=<value>…` and four lines `dungeon|advancement|nice|items <raw names>`, in final order, read by reflection from the `Randomizer`'s four properties. Raw names, so `UncleSword` stays `UncleSword`: the fill's logic tells it apart from `ProgressiveSword`.
- All 44 cases, like `pool_lists.txt`. The C replay skips `state-inverted` and `state-retro` by name, as it does today.
- `gen_golden.sh` needs no new call (it already runs `dump_pools.php` over every case); only its header lists the new fixture. The fixture is produced by running `dump_pools.php` alone, not `gen_golden.sh`. The commit holds the script, the header and the new fixture only. `pool_lists.txt` must come out byte-identical, and no `cases/` file may change.

**Generator (library, modified)**
- `RandoStatus rando_generate_until(RandoGen *, const RandoOptions *, uint32_t seed, uint32_t max_attempts, RandoTraceFn, void *, RandoStage last)`. It returns `RANDO_INCOMPLETE` right after stage `last` completes on the attempt that reaches it. A failing earlier stage (`fillPrizes`) still retries as usual. `RANDO_STAGE_NONE` runs every ported stage. `rando_generate` calls it with `RANDO_STAGE_NONE`. `RandoGen` gains `RandoStage last`. Decided over a raw-list snapshot in `RandoGen` (1 KB on the DS) and over a test helper that re-creates the generator's setup.
- `run_attempt`: after `getAdvancementItems`, `rando_prepare_pre_collect(&gen->world, &gen->pools)`. After `shufflePrizePacks`, set stage `RANDO_STAGE_PREPARE_WORLD`, call `rando_prepare_pools(&gen->world, &gen->prng, &gen->pools)`, reset the stage and return `RANDO_INCOMPLETE`. That name and the stage already exist (H-9-1).

**Prepare module (library, extended)**
- `void rando_prepare_pre_collect(RandoWorld *world, RandoPools *pools)`. Under the four glitched logics it pre-collects `PegasusBoots`. If the advancement list holds one, it removes the first, shifting the rest left in order, and appends `TwentyRupees` to the item pool. Outside standard it pre-collects `RescueZelda`, as today. The `TODO(step 2.3.6)` comment goes.
- `void rando_prepare_pools(RandoWorld *world, RandoPrng *prng, RandoPools *pools)`. Contract line: VT's `prepareWorld` after `shufflePrizePacks`; draws exactly the three shuffles (advancement, nice, item pool); may call `collectItems` (swordless). The body follows VT's order from the facts above, on the compact C lists. "unset" is an in-order removal, "push" and "array_merge" append. Swords, bottles, health and armor are matched by item class (`RANDO_ITEM_CLASS_SWORD`, `BOTTLE`, `HEALTH`, `ARMOR`; VT's item classes all extend `Item` directly, so the class match is VT's `instanceof`). The arrows for `rupeeBow` are `ARROW` and `UPGRADE_ARROW`.
- Side lists (`swords`, `bottles`, `health`, `armors`, the rupee-bow replacements) are local arrays sized `RANDO_POOL_CAPACITY` on the stack, under 1.3 KB. Each append asserts room.
- Pre-fills go through the module's asserted `place` helper (H-9-3). Uncle: `UncleSword`, asserting that the popped sword is `ProgressiveSword` (the fixed alias target). Vanilla asserts at least four swords. Without them VT would `setItem(null)`.
- Swordless calls `rando_world_collect_items(world, NULL, &items)`, adds the advancement list, tests `has_n(ProgressiveBow, 2)`, and otherwise tests the collected values view for `SilverArrowUpgrade` and `BowAndSilverArrows` and the nice list for `SilverArrowUpgrade`. A trap comment of at most 40 words on the call: VT's only `collectItems` before the fill, which fixes the collectable cache, so it is not hoisted or skipped.
- The three shuffles call `rando_fy_shuffle` on each list in place: advancement, nice, item pool.

**Config (library, extended)**
- `RandoConfig` gains `region_pyramid_bow_upgrade` (false), `region_boss_hearts_in_pool` (true in `rando_config_init`), `region_bosses_have_item` (`RandoItemId`, `RANDO_ITEM_NONE` for VT's false) and `region_take_anys` (false). No option sets them. They exist because `RandoConfig` holds one field per VT key the ported code reads. `region_force_skull_woods_key`, `region_require_better_sword`, `rom_rupee_bow` and the `wild` flags already exist. The step note records the new `sizeof(RandoWorld)` and `sizeof(RandoGen)`.

**Host (`rando/host`, extended)**
- `bool rando_spoiler_write_pool(FILE *out, const RandoPools *pools)`, beside the spoiler writer, which already has VT's `getTarget()->getName()`: one `<target name>:1` line per item, dungeon, advancement, nice, then item pool, in list order. False on a stream error.
- `rando_gen` writes `<out>/pool.txt` whenever the status is `RANDO_INCOMPLETE`, after `spoiler.json`, with the same error handling. Not on `RANDO_COMPLETE`: step 2.4 owns that.

**Gate files**
- `stages.txt`: append `check prepareWorld pool`.
- `claims.txt`, ratchet. Eleven claims move up to `prepareWorld`, each kept only if it holds: baseline, both `crystals_*-random`, both `item_functionality-*`, the three `item_pool-*`, `state-standard`, `glitches-overworld_glitches`, `weapons-swordless`. `goal-pedestal` and `goal-triforce-hunt` stay at `shufflePrizePacks`: the pool check cannot pass because of the surplus, and `prepared_pools.txt` covers them.
- New claims at `prepareWorld`, one per path no claim takes:
  - `item_placement-basic`: health and armor to advancement, a third sword, the Skull Woods pre-fill.
  - `weapons-assured`: pre-collected uncle, `FiftyRupees`.
  - `weapons-vanilla`: four pre-fills, the pedestal branch.
  - `dungeon_items-mc`, `-mcs`, `-full`: one more wild walk each.
- Not claimed: the other three `glitches-*` (the same branch as `overworld_glitches` here), `goal-ganonhunt` (surplus), and every case whose lists match baseline's path.
- Totals: 19 claims (17 at `prepareWorld`), 25 pending.
- **Speed.** Six more claims at about 7 ms each on this container, plus one more pass over loaded entries per claim. **Expected: about 0.2 s here (about 0.15 s on the 2.3.5 note's machine), under half the 0.5 s budget.** No gate restructure. The step note records the median of five runs before and after.

## Implementation Chunks

1. **Oracle fixture** (own commit, `test:`). Extend `dump_pools.php`, update `gen_golden.sh`'s header, run `dump_pools.php` over every case and commit `prepared_pools.txt`. `pool_lists.txt` must be byte-identical, and no `cases/` file may change. No C yet. `FIXTURE_PREPARED_POOLS` is added in chunk 4.
2. **Gate refinement** (own commit). The `pool` check's pre-fill pass in `run_e2e.sh`. `selftest.sh` gains the pre-fill fixtures below, and every existing fixture keeps its verdict and message. Claims are unchanged, so `make e2e` output is unchanged.
3. **Stop stage and Boots edit.** Add `rando_generate_until` and `rando_prepare_pre_collect(world, pools)`. `test_generate --fixtures` replays `pool_lists.txt` by stopping after `getAdvancementItems`. `test_prepare`'s pre-collect cases cover the list edit. The full generator still stops after `shufflePrizePacks`; e2e is unchanged.
4. **`rando_prepare_pools` and the stage.** The config fields, the function, the generator call, `test_prepare`'s branch cases and baseline anchors, `test_generate`'s new counts and its `prepared_pools.txt` replay. e2e: the existing claims still pass at their stages (the gate compares only their prefix).
5. **`pool.txt` and the claims.** `rando_spoiler_write_pool`, `rando_gen`, the `test_spoiler_json` cases, the `stages.txt` line, `claims.txt`, the four deliberate breaks, the timings.

## Testing Decisions

- **A good test checks external behavior:** the four lists' contents and order after the stage, the world's location items and pre-collected items, the draws the observer sees, the generator's trace and status, and the gate's verdict lines. No test reads a side list or a local index.
- **VT is the oracle.** Hand-written expectations restate VT's source, a golden trace line, or a fixture line, and cite it. A hand-written case that contradicts a golden or a fixture is the test's bug (CLAUDE.md).
- **`selftest.sh` (chunk 2)**, using the existing `pool` fixtures as prior art. The existing golden spoiler holds `Light World` `Uncle:1` = `Sword:1`, `Link's House:1` and `Sahasrala:1` = `Arrow:1`, and `Eastern Palace` `Big Chest:1` = `Bow:1`. New cases:
  - The C spoiler pre-fills `Uncle:1` = `Sword:1` and `pool.txt` lacks `Sword:1`: PASS.
  - The C spoiler pre-fills `Uncle:1` = `Bow:1`: DIVERGENT, `golden: [Light World] "Uncle:1": "Sword:1"` / `c: [Light World] "Uncle:1": "Bow:1"`.
  - The C spoiler fills `Light World` `Zora:1` = `Arrow:1`: DIVERGENT, `golden: <none>`.
  - The C spoiler writes `"Nothing"` for the four entries and `pool.txt` holds all four items: PASS.
  - A C pre-fill matched by a due `keys` glob is left to that check, not double-counted.
- **`test_prepare` (extended)**, prior art `logic_world`, `seeded`, `count_draw`, `record_range` and the twin-PRNG index tests (H-9-8):
  - Pre-collect: under each glitched logic the first of two `PegasusBoots` leaves a hand-built advancement list in order and one `TwentyRupees` is appended to the item pool. With no `PegasusBoots` in the list, only the pre-collect happens. NoGlitches leaves the lists alone.
  - Branches, each on an open world with small hand-built lists. The expected pre-shuffle lists are written out by hand and shuffled with a twin PRNG in VT's order (advancement, nice, item pool), so order is checked despite the shuffle:
    - SilverArrowUpgrade's move.
    - Advanced versus basic health and armor.
    - Randomized (uncle `UncleSword` then master sword pushed, a third sword under `require_better_sword`, `take_anys` dropping one for `TwentyRupees`).
    - Assured (`UncleSword` pre-collected, `FiftyRupees`).
    - Vanilla (four pre-fills; with the Triforce on the pedestal, the fourth sword dropped and `TwentyRupees` appended).
    - Swordless (`TwentyRupees2` per sword; with one `ProgressiveBow`, `SilverArrowUpgrade` appended; with two, not).
    - The bottle split.
    - `rupee_bow`.
    - `force_skull_woods_key`.
    - Each wild flag, plus `KeyH2` kept under standard and moved under NoLogic.
    - Boss hearts (`boss_hearts_in_pool` false → `BossHeartContainer` on all ten).
    - `Pyramid Fairy - Bow` with and without `pyramid_bow_upgrade`.
  - Draws: on baseline's lists, an observer sees exactly 153 draws, with ranges `0..42` down to `0..0`, then `0..39` down, then `0..69` down.
  - Baseline anchors: through `rando_generate`, the counts are 63/43/40/70, and the first and last item of each shuffled list equal `prepared_pools.txt`'s baseline record, quoted in the test with a citation. `make test` reads no fixture.
- **`test_generate` (updated, H-9-4):**
  - Baseline gives `RANDO_INCOMPLETE` and `trace.lines == 232`, matching the golden's first 232 lines, the last a `prepareWorld` `0 0` draw.
  - The `crystals_*-random` cases give 233, the two-random-crystals order case 234, and `state-standard` and the reseeding cases 232.
  - `rando_generate_until(…, RANDO_STAGE_SHUFFLE_PRIZE_PACKS)` gives 79 lines and the world step 2.3.5 checked.
  - The 79/80/81 expectations are updated, not kept: they froze the old stop point (CLAUDE.md).
  - `--fixtures` replays `pool_lists.txt` stopping after `getAdvancementItems`, and `prepared_pools.txt` through the full generator, comparing all four lists in order. Both records are read by one shared parser.
- **`test_spoiler_json` (extended):** a hand-built `RandoPools` with an `UncleSword` writes `ProgressiveSword:1`. Empty lists write an empty file. The list order is dungeon, advancement, nice, item pool.
- **Deliberate breaks** (each reverted, not committed; the step note records each outcome):
  1. Push the master sword before the uncle sword. The trace and every claim stay green, and `make logic` fails on `prepared_pools.txt`. Only the fixture sees the order before a shuffle.
  2. Drop the Boots edit. `glitches-overworld_glitches` is `DIVERGENT` at draw 80, in `prepareWorld`: a `0 42` range where the golden has `0 41`.
  3. Skip `force_skull_woods_key`. e2e stays green: the `KeyD3` stays in the pool, and the golden counts it at Pinball Room. `make logic` fails on `item_placement-basic`'s dungeon list. The note says that the gate cannot see a missing pre-fill whose item stays in the pool, and that the fixture owns it until step 2.4.
  4. Put vanilla's `Blacksmith` sword on `Pyramid Fairy - Right`. `weapons-vanilla` is `DIVERGENT` on the `pool` check's pre-fill line.
- **Speed:** `make test` gains a few dozen hand-written checks over lists of at most a few hundred items. The fixture replays stay in `make logic`, and the gate fixtures in `make selftest`.

## Hypotheses

Step 3.5 was being implemented when this PRD was started. It was committed (`67bc8d9`, note `610e3c8`) before this PRD was finished, and each item below was checked against that commit. The implementer rechecks each one against the code before starting and records each verdict in the step note.

- **H-9-1 — Generator after 3.5.** `run_attempt` ends with stage `shufflePrizePacks` (`rando_shuffle_prize_packs(&gen->world, &gen->prng)`), resets the stage to `RANDO_STAGE_NONE` and returns `RANDO_INCOMPLETE`. `RANDO_STAGE_PREPARE_WORLD` and its name `prepareWorld` exist and match `stages.txt`. *If it fails:* add the `prepareWorld` call after the last stage call on the success path.
- **H-9-2 — Gate after 3.5.** `claims.txt` holds the thirteen named claims at `shufflePrizePacks`. `stages.txt` holds five check lines and none for `shufflePrizePacks`. `make e2e` prints `13 pass, 0 divergent, 31 pending`. *If it fails:* raise only the claims that reached `shufflePrizePacks` and hold through `prepareWorld`. The step note names any left behind.
- **H-9-3 — Prepare module after 3.5.** `rando_shuffle_prize_packs` is the last function in `rando_prepare.h`. `prepare.c` keeps its asserted `place` helper and reaches `rando_pool.h` and `rando_prng.h`. *If it fails:* declare the new function after the last stage function and add the missing helper.
- **H-9-4 — `test_generate` after 3.5.** It expects `trace.lines` 79 (baseline, `state` `standard`, reseeding), 80 (single random crystals) and 81 (two random crystals). The `pool_lists.txt` replay calls `rando_generate` with one attempt and expects `RANDO_INCOMPLETE`. This step makes them 232/233/234 and moves the replay to `rando_generate_until`.
- **H-9-5 — e2e time after 3.5.** About 0.11 s at thirteen claims on the note's machine, 0.14–0.16 s on this container. *If it fails* (above 0.4 s): record it, and restructure the gate in its own commit before adding claims (§ Convergence gate).
- **H-9-6 — Layouts after 3.5.** `RandoWorld` is 2 744 bytes, with the prize packs. `RandoConfig` has `custom_prize_packs` and none of this step's four new fields. `RandoGen` has no stop-stage field.
- **H-9-7 — `shufflePrizePacks` is isolated.** It touches no location, shop, pre-collected item, collected flag, collectable cache or pool list. So `rando_prepare_pools` sees the state `setFountains` left, plus the packs, and swordless's `collectItems` is still the world's first.
- **H-9-8 — `test_prepare` helpers after 3.5.** `logic_world`, `seeded`, `count_draw`, `record_range` and the prize pack twin-PRNG pattern are present and reusable. *If it fails:* add the missing helper here.

## Out of Scope

- The fill, `isWinnable()`, the retry claim and `attempts` (step 2.4). So are `Equipped`, `meta` and `playthrough` in the C spoiler, and `pool.txt` on a complete run.
- Claiming `goal-pedestal`, `goal-triforce-hunt` or `goal-ganonhunt` at `prepareWorld`, and any gate notion of "surplus" items: the golden cannot show them.
- The inverted and retro worlds (Phase 3), `rom.genericKeys`' `Key` naming, and retro's random shops. Retro's `takeAnys`, `rupeeBow` and `wildKeys` branches are ported and unit-tested here, but not run by any case.
- A per-world `UncleSword` target (asserted instead), VT's `difficulty: custom` paths, and the Customizer.
- Regenerating any golden, changing `vt.patch`, the `claims.txt` or `stages.txt` formats, or the existing `pool_lists.txt`.

## Further Notes

- **Why a fixture and the gate both.** The trace pins every shuffle's draws but not what was shuffled. The `pool` check pins the multiset and the visible pre-fills for claimed cases, but not the order before each shuffle (which the fill will expose in step 2.4), the surplus cases, or a pre-fill whose item stayed in the pool. `prepared_pools.txt` pins all of that for 42 cases, in `make logic`.
- **Step note material** for `### Step 2.3.6`:
  - H-2-10's verdict and the gate refinement.
  - The new fixture and how it was produced.
  - `rando_generate_until`.
  - The new `sizeof(RandoWorld)` and `sizeof(RandoGen)`.
  - The claims raised, kept (`goal-pedestal`, `goal-triforce-hunt`, and why) and added.
  - The four breaks and their outcomes.
  - `test` and `e2e` times against the 0.5 s budget.
  - Where each `H-9-n` landed.
  - That Phase 2 step 3 is done.
