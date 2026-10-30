---
title: "Weapons: port VT's vanilla-swords Randomizer tests"
description: "The weapons row's three cases are claimed at all, but its owed VT tests, RandomizerTest::testVanillaSwordsSet and ::testVanillaSwordsSetWithPedestalGoal, are not ported, so step 3.14 cannot close."
status: needs-triage
---

## Problem Statement

The `weapons` row (plan Phase 3, step 14) is the last open row of the step 3.0 table. Its three
cases, `weapons-assured`, `weapons-vanilla` and `weapons-swordless`, are already claimed at `all`
in the claims file, so the generator matches VT on them. What the row still owes is VT's two
weapons tests in the top-level `RandomizerTest`:

- `testVanillaSwordsSet`: a Standard world built by `World::factory('standard', ['difficulty' =>
  'test_rules', 'accessibility' => 'full', 'mode.weapons' => 'vanilla'])`, then
  `Randomizer::prepareWorld`; it asserts that Link's Uncle, Pyramid Fairy - Left, Blacksmith and
  Master Sword Pedestal hold `UncleSword`, `ProgressiveSword`, `ProgressiveSword`,
  `ProgressiveSword`.
- `testVanillaSwordsSetWithPedestalGoal`: the same with `'goal' => 'pedestal'`; the pedestal then
  holds the `Triforce` (prepareWorld's goal switch put it there, and the vanilla branch pops the
  fourth sword and appends a `TwentyRupees` to the item pool instead).

Neither combination is fully covered today. `weapons-vanilla` is an Open world with the Ganon
goal; `goal-pedestal` has randomized weapons; nothing in the e2e matrix runs vanilla weapons on a
Standard world or with the pedestal goal. The hand-written `check_pools_vanilla` cases in the
prepare unit test cover both branches, but on synthetic pools and a hand-transcribed expectation,
not VT's own answer on VT's real Standard world pool. A maintainer cannot tick step 3.14 until
those two tests are ported the way the other rows' owed tests were.

Facts established while writing this PRD (both sides probed outside the repository):

- **VT agrees with both tests.** Run inside the patched VT checkout the oracle uses
  (`tests/rando/oracle/vt`, the clone `gen_golden.sh` makes at the pinned commit), with the shared
  PRNG seeded by `prng_seed` at 1, 2 and 3: the four items are `UncleSword`, `ProgressiveSword`
  ×3 without the goal and `UncleSword`, `ProgressiveSword` ×2, `Triforce` with it, every seed.
  `prepareWorld` makes 228 draws (229 with the pedestal goal), but no draw decides these four
  locations: the swords are popped from lists built before any shuffle, and all four are
  `ProgressiveSword`. `UncleSword`'s target is `ProgressiveSword`.
- **The C port already matches.** `rando_generate_until` with `state=standard`,
  `weapons=vanilla`, `goal` ganon or pedestal, seed 1, stopped after `RANDO_STAGE_PREPARE_WORLD`,
  leaves the same four items. No library change is expected.
- **The patched VT needs a seed.** `prepareWorld` draws; the patched `get_random_int` throws
  until `prng_seed` has been called. Called directly, its draws are labelled by the
  `prepareWorld` frame, so no trace label is needed.
- **`accessibility: full` is a legacy VT value.** VT reads `accessibility` only as
  `!== 'locations'` (region fill rules) and `!= 'locations'` (the completionist switch), plus the
  spoiler meta. `full` therefore behaves exactly as `items` for everything prepareWorld and the
  rules do. The C option set has no `full`.
- **`difficulty: test_rules`** only matters to VT where it compares against `custom`; it is the
  extractor's existing `TEST_CONFIG` value, so it is not an override.

## Solution

Port both tests as oracle fixture rows, the way step 3.6 ported
`WorldTest::testSetVanillaDungeonItems` and step 3.13 ported `RetroBowTest::testLocation`:

- `extract_logic.php` runs the two `RandomizerTest` methods inside VT and writes one row per
  asserted location (8 rows) into `noglitches_rows.txt`, with a new query, `prepared_item`. Each row
  records the World::factory overrides, the location, the item the test expects there, the test's
  expectation and VT's answer, and the extractor proves VT replays its answer from the row alone.
- `make logic` replays them through the C generator: the row's overrides become generator options,
  `rando_generate_until` stops after the prepareWorld stage, and the location must hold the item
  exactly when VT's answer says so. The prepare unit test's fixture mode counts the 8 rows.

`make test` and `make e2e` are untouched: no hand-written case, no new e2e case, no new claim. The
step closes with its note and the row's status set to closed.

## User Stories

1. As a maintainer, I want VT's `testVanillaSwordsSet` ported, so that step 3.14's owed test list is
   empty.
2. As a maintainer, I want VT's `testVanillaSwordsSetWithPedestalGoal` ported, so that the
   interaction between vanilla weapons and the pedestal goal is checked against VT, not against my
   reading of `prepareWorld`.
3. As a maintainer, I want the ported tests to carry VT's own answers, so that a transcription
   mistake in a hand-written expectation cannot hide a divergence.
4. As a maintainer, I want the extractor to fail when VT's recorded row does not replay VT's
   answer, so that a row never encodes something the replay cannot reproduce.
5. As a maintainer, I want the extractor to report a row where VT disagrees with its own test, so
   that a VT bug is visible instead of silently becoming the reference.
6. As a maintainer, I want the extractor to fail on a `RandomizerTest` method it was not told about,
   so that only the two owed methods are run and the rest (boss shuffle, crystals, pendants) stay
   out of scope.
7. As a maintainer, I want the extractor to fail when the number of asserted locations differs from
   the number of items in the assertion, so that the row-per-location expansion cannot drift.
8. As a maintainer, I want the extractor to fail on a config override the C replay cannot map, so
   that a VT test change surfaces in the extractor rather than as a mysterious C fixture failure.
9. As a maintainer, I want every golden and every other fixture byte-identical after the extractor
   change, so that the commit only adds the eight rows and their header lines.
10. As a maintainer, I want `make logic` to replay the eight rows through the real generator, so that
    the check covers world init, the goal switch, the pool build and every pre-fill stage up to
    prepareWorld, as VT's test does.
11. As a maintainer, I want the replay to fail loudly when the generator does not stop after the
    prepareWorld stage, so that a regression in an earlier stage cannot pass as "no item".
12. As a maintainer, I want the prepare unit test to count exactly eight `RandomizerTest` rows in
    fixture mode, so that a row lost from the fixture is caught.
13. As a maintainer, I want the existing row replayers (the region test's every-row replay) to keep
    passing with the new query, so that adding a query never breaks a replayer that walks every row.
14. As a developer running the fast loop, I want `make test` no slower, so that the TDD loop stays
    fast.
15. As a developer running the gate, I want `make e2e` no slower and unchanged, so that the claims
    stay exactly what they are.
16. As a maintainer, I want the hand-written `check_pools_vanilla` cases kept, so that the
    vanilla branch's list edits (the dropped sword, the `TwentyRupees` under the pedestal goal) stay
    pinned in the fast loop on small pools.
17. As a maintainer, I want the step 3.14 note to record the row count, VT's agreement, the replay
    path and the `make test`, `logic` and e2e times, so that the plan's convergence-gate bookkeeping
    is complete.
18. As a maintainer, I want the step 3.0 table to show 3.14 closed, so that the plan shows every
    Phase 3 row done.
19. As a future porter of roadmap item 2, I want the fixture header to say that `accessibility
    "full"` is replayed as `items`, so that nobody adds a C value VT itself treats as `items`.
20. As a reviewer, I want the PRD's decisions recorded in the step note's terms, so that I can check
    the commit against them without rerunning VT.

## Implementation Decisions

### How the two tests are ported: fixture rows, not hand-written C tests

- **Fixture rows replayed by `make logic`.** This follows § Convergence gate ("`make test` gets
  only hand-written cases; ported … tests replay through `make logic` fixtures") and the precedent
  of every owed test ported so far (`WorldTest::testSetVanillaDungeonItems`,
  `RegionTest::testRegionLockedItems`, `RetroBowTest::testLocation`). It records VT's own answer,
  needs no PHP to run, and leaves `make test` and `e2e` unchanged.
- Rejected: a hand-written `make test` case running the generator to prepareWorld on the two
  configs. It would cost two generator runs in the fast loop and carry a hand-transcribed
  expectation, which is what `check_pools_vanilla` already is.
- Rejected: a new e2e case (Standard + vanilla, or vanilla + pedestal). The matrix is "one case per
  non-default value"; interactions are deferred to the second pass (§ Test matrix), and it would
  need a new golden.

### Oracle (`extract_logic.php`)

- `RandomizerTest.php` joins the top-level test files, restricted to its two methods
  (`testVanillaSwordsSet`, `testVanillaSwordsSetWithPedestalGoal`); every other method of the file
  is skipped by that restriction. Its rows go into `noglitches_rows.txt`, with the other top-level
  tests.
- Both methods map to a new query kind, `prepared_item`, with no collection, location or item
  parameter.
- The test-case stub declares the properties `RandomizerTest` assigns (its four worlds and four
  randomizers), so no dynamic property is created, and the world under test falls back to the
  Standard world property when the stub's own world is unset.
- Before each `prepared_item` method runs, and before each replay, the extractor seeds the patched
  PRNG with a fixed seed (1) and starts a fresh trace. The rows carry no seed: no draw decides the
  answer.
- **Row expansion.** The one `assertEquals` of each method compares two lists of four items. The
  extractor takes the location names from the method source (its `getLocation("…")` calls, in
  order), fails unless there are exactly as many as there are items in both lists, and writes one
  row per location, the index field being the position (0 to 3). For `prepared_item` only, the
  non-boolean-assertion check is replaced by these list checks.
- **Row fields.** Variant `standard`; config overrides are the keys whose value in the captured
  world differs from the reference Standard test world, which must be a subset of `accessibility`,
  `mode.weapons` and `goal` (anything else fails the extractor); preset `-`; counts and values `E`;
  query `prepared_item`; target the location name; item the raw name of the item the test expects
  there; test expectation `1`; VT answer `1` when VT's location holds an item of that raw name.
  Expected rows: `accessibility="full",mode.weapons="vanilla"` for the first method,
  `accessibility="full",goal="pedestal",mode.weapons="vanilla"` (in the extractor's key order) for
  the second.
- **PHP replay of a row:** `World::factory(variant, TEST_CONFIG + overrides)`, seed, a
  `Randomizer` on that world, `prepareWorld`, then compare the location's item raw name. The
  extractor fails when the replay differs from the capture, and reports a VT mismatch as for the
  other rows.
- The `noglitches_rows.txt` header gains lines for `RandomizerTest.php` and the `prepared_item`
  query: its replay, the fixed seed, and that `accessibility "full"` is VT's legacy value and is
  replayed as `items`.
- Regenerated with `gen_golden.sh` in the chunk's own commit: only `noglitches_rows.txt` changes
  (8 rows and the header lines); every golden, `world_dump.txt`, `predicate_vectors.txt`, the other
  row files and the pool fixtures stay byte-identical.

### C replay (test support)

- The shared row replayer gains `prepared_item`. It builds generator options from the row, not a
  `RandoConfig`: variant `standard` sets `state=standard`; `mode.weapons` maps to `weapons`, `goal`
  to `goal`, `accessibility` to `accessibility` with `"full"` mapped to `items`; any other key fails
  the fixture. It then runs `rando_generate_until` with seed 1, unbounded attempts and
  `RANDO_STAGE_PREPARE_WORLD` as the last stage, fails the fixture unless the status is
  `RANDO_INCOMPLETE`, and returns whether the target location holds the row's item.
- Going through the generator, not a hand-assembled stage sequence, keeps the replay on the public
  interface and exercises world init, the goal switch, the pool build and every stage VT's
  `prepareWorld` runs. The generator state lives in a static, not on the stack.
- The library (`rando/`) is not expected to change: the C port already matches both tests. If a
  row fails, the fix goes in the library through `fix-bug`, never in the row.

### Where the rows are counted

- The prepare unit test, which owns prepareWorld, gains a fixture mode: it replays the
  `RandomizerTest` rows of `noglitches_rows.txt` (filter on the two method names) and fails unless
  exactly 8 are replayed. It joins the logic tests (removed from the Makefile's "no fixture mode"
  exclusion list), so `make logic` reports one more unit test. Its hand-written checks still run in
  both modes, as the other fixture-mode tests do.
- The region test's every-row replay of `noglitches_rows.txt` replays the eight rows too, through
  the same shared replayer; its total grows by 8.

### Gate, claims and docs

- Claims unchanged: the three weapons cases are already at `all`. No new case, no golden
  regeneration beyond the fixture.
- `make test` and `make e2e` unchanged in content; their times are recorded in the note against
  step 3.13's (~85 ms and ~0.2 s). `make logic` grows by one test binary and 16 short generator runs
  (8 rows, replayed by two tests). `make measure` is not rerun: the library does not change.
- Step 3.14 note: rows extracted, VT agreement, the replay path, `accessibility "full"` → `items`,
  library unchanged, times. The step 3.0 table's 3.14 status becomes closed.

## Implementation Chunks

1. **Oracle rows and their C replay (test commit).** Extractor: `RandomizerTest.php`'s two methods,
   the `prepared_item` query, row expansion, seeding, PHP replay, header lines; regenerate with
   `gen_golden.sh` (only `noglitches_rows.txt` changes). In the same commit, so `make logic` never
   goes red in a commit: the shared replayer's `prepared_item` (options mapping, generator run to
   prepareWorld), the prepare unit test's fixture mode with its count of 8, and its move into the
   logic tests. Red first: with the rows present and no replayer, `make logic` fails on the unknown
   query; green when the replay and count land.
2. **Step note (docs commit).** The `### Step 3.14` note and the step 3.0 table's status, with the
   `make test`, `logic` and e2e times.

## Testing Decisions

- A good test here checks VT's observable answer (which item a location holds after prepareWorld)
  through the generator's public entry point, not the internal pool lists or the stage functions.
- Tested: the prepare module, through `rando_generate_until`, by the 8 fixture rows in `make logic`
  (prepare unit test, with the exact count, and the region test's every-row replay). The shared
  replayer is exercised by both.
- The hand-written `check_pools_vanilla` cases (with and without the pedestal goal) stay in
  `make test` unchanged: they pin the list edits on synthetic pools, which the rows do not see.
- No e2e change; `make selftest` unaffected.
- Prior art: `WorldTest::testSetVanillaDungeonItems` (the `vanilla_dungeon_items` query: a
  World::factory-only query handled ahead of the generic path, counted by the world test),
  `RetroBowTest::testLocation` (a top-level test file restricted to named methods), and the prepare
  test's baseline checks that already drive `rando_generate_until` to a stage.

## Out of Scope

- The rest of `RandomizerTest`: `test{Crystals,Pendants}NotRandomizedByConfig*` (vary
  `prize.shuffle*`, not in the matrix), `*BossShuffle` (enemizer), `testCompletionistItemCount`
  (ROM-only), as the step 3.0 note lists.
- A C `accessibility` value `full`.
- New e2e cases for option interactions (Standard + vanilla, vanilla + pedestal): the matrix's
  second pass.
- Any library change, unless a row exposes a divergence (then `fix-bug`).
- `make measure`.

## Further Notes

- The answer is seed-independent in VT (probed at seeds 1–3) and in C; the rows deliberately carry
  no seed and the C replay does not try to reproduce VT's 228/229 draws.
- If VT's test world ever gains a config key the C options cannot express, the extractor's
  override whitelist fails first, before any C fixture does.
- After this step every Phase 3 row is closed; the plan moves to Phase 4.
