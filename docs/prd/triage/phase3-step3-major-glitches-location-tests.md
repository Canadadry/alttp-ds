---
title: "major_glitches row: VT's tests/MajorGlitches replayed against the C port"
description: "The MajorGlitches branches of the C location rules are checked only through one e2e seed; VT's tests/MajorGlitches (1 923 logic rows) has never been replayed against them."
status: needs-triage
---

## Problem Statement

Row 3.3 of Phase 3 is the `glitches: major_glitches` value. Its one case, `glitches-major_glitches`, has
been claimed at `all` since step 3.0, but the row is not done. Step 3.0's owed table says it still owes
all of VT's `tests/MajorGlitches`: 23 files and 41 test methods. A row step is done only when its cases
are claimed at `all` and its listed tests are ported.

Right now one seed is the only check on the C port's MajorGlitches logic. MajorGlitches turns on five
flags that no ported test has ever set: `canMirrorWrap`, `canOWYBA`, `canOneFrameClipOW`,
`canOneFrameClipUW` and `canTransitionWrapped`. It also turns on the seven OverworldGlitches flags.
`tests/NoGlitches` (step 2.2) sets none of these flags. `tests/OverworldGlitches` (step 3.1) sets only the
seven. The region rules have branches for every flag, transliterated from VT. One seed reaches only the
branches its own fill happens to use. A wrong one-frame-clip or YBA branch elsewhere would go unnoticed
until a seed that needs it is generated on a DS.

## Solution

Port `tests/MajorGlitches` with the mechanism step 3.1 builds for `tests/OverworldGlitches`:

- **Oracle:** add one entry to the extractor's table of VT test trees. VT then runs every logic method of
  the tree and writes one row per data-provider row to a new committed fixture. VT replays each row before
  writing it.
- **C:** the regions test replays the new fixture under `make logic`, with one more call to the row
  replay helper. `make test` never reads it.
- **Divergences:** any row where the C port's answer differs from VT's is a rule bug. It gets a
  hand-written test and a fix.

Beyond the table entry and the replay call, the step adds two small pieces:

- **A fixture config key:** the C fixture reader learns one key, `rom.freeItemMenu`. Every
  MajorGlitches row carries it, and it is ROM-only.
- **A derivation check:** one hand-written check pins `config/logic.php`'s MajorGlitches block in world
  init.

### Prototype result

A throwaway prototype was built and not committed. It ran in a scratch copy of the committed tree
(`dfe661d`, from `git archive`), with step 3.1's mechanism hand-applied as its PRD describes:

- The extractor was given a tree parameter and a `MajorGlitches` output file.
- The extractor's replay passes `logic` to `World::factory`.
- The C row replay takes the fixture path, and sets the row's `logic` before world init.

What it found:

- **VT side:** VT writes 1 923 rows from the 40 logic methods of 23 files, and replays every one.
  VT's answer equals the test's expectation on all 1 923.
  - `world_dump.txt`, `predicate_vectors.txt` and `noglitches_rows.txt` (3 067 rows) come out
    byte-identical.
  - The fixture is 791 KB. It holds 1 731 `access` rows, 144 `fill` rows, 47 `enter` rows and 1
    `complete` row. 1 890 rows use Standard worlds and 33 use Open worlds. 1 491 answers are true and 432
    false.
- **The C fixture reader** first refused every row, on an unknown config key: `rom.freeItemMenu=16`.
  VT's constructor sets it to `0x10` for glitched logic, and VT reads it only when writing the ROM. Once
  the reader accepted and ignored that key, **the C port gave VT's answer on all 1 923 rows**. It did so
  both with and without the `logic`-before-init rule. The NoGlitches rows still pass.
- **The rows really exercise the MajorGlitches branches:**
  - With `logic` and every glitch override dropped, 1 304 of the 1 923 rows flip.
  - Forcing one flag off at a time, after the overrides, flips this many rows:

  | Flag | Rows flipped | Rows whose overrides record it |
  |---|---|---|
  | `canOneFrameClipOW` | 778 | 1 388 |
  | `canOWYBA` | 613 | 1 194 |
  | `canOneFrameClipUW` | 477 | 542 |
  | `canDungeonRevive` | 127 | 315 |
  | `canTransitionWrapped` | 92 | 92 |
  | `canMirrorWrap` | 68 | 231 |
  | `canFakeFlipper` | 65 | 194 |
  | `canSuperBunny` | 6 | 10 |
  | `canWaterWalk` | 5 | 10 |
  | `canMirrorClip` | 4 | 63 |
  | `canBootsClip` | 0 | 603 |
  | `canSuperSpeed` | 0 | 413 |

- **Masking:** clearing `can_one_frame_clip_ow` in world init's derivation still passes `make logic`. The
  recorded overrides re-set every flag VT read. It fails `make e2e` (`glitches-major_glitches`
  divergent). This is why the derivation check below is hand-written.
- **Cost:** `test_regions --fixtures` went from about 30 ms to about 58 ms on the prototype host, so the
  replay adds about 25 ms to `make logic`. `make test` is unchanged.

So the expected work is one extractor table entry and header, the fixture, one accepted config key, one
replay call and one hand-written derivation check. No rule fix is expected.

## User Stories

1. As the randomizer maintainer, I want every logic method of VT's `tests/MajorGlitches` replayed against the C port, so that row 3.3's owed tests are ported and the row can be ticked.
2. As the randomizer maintainer, I want the MajorGlitches rows written by VT itself, so that the oracle stays the source of truth and no expected value is typed by hand.
3. As the randomizer maintainer, I want every row replayed by VT from its record alone before it is written, so that a record that loses information fails the extractor instead of producing a wrong fixture.
4. As the randomizer maintainer, I want adding the tree to be one entry in the extractor's tree table, so that the mechanism step 3.1 built is reused rather than forked.
5. As the randomizer maintainer, I want the extractor to fail, writing nothing, on an unknown method in the new tree, so that a VT test we don't understand never becomes a silent gap.
6. As the randomizer maintainer, I want `testDungeonItemWrite` skipped with a recorded reason, as in `tests/NoGlitches`, so that a ROM-byte test is not mistaken for a logic gap.
7. As the randomizer maintainer, I want the new rows in their own fixture file, so that `noglitches_rows.txt`, `overworld_glitches_rows.txt` and the other fixtures stay byte-identical.
8. As the randomizer maintainer, I want the fixture regenerated in its own commit, with every golden case and every existing fixture byte-identical, so that the oracle change is reviewable on its own and no golden is regenerated to get green.
9. As a porter of glitch rules, I want the C replay to build the row's world with `logic` `MajorGlitches` set before world init, so that every derived flag and every `rom.vanilla*` key holds VT's value even when the row's query never read it.
10. As a porter, I want the C fixture reader to accept `rom.freeItemMenu` and ignore it, while still refusing any other unknown key, so that a ROM-only key VT's constructor derives does not block the replay and the unknown-key guard stays.
11. As a porter, I want a row on which C differs from VT to print its file, method, provider row, query, VT's answer and C's, so that I can go from the failure to the VT test line at once.
12. As a porter, I want every divergence fixed as a bug with its own hand-written test in the regions test, so that the fix stays guarded in `make test` even if the fixture is ever regenerated.
13. As the randomizer maintainer, I want a hand-written check that `logic = MajorGlitches` derives exactly VT's twelve flags and leaves `canBunnySurf` and `canBunnyRevive` off, so that the derivation is pinned. The fixture alone can't pin it: the recorded overrides mask a missing derived flag (prototype).
14. As the randomizer maintainer, I want the same check to pin that MajorGlitches sets the four `rom.vanilla*` keys when no dungeon item is wild, so that the derivation behind the ROM write is pinned too.
15. As the randomizer maintainer, I want `make test` to grow only by hand-written cases, so that the fast loop stays fast (plan rule).
16. As the randomizer maintainer, I want the 1 923 rows in `make logic`, which CI runs, so that a regression in a MajorGlitches branch fails CI.
17. As the randomizer maintainer, I want `glitches-major_glitches` to stay claimed at `all` and e2e to stay under 0.5 s, so that this step breaks no claim and no budget.
18. As a reviewer, I want the step note to record the row count, the per-flag sensitivity, the timings and whether the library changed, so that I can see what was ported and what it cost.
19. As a reviewer, I want the step note to name the flags no MajorGlitches row pins (`canSuperSpeed`, `canBootsClip`), so that the gap is known.
20. As a DS integrator, I want the step note to say that MajorGlitches turns on VT's vanilla-dungeon-item ROM substitution, so that roadmap item 2 knows the item given in game is not always the item placed.
21. As a DS integrator, I want this step not to change the library unless a row reveals a bug, so that the ARM9 figures stay those of the previous step.
22. As a reader of the fixture, I want its header to say which VT tree its rows come from, how a `logic` override is applied, and that `rom.freeItemMenu` is ROM-only, so that the record can be replayed without reading the extractor.
23. As a future porter of row 3.12 (`tests/InvertedMajorGlitches`), I want this step to leave the tree table and the replay helper shaped as they are, so that the Inverted trees are also one entry each, once the Inverted world exists.

## Implementation Decisions

### Oracle: one more tree

- The tree table gets one entry: `MajorGlitches` writes `major_glitches_rows.txt`, which holds row
  records only. The tree table and the `logic` constructor key come from step 3.1 (H-14-1, H-14-2).
- **The tree needs no other extractor change** (prototype). The setUps match the NoGlitches ones apart
  from `'logic' => 'MajorGlitches'`: 22 Standard worlds and one Open world, the same `RescueZelda`
  pre-collect, the same medallion presets.
  - `DesertPalaceTest`'s extra `world_free_items` world is used only by `testDungeonItemWrite`.
  - Every logic method in the tree is already in the method-kind table: `testLocation`,
    `testFillLocation`, `testEntry`, `testNothingRequiredToEnter`, `testCanEnterWithEverything` and
    `testSwordRequiredToComplete`.
  - The tree has its own PHP namespace, so it loads in the same process as the other trees.
- **`testDungeonItemWrite` stays in the skipped-method list** (one method, 8 provider rows). It writes a
  ROM and reads a byte back (`Location::writeItem`). The C port outputs a location-to-item table and
  writes no ROM. `tests/NoGlitches`' copy is skipped for the same reason. Ported: 40 of the tree's 41
  methods. The 41st is closed as ROM-only in the step note and the owed table.
- **Header:** the new file's header is 3.1's tree header, naming `tests/MajorGlitches` (H-14-3), plus one
  line. Every row carries `rom.freeItemMenu`, a ROM-only key VT's constructor derives from glitched logic.
  A replay may ignore it.
- **The row format is unchanged.** Every MajorGlitches row's overrides hold:
  - `logic="MajorGlitches"`,
  - `rom.freeItemMenu=16`,
  - the four `rom.vanilla*=true` keys,
  - the glitch flags VT read.

  The extractor does not filter them. A filter would change what the record says VT's world held, and
  the C side handles the one ROM-only key (below).
- **Summary:** the extractor's summary counts the MajorGlitches rows and their VT-versus-test mismatches,
  in the form 3.1 gives the OverworldGlitches count (H-14-10). The prototype found no mismatch.
- `gen_golden.sh` needs no change (H-14-11).

### C replay: one more fixture, one more accepted key

- One path constant names the new fixture. The regions test's `--fixtures` mode replays it with its
  accept-every-row filter, as it replays the other row fixtures (H-14-5):
  - It fails unless every row record is replayed and the file has at least one.
  - It prints its own count line.
- The row replay helper is used as step 3.1 leaves it (H-14-4):
  - the fixture path as a parameter;
  - the row's `logic` applied to the constructor config before world init;
  - every override applied to the world's config after init.

  For these rows, world init therefore derives the twelve flags and the four `rom.vanilla*` keys itself
  before the recorded overrides re-set them.
- **`rom.freeItemMenu`:** the fixture config reader accepts it, checks that its JSON is an integer, and
  ignores it. `RandoConfig` gets no field for it: VT reads it only in `World::writeToRom`, and the C port
  writes no ROM. Any other unknown key still fails the record. The reader's contract comment names the
  ignored key.
  - Rejected: filtering the key in the extractor. That changes what the record says VT's world held.
  - Rejected: a `RandoConfig` field nothing reads.
- The world test's NoGlitches replays (region-locked and vanilla-dungeon) are untouched. The MajorGlitches
  tree has no such method.
- `LOGIC_TESTS`, the Makefile targets and CI are unchanged: `make logic` already runs the regions test
  with `--fixtures`.

### Divergences: one bug, one test, one fix

- None is expected (prototype). If a row diverges in the real implementation, each distinct cause is
  handled as `fix-bug` does:
  - Prove the cause against the VT rule.
  - Add a hand-written test to the regions test that builds the row's world (Standard or Open, logic
    `MajorGlitches`, the row's presets) and items, and asserts VT's answer.
  - Fix the region rule, then commit.
- Several rows sharing one cause make one bug.
- The fixture is never edited or regenerated to get green.

### Hand-written derivation check

- One case is added next to 3.1's OverworldGlitches derivation case in the world test (`make test`,
  H-14-6). A world initialized with `logic` `MajorGlitches` and no wild dungeon items:
  - has these twelve flags set: `can_super_bunny`, `can_fake_flipper`, `can_super_speed`,
    `can_boots_clip`, `can_mirror_clip`, `can_water_walk`, `can_mirror_wrap`, `can_dungeon_revive`,
    `can_owyba`, `can_one_frame_clip_ow`, `can_one_frame_clip_uw`, `can_transition_wrapped`;
  - has `can_bunny_surf` and `can_bunny_revive` clear;
  - has all four `rom_vanilla_*` set (VT's `$glitched_logic` includes MajorGlitches).
- This pins VT's `config/logic.php` MajorGlitches block and the `rom.vanilla*` defaults. `make logic`
  cannot catch a missing derived flag: the recorded overrides mask it (prototype). Only e2e would, and
  only through the one seed.
- `canSuperSpeed` and `canBootsClip` are pinned by this check only. No MajorGlitches row's answer depends
  on them (prototype).

### Claims, gate and library

- No claim changes. `glitches-major_glitches` stays at `all` (H-14-9) and the claims file is untouched.
- No e2e change: no generator run or check is added.
- The library is not expected to change (H-14-8).
  - If no bug fix touches the library, `make measure` is not rerun, and the note says the library is
    unchanged.
  - If one does, `make measure` is run. Its maximum host time per case, code+rodata (host and ARM proxy)
    and stack peak are recorded against the previous step's figures.

## Implementation Chunks

1. **Oracle: the MajorGlitches fixture (one commit, oracle only).**
   - Add the tree table entry, the header and the summary count.
   - Run `gen_golden.sh --out <tmp>`. Check that these are byte-identical to the committed files:
     - every case's `spoiler.json`, `trace.txt` and `attempts`;
     - every existing fixture (`world_dump.txt`, `predicate_vectors.txt`, `noglitches_rows.txt`,
       `overworld_glitches_rows.txt`, `pool_lists.txt`, `prepared_pools.txt`).
   - Then write the new fixture into the oracle directory.
   - Expect 1 923 rows from 40 methods in 23 files, with 0 VT-versus-test mismatches. A different count
     is investigated, not accepted: VT is pinned.
   - Commit only the extractor and the new fixture. `make test`, `logic` and `e2e` are unchanged by
     construction, since nothing reads the file yet.
2. **C: derivation check, accepted key, replay.**
   - The MajorGlitches derivation case comes first. It goes green at once, since the library already
     derives the flags.
   - Then `rom.freeItemMenu` in the fixture config reader, the path constant, and the regions test
     replaying the new fixture.
   - Gate: `make test`, `make logic` (now also the 1 923 rows) and `make e2e` green, with
     `glitches-major_glitches` still passing at `all`.
3. **Divergence fixes, only if chunk 2's `make logic` reports a row.**
   - One `fix-bug` cycle per distinct cause: test in the regions test, rule fix, commit.
   - Repeat until `make logic` is green.
   - With a library change, run `make measure`.
4. **Step note `### Step 3.3`** in the plan, and the row ticked (see Further Notes).

## Testing Decisions

- **What makes a good test here:** it asks the C port the same question VT's test asked (`canAccess`,
  `fill`, `canEnter`, `canComplete` on a world built from the record) and compares the answer with VT's
  recorded answer. It never looks at how a rule is written.
- **The 1 923 MajorGlitches rows** are replayed by the regions test's `--fixtures` mode under
  `make logic`. Prior art: step 2.2's NoGlitches row replay and step 3.1's OverworldGlitches replay, both
  through the test-support row replay helper.
- **`make test` gains exactly one hand-written case** (the MajorGlitches derivation check). It also gains
  one hand-written regions case per divergence fixed in chunk 3. No fixture is read by `make test`.
- **The accepted key** is tested through `make logic`: without it, every MajorGlitches row fails on an
  unknown key. No separate test.
- **Mutation checks during implementation** (not committed, recorded in the note):
  - Clearing `can_one_frame_clip_ow` (the flag that flips the most rows) in world init must fail
    `make test` (derivation case) and `make e2e`. It is expected to pass `make logic`, as the prototype
    showed. The note records that masking.
  - Forcing `can_one_frame_clip_ow` off after the overrides in the replay must fail `make logic`. The
    prototype flipped 778 rows.
  - Dropping the `logic`-before-init step must still pass `make logic` (prototype).
- **Not pinned by any MajorGlitches row:** `canSuperSpeed` (also unpinned by OverworldGlitches) and
  `canBootsClip`. The derivation case pins both flags. This step adds no hand-written test for their rule
  sites: the expected values would not come from VT. The `all` claims keep covering them.
- **Timing:**
  - `make test` stays at the previous step's figure (H-14-9).
  - `make logic` grows by about 25 ms (prototype: `test_regions --fixtures` went from 30 to 58 ms).
  - e2e stays at about 0.17 s, under 0.5 s, unchanged in shape.

## Out of Scope

- `tests/InvertedMajorGlitches`, `tests/Inverted` and `tests/InvertedOverworldGlitches` (row 3.12: no
  Inverted world yet).
- `glitches-no_logic`'s fill divergence (row 3.4).
- A HybridMajorGlitches derivation check (row 3.2 is closed and owes no test).
- `testDungeonItemWrite` and any ROM writing, including the `rom.vanilla*` item substitution. That
  substitution belongs to roadmap item 2.
- Hand-written tests for glitch rule sites no VT row pins (`canSuperSpeed`, `canBootsClip`).
- Any change to claims, the e2e gate, the generator or the playthrough.
- Step 3.1's work: the tree table, the `logic` constructor key, the path parameter, the OverworldGlitches
  fixture and its derivation case.
- The host-only warn-review items left for the pull request, and `item_placement-basic`'s A-3 overrun.

## Further Notes

### Hypotheses about step 3.1's outcome

This PRD was written while step 3.1 was being implemented. It was written against the committed step 3.6
code (`dfe661d`) and 3.1's PRD. Each assumption about what 3.1 leaves behind is listed here. The
implementer checks each one against 3.1's commits and step note before chunk 1. The step note records a
verdict for each.

- **H-14-1 — the extractor has a tree table.** It maps each VT test tree under `tests/` to its row
  fixture. The source list and the row dump take the tree as a parameter. `TOP_LEVEL_METHODS` is appended
  for the NoGlitches tree only. Adding MajorGlitches is one table entry.
  - If 3.1 landed another shape, chunk 1 adds the tree in whatever form 3.1's table takes.
  - If it landed none, chunk 1 builds it as 3.1's PRD describes.
- **H-14-2 — `logic` is a constructor key in the extractor's replay.** The replay passes a `logic`
  override to `World::factory` and applies the rest with `testSetConfig`. The prototype ran this rule.
  - Not checked: whether the old rule (`testSetConfig('logic')` after a NoGlitches factory) also replays
    the MajorGlitches rows. The records carry `rom.vanilla*` and `rom.freeItemMenu`, so it very likely
    does.
  - If 3.1 kept the old rule, chunk 1 checks that every MajorGlitches row still replays, and fails
    otherwise.
- **H-14-3 — the fixture header.** Each row fixture other than NoGlitches has a header copied from the
  NoGlitches row header, which names its tree and says how a `logic` override is applied. The
  MajorGlitches header follows it and adds the `rom.freeItemMenu` line.
- **H-14-4 — the C row replay helper.** It takes the fixture path as its second parameter. For every
  query except `vanilla_dungeon_items`, it applies only the row's `logic` to the constructor config before
  world init, through `apply_overrides`. It applies all overrides after init. `vanilla_dungeon_items`
  keeps its 3.6 path.
  - The prototype passed every row with and without the before-init step, so a different 3.1 mechanism
    changes no expected result.
- **H-14-5 — the regions test's `--fixtures` mode.** It replays each row fixture with its
  accept-every-row filter. It fails unless every record of a file is replayed and each file has at least
  one, and prints one count line per fixture. The MajorGlitches fixture is one more call or one more
  entry in that list.
- **H-14-6 — the derivation case.** 3.1 adds an OverworldGlitches derivation case to the world test's
  derivation checks. The MajorGlitches case sits next to it, in the same shape.
  - If 3.1 wrote it as a table of logic values and expected flags, MajorGlitches is one more entry.
- **H-14-7 — the fixture config reader.** 3.1 does not add `rom.freeItemMenu` to it: OverworldGlitches
  is not glitched logic in VT's constructor, so its rows don't carry the key. This step adds it.
  - If 3.1 already added it, this step adds nothing there.
- **H-14-8 — no library change at 3.1.** 3.1's PRD expects no rule fix. With no library change, the C
  port still matches all 1 923 MajorGlitches rows, as the prototype showed on `dfe661d`.
  - If 3.1 fixed a rule, the shared OverworldGlitches branches may have changed. Chunk 2's `make logic`
  is then the check, and chunk 3 handles any divergence.
- **H-14-9 — the baseline figures.** `glitches-major_glitches` is still claimed at `all`. Claims: 41 at
  `all`, `glitches-no_logic` at `prepareWorld`; 42 pass, 2 pending. e2e is about 0.17 s. `make test` is
  at 3.1's figure (3.6: 65–82 ms). `make logic` is at 3.1's figure. The library figures are 3.6's: ARM
  code+rodata 80.4 KB, stack peak 6 328 B, maximum host case 19.8 ms.
- **H-14-10 — the extractor's summary.** It reports a row and mismatch count per row tree, and the
  MajorGlitches count joins it in the same form.
- **H-14-11 — `gen_golden.sh` needs no change.** It runs the extractor into the oracle directory or
  `--out`, and writes whatever row files the tree table names.
- **H-14-12 — 3.1's note leaves `canSuperSpeed` to this row.** It records `canSuperSpeed` as unpinned
  and leaves it for the MajorGlitches rows. This step finds it is still unpinned (0 rows), and so is
  `canBootsClip`, and records both. The decision is to add no hand-written rule-site test (see Testing
  Decisions).

### Step note (`### Step 3.3`) must record

- The fixture: 1 923 rows from 40 methods in 23 files, written and replayed by VT, 0 mismatches against
  the tests. Goldens and existing fixtures byte-identical.
- `testDungeonItemWrite` (8 rows) not ported, because it is ROM-only. In step 3.0's owed table, row 3.3
  is closed as "40 of 41 methods; `testDungeonItemWrite` ROM-only".
- `rom.freeItemMenu` accepted and ignored by the fixture reader, and why.
- The divergences found and fixed (expected: none), each with its test.
- The per-flag sensitivity table, with `canSuperSpeed` and `canBootsClip` as unpinned. Also the masking
  finding: a missing derived flag passes `make logic` and is caught only by the derivation case and e2e.
- For roadmap item 2: MajorGlitches (with HybridMajorGlitches and NoLogic) sets `rom.vanilla*`, so VT
  writes generic keys, big keys, maps and compasses into the ROM in place of the dungeon-specific ones.
  The item given in game then differs from the item placed.
- The verdict on each H-14 hypothesis.
- `make test`, `make logic` and e2e times against 3.1's figures. `make measure` figures only if the library
  changed.
- Row 3.3 closed in step 3.0's owed table, and the step ticked.
