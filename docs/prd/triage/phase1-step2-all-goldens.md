---
title: "Every golden committed: the 44 matrix cases and the retry seed"
description: "Only the baseline golden exists, so no later step can claim any option case or the retry loop against VT."
status: needs-triage
---

## Problem Statement

Phase 1 step 1 leaves the VT oracle working and proven deterministic on one case only, the baseline. Every later step of the randomizer plan is checked against goldens. Phase 2 step 4 claims "the retry-seed cases", and each Phase 3 step claims the cases of one option row. None of those cases exist yet. Without them a C port step has nothing to reproduce. The retry contract (`attempts` recorded, the C port must succeed on the same attempt) has never been exercised on a case where the first attempt fails. And nobody knows whether VT generates every matrix configuration deterministically, or generates it at all.

## Solution

Extend the oracle's case list to the full test matrix of the plan: the baseline plus one case for each non-default value of each option, 44 cases in all, every one on seed 1. Add one retry case: the baseline config on the first seed above 1 whose generation needs at least one failed attempt. Generate every golden with the step 1 oracle, generate the whole set a second time in a fresh run, and confirm the two runs are byte-identical. Then commit the goldens on their own. After this step `tests/rando/cases/` holds 45 cases (44 matrix + 1 retry). `make -C tests/rando e2e` stays green, with every case reported as pending.

## Hypotheses

Step 1 is not finished while this PRD is written. Everything below is assumed about its outcome. Each one must be checked against step 1's note and committed code before this PRD moves to `docs/prd/triage/`.

- **H-1-1** — Step 1 commits the oracle as the plan's Layout describes: `tests/rando/oracle/vt.patch`, `tests/rando/oracle/gen_golden.sh` and a PHP retry wrapper, all under `tests/rando/oracle/`. `gen_golden.sh` clones VT at the pinned commit `219fcafd…` into a git-ignored scratch location (not vendored), applies `vt.patch`, runs `composer install`, runs the retry wrapper for each case of the case list, and writes `tests/rando/cases/<name>/`.
- **H-1-2** — The case list has exactly one committed source that `gen_golden.sh` reads. It is either a manifest or table inside the oracle, or the committed input files (`config.json`, `seed`) of each `cases/<name>/`. In either form, adding a case means adding one entry: name, config, seed. Step 1 leaves exactly one entry in it, the baseline.
- **H-1-3** — Step 1 names the baseline case `baseline`. It is the VT `default` preset (`config/alttp.php`) with seed `1`. Its golden is committed, with all five files: `config.json`, `seed`, `spoiler.json`, `trace.txt`, `attempts`.
- **H-1-4** — `config.json` holds VT's own option names and values, exactly as the plan's matrix table spells them (`glitches`, `item_placement`, `dungeon_items`, `accessibility`, `goal`, `crystals_tower`, `crystals_ganon`, `state`, `weapons`, `item_pool`, `item_functionality`). Setting one option to a non-default value is a one-field change to the baseline's config.
- **H-1-5** — The `attempts` file holds the attempt index that succeeded, as the plan's retry pseudocode counts it. `0` means the first attempt succeeded, and `attempts > 0` means at least one attempt failed. If step 1 counts 1-based total attempts instead, read this PRD's "`attempts` > 0" as "at least one failed attempt", which is `attempts >= 2`.
- **H-1-6** — The retry wrapper has the watchdog the plan requires. A case that reaches it ends with a non-zero exit and a message that names the case. It never hangs. A VT exception thrown on every attempt shows up the same way, or as a distinct error that names the case.
- **H-1-7** — `gen_golden.sh` can be pointed at one case or a subset of cases, or at least at a single (config, seed) pair, without rewriting the other goldens. The retry-seed scan depends on this. If it can only regenerate the whole list, the scan runs the retry wrapper directly inside the scratch VT clone. Either way, it never writes into `tests/rando/cases/`.
- **H-1-8** — "Fresh run" means what step 1 meant for the baseline: a new VT clone and a new `composer install` in an empty scratch location, with no state carried over from the previous run. Step 1's note records how the baseline was generated twice and compared. This step repeats that procedure on every case.
- **H-1-9** — `make -C tests/rando e2e` reports a case that no finished step has claimed as pending. Doing so costs no C-port generation and no PHP, and the target exits 0. Adding 44 unclaimed cases therefore leaves `e2e` green and costs only the time to list the cases. `make -C tests/rando test` never reads `cases/`.
- **H-1-10** — Step 1's note records the PHP and composer versions next to the pinned VT commit. This step uses the same versions. Regenerating the baseline in this step reproduces step 1's committed golden byte for byte.
- **H-1-11** — `gen_golden.sh` writes `spoiler.json` already normalized as the plan's Comparison rule says: keys sorted, and hash, date and build dropped. It writes `trace.txt` one line per draw, in the plan's `attempt draw# stage min max -> r` format. So two byte-identical runs mean identical files, with no post-processing.

## User Stories

1. As a C-port implementer in Phase 3, I want a committed golden for every non-default value of every option, so that my step has a case to claim and reproduce.
2. As a C-port implementer in Phase 2 step 4, I want at least one committed case whose `attempts` is above zero, so that the retry loop is checked against VT on a real failure and not only on first-attempt successes.
3. As the plan pilot, I want every case to come from the patched VT oracle and nothing else, so that the goldens remain the source of truth that CLAUDE.md requires.
4. As the plan pilot, I want every golden generated twice in fresh runs and found byte-identical, so that a later divergence is always a C-port bug and never oracle noise.
5. As the plan pilot, I want the goldens in a commit of their own, separate from any tooling or case-list change, so that the history shows exactly when VT output entered the repository.
6. As a reviewer, I want the case names to give the option and value they exercise (`glitches-overworld_glitches`, `crystals_tower-random`, …), so that a `DIVERGENT` line tells me which option is at fault without opening `config.json`.
7. As a reviewer, I want every matrix case on seed 1, like the baseline, so that each case differs from the baseline in exactly one option and nothing else.
8. As a reviewer, I want the retry case named after its seed (`retry-seed<N>`), so that I can tell it apart from the matrix cases and know which seed it uses.
9. As a reviewer, I want the retry seed to be the first seed after 1 that needs a retry, scanned in ascending order, so that the choice can be reproduced and is not cherry-picked.
10. As a developer running the fast loop, I want `make -C tests/rando test` unchanged by this step, so that the dev loop stays seconds long.
11. As a developer, I want `make -C tests/rando e2e` to stay green, reporting all 45 cases as pending, so that `check-e2e` never sends an unclaimed case into `fix-bug`.
12. As a developer, I want `e2e` no slower in any way that matters after the cases are added, so that the CLAUDE.md rule on fast loops holds.
13. As the plan pilot, I want a case VT cannot generate (watchdog reached, or the config rejected on every attempt) recorded in the step note and handed to `guideline`, not dropped or reseeded quietly, so that the golden set never weakens without a recorded decision.
14. As a Phase 4 implementer, I want every case's `attempts` and `trace.txt` committed, so that I can measure attempts and RNG draws per case from the goldens alone.
15. As a future maintainer, I want the step note to list each case with its `attempts`, and the retry seed together with the seeds scanned before it, so that the retry choice can be audited without rerunning PHP.
16. As a future maintainer, I want the step note to record that the double run was byte-identical, and the PHP and composer versions it ran with, so that a later regeneration can tell whether its environment matches.
17. As a C-port implementer, I want the `crystals_tower-random` and `crystals_ganon-random` cases to keep `random` in their `config.json`, so that the C port has to reproduce VT's own draw of the crystal count.
18. As a C-port implementer, I want each case's `config.json` to differ from the baseline's in exactly the one option it names, so that I can derive the case's config by reading one field.
19. As the plan pilot, I want the baseline golden committed by step 1 left byte-identical, so that this step changes no case that was already there.
20. As a reviewer of the goldens commit, I want it to contain only files under `tests/rando/cases/`, so that I can review the oracle's output apart from any code.
21. As a developer, I want the VT clone and composer artefacts kept out of the repository, so that the commit holds only goldens and the repository stays free of vendored VT.
22. As a CI maintainer, I want running the tests to still need only gcc after this step, so that the CI described in Phase 2 step 1 never needs PHP.

## Implementation Decisions

- **Modules touched.** Only the case list (H-1-2) and the generated `tests/rando/cases/` tree. No C code. No change to `vt.patch`, to the retry wrapper's logic, to the e2e harness or to the Makefile targets. If extending the case list turns out to need a change to `gen_golden.sh` (say, a list format that cannot hold a non-baseline config), keep that change as small as possible and put it in the tooling commit.
- **Matrix cases.** One case per non-default value in the plan's matrix table, every other option at the baseline (`default` preset) value, seed `1`. That is 43 cases plus the existing `baseline`: glitches ×4, item_placement ×1, dungeon_items ×3, accessibility ×2, goal ×6, crystals_tower ×8 (`0`…`6`, `random`), crystals_ganon ×8, state ×3, weapons ×3, item_pool ×3, item_functionality ×2.
- **Case names** (guideline Q5): `<option>-<value>`, using VT's option name and value verbatim, for example `glitches-overworld_glitches`, `goal-triforce-hunt`, `crystals_tower-0`, `crystals_ganon-random`, `item_pool-crowd_control`. The baseline keeps the name `baseline`. Retry cases are named `retry-seed<N>`.
- **Seeds** (guideline Q5): seed `1` for every matrix case.
- **Retry seed** (guideline Q1): exactly one retry case. Its config is the baseline config. Its seed is found by scanning seeds `2, 3, 4, …` in ascending order through the oracle and keeping the first one whose `attempts > 0` (H-1-5). There is no further requirement on how the attempt fails (exception or unwinnable) or on how many attempts fail. The scan never writes into `tests/rando/cases/`: candidates are generated in scratch (H-1-7). Only the chosen seed enters the case list. The step note records the seeds scanned and why each was rejected (`attempts = 0`).
- **Determinism check** (guideline Q2): a one-off check done during this step, with no new committed target. The whole case set is generated twice in fresh runs (H-1-8), and every file of every case is compared byte for byte between the two runs. Any difference stops the step. The comparison result goes into the step note. If step 1 left a re-runnable double-generation mode, use it. Either way, nothing is added to `test` or `e2e`.
- **Ungeneratable case** (guideline Q3): if a matrix case reaches the watchdog, or VT rejects its config on every attempt, keep its seed and keep it in the list. Record the case and the failure in the step note, generate the other cases anyway, and ask `guideline` about that case before the step can finish. Never drop a case, never reseed it, and never change `vt.patch` to make a case pass without that decision.
- **Commits** (guideline Q4): two commits, in this order.
  1. Tooling: the case list extended to 45 entries (and any minimal `gen_golden.sh` change). No generated file.
  2. Goldens only: the output of `gen_golden.sh` for the full list, meaning 44 new case directories. The baseline's directory is byte-identical (H-1-10), so it does not appear in the diff. This is the "own commit" that CLAUDE.md requires for regenerating goldens.
- **Environment.** The PHP and composer versions recorded in step 1's note (H-1-10). If they cannot be reproduced, record the versions actually used in the step note and check that the baseline still comes out byte-identical to its committed golden. If it does not, stop and ask `guideline`.

## Implementation Chunks

1. **Case list.** Add the 43 matrix entries with the names and seed above. Run the retry-seed scan in scratch and add the `retry-seed<N>` entry. Check that every new entry's config differs from the baseline in the one option its name gives (the retry entry differs in none). Commit as tooling (`test:` or `chore:`). Every case without a golden yet must leave `make -C tests/rando test` and `e2e` green. If step 1's harness treats a listed case with no golden as an error, the two chunks are run as one unit: two consecutive commits (tooling, then goldens), with no `e2e` run and no push between them.
2. **Goldens.** Run `gen_golden.sh` on the full list in a fresh run. Run it again in a second fresh run and compare byte for byte. Check that `baseline` is unchanged and that `retry-seed<N>/attempts > 0`. Run `make -C tests/rando test` and `make -C tests/rando e2e` (all 45 pending, exit 0). Commit only `tests/rando/cases/`.

## Testing Decisions

- A good test here checks external, reproducible facts about the goldens. It never checks how the oracle computes them.
- No unit test is added. This step writes no C code, and `make -C tests/rando test` covers C modules only. Checks over the goldens live in the step itself, not in a test target, because neither `test` nor `e2e` may get slower or need PHP (CLAUDE.md, guideline Q2).
- The step's verification, done by the implementer and repeated by the verifier:
  - `tests/rando/cases/` holds exactly 45 directories: `baseline`, the 43 `<option>-<value>` names, and one `retry-seed<N>`. Each one holds the five files `config.json`, `seed`, `spoiler.json`, `trace.txt` and `attempts`.
  - Each matrix `seed` is `1`. Each matrix `config.json` differs from `baseline/config.json` in exactly the field its name gives, with the value its name gives.
  - `retry-seed<N>/attempts` is above 0 (H-1-5). Every seed from 2 to N−1 was scanned and had `attempts = 0`.
  - Two fresh full runs produce byte-identical trees (a recursive diff of the two outputs is empty).
  - The `baseline` directory is unchanged against `HEAD` before the goldens commit.
  - `make -C tests/rando test` and `make -C tests/rando e2e` both exit 0, and `e2e` reports every case as pending (H-1-9).
- Prior art: step 1's double generation of the baseline (step 1's note), which this step repeats on the full set.

## Out of Scope

- Any C code: PRNG, world, fill, spoiler output. Phase 2 and later.
- Claiming any case, or changing how pending or claimed cases are reported. The convergence gate's claim logic is Phase 2 step 1.
- CI (Phase 2 step 1).
- Option interactions (pairs of non-default values). The plan leaves them for a second pass.
- More than one retry case, or retry cases on non-baseline configs.
- Changes to `vt.patch`, to the retry wrapper's contract, or to the trace format, unless `guideline` decides one is needed for an ungeneratable case.
- A committed determinism-check target.
- Expanding Phase 2 step 3 into per-consumer steps. That is the end-of-phase `plan-review` consultation, run by `plan-pilot`, not this step.

## Further Notes

- Decisions taken by `guideline` for this PRD: Q1 one retry case, baseline config, first seed ≥ 2 with `attempts > 0`. Q2 a one-off double-run check, recorded in the step note, with no new target. Q3 an ungeneratable case is kept unreseeded, recorded in the step note and sent to `guideline`. Q4 a tooling commit, then a goldens-only commit. Q5 seed 1 for every matrix case, names `<option>-<value>`, `baseline` and `retry-seed<N>`.
- The step note for this step should list each case with its `attempts`, the retry seeds scanned, the result of the double run, and the PHP and composer versions.
- Expect `trace.txt` files of uneven size. Cases that retry or run long fills (`no_logic`, `item_pool-crowd_control`, `goal-triforce-hunt`) can have much longer traces than the baseline. That is expected, and it is the data Phase 4 step 1 will measure.
