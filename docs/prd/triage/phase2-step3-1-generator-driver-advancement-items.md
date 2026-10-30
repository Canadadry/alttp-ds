---
title: "Rando generator driver, per-attempt reseeding, C trace and getAdvancementItems"
description: "The C port has a PRNG and (after step 2.2) a world, but no program the e2e gate can run: no options, no attempt loop, no trace, no item pool, so no case can be claimed at any stage."
status: needs-triage
---

## Problem Statement

After Phase 2 steps 1 and 2 the C port can draw VT's random numbers and answer VT's location rules, but nothing ties them together into a generation:

- The e2e gate runs `$RANDO_GEN <case_dir> <out_dir>` (default `build/rando_gen`, step 2.1 note), and that program does not exist. Every case is `PENDING`, and none can be claimed.
- No C code reads a case's options. The 11 matrix options (`glitches` … `item_functionality`) have no C representation, and nothing maps them to the world config the way the oracle wrapper and VT's `World` constructor do.
- There is no attempt loop. The retry contract (plan § 2) reseeds the PRNG with `splitmix32(seed + attempt)` at the start of every attempt, and both sides must agree on it before a single stage can be compared.
- No C trace is written. The gate compares the C `trace.txt` against the golden's, line by line, each line naming the VT stage that drew. Without a stage label on every C draw, a divergence cannot say where it happened.
- VT's first RNG consumer in every matrix case is `getAdvancementItems` (preceded by `crystals` when a crystal count is `random`): four `getBottle()` draws that resolve the four `BottleWithRandom` entries of the advancement list. The C port has no item pool at all: no `config/item.php` lists, no per-item count overrides, no `getBottle`.

The draws themselves are trivial. The traps are around them: VT's world factory draws nothing, so the crystal draws and the pool getters touch the same RNG stream back to back; `BottleWithRandom` is a config pseudo-name, not an item; `rom.CatchableFairies` silently narrows the bottle range from `0..6` to `0..5` under `hard` and `expert` item functionality; a count override of `0` is not an absent override. And the list *order* the getters build is invisible in the trace, yet every later shuffle and the whole fill depend on it: an order bug here would surface hundreds of draws later, in step 2.4's spoiler diff.

## Solution

1. **Options in the library.** A small C struct holding the 11 matrix options as enums, settable by VT's option names and values, and a mapping from options to the input `RandoConfig` that reproduces the oracle wrapper's `World::factory` arguments. VT's `World` constructor derivations stay where step 2.2 put them, in world init, which gains the `item.pool` and `item.functionality` branches this step's stage reads.
2. **The item pool getters.** VT's four `config/item.php` lists (`dungeon`, `advancement`, `nice`, `junk`) as `const` tables in config order, the `item.count.*` override chain, the 216 cap, and `getBottle`, all reading the initialized world's derived config. Built in `prepareWorld`'s order: dungeon pool, advancement items, nice items, item pool.
3. **The generator in the library.** A public `rando_generate` entry point: the per-attempt loop of the retry contract, per-attempt reseeding, the `crystals` stage, world init, the pool stage labelled `getAdvancementItems`, and a stage-aware trace callback that numbers draws per attempt. It runs the stages ported so far and then reports that it is incomplete; each later sub-step extends it.
4. **The host driver `rando_gen`.** Reads a case directory (`config.json`, `seed`, `attempts`), runs the generator, writes `trace.txt` into the output directory as it goes, and exits with a code that says how far it got. `make -C tests/rando e2e` builds it first.
5. **A VT pool fixture.** A new oracle script dumps, for every case at attempt 0, the four lists exactly as VT builds them (bottles resolved). The C test replays every record in the `logic` mode, so the list contents and order of all 44 configs are pinned now, not discovered in step 2.4.
6. **Claims.** `baseline getAdvancementItems`, plus the four cases whose path through these stages differs from baseline's: `crystals_ganon-random`, `crystals_tower-random`, `item_functionality-hard`, `item_functionality-expert`. Every other case reaches `getAdvancementItems` through baseline's exact code path, so claiming it would only slow the e2e loop; their pools are still pinned by the fixture.

`make -C tests/rando e2e` then prints five `PASS … (getAdvancementItems)` lines and 39 `PENDING`.

## User Stories

1. As the e2e gate, I want a `build/rando_gen` program taking `<case_dir> <out_dir>`, so that a claim can be checked at all.
2. As the e2e gate, I want `make -C tests/rando e2e` to build the generator before running the cases, so that a fresh checkout and CI never report `no C generator`.
3. As the e2e gate, I want the C `trace.txt` in the golden's exact format, `attempt draw# stage min max -> r`, one line per draw, so that the trace-prefix comparison is a byte comparison.
4. As the e2e gate, I want the draw number to restart at 1 on every attempt, so that a divergence reads as "attempt N, draw #K", as the golden's does.
5. As the e2e gate, I want the trace written even when the generator stops early, fails or crashes after the claimed stage, so that a stage claim only depends on the stages it names.
6. As the e2e gate, I want the generator to exit nonzero whenever it has not produced a full generation, so that no `all` claim can pass on a partial run.
7. As a developer reading a divergence, I want every C draw labelled with the stage that made it, using the exact names of the stage table, so that the trace diff names the stage that drifted.
8. As a developer, I want a draw made outside any stage to abort with a message, as the patched VT throws on an unlabelled draw, so that a forgotten label cannot produce a misleading trace.
9. As a developer, I want the library's stage list checked against the stage table the gate reads, so that a renamed or reordered stage fails the fast loop instead of every claim.
10. As a C port developer, I want the 11 matrix options as a C struct of enums with the baseline as its default, so that the DS options menu and the host driver build the same input.
11. As a C port developer, I want to set an option by VT's key and value strings and get a failure on an unknown key or value, so that the host driver rejects a malformed case instead of silently running baseline.
12. As a C port developer, I want crystal counts to hold `0`…`7` or `random`, with `random` resolved inside each attempt, so that the draws land in the `crystals` stage, ganon first then tower, as the oracle wrapper draws them.
13. As a C port developer, I want `glitches` mapped to VT's logic names (`NoGlitches`, `OverworldGlitches`, `HybridMajorGlitches`, `MajorGlitches`, `NoLogic`) exactly as the oracle wrapper maps them, so that later logic steps read one config field.
14. As a C port developer, I want `item_pool` `crowd_control` to force `itemPlacement` to `advanced`, as the oracle wrapper does, so that the config VT sees is the config C sees.
15. As a C port developer, I want world init's constructor derivations to set `rom.CatchableFairies` to false under `item_functionality` `hard` and `expert` (with the `rom.BottleFill.Magic` values of those branches), as VT's `World` constructor does, so that `getBottle` draws in `0..5` for those cases and every derivation keeps one home.
16. As a C port developer, I want the `goal`-specific `item.count` overrides (`TriforcePiece` 30 for `triforce-hunt`, 50 for `ganonhunt`) and `crowd_control`'s (`BugCatchingNet`, `HalfMagic`, `CaneOfByrna`, `Cape` at 0, `TwentyRupees2` at 4, and its `item.overflow.count.Shield`) derived by world init, as it already derives `item.Goal.Required` for the hunt goals, so that the four lists match VT's in every matrix config.
17. As a C port developer, I want an override of 0 to mean zero items and an absent override to mean the table's default count, so that `crowd_control`'s removals work as VT's `?? default` does.
18. As a C port developer, I want each list entry capped at 216, as VT caps it, so that a large override behaves as VT's does.
19. As a C port developer, I want the four VT lists in `config/item.php` order with their default counts, so that each list comes out in VT's order and later shuffles see the same input.
20. As a C port developer, I want `BottleWithRandom` to be a table marker, not an item, resolved by `getBottle` at the position it occupies, so that the bottle lands where VT's lands in the list.
21. As a C port developer, I want `getBottle(filled)` exposed with VT's bottle order and range, so that step 2.3.4's fountains reuse it.
22. As a C port developer, I want the four lists built in `prepareWorld`'s order (dungeon, advancement, nice, item pool), so that a future config with a random bottle outside the advancement list draws in VT's order.
23. As a C port developer, I want a random bottle in any list other than advancement to abort with a message, because VT would label that draw with a stage the table lacks, so that an unported path cannot pass silently.
24. As a C port developer, I want the lists stored in the generator state with fixed capacities, no `malloc`, so that the plan's C library constraints hold.
25. As a C port developer, I want one top-level generator state holding the options, the PRNG, the pools and the world (which holds the derived config), so that an attempt resets everything by reinitializing it.
26. As a C port developer, I want each attempt to reseed with `rando_prng_seed(splitmix32(seed + attempt))`, `seed + attempt` wrapping at 2^32, so that the C and oracle loops start every attempt from the same state.
27. As a C port developer, I want the attempt loop unbounded by default and bounded by an optional cap, so that the shipped generator follows the retry contract and the tests never hang.
28. As a C port developer, I want the generator to report one of: complete, incomplete (ran every ported stage), unsupported (the config needs a world variant not ported), gave up (cap reached), so that the driver and the tests can tell those apart.
29. As a C port developer, I want the world initialized before the pools, in VT's order (`World::factory`, then `prepareWorld`'s getters), so that the getters read the config world init derived and no derivation is duplicated outside it.
30. As a C port developer, I want `state` `open` and `standard` mapped to step 2.2's two variants and `inverted` and `retro` reported unsupported before world init, so that the generator never initializes a world it does not have.
31. As a C port developer, I want the trace hook to receive attempt, draw number, stage, min, max and result, and the library itself to print nothing, so that the library stays host-independent and the DS build links no stdio.
32. As a host-tool developer, I want `config.json` read by a strict reader that accepts exactly one JSON object of 11 string values with the matrix keys, as the oracle wrapper validates it, so that a typo in a case fails loudly.
33. As a host-tool developer, I want `seed` read as a decimal uint32 on one line, as the oracle wrapper reads it, so that both sides agree on every seed.
34. As a host-tool developer, I want the driver to cap the loop at the golden's `attempts` plus one when the case has an `attempts` file, and at the oracle's 1000-attempt watchdog otherwise, so that "the C port stops after the golden's attempts" (plan § Retry contract) holds.
35. As a host-tool developer, I want `attempts` written only on a complete generation, as the oracle writes it only on success, so that a partial run never looks like a finished one.
36. As a host-tool developer, I want distinct exit codes for complete, usage error, input error, incomplete, unsupported and gave up, so that a human running the driver by hand knows what happened.
37. As a maintainer, I want VT's own four lists for every case dumped by an oracle script into a committed fixture, so that the C lists are proven equal to VT's, order included, for all 44 configs.
38. As a maintainer, I want the dump to check its own draws against each case's golden trace prefix, so that the fixture is proven to follow the exact path the golden followed.
39. As a maintainer, I want the dump to share the oracle wrapper's case loading and world setup rather than copy it, so that the two can never drift; and the goldens proven byte-identical after that refactor.
40. As a maintainer, I want the dump run by `gen_golden.sh` on every case, whatever cases are named, so that the fixture is always complete.
41. As a maintainer, I want the fixture's header to name what writes it and what verifies it, in the PRNG vector file's style, so that a reader knows which check to trust.
42. As a developer, I want `make -C tests/rando test` to run only hand-written cases of the new modules and stay at its current ~50 ms, so that the loop stays fast (CLAUDE.md).
43. As a developer, I want the fixture replay in step 2.2's `logic` target, so that it runs in CI without slowing the fast loop.
44. As a developer, I want `make -C tests/rando e2e` to claim only the cases whose code path differs, so that the e2e loop stays fast while every distinct branch of this step is under the gate.
45. As the author of step 2.3.2, I want a generator I extend by appending a stage after `getAdvancementItems`, with the trace, the stage label and the stop point already in place, so that `setMedallions` is a stage port only.
46. As the author of step 2.4, I want the attempt loop and its reseeding in place and correct for attempt 0, so that adding failure only adds the "fail" branch's callers and the injection test.
47. As a Phase 3 author, I want each option value already parsed and mapped, with the constructor derivations that have no config field yet marked `TODO`, so that an option step knows exactly what it owes.
48. As a DS integrator, I want the generator's input to be the options struct and a seed, and its state one fixed-size struct, so that roadmap item 2 calls it with what the save slot holds.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. Each rests on the plan's Rules and step notes, CLAUDE.md, the committed harness, step 2.2's PRD, and VT's source at the pinned commit. Assumptions about step 2.2's outcome are hypotheses, labelled `H-4-n` and listed under Hypotheses.

**Facts read from VT, the goldens and step 2.2's committed code (not hypotheses)**
- `prepareWorld` calls `getDungeonPool`, `getAdvancementItems`, `getNiceItems`, `getItemPool` in that order, after setting the goal's Triforce (no draw). Only the advancement list holds `BottleWithRandom` (count 4) in `config/item.php`, and no matrix override touches it.
- `getBottle(filled)` indexes `[Bottle, BottleWithRedPotion, BottleWithGreenPotion, BottleWithBluePotion, BottleWithBee, BottleWithGoldBee, BottleWithFairy]` with `get_random_int(filled ? 1 : 0, 7 - (CatchableFairies ? 1 : 2))`.
- `item.count.<name>` resolves through `World::config()`'s chain; at the pinned commit only `alttp.goals.<goal>.item.count` and the `crowd_control` branch of the `World` constructor set any. `World::config` returns the default only for `null`, so 0 is an override.
- Every golden's attempt 0 starts with `getAdvancementItems`, 4 draws, except `crystals_*-random` (one `crystals` draw first). The range is `0..6`, except `item_functionality-hard` and `-expert` (`0..5`). `state-retro` continues with `setShops`, all others with `setMedallions`. All goldens use seed 1 and succeed on attempt 0.

**Options module (new, library)**
- `RandoOptions`: one enum per matrix key, value sets exactly the matrix's (§ Test matrix), crystal counts as 0–7 or a `random` value. A default constructor gives the baseline.
- `rando_options_set(options, key, value)`: VT's key and value strings; returns false on an unknown key or value and leaves the struct unchanged. The name tables are `const char *` arrays.
- `rando_config_from_options(options, crystals_ganon, crystals_tower, config)`: starts from `rando_config_init` and sets only what the oracle wrapper passes to `World::factory` (H-4-3):
  - `logic` from `glitches`, the oracle wrapper's mapping;
  - `item_placement` from `item_placement`, forced to `advanced` under `crowd_control`;
  - `dungeon_items`, `accessibility`, `goal`, `mode_state`, `mode_weapons`, `crystals_ganon`, `crystals_tower` (resolved ints);
  - `item_pool` and `item_functionality`, two new `RandoConfig` enums (default `normal`).

  It derives nothing: the wild flags, glitch flags, better bow and sword, Skull Woods key and `item_goal_required` are already derived by step 2.2's world init on its copy of the config.
- **World init's derivations (modified).** Step 2.2's derivation function, VT's `World` constructor, gains the two branches the matrix uses and that touch a `RandoConfig` field:
  - `item.pool`: `item_overflow_count_shield` (hard 2, expert 1, crowd_control 0) and, for `crowd_control`, the per-item count overrides above;
  - `item.functionality`: `rom_catchable_fairies` false and `rom_bottle_fill_magic` (hard `0x40`, expert `0x20`);
  - the hunt goals' `TriforcePiece` count override, beside the existing `item_goal_required` derivation (both come from `alttp.goals.<goal>`).

  The per-item count override is a new `RandoConfig` array indexed by item id, `RANDO_CONFIG_NULL` meaning absent (VT's `?? default`). Keys of those branches with no `RandoConfig` field (the other overflow counts, `rom.CapeMagicUsage.*`, `rom.BottleFill.Health`, `shops.HardMode`, …) get one `TODO` naming the VT block; the step whose claim first reads one adds its field. Step 2.2's fixtures never set `item.pool` or `item.functionality`, so `make logic` must stay green unchanged.
- Options are library code, not host code, because the DS options menu (roadmap item 3) builds the same struct.

**Pool module (new, library)**
- Four `const` tables in `config/item.php` order: per entry a `RANDO_ITEM_<RawName>` id (H-4-2) or the `BottleWithRandom` marker, and the default count.
- `RandoPools`: four ordered lists of item ids stored as `uint8_t`, as `RandoWorld` stores them, with fixed capacities (256 entries each, asserted with a message naming the list on overflow).
- `rando_pool_build(world, prng, pools)`: builds the four lists in `prepareWorld`'s order from the world's derived config, applying overrides, the 216 cap and the marker. A marker outside the advancement list asserts (the trap of story 23).
- `rando_get_bottle(world, prng, filled)`: VT's `getBottle`, reading `rom_catchable_fairies` from the world's config.
- Comments kept (CLAUDE.md): the marker is not an item; 0 override versus none; the CatchableFairies range.

**Generator module (new, library, public header `rando.h`)**
- `rando.h` is the library's public entry point, as the plan's layout reserves it (H-4-1).
- **Stages.** An enum in the stage table's order, with a `const` name table whose strings equal the table's: `crystals`, `getAdvancementItems`, `setShops`, `setMedallions`, `placeBosses`, `fillPrizes`, `setFountains`, `shufflePrizePacks`, `prepareWorld`, `fill`.
- **State (`RandoGen`).** One top-level struct: the options, the seed, the current attempt and stage, the per-attempt draw counter, the `RandoPrng`, `RandoPools` and the `RandoWorld`, whose embedded `RandoConfig` is the derived config every stage reads (H-4-7). The input config built from the options is a local of each attempt. No pointers between parts other than the caller's trace hook and its context.
- **Trace hook.** `RandoTraceFn(ctx, attempt, draw, stage_name, min, max, r)`, optional. The generator installs its own `RandoDrawObserver` on the PRNG, which numbers the draw, asserts a stage is current (message: draw outside any stage), and forwards to the hook.
- **Entry point.** `rando_generate(gen, options, seed, max_attempts, hook, ctx)` returns a status: `RANDO_COMPLETE`, `RANDO_INCOMPLETE`, `RANDO_UNSUPPORTED`, `RANDO_GAVE_UP`; the attempt reached is readable from the state. `max_attempts` 0 means unbounded (the shipped behavior).
- **One attempt**, in VT's order:
  1. reseed: `rando_prng_seed(prng, rando_splitmix32(seed + attempt))`, wrapping;
  2. stage `crystals`: ganon then tower, one `rando_random_int(0, 7)` each for a `random` option;
  3. config from options;
  4. `state` `inverted` or `retro`: stop with `RANDO_UNSUPPORTED`; else `rando_world_init(world, variant, config)` with `RANDO_VARIANT_OPEN` or `RANDO_VARIANT_STANDARD` (H-4-4);
  5. stage `getAdvancementItems`: the four pool getters on the initialized world;
  6. end of ported stages: `RANDO_INCOMPLETE`.
- **The loop.** An attempt that returns "failed" (no stage can fail yet) moves to the next attempt; complete, incomplete, unsupported stop it; reaching `max_attempts` gives `RANDO_GAVE_UP`. The failure branch has no caller until step 2.4, which adds the plan's injection test; this step's tests cover reseeding at attempt 0 and the cap.
- World init comes before the pools, in VT's order, because step 2.2's world init owns the constructor derivations the getters read (`rom_catchable_fairies`, the count overrides). The Inverted and Retro cases therefore stop before their first `getAdvancementItems` draw until Phase 3 ports their worlds; neither is claimed.

**Host driver (new, host only)**
- Host-only sources live apart from the library sources, so that the library's source wildcard (and the future ARM9 build) never picks them up. Three units: the case reader, the trace writer, and `main`.
- **Case reader.** A strict reader for `config.json`: one object, exactly the 11 keys, string values without escapes, whitespace anywhere JSON allows; each pair goes through `rando_options_set`. `seed`: `^[0-9]{1,10}\n?$` and ≤ 2^32−1, as the oracle wrapper checks it. `attempts`: same syntax, optional.
- **Trace writer.** Opens `<out_dir>/trace.txt` line-buffered, so each line reaches the file before the next draw and survives an `abort`. Format `%u %u %s %lld %lld -> %lld` with the C99 `inttypes` macros, one `\n` per line, no trailing blank line.
- **Exit codes:** 0 complete (writes `attempts`; spoiler.json comes with step 2.3.2 onward), 2 usage, 1 input error (missing or malformed case file, unwritable output), 3 incomplete, 4 unsupported, 5 gave up. `run_e2e.sh` ignores the code for stage claims and requires 0 for `all` claims, so this step changes nothing in it.
- **Cap:** golden `attempts` + 1 when the file exists, else 1000.
- No spoiler and no `pool.txt` yet: `getAdvancementItems` determines no spoiler entry (its bottles are only placed by the fill) and the pool check is owed by `prepareWorld` (step 2.3.6). `stages.txt` gains no `check` line.

**Oracle pool fixture (new)**
- A shared oracle include takes over the oracle wrapper's case loading (config and seed validation, the glitches mapping, the `Seed` pre-boot) and its per-attempt world setup (cache clears, `max_world`, trace start, reseed, labelled crystal draws, `World::factory` arguments). The oracle wrapper is reduced to the loop, the randomize call and the outputs. Proof: `gen_golden.sh --fresh --out <scratch>` output byte-identical to every committed golden, recorded in the commit message.
- A new dump script, run by `gen_golden.sh` right after `prng_check.php` (H-4-6), over every case directory the shared case discovery finds (whatever cases are named on the command line). For each case, attempt 0: the shared setup, then the four getters in `prepareWorld`'s order, each draw labelled with the getter's own name. It checks its draws equal the golden trace's first draws, then writes one record per list.
- Format: one `case <name> <seed> <key>=<value> …` line with the 11 options, then `dungeon`, `advancement`, `nice` and `items` lines, each the list's raw item names in order, space-separated. A header in the PRNG vector file's style names the writer and the C check.
- The fixture is a new oracle output, not a golden: its first generation is its own commit, and it is regenerated only when VT, its patch or the case list changes (CLAUDE.md).

**Makefile and CI**
- Host-only sources must sit outside the library's source directory, since the library wildcard takes every `.c` one level deep under it (H-4-5). The host units (except `main`) compile into their own archive, linked into every unit test before step 2.2's library archive; `build/rando_gen` links `main` with both.
- `e2e` depends on `build/rando_gen`.
- New unit tests registered in `UNIT_TESTS`: `test_options`, `test_pool`, `test_generate`, `test_case_reader`. Only `test_generate` has a fixture mode: it parses its arguments with the test support's `test_fixtures_mode` and is run by `logic`. `logic` runs every `UNIT_TESTS` entry except those its filter names, so the filter (today `test_prng`) gains `test_options`, `test_pool` and `test_case_reader`.
- The pool fixture's path joins the test support's fixture path macros.
- CI needs no new step: `e2e` builds the generator, `logic` runs the fixture replay.

**Claims**
- `claims.txt` gains five lines at stage `getAdvancementItems`: `baseline`, `crystals_ganon-random`, `crystals_tower-random`, `item_functionality-hard`, `item_functionality-expert`.
- Rule: claim one case per distinct code path through the claimed stages. Every other case reaches `getAdvancementItems` by baseline's path (same draws, same labels, the options differing only in fields no ported stage reads); claiming them costs e2e time and proves nothing more. Their list contents are pinned by the fixture instead. Phase 3 option steps claim their rows as the plan says.
- Speed: CLAUDE.md forbids a slower e2e loop, but the convergence gate exists to add claims; the five are the minimum that covers this step's branches. The commit records e2e's time before and after; budget well under 0.5 s.

## Implementation Chunks

Each chunk leaves `make -C tests/rando test`, `logic`, `selftest` and `e2e` green.

1. **Oracle pool fixture.** Shared oracle include, the oracle wrapper rewritten on it, the dump script and its `gen_golden.sh` call, the committed fixture. Verify: every golden byte-identical from a `--fresh --out <scratch>` run; the dump's self-check passes on all 44 cases; a second dump byte-identical. Own commit.
2. **Options, derivations and case reader.** `RandoOptions`, `rando_options_set`, `rando_config_from_options`; the new `RandoConfig` fields (`item_pool`, `item_functionality`, the per-item count overrides) with their `rando_config_init` defaults; world init's `item.pool`, `item.functionality` and hunt-goal count derivations; the host case reader and its archive. `test_options`, `test_case_reader`, a few hand-written derivation cases in `test_world`; `make logic` unchanged and green.
3. **Pool.** The four tables, `RandoPools`, `rando_pool_build`, `rando_get_bottle`, on an initialized world. `test_pool`.
4. **Generator.** `rando.h`, stages, `RandoGen`, trace hook, reseeding, the loop, crystals, the pool stage, world init, the statuses. `test_generate`, including its `--fixtures` replay of every pool record, added to `logic`.
5. **Driver, gate and claims.** Trace writer, `main`, the Makefile's `rando_gen` target and `e2e` dependency, the five claims. Verify `e2e`: five `PASS … (getAdvancementItems)`, 39 `PENDING`, nothing `DIVERGENT`; a hand-made wrong claim (`baseline setMedallions`) reports the C trace ending after 4 draws, then is reverted. Record `test` and `e2e` times.

## Testing Decisions

- **A good test checks external behavior:** the options a string sets, the config fields a set of options produces, the lists the pool builder returns, the draws the trace hook receives, the status and attempt the generator returns. Never struct layout or capacities.
- **VT is the oracle.** The fixture's lists are VT's output, proven on VT's own trace; the hand-written expectations restate VT's source or a golden's lines and cite them. A failing hand-written case that contradicts a golden is the test's bug (CLAUDE.md: the goldens are the source of truth).
- **`test_options`:** the baseline default; every matrix key and value accepted; an unknown key and an unknown value rejected with the struct unchanged; `crowd_control` forcing `advanced`; `glitches` to logic names; the input config otherwise equal to `rando_config_init`'s plus the factory arguments (no derivation).
- **`test_world` (extended, hand-written):** after `rando_world_init`, `hard` and `expert` functionality clear `rom_catchable_fairies` and set `rom_bottle_fill_magic`; `crowd_control` sets its count overrides and shield overflow; the hunt goals set the `TriforcePiece` override; `normal` leaves every override absent.
- **`test_pool`:** on a world initialized from the baseline options, with a PRNG seeded as baseline attempt 0, the advancement list's four bottles are `BottleWithBluePotion`, `BottleWithBee`, `BottleWithFairy`, `BottleWithRedPotion` (the golden draws 3, 4, 6, 1) at the marker's position; the range is `0..5` with CatchableFairies false; `filled` starts at 1; an override of 0 removes an entry and no override keeps the default; the 216 cap; list lengths of the baseline pool, restated from `config/item.php`.
- **`test_generate`:** baseline seed 1 gives `RANDO_INCOMPLETE` at attempt 0 and exactly the golden's first four lines through the hook; a `random` crystal option adds one `crystals` draw first, ganon's before tower's; `state` inverted and retro return `RANDO_UNSUPPORTED` with no `getAdvancementItems` draw; reseeding equals `rando_splitmix32(seed + attempt)` including the wrap at 2^32−1; the library's stage names equal `stages.txt`'s `stage` lines in order. With `--fixtures`: every record of the pool fixture, options set by name, lists compared name by name (`rando_item_info(id)->name`), first mismatch printed with case, list, index, expected and actual. The `state-inverted` and `state-retro` records cannot run (no world) and are skipped by name; the test asserts exactly those two were skipped, and the Phase 3 `state` steps remove each skip.
- **`test_case_reader`:** the baseline file accepted; a missing key, an extra key, a non-string value, an escape, trailing garbage and a bad seed each rejected with a message naming the file.
- **Driver and trace writer:** covered by the e2e claims, whose prefix comparison is byte-exact, and by chunk 5's manual wrong-claim check. No selftest change: the gate's own fixtures are untouched.
- **Speed:** `test` runs hand-written cases only and stays near ~50 ms (step 2.2's budget: move a case behind `--fixtures` past ~100 ms, never delete it). The fixture replay is in `logic`. `e2e` runs five claims.
- **Prior art:** `test_prng` (plain `main`, vector file, first-failure exit), step 2.2's `--fixtures` mode, `test_fixtures_mode` and the `Fixture` line reader of the test support (H-4-9), `prng_check.php` and step 2.2's extractor for an oracle script run by `gen_golden.sh`, `gen_case.php` for VT bootstrapping.

## Hypotheses

Phase 2 step 2 is now done and pushed. Each hypothesis below has been checked against its committed code and the step 2.2 note; the sections above already apply the verdicts.

- **H-4-1 — Library layout and naming: holds.** Headers `rando_items.h`, `rando_bosses.h`, `rando_config.h`, `rando_world.h` sit beside `rando_prng.h`; sources are flat in the library's source directory with a `regions/` sub-directory; `Rando*` types and `rando_*` functions; no `rando.h` exists, so the generator's API creates it.
- **H-4-2 — Item ids: holds.** Items are `RANDO_ITEM_<RawName>` in `Item::all` order, `RANDO_ITEM_COUNT` and the `RANDO_ITEM_NONE` sentinel, `rando_item_find(raw name)` and `rando_item_info(id)->name`; the seven bottles, `TriforcePiece` and `TwentyRupees2` exist. Detail applied: ids are stored as `uint8_t` in world state, and the pools do the same.
- **H-4-3 — Config struct: failed in part.** As assumed, `RandoConfig` has `logic`, a three-valued `item_placement`, `accessibility`, `goal`, integer crystal counts, `mode_state` (with `INVERTED` and `RETRO` values), `mode_weapons`, `rom_catchable_fairies` (default true), and no `item.pool`, `item.functionality` or count override. Not as assumed: `dungeon_items` is a plain enum field, not a setter, and `rando_world_init` derives the constructor keys itself on its copy (`mode_state` from the variant, glitch flags from `logic`, wild flags, vanilla keys, better bow and sword, Skull Woods key, `item_goal_required` for the hunt goals). *Applied:* the options mapping sets only the factory arguments; the `item.pool` / `item.functionality` / hunt-goal count derivations go into world init's derivation function, next to step 2.2's, instead of the options mapping.
- **H-4-4 — World variants and init: holds, with a consequence.** `rando_world_init(RandoWorld *, RandoVariant, const RandoConfig *)` is a pure function with `RANDO_VARIANT_STANDARD` and `RANDO_VARIANT_OPEN` only, and handles every `logic` value. It also loads the shops' default inventory (step 2.2 deviation), which draws nothing. *Consequence applied:* since init owns the derivations the getters read, the generator initializes the world before the pools (VT's own order), and `inverted`/`retro` stop as unsupported before any `getAdvancementItems` draw; the earlier "pools first" decision is dropped.
- **H-4-5 — Build rules: holds.** Library objects compile once into `librando.a`; the test support object is linked into every unit test; `logic` runs every `UNIT_TESTS` binary except a filter (`test_prng`) with `--fixtures`. *Applied:* the three new tests without a fixture mode join that filter; the library wildcard takes `src/*.c` and `src/*/*.c`, so host-only code must live outside the library's source directory.
- **H-4-6 — Oracle hook: holds.** `gen_golden.sh` runs `prng_check.php`, then `extract_logic.php` into `${out:-oracle dir}`, before the cases. The pool dump runs right after `extract_logic.php` with the same output rule.
- **H-4-7 — No PRNG in the world: holds.** `RandoWorld` holds the derived `RandoConfig`, the variant, location and shop items, bosses, pre-collected items and the `collectItems` memo, and no PRNG. The PRNG lives in `RandoGen`. Detail applied: `RandoGen` keeps no separate resolved config; stages read `world.config`.
- **H-4-8 — No spoiler output from step 2.2: holds.** No spoiler or `Equipped` writer exists; raw names come from `rando_item_info`. This step still writes neither spoiler.json nor pool.txt.
- **H-4-9 — Test support: holds.** The test support offers `test_fixtures_mode`, `test_fail`, the `Fixture` line reader (`fixture_open`/`fixture_next`/`fixture_fail`), fixture path macros and `fixture_apply_config`. `test_generate --fixtures` reuses the reader and adds its path macro.
- **H-4-10 — Unported config is harmless at init: holds, scope reduced.** World init reads nothing this step leaves wrong; most constructor derivations the PRD planned to leave as `TODO` are already in world init. Only keys with no `RandoConfig` field remain `TODO`.

## Out of Scope

- Every stage after `getAdvancementItems`: `setShops`, `setMedallions`, `placeBosses`, `fillPrizes`, `setFountains`, `shufflePrizePacks`, `prepareWorld`'s own work, the fill and `isWinnable()` (steps 2.3.2 to 2.4).
- The spoiler writer, `pool.txt`, and any `check` line in `stages.txt` (step 2.3.2 onward).
- Attempt failure and its injection test (step 2.4).
- The `World` constructor keys with no `RandoConfig` field yet (left as `TODO`s), Inverted and Retro worlds, glitch flags from `config/logic.php` (Phase 3).
- Claiming any case beyond the five, or at any stage but `getAdvancementItems`.
- Changing or regenerating a golden, `vt.patch`, the stage table's stages, or `run_e2e.sh`.
- Linking the generator into the ARM9 build (roadmap item 2) and measuring its size (step 2.5).

## Further Notes

- **Why the list order matters now.** The trace cannot see the order the getters build: `fy_shuffle` draws one number per element whatever the elements are. A swapped table entry would pass every claim through step 2.3.6 and diverge only in step 2.4's fill, far from its cause. The fixture closes that gap at the source.
- **Why `crystals` belongs to the library, not the driver.** `random` crystals are drawn inside each attempt after the reseed, so a retry redraws them. The DS menu will offer `random` too.
- **Step note material** for `### Step 2.3.1`: the claims and the rule behind them, the e2e time, the exit codes, the fixture's record count, and where each hypothesis landed.
- The generator's "incomplete" status is permanent scaffolding until step 2.4: each sub-step moves the stop point one stage further, and the last one removes it.
