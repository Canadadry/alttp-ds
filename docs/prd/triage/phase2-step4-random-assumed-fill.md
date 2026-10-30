---
title: "Rando RandomAssumed fill, isWinnable() and the retry loop: the first full spoiler"
description: "After step 2.3.6 the C generator stops just before VT's fill, so no case can reach full spoiler equality, no attempt can fail after fillPrizes, and the retry loop has never run past the prize stage."
status: needs-triage
---

## Problem Statement

Plan step (Phase 2, step 4, verbatim):

> **`RandomAssumed` fill, `isWinnable()` and the retry loop.** Claims the baseline case: full spoiler equality and the same `attempts`; also claims the retry-seed cases. Every earlier stage claim is raised to `all` if it holds; one that does not stays at its stage and the step note names it for its Phase 3 row. No golden has a failed attempt (step 1.2 note), so the loop is also verified by the identity "attempt k of seed s is attempt 0 of seed s+k": seed 0 with attempt 0 forced to fail (host-only injection, once mid-fill, once at `isWinnable()`) must reproduce the baseline golden, with `attempts` 1.

After Phase 2 step 3.6 (committed; H-10-1 to H-10-9 checked against it), the C generator replays VT's draws through `prepareWorld` and stops with `RANDO_INCOMPLETE`, holding VT's four prepared lists. Everything after that is missing:

- **The fill.** VT's `RandomAssumed::fill` makes 573 of baseline's 805 draws. It shuffles the empty locations, places the dungeon items one by one with the rest of the pool assumed, junk-fills part of Ganon's Tower, places the advancement items in reverse order with the same assumed search, then fast-fills the nice items and the item pool. It throws in two places (more items than empty locations, no fillable location for an item).
- **`isWinnable()`.** The oracle wrapper rejects an attempt whose filled world cannot be won. Neither check exists in C.
- **The rest of the spoiler.** The C writer has the region sections, `Shops` and `Bosses`. `Equipped`, `meta` and `playthrough` are still a `TODO`. `playthrough` is VT's `PlaythroughService`: spheres on a shadow copy of the world, a pruning pass, then a replay.
- **The retry loop past the prizes.** The loop exists, but its only failure path is `fillPrizes`, and it is only unit-tested. No golden has a failed attempt (step 1.2 note), so the e2e gate can never see a retry.

So no claim can be `all`. The gate's full byte comparison of `spoiler.json`, `attempts` and the whole trace has never run on a real case.

## Solution

1. **World queries for the fill and the win check.** VT's `collectOtherItems` (unlike `collectItems`, it does not merge the pre-collected items again). The bare win condition on an item multiset. `WorldCollection::isWinnable` for one world. An ordered pre-collected list beside the multiset, because `Equipped` numbers the slots in insertion order. VT's `World::copy` for the shadow world.
2. **A filler module** in the library, ported from `RandomAssumed::fill` and `Filler`. It has two calls in VT's order: the dungeon phase (location shuffle and dungeon items), then the rest (Ganon's Tower junk, advancement, nice, item pool). Each returns false where VT throws.
3. **The generator** runs stage `fill` after `prepareWorld`, then `isWinnable()`. A false return from either fails the attempt, and the loop retries with the next seed offset, as the retry contract says. `rando_generate` now returns `RANDO_COMPLETE`. Host builds (and only host builds) get a fault-injection point between the two fill phases and one at `isWinnable()`.
4. **Stage claims keep their meaning.** `run_e2e.sh` passes a stage claim's stage to the generator as a third argument. `rando_gen` then stops after that stage with `rando_generate_until` and writes the same partial `spoiler.json` and `pool.txt` as before. An `all` claim passes no stage and gets the full run.
5. **The full spoiler.** The host spoiler writer gains `Equipped` and `meta`. A new host-only playthrough module ports `PlaythroughService::getPlayThrough`, and the writer serializes its output. `rando_gen` writes the full spoiler and `attempts` on a complete run.
6. **The retry identity test** in `make logic`. It runs seed 0 with attempt 0 forced to fail, once mid-fill and once at `isWinnable()`. In both runs the second attempt's trace must be the baseline golden trace with the attempt column set to 1, the spoiler must be byte-identical to the golden's, and `attempts` must be 1.
7. **The e2e budget**, settled before the claims move. The generator used by e2e is built optimized. If that is still not enough, the claims' generators run in parallel. Each change gets its own commit, measured against the 0.5 s budget.
8. **Claims.** `baseline all`. Each of the other eighteen claims is raised to `all` if it holds. One that does not stays at its stage, and the step note names it with its Phase 3 row. The retry-seed claim is empty: no retry-seed case exists (step 1.2 note).

`make -C tests/rando e2e` then prints `PASS baseline (all)` and up to 18 more `(all)` lines, with 25 `PENDING`, under 0.5 s.

## User Stories

1. As the e2e gate, I want `baseline all` to pass, so that the C port reproduces VT's spoiler, trace and `attempts` for the default preset byte for byte.
2. As the e2e gate, I want every claim that held at the end of step 2.3.6 raised to `all` when it holds, so that the ratchet moves every claim as far as it can go.
3. As the e2e gate, I want a claim whose `all` comparison fails kept at its stage, with the step note naming its Phase 3 row, so that the ratchet never moves down and the failure has an owner.
4. As the e2e gate, I want a stage claim to run the generator only up to that stage, so that its trace prefix, partial spoiler and `pool.txt` mean exactly what they meant in steps 2.3.x, whatever the fill does.
5. As the e2e gate, I want an `all` claim to run the generator to completion and require exit status 0, so that a generator that gives up or stops early is `DIVERGENT`.
6. As the e2e gate, I want `make e2e` under 0.5 s with every `all` claim, so that the loop stays fast (CLAUDE.md, § Convergence gate).
7. As a C port developer, I want the empty locations shuffled with VT's `randomCollection(count)`, in world location order, so that the fill's first 216 baseline draws match.
8. As a C port developer, I want the dungeon items placed in list order, each on the first fillable location of the shuffled order, with the remaining dungeon items plus the advancement and nice lists assumed, so that the dungeon placement is VT's.
9. As a C port developer, I want maps and compasses placed on a random fillable location (one draw over the fillable count) and every other item on the first one, so that the 23 baseline map and compass draws match.
10. As a C port developer, I want each placement's assumed items built as VT builds them: the remaining fill items minus the current one, plus the base items, plus the pre-collected items, then `collectOtherItems` repeated until the collected count stops growing, after a reset of the collected locations, so that every canFill sees VT's item multiset.
11. As a C port developer, I want `collectOtherItems` ported apart from `collectItems`, so that the pre-collected items (three `BossHeartContainer`) are counted once and not twice.
12. As a C port developer, I want `accessibility: none`'s skip of access checks ported (no dungeon item unless its wild flag is set, and the win condition holds on the assumed items), so that the function is VT's even though no claimed case reaches it.
13. As a C port developer, I want VT's two exceptions to return false: more fill items than empty locations, and no fillable location for an item, so that both fail the attempt as VT's do.
14. As a C port developer, I want Ganon's Tower junk ported: one draw in `getGanonsTowerJunkFillRange()` (that is `0..0` under NoLogic and the three non-inverted glitched logics), `randomCollection` over the tower's empty locations in region order, a shuffle of the item pool, the first k items popped from the end onto those locations, so that the junk draws and placements match.
15. As a C port developer, I want the advancement items shuffled and placed over the remaining empty locations in reverse shuffled order, with no base items, so that the required phase is VT's.
16. As a C port developer, I want the remaining empty locations reshuffled, the nice list shuffled and fast-filled (popping from the end), then the rest of the item pool shuffled and fast-filled on what is still empty, so that the last draws and the unplaced surplus (`goal-pedestal`, `goal-triforce-hunt`) are VT's.
17. As a C port developer, I want every fill draw labelled `fill` in the trace, so that a divergence reads as "fill, draw #K".
18. As a C port developer, I want `isWinnable()` ported as a reset of the collected locations, `collectOtherItems` from the pre-collected items until the count is stable, then the win condition on the result, so that the attempt check is VT's.
19. As a C port developer, I want a failed fill phase or a lost world to fail the attempt, after which the loop reseeds with `splitmix32(seed + attempt)` and rebuilds everything, so that the retry contract holds past `fillPrizes`.
20. As a C port developer, I want pre-collected items added in order through one world call, so that `Equipped` and the playthrough's slot 0 number them as VT does.
21. As a C port developer, I want `Equipped` written from the pre-collected items in insertion order, skipping arrow upgrades, bomb upgrades and events, with each item's target name, so that `weapons-assured` writes `ProgressiveSword:1` in slot 4.
22. As a C port developer, I want `meta` written with VT's keys, values and JSON types, so that a drawn crystal count is an integer while a fixed one stays the config's string, and `entry_crystals_*` holds the case's config strings.
23. As a C port developer, I want the playthrough to copy the world, strip VT's 24 junk items from the copy's locations, build spheres starting from the equipment slots, prune spheres from the outermost in, and replay the required locations in rounds, so that `playthrough` matches.
24. As a C port developer, I want the playthrough to check access against the real world wherever VT passes `$world->getLocations()`, and against the shadow world elsewhere, so that the key rules read the same locations VT reads.
25. As a C port developer, I want round entries written with each item's raw name, and the slot-0 equipment written with its raw name skipping health too, so that `UncleSword:1` appears where VT writes it.
26. As a C port developer, I want rounds that hold only `Trade` locations dropped with `longest_item_chain` decremented, and the round keys left with their gaps, so that VT's numbering is kept.
27. As a C port developer, I want `regions_visited` computed as VT computes it (regions per round plus one for slot 0), so that the counter matches.
28. As a test author, I want host-only fault injection that fails a chosen number of first attempts between the two fill phases or at `isWinnable()`, and records whether it fired, so that the retry path is exercised without any golden.
29. As a test author, I want the injection compiled out of every build that does not ask for it, so that the DS build carries none of it.
30. As a maintainer, I want the retry identity test to show that seed 0 with attempt 0 failed mid-fill, and again at `isWinnable()`, reproduces baseline's spoiler and trace at attempt 1, so that no state leaks from a failed attempt into the next.
31. As a maintainer, I want the fill, the world queries and the playthrough unit-tested on small hand-built worlds in `make test`, so that each VT branch has a fast, local test.
32. As a maintainer, I want full generations kept out of `make test`, so that the fast loop stays fast (CLAUDE.md).
33. As a maintainer, I want the e2e time measured (median of five runs) before and after each change and recorded against the 0.5 s budget, so that the step note carries it like every earlier one.
34. As a maintainer, I want every supported case run once with a temporary all-claims file, with the result recorded in the step note, so that Phase 3 starts from known per-case outcomes.
35. As a maintainer, I want five deliberate breaks run and their outcomes recorded, so that the step note says which guard owns which mistake.
36. As the author of Phase 2 step 5, I want the fill's working state and the playthrough's storage described (which are stack, which are static), so that the host measurement knows what to count.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. They rest on the plan's Rules (retry contract, § Convergence gate with the ratchet and the e2e budget, § Comparison), step notes 1.1–2.3.6, the step 2.3.6 PRD, CLAUDE.md, the committed harness and library, and VT's source at the pinned commit. Every assumption about step 3.6's outcome is labelled `H-10-n` (see Hypotheses).

**Facts read from VT, the oracle and the goldens (not hypotheses)**
- `Randomizer::randomize` calls `prepareWorld`, then `fill($dungeon_items, $advancement_items, $nice_items, $trash_items)`. Texts, credits and hints are commented out by `vt.patch`. `gen_case.php` then calls `WorldCollection::isWinnable()`. An exception in either one fails the attempt. After the loop it calls `getSpoiler` on the winning world, drops `meta.build`, sorts every non-list array's keys (`ksort SORT_STRING`) and pretty-prints with unescaped slashes and unicode.
- No RNG call after the fill. `isWinnable`, `getSpoiler` and `PlaythroughService` draw nothing, and neither does `World::copy` (its `new static` builds regions without drawing). So the golden trace ends with the fill: baseline's 573 `fill` draws are lines 233–805.
- The fill's draws, in order (baseline counts in brackets):
  1. `randomCollection(n)` over the world's empty locations in location order [216].
  2. One `random()` over the fillable locations for each map and compass, in dungeon-list order [23].
  3. `get_random_int(lo, hi)` for the junk count [1].
  4. `randomCollection(k)` over Ganon's Tower's empty locations in region order [k = 0 in baseline].
  5. `fy_shuffle` of the item pool [70].
  6. `fy_shuffle` of the advancement list [43].
  7. `randomCollection` over the remaining empty locations [110 − k].
  8. `fy_shuffle` of the nice list [40].
  9. `fy_shuffle` of the rest of the item pool [70 − k].
- `fastFillItemsInLocations` walks the locations in order, skips filled ones and pops items from the end of its list. `fillItemsInLocations` walks its items in list order. `ItemCollection::removeItem` removes one count and the first matching value. `count()` sums the counts.
- `collectOtherItems` differs from `collectItems`: it does not merge the pre-collected items, it returns only what it found, and it tests `hasItem` inside the loop. Both read the collectable set that the first `collectItems` call cached. That first call is `fillPrizes`' first win check, so the cache exists before the fill.
- `getSpoiler`:
  - `Equipped`: the pre-collected items in insertion order (`ItemCollection` iterates `cached_values`), minus `Upgrade\Arrow`, `Upgrade\Bomb` and `Event`. Slots are numbered from 1, and each value is `getTarget()->getName()`. The section is absent when no item qualifies.
  - `meta`: the oracle's three passed keys (`entry_crystals_ganon`, `entry_crystals_tower`, `worlds`) merged with VT's list.
  - `playthrough`: `PlaythroughService::getPlayThrough($world)`.
- Goldens:
  - `crystals_ganon-random` has `meta.crystals_ganon` = `5` (integer) and `entry_crystals_ganon` = `"random"`. Fixed counts are the strings `"7"` and so on.
  - `item_pool-crowd_control`'s `item_placement` is `"advanced"`.
  - `goal-completionist`'s `accessibility` is `"locations"`.
  - `glitches-major_glitches` has `rom_mode` = `logic`.
  - `weapons-assured` has `Equipped` slot 4 `ProgressiveSword:1` and playthrough slot 0 `UncleSword:1`.
  - `weapons-vanilla`'s playthrough has `Link's Uncle:1` = `UncleSword:1`.
  - Five goldens have a playthrough `"0"` key.
- `PlaythroughService`:
  - The shadow world is `World::copy()`: fresh collected state and collectable cache, the same items, shops, bosses and an ordered copy of the pre-collected items.
  - The junk strip walks world locations only, not shop slots.
  - `getLocationSpheres` checks access against the shadow's own locations. The pruning pass and the final replay pass `$world->getLocations()`, the real world's.
  - Shop slots are fresh `Location`s whose region is the real world's region, so their access with no locations argument also reads the real world.
  - `Prize\Event` locations (`Agahnim 2`, `Ganon`) appear in rounds. `Trade` locations are filtered only at output.
  - Rounds skip keys (unless wild keys without generic keys), maps, compasses and `RescueZelda`.
- At HEAD before this step the gate's `all` mode compares `attempts`, the whole trace and `spoiler.json` byte for byte, and requires generator exit 0. It needs no change for this step.
- Measured on this container (4 cores):
  - At the step 2.3.5 commit: on baseline's golden-filled world, one full `collectItems` ≈ 200 µs at `-O0` and ≈ 100 µs at `-O2`; one canFill sweep of the empty locations ≈ 46 µs at `-O0` and ≈ 17 µs at `-O2`.
  - At the step 2.3.6 commit (`54ec1a5`): `make e2e` median 0.18 s over five runs (0.17–0.20; the note says ~0.17 s) at 19 stage claims, about 9 ms per claim. One `rando_gen` baseline run that stops after `prepareWorld` ≈ 3.2 ms, process included. One `all` claim whose generator only copies the golden costs about 16 ms of e2e in total, almost all of it fixed `run_e2e.sh` startup, so the `all` comparison itself costs about 1–3 ms per claim.
  - Projection for an `all` claim: about 6 ms (generator up to `prepareWorld`, process, `all` comparison) plus the fill and the playthrough. The fill's 106 searched placements cost about 15–30 ms at `-O0`, and the playthrough is of the same order, so X ≈ 15–45 ms at `-O0` and ≈ 8–22 ms at `-O2`.
  - With 19 `all` claims, that gives 0.4–1.0 s at `-O0`, over budget, and 0.27–0.53 s at `-O2`, at or near the 0.4 s alarm. The real 0.18 s at 19 stage claims confirms the fixed and per-claim costs the projection uses: today's stage claims cost more per claim (the `pool` check) than an `all` comparison, so the gate is not the problem; the fill is.

**World (library, extended)**
- `void rando_world_add_pre_collected(RandoWorld *, RandoItemId)` appends to an ordered list in `RandoWorld` (`RANDO_PRE_COLLECTED_MAX` 16, asserted) and to the multiset. Every pre-collect goes through it: start equipment, Boots, `RescueZelda`, and 3.6's assured `UncleSword` (H-10-3). A read accessor returns the count and the i-th item. `rando_world_init` clears both. Decided over a fixed VT insertion order in the host writer, which would silently break when a pre-collect moves.
- `bool rando_world_win_condition(const RandoWorld *, const RandoItems *)`: VT's closure on the given items, with no collection. `rando_world_check_win_condition` becomes collect, then this.
- `void rando_world_collect_other_items(RandoWorld *, const RandoItems *collected, RandoItems *found)`. Contract: no pre-collected merge, `found` holds only new items, and world locations and shop slots are marked collected as `collectItems` does. Trap comment on it (at most 40 words): the fill and `isWinnable` must use this one, because `collectItems` would count the pre-collected items twice.
- `int rando_world_collected_count(const RandoWorld *)`: collected locations plus collected shop slots, for VT's "until the count stops growing" loop.
- `bool rando_world_is_winnable(RandoWorld *)`: VT's `WorldCollection::isWinnable` for one world. It mutates the collected state only.
- `void rando_world_copy(RandoWorld *dst, const RandoWorld *src)`: VT's `World::copy`. Everything is copied by value, the collected state is cleared, and the collectable cache is reset so that it is recomputed on first use.
- `bool rando_world_can_access_shop(const RandoWorld *, RandoShopId, const RandoItems *)` is made public. It is today's static helper, needed by the host playthrough.
- Shop replacement items (`slot.2`) stay unmodelled, as in today's collect. No matrix case outside retro has one; Phase 3 owns it.

**Filler (library, new module, the plan layout's `filler.c`)**
- `RandoFillState` is caller-owned: the shuffled location order (at most `RANDO_LOCATION_COUNT` ids as `uint8_t`) and its length.
- `bool rando_fill_dungeon(RandoWorld *, RandoPrng *, const RandoPools *, RandoFillState *)`: the location shuffle and `fillItemsInLocations(dungeon, order, advancement ∪ nice)`.
- `bool rando_fill_rest(RandoWorld *, RandoPrng *, RandoPools *, RandoFillState *)`: Ganon's Tower junk, the reversed advancement phase, the reshuffle, nice, then the item pool. It consumes the pools: their contents are unspecified on return.
- Both return false exactly where VT throws, leaving the world as it stands. Two calls rather than one, because VT's fill has a natural boundary there (the "random junk fill" comment), the mid-fill fault point needs one, and the dungeon phase can be unit-tested alone.
- Item multisets are built with the two-view `RandoItems` operations VT's calls map to: `removeItem` → `rando_items_remove`, `merge` → `rando_items_merge`. Item classes decide map, compass, key and big key, and the wild flags come from the config.
- Working state lives on the stack: a few `RandoItems` (about 716 bytes each) plus small index arrays, under 4 KB per call. No static mutable state. The step note records the figure for Phase 2 step 5.
- `getGanonsTowerJunkFillRange` is ported as written, including `pedestal` and `triforce-hunt`'s `floor(15·t/7)..floor(25·t/7)` and the inverted exception, as a static helper.

**Generator (library, modified)**
- `run_attempt`, after `prepareWorld`: `end_stage` (as every earlier stage does, so `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)` still stops there), then stage `RANDO_STAGE_FILL`, `rando_fill_dungeon`, the mid-fill fault point, `rando_fill_rest`, stage none, the `isWinnable` fault point, then `rando_world_is_winnable`. Any false fails the attempt (`return false`), so the existing loop reseeds and rebuilds the world and the pools. A full success returns `RANDO_COMPLETE`. `end_stage` runs after `rando_fill_rest`, so `RANDO_STAGE_FILL` honours `rando_generate_until` and stops before `isWinnable`.
- `RandoStatus`'s comment changes: `RANDO_INCOMPLETE` now comes only from `rando_generate_until`.
- **Fault injection, host only.** Under `#ifdef RANDO_FAULTS`, `RandoGen` gains `RandoFault fault` (`RANDO_FAULT_NONE` = 0, `RANDO_FAULT_MID_FILL`, `RANDO_FAULT_IS_WINNABLE`), `uint32_t fault_attempts` (the fault fires on every attempt below it) and `uint32_t fault_fired`. The generator reads `fault` and `fault_attempts` and never writes them, so a zeroed `RandoGen` has no fault; it only increments `fault_fired`. `tests/rando`'s Makefile defines `RANDO_FAULTS` for every host object. The DS build does not. Decided over a function-pointer hook (more surface for the same two points) and over `setjmp` out of the fill (unwinding that VT's exceptions do not need in C).

**Gate and host driver**
- `run_e2e.sh`: a stage claim runs `$RANDO_GEN <case_dir> <out_dir> <stage>`. An `all` claim runs it with two arguments. This changes step 2.1's generator contract, and `run_e2e.sh`'s usage comment states it. Without it, a claim left at `prepareWorld` would get a filled spoiler and no `pool.txt`, so it would go `DIVERGENT` for reasons outside its stage.
- `rando_gen <case_dir> <out_dir> [<stage>]`. A stage name maps to its `RandoStage` through `rando_stage_name`; an unknown name exits with usage status 2. With a stage, it calls `rando_generate_until` and writes `spoiler.json` and `pool.txt` exactly as 3.6 does (H-10-5). Without one, it calls `rando_generate`, and on `RANDO_COMPLETE` computes the playthrough and writes the full `spoiler.json` and `attempts`.
- `selftest.sh` gains a fake generator that records its arguments. It asserts three arguments, with the claim's stage, for a stage claim, and two for an `all` claim. Every existing verdict and message is unchanged.

**Spoiler writer (host, extended)**
- Signature: `bool rando_spoiler_write_json(FILE *, const RandoWorld *, const RandoOptions *, const RandoPlaythrough *playthrough)`. With `playthrough` NULL, no `playthrough` key is written (partial runs). `Equipped` and `meta` are always written: nothing in a stage claim reads them, because the `pool` check excludes both sections and no `keys` glob matches their keys.
- `meta`: the 27 keys of the golden, sorted bytewise. String values map the config enums to VT's strings: `itemPlacement`, `item.pool`, `item.functionality`, `dungeonItems`, `logic` (`NoGlitches`…), `accessibility` (after completionist's switch), `rom_mode` (= `logic`), `goal`, `mode` (`mode.state`) and `weapons`. Literals: `hints` `"on"`, `spoilers` `"on"`, the five `enemizer.*` values, `size` 2, `world_id` 1, `worlds` 1, `tournament` false, `allow_quickswap` true, `pseudoboots` false. `crystals_*` is the resolved integer when the option is random, otherwise the option's string. `entry_crystals_*` is the option's string. Any option value-name table that is missing is added to the options module.
- `Equipped`: from the ordered pre-collected list, as in the facts above.
- `playthrough`: keys `"0"` (only when slot 0 has an item), the round numbers, `longest_item_chain` and `regions_visited`, all sorted bytewise. Each round object maps region spoiler names to location-key → raw-item-name objects. Location keys get the `:1` suffix the region sections use; shop slots are `"<shop> - <slot>:1"`.

**Playthrough (host, new module)**
- `bool rando_playthrough(const RandoWorld *world, RandoPlaythrough *out)`. It computes VT's `getPlayThrough($world, true)` and never modifies `world`. Storage is a static shadow `RandoWorld` and static arrays in the host module, which is allowed in host code and keeps `RandoPlaythrough` itself small. It returns false only when an internal capacity assert would be hit, which is asserted.
- `RandoPlaythrough` holds the slot-0 items (raw ids), and for each kept round its number and its entries (region spoiler name, location or shop slot, raw item id), plus `longest_item_chain` and `regions_visited`.
- A pseudo-location is a world location id, a shop slot, or an equipment slot. Spheres, `collectable_locations`, `required_locations` (with insertion-ordered sphere keys, since VT walks `array_reverse(array_keys(...))`) and `location_order` are arrays of pseudo-locations. VT's loose `in_array` on locations is identity here.
- Access: sphere building uses the shadow world for world locations and the real world for shop slots. Pruning and replay use the real world for both. The win condition runs on the shadow world. "Put back" reads the real world's item (for shop slots, the real world's cached collectable shop item).
- `break 2` in the bubbling pass leaves the two inner loops only, then "put all higher items back" runs for that check sphere. The port keeps that control flow line for line. A trap comment (at most 40 words) sits on the pass.

**e2e budget (decided before the claims move)**
- Order of remedies. Each gets its own commit, `selftest` stays unchanged, and the median of five runs goes in the step note:
  1. **Always:** the generator that e2e runs is built with `-O2` from its own object directory. Unit tests, `logic` and `selftest` keep today's flags. Same sources, so a divergence that shows only at `-O2` is a real bug for `fix-bug`.
  2. **Only if the projected median is still above 0.4 s:** `run_e2e.sh` runs the claimed cases' generators concurrently (at most the online CPU count, capped at 8), then evaluates and prints verdicts in case order. Output must be byte-identical, which `selftest` proves.
- The projection is measured with a temporary claims file (`RANDO_CLAIMS`) holding all nineteen claims at `all`, before `claims.txt` changes.
- Stage claims stop at their stage (see Gate), so they do not pay for the fill.

**Claims (`claims.txt`, ratchet)**
- `baseline all`: required. If it does not hold, the step is not done.
- Each other claim in the 3.6 claims file (H-10-4) is raised to `all` when it passes, including `goal-pedestal` and `goal-triforce-hunt`: the full comparison has no surplus problem. A claim that fails `all` keeps its 3.6 stage. The step note names it with its Phase 3 row (for example `glitches-overworld_glitches` → the `glitches` `overworld_glitches` step).
- No new case is claimed: the 25 pending cases belong to their Phase 3 rows. The retry-seed claim is empty.

## Implementation Chunks

1. **World queries** (`feat:`). Ordered pre-collected list and `rando_world_add_pre_collected` (every pre-collect rerouted), `win_condition`, `collect_other_items`, `collected_count`, `is_winnable`, `copy`, public shop access. `test_world` cases. Nothing calls the new queries yet; e2e unchanged.
2. **Filler** (`feat:`). The two fill calls and the junk range helper. New `test_filler` registered in `UNIT_TESTS` (not in `LOGIC_TESTS`). e2e unchanged.
3. **Stage argument** (`test:`). `rando_gen`'s optional stage, `run_e2e.sh` passing it for stage claims, and the `selftest` argument fixture. e2e output unchanged: the generator still stops at `prepareWorld`.
4. **Fill stage, isWinnable and retry** (`feat:`).
   - `run_attempt` wiring, `RANDO_FAULTS`, the Makefile define, `RandoStatus`'s comment.
   - `test_generate`: the H-10-6 expectations move to `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)`.
   - `--fixtures` gains the full-generation checks and the retry identity test on traces and `attempts`.
   - `rando_gen` writes the region sections, `Shops`, `Bosses`, `Equipped` and `meta` plus `attempts` on a complete run (no playthrough yet).
   - e2e is unchanged: every claim is still a stage claim and stops at its stage.
5. **Spoiler completion** (`feat:`). `Equipped`, `meta`, the playthrough module and its serialization, and the new writer signature. `test_spoiler_json` and new `test_playthrough` cases. The retry identity test gains the byte-equal spoiler. Checked with a temporary `baseline all` claims file: it must pass before chunk 6.
6. **Budget, then claims.**
   - Measure the nineteen-claim projection. Make the `-O2` generator commit (`perf:`), and the parallel commit only if needed.
   - Raise the claims in `claims.txt` (`test:`).
   - Run the all-cases survey and the five deliberate breaks, and record the timings.

## Testing Decisions

- **A good test checks external behaviour:**
  - the world's location items and pre-collected order;
  - the draws an observer sees;
  - the generator's trace, status and attempt;
  - the spoiler bytes;
  - the gate's verdict lines.
  No test reads a fill-internal order array, a playthrough sphere, or a static buffer.
- **VT is the oracle.** Hand-written expectations restate VT's source, a golden line or a fixture line, and cite it. A hand-written case that contradicts a golden is the test's bug (CLAUDE.md).
- **`make test` stays fast.** Only hand-built cases go into it: small worlds, lists of at most a few dozen items, and no full generation. The step note records `make test` before and after. Full generations go to `make logic` (CI runs it) and to e2e.
- **`test_world` (extended, chunk 1)**, prior art `logic_world` and the existing collect tests:
  - Ordered pre-collect: three hearts, `BombUpgrade10`, `PegasusBoots`, `UncleSword` read back in order, with the multiset counts unchanged from `rando_items_add`.
  - `collect_other_items`: on a world with three pre-collected hearts it returns only placed items. A heart-dependent rule sees three hearts, not six.
  - `collected_count` covers shop slots.
  - `win_condition`: the Triforce alone wins. Triforce-hunt needs `item_goal_required` pieces and North East Light World entry.
  - `is_winnable`: false on a fresh world with Ganon's Triforce out of reach. True with the Triforce on an always-reachable location. True after pre-collecting everything.
  - `copy`: same items, bosses, shops and pre-collected order; collected state cleared; the cache recomputed. The source is untouched.
- **`test_filler` (new, chunk 2).** Each case fills all but a handful of a world's locations by hand, then runs the fill on hand-built lists. Draws are checked with an observer and a twin PRNG in VT's order, prior art the prize-pack twin-PRNG tests.
  - The location shuffle draws once per empty location, in location order.
  - A dungeon item goes to the first fillable location of the shuffled order. A map draws `0..fillable−1` and lands on that index.
  - More dungeon items than empty locations: false, with no draw after the shuffle.
  - A `BigKeyP1` with no empty Eastern Palace location: false (`No Available Locations`).
  - Junk range: `0..floor(15·t/7)` for ganon, `floor(15·t/7)..floor(25·t/7)` for pedestal and triforce-hunt, `0..0` for NoLogic and overworld glitches. k items land on the tower locations popped from the end of the shuffled pool.
  - The advancement phase walks the reversed order.
  - The fast fills pop from the end and leave the surplus unplaced when items outnumber locations.
  - `accessibility: none` with the win condition already met places an unreachable item. A non-wild key still gets the access check.
- **`test_generate` (chunk 4, H-10-6):**
  - In `make test`, every expectation that today calls `rando_generate` and expects `RANDO_INCOMPLETE` calls `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)` instead: baseline's 232 lines, the 233-line random-crystal cases, the 234-line `check_crystals_order` case, `state-standard`'s 232, `check_reseed`'s unbounded call (`gen.draw == 232`), and `check_stop_stage`'s last call. Otherwise seed 12345's unbounded call would run the fill and could retry. A `_until(…, RANDO_STAGE_FILL)` case on baseline gives 805 lines and the golden's prefix, read with `check_golden_stop` (next stage: none, end of file). That is one full fill, the only one allowed in `make test`, at about the cost the projection gives. If it measures above 15 ms, it moves to `--fixtures`.
  - In `--fixtures`, `rando_generate` on baseline: `RANDO_COMPLETE`, attempt 0, the whole golden trace.
  - `Trace` holds 32 768 bytes today. A full golden trace is at most 22 176 bytes (`dungeon_items-full`), but the identity test records a failed attempt plus a full one, so `TRACE_MAX` goes to 65 536 (the buffers are static or on the host stack).
  - **Retry identity**, in `--fixtures`. A zeroed `RandoGen` with `fault_attempts` 1, seed 0, unbounded attempts, run once with `RANDO_FAULT_MID_FILL` and once with `RANDO_FAULT_IS_WINNABLE`. Each run expects `RANDO_COMPLETE`, `gen.attempt == 1` and `fault_fired == 1`. The attempt-1 trace lines must equal the golden baseline trace with the first column 0→1. The attempt-0 lines must end in a `fill` draw. From chunk 5, the written spoiler must be byte-equal to the golden's, and the written `attempts` must be `1`.
  - **Fallback.** If seed 0's attempt 0 fails on its own before the fault point (`fault_fired == 0`), the natural retry is recorded as the identity. A second run, seed `0xFFFFFFFF` with `fault_attempts` 2, keeps the injection exercised (`(seed + attempt) & 0xFFFFFFFF` = 1 at attempt 2, so `attempts` 2). The step note records which path ran.
- **`test_spoiler_json` (extended, chunk 5):**
  - `Equipped` from a hand-built pre-collected order. Hearts plus Boots plus `UncleSword` gives slots 1–5 with `ProgressiveSword:1` last; upgrades and `RescueZelda` are skipped. With only upgrades, no `Equipped` key.
  - `meta`: fixed crystals give strings, random crystals give an integer plus `"random"` entries, and crowd control gives `item_placement` `"advanced"`.
  - A NULL playthrough writes no `playthrough` key.
  - Key order is bytewise across the new sections.
- **`test_playthrough` (new, chunk 5)**, on hand-built worlds:
  - Everything pre-collected and only the event locations filled: round 1 is `Dark World` / `Ganon:1` = `Triforce:1`. Slot 0 lists the non-health, non-upgrade, non-event items with raw names (`UncleSword:1`). `longest_item_chain` 1, `regions_visited` 2.
  - A junk item on a reachable location never appears.
  - A round made only of `Pyramid Fairy - Bow` is dropped and decrements the chain.
  - A key, map or compass is replayed but not listed.
  - A required location whose item is needed for a later sphere is kept by the bubbling pass.
- **`selftest.sh` (chunk 3):** the argument-recording fake generator.
- **e2e (chunk 6):** `baseline all` plus every claim that holds.
- **Deliberate breaks** (each reverted, not committed; the step note records each outcome):
  1. Fast fill from the front instead of popping from the end. The trace is unchanged and `baseline` is `DIVERGENT` on `spoiler.json`: placements that draw nothing are owned by the spoiler comparison.
  2. The fill uses `collectItems` (pre-collected counted twice) instead of `collectOtherItems`. The outcome is recorded either way. If every claim stays green, the note says that only `test_world`'s heart case owns it.
  3. Skip the map and compass `random()` draw. `baseline` is `DIVERGENT` at draw 449 (fill draw 217), in `fill`.
  4. `rando_world_copy` keeps the collectable cache, or the generator stops reinitializing the world between attempts. The retry identity test fails in `make logic` while every e2e claim stays green: only the identity test owns cross-attempt state.
  5. Round entries with target names. `weapons-vanilla` (if at `all`) is `DIVERGENT` on `Link's Uncle:1`. Otherwise `test_playthrough` fails.

## Hypotheses

Step 3.6 landed while this PRD was written (`07359e9`, `e96c2db`, `db23a53`, `d6fff45`, `9d0da71`, note `54ec1a5`). Each item below was checked against `54ec1a5`. Verdicts are recorded here; the implementer re-checks them before starting and copies them into the step note.

- **H-10-1 — Generator after 3.6. Holds.** `run_attempt` ends with stage `prepareWorld` (`rando_prepare_pools(&gen->world, &gen->prng, &gen->pools)`), resets the stage and returns `RANDO_INCOMPLETE`. `rando_generate_until` and `RandoGen.last` exist. One detail the PRD now uses: every stage before `prepareWorld` ends with `end_stage(gen, status)`, which returns `RANDO_INCOMPLETE` when the stage is `last`, and `prepareWorld` does not call it yet because it is the last stage. This step adds `end_stage` after `prepareWorld` and after `fill`, so `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)` and `(…, RANDO_STAGE_FILL)` both stop as documented.
- **H-10-2 — Prepared pools after 3.6. Holds.** `make logic` prints "42 prepared pool records replayed, 2 skipped (no world)". The replay compares all four lists in order through `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)` and checks that the golden's next stage is `fill`.
- **H-10-3 — World before the fill. Holds.** `rando_prepare_pools` places `Pyramid Fairy - Bow` (`BowAndArrows`, or `BowAndSilverArrows` under the upgrade flag) and the other pre-fills. Assured's uncle sword is pre-collected with `rando_items_add(&world->pre_collected, uncle)`, as are the start equipment, Boots and `RescueZelda`. All four calls go through `rando_world_add_pre_collected` in chunk 1.
- **H-10-4 — Gate after 3.6. Holds.** `stages.txt` ends with `check prepareWorld pool`. `claims.txt` holds 17 claims at `prepareWorld` and `goal-pedestal` and `goal-triforce-hunt` at `shufflePrizePacks`. `make e2e` prints 19 pass, 0 divergent, 25 pending, with a median of 0.18 s here (the note says ~0.17 s).
- **H-10-5 — `rando_gen` after 3.6. Holds.** It writes `pool.txt` through `rando_spoiler_write_pool` only when the status is `RANDO_INCOMPLETE`, after `spoiler.json`. `rando_spoiler_write_json` still takes `(FILE *, const RandoWorld *)`; chunk 5 changes it.
- **H-10-6 — `test_generate` after 3.6. Partly fails.** As assumed, it expects 232, 233 and 234 lines with `RANDO_INCOMPLETE` from `rando_generate`. Three things differ from the assumption, and Testing Decisions now covers them:
  - The `prepared_pools.txt` replay already calls `rando_generate_until(…, RANDO_STAGE_PREPARE_WORLD)`, not the full generator. It needs no change.
  - `check_reseed` also calls `rando_generate` with unbounded attempts and expects `gen.draw == 232`, and `check_stop_stage` ends with a full `rando_generate` call (232 lines). Both move to `_until`.
  - `TRACE_MAX` is 32 768. That fits one full trace but not a failed attempt plus a full one, so it grows. A `check_golden_stop` helper (prefix plus the golden's next stage) also exists; the PRD reuses it.
- **H-10-7 — Layouts after 3.6. Holds.** `RandoConfig` has `region_pyramid_bow_upgrade`, `region_boss_hearts_in_pool`, `region_bosses_have_item` (`RandoItemId`) and `region_take_anys`. The 2.3.6 note gives `RandoWorld` 2 748 bytes and `RandoGen` 3 896. This step's note gives both again after the ordered pre-collected list (about 17 bytes) and the host fault fields (12 bytes).
- **H-10-8 — Helpers after 3.6. Holds.** `test_prepare` has `seeded`, `count_draw`, `record_range` and `logic_world`. `test_generate` has `Trace`, `record`, `check_golden_prefix` and `check_golden_stop`.
- **H-10-9 — Stage labels after 3.6. Holds.** `prepareWorld`'s shuffles are labelled `prepareWorld` (baseline line 232 is `0 232 prepareWorld 0 0 -> 0`). `RANDO_STAGE_FILL` is declared but nothing uses it. `stages.txt` ends its stage list with `fill`, and no check line names it.

## Out of Scope

- New claims for the 25 pending cases. Each goes with its Phase 3 row, and `accessibility-none`'s skip path is ported and unit-tested only.
- The inverted and retro worlds, `rom.genericKeys`, shop replacement items, and retro's random shops.
- Multiworld (`WorldCollection` with more than one world), the Customizer, `difficulty: custom`, and VT's `$walkthrough = false` branch.
- Texts, credits, hints, the ROM writer, and the ARM9 build or integration (roadmap items 2 and 3).
- Phase 2 step 5's host measurement. This step only records the fill's stack estimate and the struct sizes.
- Regenerating any golden, changing `vt.patch`, or changing the `claims.txt` and `stages.txt` formats.
- Any VT-unfaithful speed-up of the fill or the playthrough (caching access results, early exits VT does not take). The budget is met by build flags and parallelism only.

## Further Notes

- **Why the stage argument.** After this step a full run always fills the world. Without the argument, a claim left at a stage would compare a filled spoiler, and lose `pool.txt`, against checks written for a partial one. The argument keeps every stage claim on exactly the run it was proven on, and it saves those claims the fill's cost.
- **Why the retry identity test is in `make logic`.** It is two full generations plus playthroughs and two golden reads. That is not a fast-loop check (CLAUDE.md), but CI runs `logic`, and break 4 shows that it is the only guard on state carried across attempts.
- **Step note material** for `### Step 2.4`:
  - The world queries, the filler, the stage argument and the new generator contract.
  - `RANDO_FAULTS`, and which identity path ran (seed 0, or the `0xFFFFFFFF` fallback).
  - The `Equipped` and `meta` rules, and the playthrough module.
  - `sizeof(RandoWorld)` and `sizeof(RandoGen)` (host, with faults), and the fill's stack estimate.
  - The e2e budget remedies applied, with medians before and after each.
  - The claims raised, the claims kept, and each kept claim's Phase 3 row.
  - The all-cases survey (which of the 42 supported cases pass `all`).
  - The five breaks and their outcomes, the `make test` and `e2e` times, and where each `H-10-n` landed.
  - That the retry-seed claim is empty (step 1.2 note).
