---
title: "Rando Phase 3 step 0: parallel e2e gate, every green case claimed at all, per-row owed VT tests, nine rows closed, library duplications merged"
description: "Phase 2 left 40 of the 42 supported cases byte-identical to VT but only 19 claimed, the gate's wall time grows by one serial generator run per claim, nobody knows which VT tests each Phase 3 row owes, and the library carries duplications every Phase 3 row would have to edit twice."
status: needs-triage
---

## Problem Statement

Plan Phase 3 intro (verbatim, `docs/randomizer.md` § 3):

> One step per option row of the test matrix. Each claims that row's cases and ports the location
> tests VT has for it (`tests/Inverted`, `tests/OverworldGlitches`, …). The `glitches` and `state`
> rows, whose values each bring their own logic or world, get one step per value instead. A row
> step is done when its cases are claimed at `all` and its listed tests are ported.

Plan step (Phase 3, "Execution order" item 0, verbatim):

> 0. **Claim what is green.** First the e2e budget restructure (§ Convergence gate), then every
>    case that holds claimed at `all`. The note lists, per row, the VT test files and methods that
>    row owes, enumerated from VT's `tests/` tree. A row whose cases are all claimed and whose list
>    is empty gets its `### Step 3.k` note here and is ticked, with no PRD. Then, under those
>    claims, one refactor commit (`test`, `logic`, `e2e` unchanged) merging the library
>    duplications Phase 2's warn-review found on Phase 3's paths: the map/compass-not-wild test,
>    `randomCollection`, the collect-until-stable loop, the list find/remove logic, and the inline
>    config defaults, shop prices and bottle index.

The rule it applies (§ Convergence gate, verbatim):

> `make -C tests/rando e2e` stays under 0.5 s on the host, and every step note records its time. A
> step whose new claims or checks would break that budget first restructures the gate, in its own
> commit, so that the gate's wall time no longer grows with what it pays for: per-claim checks
> (done in 2.3.3) or, at `all`, one generator run per claim (run claims in parallel with output
> kept in case order, and/or make the generator cheaper); `make selftest` proves the verdicts and
> messages unchanged.
>
> `make test` gets only hand-written cases; ported location tests replay through `make logic`
> fixtures, as in step 2.2. Every step note records the `make test` time against step 2.5's
> figure. Steps that change logic record `make measure`'s maximum host time per case.

Where things stand at `473fb7d` (end of Phase 2), measured on this container (4 cores, Xeon 2.1 GHz, `/bin/sh` is dash, awk is mawk 1.3.4) unless a line says otherwise:

- `claims.txt` has 19 claims: 18 at `all`, `dungeon_items-mcs` at `prepareWorld`. `make e2e` prints 19 pass, 0 divergent, 25 pending in 0.26 to 0.29 s.
- The step 2.4 survey holds: with every supported case claimed at `all`, 40 pass byte-identical; `dungeon_items-mcs` diverges in the playthrough only (`spoiler.json` line 479); `glitches-no_logic` diverges at fill draw 457 (`golden: 0 457 fill 0 8 -> 8`, `c: 0 457 fill 0 6 -> 4`). `state-inverted` and `state-retro` are unsupported (the generator returns unsupported before world init) and stay pending.
- `glitches-no_logic` claimed at `prepareWorld` passes; claimed at `fill` it diverges at the same draw. So the ratchet's highest holding stage for it is `prepareWorld`.
- The same 42-claim set run through today's serial `run_e2e.sh` takes 0.55 to 0.76 s: over budget. The 42 generator runs alone take 0.46 s serially and 0.13 s when all launched at once.
- The generator is not the only serial cost. Each process spawn costs about 1.5 ms here. Today's gate spawns per claim a `mkdir`, the generator and the claim awk; with a stub generator (`RANDO_GEN=/bin/true`) 42 claims still take 0.34 s. A throwaway prototype that launches every claim as one background job (subshell, generator, claim awk), creates every output directory with one `mkdir`, and assembles the report with one final awk over per-claim files, took 0.17 to 0.19 s for the 42 claims with identical stdout; a first prototype that assembled the report with three `$(cat …)` per case stayed at 0.41 s.
- `make test` takes 70 to 93 ms (step 2.4 note: ~80 ms). `make selftest` takes 1.2 s.
- VT at the pinned commit is in the git-ignored `tests/rando/oracle/vt/`. Its `tests/` tree: `tests/NoGlitches` (23 test files, replayed in full through `make logic` since step 2.2), `tests/OverworldGlitches` (23 files, 41 methods), `tests/MajorGlitches` (23, 41), `tests/Inverted` (22, 43), `tests/InvertedOverworldGlitches` (22, 38), `tests/InvertedMajorGlitches` (22, 40); top-level `RandomizerTest`, `WorldTest`, `RegionTest`, `RetroBowTest`, `ItemTest`, `HelpersTest`, `LightWorldTest`, `RomTest`; `tests/Support/{CollectionTest,ItemCollectionTest,FlipsTest,Lz2Test,XXTEATest}`; `tests/Unit/Http/Requests/CreateRandomizedGameTest`; `tests/Browser`, `tests/js`, `tests/samples`. No directory exists for `HybridMajorGlitches` or `NoLogic`. `ItemCollectionTest`'s `testCanBunnyRevive`, `testCanAcquireFairy`, `testCanExtendMagic` and `testBottleCounting` (the only VT tests that vary `item.functionality`) are already ported in `test_items.c`.
- The duplications, as they stand in `rando/`:
  - **map/compass-not-wild test**: "a key, big key, map or compass whose `region_wild_*` flag is off" is written out four times: `rando_world_region_can_fill` (world), `wild_or_not_dungeon_item` (the rules header, negated), the filler's `can_skip_access_checks` (positive form), and the host playthrough's pruning (map and compass clauses only).
  - **`randomCollection`**: the filler's `random_collection` (on `uint8_t` ids) and an inline copy in `rando_fill_prizes` (on `RandoLocationId`, an enum).
  - **collect-until-stable loop**: `collectOtherItems` then merge, until the collected-location count stops changing, written in the filler's `fill_items_in_locations` and in `rando_world_is_winnable`.
  - **list find/remove**: "find the first occurrence of an item in a pool list, remove it, shift the rest" in `drop_first_boots` and `force_skull_woods_key`; "is the item in this list" in `in_pool`; `drop_first_boots` also appends to a pool list with its own inline assert instead of the module's push.
  - **inline config defaults**: `derive_config` writes per-option values as literals inside switch and if chains (`item.pool`: shield overflow counts 2/1/0 and crowd control's five item counts; `item.functionality`: bottle refill 0x40/0x20 and catchable fairies; hunt goals: required pieces 20/40 and Triforce piece counts 30/50).
  - **shop prices**: `rando_set_shops` adds the overflow shield at literal prices 500 and 50, which are exactly the default prices the world table already holds for those slots.
  - **bottle index**: `rando_get_bottle` bounds its draw with the literal `6`, the fairy bottle's index in its local table.

So: 22 green cases (and `glitches-no_logic` up to `prepareWorld`) are unclaimed and could break unnoticed; claiming them breaks the 0.5 s budget unless the gate is restructured first; Phase 3's row steps cannot be planned without knowing what VT tests each owes; at least nine rows owe nothing and would each cost a PRD for no work; and every row that remains would edit the duplicated code in two to four places.

## Solution

Four commits, in this order:

1. **Parallel e2e gate.** `run_e2e.sh` runs every claim at once as an independent background job and prints the report in case order once all have finished, with the same lines, the same detail blocks, the same summary and the same exit status as today. The serial part of the script spawns a fixed number of processes, not a number that grows with the cases. `make selftest` proves verdicts, messages and order unchanged, including when an earlier case finishes last.
2. **Claims.** `claims.txt` claims at `all` every supported case that holds (22 new, 40 at `all` in total), keeps `dungeon_items-mcs` at `prepareWorld`, adds `glitches-no_logic` at `prepareWorld`, and leaves `state-inverted` and `state-retro` unclaimed. `make e2e`: 42 pass, 0 divergent, 2 pending, well under 0.5 s.
3. **Owed tests and closed rows.** The step note `### Step 3.0` lists, per Phase 3 row, the VT test files and methods it owes, plus the VT tests no row owes and why. Nine rows whose cases are all claimed at `all` and whose list is empty get their `### Step 3.k` note and are ticked in `progress.md`: 3.2, 3.5, 3.7, 3.8, 3.9, 3.10, 3.11, 3.15, 3.16. Seven rows stay open: 3.1, 3.3, 3.4, 3.6, 3.12, 3.13, 3.14.
4. **Refactor.** One commit merging the seven duplications into single library functions or tables, with `make test`, `make logic` and `make e2e` output unchanged.

## User Stories

1. As a maintainer, I want `make e2e` to stay under 0.5 s with 42 claims, so that the fast loop stays fast (CLAUDE.md) while every green case is guarded.
2. As a maintainer, I want the gate's wall time bounded by its slowest claim rather than the sum of all claims, so that later Phase 3 claims do not force another restructure.
3. As a maintainer, I want the gate's restructure in its own commit before any claim changes, so that a regression in verdicts is attributable to the gate and not to a claim.
4. As `check-e2e`, I want the report lines in case order exactly as today, so that my parsing of `DIVERGENT <case>` lines and detail blocks keeps working.
5. As `check-e2e`, I want each `DIVERGENT` line followed by that case's own detail lines and no other case's, so that parallel runs never interleave details.
6. As a maintainer, I want the summary line and the exit status (0, 1, 2) unchanged, so that CI and the skills keep the same contract.
7. As a maintainer, I want claims-file and stage-table errors to fail before any generator runs, with the same messages, so that a malformed `claims.txt` reads the same as before.
8. As a maintainer, I want the generator's own stderr still on stderr, grouped per claim and emitted in case order, so that a crash message is attributable to its case.
9. As a maintainer, I want the "no C generator" detail line unchanged per claim, so that a missing build reads the same.
10. As a maintainer, I want `make selftest` to prove the order holds when the first case's generator is the slowest, so that a race in the assembly is caught by a test, not by a confused log.
11. As a maintainer, I want `make selftest` to prove two divergent claims print their details under their own verdict lines, so that detail interleaving is caught.
12. As a maintainer, I want the gate to work on a machine with fewer cores, only slower, so that CI runners with 2 cores still pass.
13. As a maintainer, I want the serial part of the gate to spawn a fixed number of processes, so that process start-up cost (about 1.5 ms each here) does not reintroduce linear growth.
14. As a maintainer, I want every supported case that is byte-identical to VT claimed at `all`, so that any later change that breaks one is caught.
15. As a maintainer, I want `glitches-no_logic` claimed at the highest stage that holds (`prepareWorld`), so that the ratchet guards everything before its fill divergence.
16. As a maintainer, I want `dungeon_items-mcs` kept at `prepareWorld`, so that its known playthrough divergence stays a Phase 3 row's job and never sends `check-e2e` into `fix-bug`.
17. As a maintainer, I want `state-inverted` and `state-retro` left unclaimed, so that they stay pending until their rows port their worlds.
18. As a Phase 3 row implementer, I want the step note to list the VT test files and methods my row owes, so that my PRD's scope is known before it is written.
19. As a Phase 3 row implementer, I want the VT tests no row owes listed with a reason, so that nobody reopens the question of whether `RomTest` or the boss-shuffle tests must be ported.
20. As a Phase 3 row implementer, I want tests that span two rows assigned to exactly one row, so that no test is ported twice or never.
21. As plan-pilot, I want rows that owe nothing and whose cases are all at `all` closed with a `### Step 3.k` note and ticked, so that I do not spend a PRD and an autopilot run on an empty step.
22. As plan-pilot, I want the closed rows named explicitly, so that I skip exactly those and no others.
23. As a reviewer, I want each closing note to state which cases are claimed at `all` and why the owed list is empty, so that the closure can be checked against the plan's "done when" rule.
24. As the `dungeon_items` row implementer (3.6, next), I want one map/compass-not-wild predicate, so that the rule-shape work edits one place.
25. As the glitch and inverted row implementers, I want one collect-until-stable function, so that two-world reads and new rules change one loop.
26. As the retro row implementer, I want one `randomCollection`, so that a new consumer reuses it instead of a third copy.
27. As the retro and inverted row implementers, I want one pool find/remove helper, so that new prepareWorld branches do not copy the loop again.
28. As the retro row implementer, I want `setShops`' prices read from the shop table, so that retro's price changes live in one place.
29. As a future implementer of other option values, I want `derive_config`'s per-option literals in tables, so that a new value is a table row, not a new switch case.
30. As a maintainer, I want the bottle draw's bound derived from its table, so that adding or reordering a bottle cannot silently skew the draw.
31. As a maintainer, I want the refactor under the 40 `all` claims, so that every draw of every merged path is compared with VT byte for byte.
32. As a maintainer, I want the refactor to leave `make test`, `make logic` and `make e2e` output unchanged, so that it is provably behavior-neutral.
33. As a maintainer, I want the step note to record `make e2e` and `make test` times and `make measure`'s maximum host time per case, so that the budgets are shown, not asserted.
34. As the author of roadmap item 2, I want the merged helpers to add no writable static and no stack growth beyond noise, so that step 2.5's ARM9 budget still holds.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. They rest on the plan (Goal, § Rules incl. § Convergence gate, § 3 Phase 3, step notes 1.1 to 2.5), CLAUDE.md, the committed `rando/` and `tests/rando/`, VT's `tests/` tree at the pinned commit, and the measurements above.

**Gate restructure (`run_e2e.sh`)**

- The parsing and validation pass (the first awk over `stages.txt` and `claims.txt`) is unchanged, and still runs first: any exit-2 error happens before a generator starts.
- Case discovery through `cases.sh` is unchanged.
- The per-case claim lookup is done once, not per case in a shell `read` loop nested in the case loop: the case list and the parsed claims are joined into one per-case plan (index, case, stage, draws, due file), either by the validation awk or by one extra awk.
- All output directories are created with one `mkdir` call before launch.
- Each claimed case runs as one background job: generator then claim awk, exactly as `run_claim` does today, with its stdout (detail lines) to a per-index detail file, its stderr (generator stderr) to a per-index stderr file, and its status to a per-index status file written by a shell builtin. The output directory is named by the case's index, not by a counter incremented inside the job (a background subshell's counter never reaches the parent).
- No job cap: every claim is launched at once, then one `wait`. The case list is fixed at 44, each job is a few milliseconds of work, and a cap would add a scheduler to a shell script for no measured gain. Recorded so that a later reader does not add one by reflex.
- One final awk reads the plan and the per-index files in case order and prints `PENDING`, `PASS <case> (<stage>)` or `DIVERGENT <case>` plus its detail lines, then the summary line, and exits with today's status. It copies each claim's generator stderr to stderr just before that claim's verdict. `mawk` and `gawk` both accept `/dev/stderr` as an output file; the script already requires an awk with `getline` from files and `ENVIRON`.
- The claim comparison awk (`CLAIM_AWK`) is unchanged.
- Usage comment at the top stays accurate (it documents the generator contract and exit codes); no other comment is added beyond what CLAUDE.md allows.
- Proof of "verdicts and messages unchanged": `make selftest` (existing fixtures unchanged and passing) plus the new fixtures below, and `make e2e` stdout byte-identical before and after the commit on the 19-claim `claims.txt`.
- Expected time: about 0.1 s at 19 claims; about 0.18 s at 42 claims (prototype figure).

**Claims (`claims.txt`)**

- Existing 19 lines unchanged (ratchet: nothing moves down; `dungeon_items-mcs prepareWorld` stays).
- Appended, in case-name order, the 23 supported cases no claim names today: `accessibility-locations all`, `accessibility-none all`, `crystals_ganon-0` … `crystals_ganon-6 all` (7), `crystals_tower-0` … `crystals_tower-6 all` (7), `glitches-hybrid_major_glitches all`, `glitches-major_glitches all`, `glitches-no_logic prepareWorld`, `goal-completionist all`, `goal-dungeons all`, `goal-fast_ganon all`, `goal-ganonhunt all`. That is 22 new lines at `all` and one at `prepareWorld`. Totals: 40 at `all` (18 + 22), 2 at `prepareWorld` (`dungeon_items-mcs`, `glitches-no_logic`), 2 unclaimed (`state-inverted`, `state-retro`): the gate prints 42 pass, 0 divergent, 2 pending.
- If any case listed above does not pass at `all` when the commit is made, it is claimed at its highest holding stage instead (the ratchet), and the step note names it for its row; this does not change the rows closed below unless that case belongs to one of them, in which case that row stays open.
- `glitches-no_logic`'s stage is `prepareWorld` by the ratchet (verified: `fill` diverges at draw 457).

**Per-row owed VT tests (content of the `### Step 3.0` note)**

A test is owed by a row when it exercises generator behavior the C port must reproduce (world, rules, pools, placement, prepareWorld) under that row's option value, and is not already ported. Interactions between two rows go to the row that comes later in execution order, so that both settings exist when it is ported. Rows, with `progress.md` numbering:

| Row | Cases | Owed VT tests | Status |
|---|---|---|---|
| 3.1 glitches: overworld_glitches | `glitches-overworld_glitches` at `all` | every method of `tests/OverworldGlitches` (23 files, 41 methods) | open |
| 3.2 glitches: hybrid_major_glitches | `glitches-hybrid_major_glitches` at `all` | none (no VT test directory; `WorldTest`'s HybridMajorGlitches rows go to 3.6) | **closed** |
| 3.3 glitches: major_glitches | `glitches-major_glitches` at `all` | every method of `tests/MajorGlitches` (23 files, 41 methods) | open |
| 3.4 glitches: no_logic | `glitches-no_logic` at `prepareWorld` | none (no VT test directory) | open: its case is not at `all` |
| 3.5 item_placement | `item_placement-basic` at `all` | none | **closed** |
| 3.6 dungeon_items | `-mc`, `-full` at `all`; `-mcs` at `prepareWorld` | `WorldTest::testSetVanillaDungeonItems` (8 rows: logic × wild flags → `rom.vanilla*`); `RegionTest::testRegionLockedItems` (the top-level one: a bare region with `region.wildMaps` / `region.wildCompasses` toggled) | open |
| 3.7 accessibility | `-locations`, `-none` at `all` | none | **closed** |
| 3.8 goal | all six goal cases at `all` | none (`testCompletionistItemCount` is ROM-only, see below; `testVanillaSwordsSetWithPedestalGoal` goes to 3.14) | **closed** |
| 3.9 crystals_tower | `-0` … `-6`, `-random` at `all` | none | **closed** |
| 3.10 crystals_ganon | `-0` … `-6`, `-random` at `all` | none | **closed** |
| 3.11 state: standard | `state-standard` at `all` | none (the Standard world's tests are `tests/NoGlitches`, ported in 2.2) | **closed** |
| 3.12 state: inverted | `state-inverted` unclaimed | every method of `tests/Inverted` (22 files, 43), `tests/InvertedOverworldGlitches` (22, 38), `tests/InvertedMajorGlitches` (22, 40) | open |
| 3.13 state: retro | `state-retro` unclaimed | `RetroBowTest::testLocation` (32 rows) | open |
| 3.14 weapons | `-assured`, `-vanilla`, `-swordless` at `all` | `RandomizerTest::testVanillaSwordsSet`, `RandomizerTest::testVanillaSwordsSetWithPedestalGoal` | open |
| 3.15 item_pool | `-hard`, `-expert`, `-crowd_control` at `all` | none | **closed** |
| 3.16 item_functionality | `-hard`, `-expert` at `all` | none: `ItemCollectionTest`'s functionality tests are already in `test_items.c` | **closed** |

Not owed by any row, each with its reason in the note:

- `tests/NoGlitches`: ported in step 2.2 (`make logic`).
- `tests/Support/ItemCollectionTest`: ported in steps 2.2 and later (`test_items.c`).
- `RandomizerTest::testCrystalsNotRandomizedByConfig{CrossWorld,NoCrossWorld}`, `testPendantsNotRandomizedByConfig{NoCrossWorld,CrossWorld}`: they set `prize.shuffleCrystals` / `prize.shufflePendants`, which the test matrix never varies.
- `RandomizerTest::test{Simple,Full,No}BossShuffle`, `testSwordless{Simple,Full}BossShuffle`: enemizer boss shuffle, out of scope (§ Reference and oracle).
- `RandomizerTest::testCompletionistItemCount`: `getTotalItemCount()` is only read by ROM writing (`World::writeToRom`), not by generation.
- `LightWorldTest` (Blacksmith ROM address), `RomTest`, `tests/Support/{FlipsTest,Lz2Test,XXTEATest}`: ROM writing and patch formats.
- `WorldTest::testGetRegionDoesntExist`: a PHP exception on an unknown name; the C port addresses regions by id.
- `ItemTest`: item names and ROM bytes; the C item table is checked by `make logic`'s world fixture.
- `HelpersTest`: PHP helpers (`mt_shuffle`, `ksortr`, patch minify, SNES/PC address maps); `fy_shuffle` has its own vectors.
- `tests/Support/CollectionTest`: generic `randomCollection`; its draws are covered by `test_filler` and every `all` claim.
- `tests/Unit/Http/Requests/CreateRandomizedGameTest`, `tests/Browser`, `tests/js`: the web front end.

**Closing notes for the nine closed rows**

- Rows closed: **3.2, 3.5, 3.7, 3.8, 3.9, 3.10, 3.11, 3.15, 3.16**. Each gets a `### Step 3.k` note in `docs/randomizer.md` § 6, after `### Step 3.0`, in numeric order. Each note has one or two bullets: the row's cases claimed at `all` by step 3.0, the owed list empty (with the one-line reason from the table), "no PRD".
- The same commit ticks those nine items in `progress.md` (plan-pilot reads a `### Step 3.k` note as the row done). Rows 3.1, 3.3, 3.4, 3.6, 3.12, 3.13 and 3.14 stay unticked.
- 3.0 itself is ticked by the usual flow once its note is complete (after the refactor chunk adds its bullet).
- No other plan text changes: the execution order (3.6 next, then the remaining open rows in table order) already skips nothing that is closed, because a closed row has a note.

**Refactor (one commit, behavior-neutral)**

Each merge keeps VT's draw order and every assert message's meaning. Placement is in the library (`rando/`), pure C99, no writable static.

- **Map/compass-not-wild test.** One library predicate, declared with `RandoConfig`: true when the item class is key, big key, map or compass and its `region_wild_*` flag is off ("confined to its dungeon"). `rando_world_region_can_fill` uses it (keeping its separate Hyrule Castle key clause), the rules header's `wild_or_not_dungeon_item` becomes its negation, the filler's `can_skip_access_checks` uses its negation, and the host playthrough uses it restricted to map and compass classes. It takes a class, not an item, so the playthrough's subset is a class test plus the predicate.
- **`randomCollection`.** One public function beside `rando_fy_shuffle` in the PRNG module: takes `uint8_t` ids, a count, how many to take, an output array; consumes the input; returns the number written (fewer when the input runs out, as VT's `randomCollection` does). The filler's static copy goes; `rando_fill_prizes` uses it on `uint8_t` location ids (the filler already asserts at compile time that location ids fit `uint8_t`).
- **Collect-until-stable.** One world function: given a world and an item set, resets the world's collected flags, then repeats `collectOtherItems` and merges what it found until the collected count stops changing; the set grows in place. `rando_world_is_winnable` calls it on the pre-collected set; the filler merges its base and pre-collected items first, then calls it. Moving the reset before the filler's merge is neutral: the merge reads no collected flag.
- **List find/remove.** The pool module gains a find (index or "absent") and a remove-first (returns whether it removed) on a pool list, and its append. `drop_first_boots`, `force_skull_woods_key` and `in_pool` use them; `drop_first_boots`'s inline append uses the pool append.
- **Inline config defaults.** `derive_config`'s per-option literals become per-value tables (one row per `item.pool` value, one per `item.functionality` value, one per hunt goal), read by the derivation. The tables live with the derivation (world module), keep VT's values, and carry no comment beyond a contract one if needed. `rando_config_init`'s VT defaults are not touched.
- **Shop prices.** `rando_set_shops` adds the overflow shield at the shop table's default price for that slot instead of the literals.
- **Bottle index.** `rando_get_bottle`'s upper bound is computed from its table's length, with the fairy bottle checked (compile time or assert) to be the table's last entry.
- If one of the seven turns out not to be a pure merge (output of `test`, `logic` or `e2e` changes), it is dropped from the commit, and the step note names it as left for the row that touches it next. The commit never changes a test or a golden.

**Step note `### Step 3.0`**

Bullets, in the style of the Phase 2 notes:

- the gate restructure: what runs in parallel, `make e2e` before/after at 19 claims;
- the claims: 40 at `all`, `dungeon_items-mcs` and `glitches-no_logic` at `prepareWorld`, inverted and retro pending; `make e2e` 42 pass, 0 divergent, 2 pending, and its time;
- the per-row owed table (rows, owed tests, open/closed) and the not-owed list with reasons, compressed to fit the note;
- the refactor: which merges landed (and any dropped);
- `make test` time against ~80 ms (step 2.4 figure, restated by 2.5), and `make measure`'s maximum host time per case (the refactor touches logic paths), against step 2.5's ~20 ms (`item_placement-basic`).

## Implementation Chunks

1. **Parallel e2e gate.** Restructure `run_e2e.sh` as above; add the selftest fixtures (ordering with a slow first case, two divergent details, generator stderr). `make selftest` green, `make e2e` stdout identical on the current `claims.txt`, time recorded in the commit message. Commit type `test`.
2. **Claims.** Append the 23 claims to `claims.txt` (22 at `all`, `glitches-no_logic` at `prepareWorld`). `make e2e` prints 42 pass, 0 divergent, 2 pending; time recorded. Commit type `test`.
3. **Owed tests and closed rows.** Write `### Step 3.0` (gate, claims, owed table, not-owed list, times) and the nine `### Step 3.k` notes; tick the nine rows in `progress.md`. Commit type `docs`.
4. **Refactor.** The seven merges in one commit; `make test`, `make logic`, `make e2e` output unchanged; `make measure` run; append the refactor bullet and `make measure`'s figure to `### Step 3.0`, in the same commit or a following `docs` commit. Commit type `refactor`.

## Testing Decisions

- A good test here checks external behavior only: the gate's stdout, stderr and exit status for a given set of cases, claims and generator outputs; the generator's outputs against VT's goldens. No test inspects the gate's temporary files or the order jobs finish in.
- **Gate (chunk 1):** `selftest.sh` gains fixtures in its existing style (fake generator driven by per-case files under `FAKE_OUT`, `gate <label> <claims> <status> <stdout> [<stderr>]`):
  - two claimed cases where the fake generator of the first case sleeps briefly (0.2 s at most; `make selftest` must stay around 1 to 2 s) before writing its outputs: stdout lists the first case first;
  - both cases divergent: each `DIVERGENT` line is followed by its own detail lines only;
  - the fake generator writes a line on stderr and exits non-zero on an `all` claim: stderr carries that line, stdout carries `C generator exited <n>` under that case;
  - every existing fixture passes unchanged (verdicts, messages, exit codes).
  These run in `make selftest`, never in `make test` (which keeps only the discovery self-test).
- **Claims (chunk 2):** the gate itself; no new unit test.
- **Docs (chunk 3):** none; the reviewer checks the owed table against VT's `tests/` tree.
- **Refactor (chunk 4):** no new test. It is guarded by the 40 `all` claims (every draw of `fillPrizes`, `setFountains`, `prepareWorld`, the fill and `isWinnable`, and the whole spoiler, compared byte for byte), the two `prepareWorld` claims, `make logic`'s fixture replay (region `canFill` vectors exercise the merged predicate), and the existing unit tests (`test_filler`, `test_prepare`, `test_pool`, `test_world`, `test_playthrough`). New public helpers (`randomCollection`, pool find/remove) get no dedicated unit test in this commit: the step requires `test` unchanged, and their draws and effects are already pinned through the callers.
- Prior art: `selftest.sh`'s gate fixtures (step 2.1, extended in 2.3.3 and 2.3.6); step 2.3.3's gate restructure, proven by an identical `make e2e` and an unchanged `make selftest`.
- `make test` stays at its current cost: nothing is added to `UNIT_TESTS`, and no fixture replay moves into `test`.

## Out of Scope

- Any row's porting work: `tests/OverworldGlitches`, `tests/MajorGlitches`, the inverted directories, `RetroBowTest`, the `WorldTest`/`RegionTest` items for 3.6, the `RandomizerTest` sword tests for 3.14.
- Fixing `dungeon_items-mcs` (row 3.6) or `glitches-no_logic` (row 3.4).
- Inverted and retro support in the generator.
- The host playthrough's static state (row 3.6).
- Phase 2's host-only warn-review warnings the plan leaves for the pull request (`rando_gen.c` statics and duplicated file handling, `spoiler_json.c` scratch buffers, the watchdog defined three times, `measure.sh` constants).
- Making the generator itself cheaper, including `item_placement-basic`'s ~20 ms (step 2.5: before roadmap item 2).
- Regenerating any golden; changing `stages.txt` or the claim comparison.
- Remote sessions, merging, pushing a `claude/*` branch.

## Further Notes

- Claim arithmetic: 42 supported cases; 19 claimed today (18 `all`, 1 `prepareWorld`); 23 added (22 `all`, 1 `prepareWorld`); result 40 `all`, 2 `prepareWorld`, 2 pending. The step note records the counts the gate prints.
- Step 2.4's survey was run on the 42 supported cases at `all`; this PRD re-ran it at `473fb7d` with the same result (40 identical).
- The parallel gate keeps every claim independent: a claim's output directory, files and generator run share nothing with another's, which is what makes the fan-out safe without locks.
- On a 4-core host the 42-claim run is bounded by the two slowest generator runs (`item_placement-basic` and `goal-ganonhunt`, ~25 ms each) plus the fixed serial part (~30 ms of validation awk and assembly); GitHub's `ubuntu-latest` also has 4 cores.
