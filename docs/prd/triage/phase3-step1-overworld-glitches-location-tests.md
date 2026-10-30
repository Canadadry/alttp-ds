---
title: "overworld_glitches row: VT's tests/OverworldGlitches replayed against the C port"
description: "The glitch branches of the C location rules are checked only through one e2e case; VT's 41 OverworldGlitches test methods (1 204 rows) have never been replayed against them."
status: needs-triage
---

## Problem Statement

Row 3.1 of Phase 3 is the `glitches: overworld_glitches` value. Its one case,
`glitches-overworld_glitches`, has been claimed at `all` since step 3.0, but the row is not done: step
3.0's owed table says it still owes all of VT's `tests/OverworldGlitches`, which is 23 files and 41 test
methods. A row step is done only when its cases are claimed at `all` and its listed tests are ported.

Right now the only check on the C port's overworld-glitch logic is that one seed. The region rules have
glitch branches transliterated from VT. They read `can_boots_clip`, `can_fake_flipper`,
`can_dungeon_revive` and the other flags `config/logic.php` turns on for `OverworldGlitches`. Step 2.2
ported only `tests/NoGlitches`, where every flag is off, and its note says so: "Turtle Rock's middle/lower
paths need glitch flags (untested until Phase 3)". One seed reaches only the branches its own fill happens
to use. A wrong glitch branch elsewhere would go unnoticed until a seed that needs it is generated on a DS.

## Solution

Port `tests/OverworldGlitches` the way step 2.2 ported `tests/NoGlitches`:

- The oracle's logic extractor runs every method of the tree inside VT and writes one row per data-provider
  row to a new committed fixture. Each row is replayed by VT itself before it is written.
- The world test support replays the fixture against the C port under `make logic`, never under
  `make test`.
- Any row on which the C port gives a different answer from VT is a rule bug. It gets a hand-written test
  and a fix.

A throwaway prototype was built in scratch copies and not committed, first on the step 3.0 code
(`39cf756`), then again on the committed step 3.6 code (`dfe661d`, rebuilt from `git archive`), using
3.6's extractor and test support. On the step 3.6 code:

- VT writes 1 204 rows from 41 methods in 23 files, and replays every one. Its answer equals the test's
  expectation on all 1 204. The file is byte-identical to the one the step 3.0 prototype wrote.
- The existing three logic fixtures come out byte-identical, `noglitches_rows.txt` with its 3 067 rows
  included.
- The C port gives VT's answer on all 1 204 rows, both with and without the `logic`-before-init replay
  rule (below). The 3 067 NoGlitches rows still pass with that rule too. **No rule divergence exists at
  HEAD.**
- The rows really exercise the glitch branches. With the glitch overrides dropped, 558 rows flip. Forcing
  one flag off at a time flips this many rows:

  | Flag | Rows flipped |
  |---|---|
  | `canBootsClip` | 262 |
  | `canDungeonRevive` | 247 |
  | `canFakeFlipper` | 214 |
  | `canMirrorClip` | 80 |
  | `canSuperBunny` | 12 |
  | `canWaterWalk` | 5 |
  | `canSuperSpeed` | 0 |

- The replay adds about 10 to 15 ms to `make logic`.

So the expected work is the extractor, the fixture, the replay and one hand-written derivation test. No
rule fix is expected: step 3.6's two-world rewrite left every glitch branch's outcome unchanged (see the
Hypotheses section).

## User Stories

1. As the randomizer maintainer, I want every method of VT's `tests/OverworldGlitches` replayed against the C port, so that row 3.1's owed tests are ported and the row can be ticked.
2. As the randomizer maintainer, I want the OverworldGlitches rows written by VT itself, so that the oracle stays the source of truth and no expected value is typed by hand.
3. As the randomizer maintainer, I want every row replayed by VT from its record alone before it is written, so that a record that loses information fails the extractor instead of producing a wrong fixture.
4. As the randomizer maintainer, I want the extractor to fail, writing nothing, on an unknown method in the new tree, as it does for `tests/NoGlitches`, so that a VT test we don't understand never becomes a silent gap.
5. As the randomizer maintainer, I want the new rows in their own fixture file, so that `noglitches_rows.txt` (3 067 rows since step 3.6, with the top-level `RegionTest` and `WorldTest` rows) and the other fixtures stay byte-identical.
6. As the randomizer maintainer, I want the fixture regenerated in its own commit, with every golden case and every existing fixture byte-identical, so that the oracle change is reviewable on its own and no golden is regenerated to get green.
7. As a porter of glitch rules, I want the C replay to build a world whose effective config equals the one VT's test world had, so that a row's answer compares like with like.
8. As a porter of glitch rules, I want a flag VT never read during a row (short-circuited) to still hold its `OverworldGlitches` value in the C replay, so that a C rule shaped differently from VT's cannot pass or fail because of a flag left at its NoGlitches default.
9. As a porter, I want a row on which C differs from VT to print its file, method, provider row, query, VT's answer and C's, as NoGlitches rows do, so that I can go from the failure to the VT test line at once.
10. As a porter, I want every divergence fixed as a bug with its own hand-written test in the regions test, so that the fix stays guarded in `make test` even if the fixture is ever regenerated.
11. As the randomizer maintainer, I want `make test` to grow only by hand-written cases, so that the fast loop stays fast (plan rule).
12. As the randomizer maintainer, I want the 1 204 rows in `make logic`, which CI runs, so that a regression in a glitch branch fails CI.
13. As the randomizer maintainer, I want a hand-written check that `logic = OverworldGlitches` derives exactly VT's seven flags and leaves the other seven off, so that the derivation the replay relies on is pinned. The fixture alone can't pin it: a recorded override would mask a missing derived flag.
14. As a future porter of row 3.3 (`tests/MajorGlitches`) and row 3.12 (the three Inverted trees), I want the extractor and the replay to take a VT test tree and a fixture path as parameters, so that each later tree is one table entry and one replay call.
15. As the randomizer maintainer, I want `glitches-overworld_glitches` to stay claimed at `all` and e2e to stay under 0.5 s, so that this step breaks no claim and no budget.
16. As a reviewer, I want the step note to record the row count, the per-flag sensitivity, the `make test`, `make logic` and e2e times, and whether the library changed, so that I can see what was ported and what it cost.
17. As a reviewer, I want the step note to name the flags no OverworldGlitches row pins (`canSuperSpeed`), so that the gap is known and left for the MajorGlitches rows.
18. As a DS integrator, I want this step not to change the library at all unless a row reveals a bug, so that the ARM9 figures stay those of the previous step.
19. As a reader of the fixture, I want its header to say which VT tree its rows come from and how a `logic` override is applied, so that the record can be replayed without reading the extractor.
20. As the randomizer maintainer, I want `gen_golden.sh` to write the new fixture with the others (it already runs the extractor), so that one regeneration command still produces every oracle file.

## Implementation Decisions

### Oracle: the extractor walks a list of VT test trees

- The logic extractor keeps its single entry point and its arguments (`<vt_dir> <out_dir>`).
- Since step 3.6, the source list returns one `(file, class, methods run)` entry per test file: every
  `tests/NoGlitches` file, then the `TOP_LEVEL_METHODS` table (`RegionTest.php`, `WorldTest.php`, under
  `tests/`). The row dump walks that list.
- This step gives both functions a tree parameter:
  - The source list discovers files under `tests/<tree>`.
  - It appends `TOP_LEVEL_METHODS` for the `NoGlitches` tree only. Those methods belong to the
    NoGlitches fixture and must not be repeated in another file.
  - The row dump passes its tree on.
- A table maps each tree to its output file:
  - `NoGlitches` writes `noglitches_rows.txt`: its rows, the top-level rows, then the collect scenarios,
    as now.
  - `OverworldGlitches` writes the new `overworld_glitches_rows.txt`, which holds row records only.
- The guard that a NoGlitches file never shares a top-level file's name stays on the NoGlitches tree.
- The OverworldGlitches setUps are identical to the NoGlitches ones apart from `'logic' => 'OverworldGlitches'`:
  22 Standard worlds and one Open world, the same `RescueZelda` pre-collect, the same medallion presets.
  So the stub `TestCase` (with 3.6's `$region` and `worldUnderTest()`), the method-kind table and the
  capture code need no change. All six methods in the
  tree are already in the method-kind table: `testLocation`, `testFillLocation`, `testEntry`,
  `testNothingRequiredToEnter`, `testCapeOrUpgradedSwordRequiredToEnter` and
  `testSwordRequiredToComplete`. The trees use distinct PHP namespaces, so both load in one process.
  Unknown-method and replay failures still exit 1 and write nothing.
- The row record format is unchanged: 15 tab-separated fields, with the file column relative to the row's
  tree. The overrides are what VT's capture recorded against the NoGlitches reference world. For these rows
  that means `logic="OverworldGlitches"` plus the glitch flags VT actually read (for example
  `canBootsClip=true`).
- **`logic` is a constructor key.** For every query except `vanilla_dungeon_items`, the extractor's own
  replay passes a `logic` override to `World::factory` with the test config. It applies all the parsed
  overrides with `testSetConfig` as now.
  - `vanilla_dungeon_items` rows (3.6) already build their world from the test config plus every
    override, and are untouched.
  - No other NoGlitches row has a `logic` override, so their replay is unchanged.
  - The prototype showed this replay and the old one (`testSetConfig('logic', …)` after a NoGlitches
    factory) give identical output on all 1 204 rows. `noglitches_rows.txt` stays byte-identical.
  - The constructor form is chosen because it is the C replay's form too (below).
- The new file's header copies the NoGlitches row header, naming its tree, and adds one line: a `logic`
  override is passed to `World::factory`, and the others go through `testSetConfig`. `noglitches_rows.txt`'s
  header is not touched, so the file stays byte-identical.
- The extractor's summary line also counts the OverworldGlitches rows and their VT-versus-test mismatches.
  It is not a fixture. Each mismatch is also printed to stderr, as for NoGlitches; the prototype found
  none.
- `gen_golden.sh` needs no change, since it already runs the extractor into the oracle directory or
  `--out`. Its header comment, which says "the logic fixtures (extract_logic.php)", stays true.

### C replay: a fixture path, and `logic` before world init

- The test-support row replay keeps its shape (test name, row filter, total out-parameter) and gains the
  fixture path as its second parameter. A second path constant names the new fixture.
  - Its three existing callers pass the NoGlitches path, and their output is unchanged: the regions
    test's accept-every-row replay (3 067 rows), and the world test's region-locked (450) and
    vanilla-dungeon (8) replays.
  - The row-filter contract becomes "a row of that test file (relative to the fixture's VT tree, or
    `tests/` for the top-level files) and method".
- **Replay world:** for every query except `vanilla_dungeon_items`, 3.6's `apply_overrides` is reused on
  the constructor config before world init, with only the row's `logic` entry. World init's logic-glitch
  derivation then sets every flag `config/logic.php` gives that logic. All the overrides are then applied
  to the world's config after init, as now (`testSetConfig`; `logic` again, which changes nothing).
  - `vanilla_dungeon_items` keeps its 3.6 path, which applies every override before init.
  - Applying *all* overrides before init is rejected: keys such as `dungeonItems` would then go through
    world init's derivation, which VT's `testSetConfig` never runs.
  - Why: VT's test world resolves every glitch flag lazily from `logic.OverworldGlitches`, including flags
    the row's query never read and so never recorded. The C rules read struct fields, so a flag left at
    its NoGlitches default would give a wrongly shaped C rule a different input from VT's.
  - On the 3.6 code, the prototype passed all 1 204 rows both with and without this, and all 3 067
    NoGlitches rows with it. It is chosen for correctness of the
    comparison, not to turn anything green.
  - This is a contract of the replay helper and is stated in its header comment.
- The regions test's `--fixtures` mode replays both fixtures with its accept-every-row filter. It fails
  unless every row record of each file is replayed and each file has at least one. It prints one count line
  per fixture.
- The world test's `testRegionLockedItems` replay stays on the NoGlitches fixture: the OverworldGlitches
  tree has no such method.
- `LOGIC_TESTS`, the Makefile targets and CI are unchanged: `make logic` already runs the regions test with
  `--fixtures`.

### Divergences: one bug, one test, one fix

- None is expected at HEAD (prototype on the 3.6 code). If a row diverges in the real implementation,
  each distinct cause is handled as `fix-bug` does:
  - Prove the cause against the VT rule.
  - Add a hand-written test to the regions test that builds the row's world (Standard or Open, logic
    `OverworldGlitches`, the row's presets) and items, and asserts VT's answer. It uses 3.6's rule shape
    `(world, locations, items)` through the single-world entry points.
  - Fix the region rule, then commit.
- Several rows sharing one cause make one bug.
- The fixture is never edited or regenerated to get green.

### Hand-written derivation check

- One case is added to the world test's derivation checks (`make test`):
  - A world initialized with `logic` `OverworldGlitches` has `can_super_bunny`, `can_fake_flipper`,
    `can_super_speed`, `can_boots_clip`, `can_mirror_clip`, `can_water_walk` and `can_dungeon_revive` set.
  - It has `can_mirror_wrap`, `can_owyba`, `can_one_frame_clip_ow`, `can_one_frame_clip_uw`,
    `can_transition_wrapped`, `can_bunny_surf` and `can_bunny_revive` clear.
  - It has every `rom_vanilla_*` clear: VT's `$glitched_logic` excludes OverworldGlitches.
- A NoGlitches world has all fourteen flags clear.
- This pins VT's `config/logic.php` OverworldGlitches block, which the replay depends on (user story 13).

### Claims, gate and library

- No claim changes. `glitches-overworld_glitches` stays at `all` and the claims file is untouched.
- No e2e change: no generator run or check is added.
- The library is not expected to change. If no bug fix touches the library, `make measure` is not rerun and
  the note says the library is unchanged. If one does, `make measure` is run and its maximum host time per
  case, code+rodata (host and ARM proxy) and stack peak are recorded against the previous step's figures.

## Implementation Chunks

1. **Oracle: the OverworldGlitches fixture (one commit, oracle only).**
   - Generalize the extractor to the tree table.
   - Make `logic` a constructor key in its replay.
   - Write the new header.
   - Run `gen_golden.sh --out <tmp>` and check that every case's `spoiler.json`, `trace.txt` and
     `attempts`, and every existing fixture (`world_dump.txt`, `predicate_vectors.txt`,
     `noglitches_rows.txt` with its 3 067 rows, `pool_lists.txt`, `prepared_pools.txt`), are byte-identical to the committed
     files. Then write the fixture into the oracle directory.
   - Expect 1 204 rows, 41 methods, 23 files, 0 VT-versus-test mismatches. A different count is
     investigated, not accepted: VT is pinned.
   - Commit only the extractor and the new fixture. `make test`, `logic` and `e2e` are unchanged by
     construction, since nothing reads the file yet.
2. **C replay and derivation check.**
   - The derivation case comes first (red-green is immediate, since the library already derives the flags).
   - Then the path parameter and the `logic`-before-init rule in the row replay, and the regions test
     replaying both fixtures.
   - Gate: `make test`, `make logic` (now also the 1 204 rows) and `make e2e` green, with
     `glitches-overworld_glitches` still passing at `all`.
3. **Divergence fixes, only if chunk 2's `make logic` reports a row.** One `fix-bug` cycle per distinct
   cause (test in the regions test, rule fix, commit), until `make logic` is green. With a library change,
   run `make measure`.
4. **Step note `### Step 3.1`** in the plan, and the row ticked (see Further Notes).

## Testing Decisions

- **What makes a good test here:** it asks the C port the same question VT's test asked (`canAccess`,
  `fill`, `canEnter`, `canComplete` on a world built from the record) and compares the answer with VT's
  recorded answer. It never looks at how a rule is written.
- **The 1 204 OverworldGlitches rows** are replayed by the regions test's `--fixtures` mode under
  `make logic`, as step 2.2's NoGlitches rows are. Prior art: the NoGlitches row replay in the test support
  and the regions test's main.
- **`make test` gains exactly one hand-written case** (the derivation check), plus one hand-written regions
  case per divergence fixed in chunk 3. No fixture is read by `make test`.
- **Mutation checks during implementation** (not committed, recorded in the note):
  - Clearing one derived OverworldGlitches flag in world init must fail `make test` (derivation case) and
    `make logic`. Use `can_boots_clip`, the flag that flips the most rows.
  - Dropping the `logic`-before-init step must still pass `make logic`: the prototype shows the recorded
    overrides suffice today, and the step exists for comparisons a later rule change could break.
- **Not pinned by any OverworldGlitches row:** `canSuperSpeed`. VT reads it, and it appears in 157 rows'
  overrides, but no answer depends on it. This step adds no hand-written test for its rule sites: their
  expected values would not come from VT. The derivation case pins the flag itself; its rule sites wait for
  the MajorGlitches rows (row 3.3) and stay covered by the `all` claims meanwhile.
- **Timing:**
  - `make test` stays at step 3.6's figure, 65–82 ms (H-13-7).
  - `make logic` grows by about 10 to 15 ms (prototype on the 3.6 code: 16 ms for the 1 204 rows).
  - e2e stays at about 0.17 s (step 3.6).
  - e2e stays under 0.5 s and is unchanged in shape.

## Out of Scope

- `tests/MajorGlitches` (row 3.3), the three Inverted trees (row 3.12) and `glitches-no_logic`'s fill
  divergence (row 3.4). This step only makes the extractor and the replay ready to take another tree.
- Hand-written tests for glitch rule sites no VT row pins (`canSuperSpeed`).
- Any change to claims, the e2e gate, the generator or the playthrough.
- Step 3.6's work: the two-world rule shape, the playthrough's world pairs, and its owed `RegionTest` and
  `WorldTest` rows in `noglitches_rows.txt`.
- The host-only warn-review items left for the pull request, and `item_placement-basic`'s A-3 overrun.

## Further Notes

### Hypotheses about step 3.6's outcome, with verdicts

This PRD was first written while step 3.6 was being implemented, against step 3.0's committed code
(`39cf756`) and 3.6's PRD. Step 3.6 is now committed (`dfe661d`, its step note in the plan). Each
hypothesis was checked against it, and the prototype was rerun on its code.

- **H-13-1 — holds.** 3.6 changes no glitch branch's outcome. On the committed 3.6 library, the C port
  gives VT's answer on all 1 204 rows, and the per-flag sensitivity table is unchanged. Chunk 3 is expected
  to be empty.
- **H-13-2 — holds.** The row replay still calls the single-world entry points (`rando_world_can_access`,
  `rando_world_fill`, `rando_world_region_can_enter`, `_can_complete`, `_can_fill`), which 3.6 kept. The
  OverworldGlitches rows need no `rando_world_can_access_with`.
- **H-13-3 — fails in its shape, holds in substance.**
  - What changed: 3.6 replaced the per-directory class list with a source list of
    `(file, class, methods run)` entries. That list appends the `TOP_LEVEL_METHODS` table (`RegionTest`,
    `WorldTest`). 3.6 also added the `vanilla_dungeon_items` query, with its own factory-built replay.
  - What still holds: `noglitches_rows.txt` (3 067 rows) is the byte-identical baseline, and the new rows
    never go into it.
  - The extractor decisions above are rewritten for that shape: a tree parameter on the source list and
    the row dump, `TOP_LEVEL_METHODS` for the NoGlitches tree only, and the constructor-`logic` rule
    leaving `vanilla_dungeon_items` alone. The prototype ran exactly this on the 3.6 extractor.
- **H-13-4 — holds.** The row replay helper still takes the test name, the row filter and the total.
  3.6 added the `vanilla_dungeon_items` query and the `region_named` and bare-`Region` lookups inside it,
  not parameters. The fixture path becomes the second parameter, and the three callers (regions: every
  row; world: region-locked, vanilla-dungeon) pass the NoGlitches path.
- **H-13-5 — holds.** `glitches-overworld_glitches` is still at `all`. Claims: 41 at `all`,
  `glitches-no_logic` at `prepareWorld`; 42 pass, 2 pending; e2e about 0.17 s.
- **H-13-6 — holds; the mechanism is decided.** 3.6's test support has `apply_overrides` (a list of
  overrides applied to a config). Its `vanilla_dungeon_items` path applies every override before world
  init. This step reuses `apply_overrides` on the constructor config with only the `logic` entry, then
  keeps the after-init application for all overrides. Pre-applying everything is rejected (see "C replay").
- **H-13-7 — holds.** The baseline is 3.6's note: ARM code+rodata 80.4 KB, stack peak 6 328 B, maximum
  host case 19.8 ms (`item_placement-basic`), `make test` 65–82 ms, e2e about 0.17 s.
- **H-13-8 — holds.** The regions test's main still ends with the accept-every-row replay ("all N rows
  replayed"), after 3.6's five new two-world cases. The OverworldGlitches replay is appended after it.
  3.6's world test vanilla-row replay covers NoGlitches, MajorGlitches, HybridMajorGlitches and NoLogic
  but not OverworldGlitches, so this step's derivation case still adds the OverworldGlitches line.

### Step note (`### Step 3.1`) must record

- The fixture: 1 204 rows from 41 methods in 23 files, written and replayed by VT, 0 mismatches against
  the tests. Goldens and existing fixtures byte-identical.
- The replay: `logic` as a constructor key on both sides, and `make logic`'s row count per fixture.
- The divergences found and fixed (expected: none), each with its test.
- The per-flag sensitivity table, and `canSuperSpeed` as unpinned.
- The verdict on each H-13 hypothesis (all hold except H-13-3's shape, as recorded above).
- `make test`, `make logic` and e2e times against step 3.6's figures (H-13-7). `make measure` figures only if the
  library changed.
- Row 3.1 closed in step 3.0's owed table, and the step ticked.
