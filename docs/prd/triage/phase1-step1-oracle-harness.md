---
title: "Bare rando harness and a deterministic VT oracle for the baseline case"
description: "There is no test harness and no way to replay VT, so no golden exists and no later step can be verified, committed or checked against VT."
status: needs-triage
---

## Problem Statement

The randomizer plan checks every C-port step against goldens written by the VT generator. None of that exists yet. There is no `tests/rando/` tree. `make -C tests/rando test` and `make -C tests/rando e2e` do not exist, yet every skill in the loop calls them: `prd-autopilot`'s verifier, `git-commit`'s test gate, `check-e2e` and `fix-bug`. Stock VT cannot be replayed either: `get_random_int()` calls PHP's CSPRNG `random_int()`. VT never retries on its own, and it also runs text, credit and hint passes that the C port will skip. Until the harness exists, no step can be verified or committed. Until a deterministic, patched VT writes a first golden, nobody knows whether VT can be made deterministic at all, or in what order it draws from the RNG. Phase 2 step 3 is planned around that order.

## Solution

Two deliveries, in this order.

1. **The bare harness.** `make -C tests/rando test` builds and runs the randomizer's unit tests with host gcc. There are none yet, so it reports zero tests and exits 0. `make -C tests/rando e2e` lists every case under `tests/rando/cases/`. No step has claimed any case and no C port exists, so it reports each case as pending and exits 0. With no cases at all, it says so and exits 0. Neither target needs PHP, devkitPro, the ROM or the network.
2. **The oracle.** Three files go under `tests/rando/oracle/`. `vt.patch` swaps VT's RNG for the shared deterministic PRNG, adds a per-draw trace, and comments out `setTexts()`, `randomizeCredits()` and `HintService`. A PHP retry wrapper runs the plan's retry contract around VT and writes a case's golden. `gen_golden.sh` clones VT at the pinned commit into a git-ignored directory, applies the patch, runs `composer install` and runs the wrapper for each case. The first case is `baseline`: the VT `default` preset with seed `1`. It is generated, checked by hand (winnable, `attempts` recorded), then generated twice more in fresh runs and found byte-identical. The PHP and composer versions go into the plan next to the pinned commit. The step note lists the baseline's RNG consumers in draw order, with a draw count for each.

## User Stories

1. As the `prd-autopilot` verifier, I want `make -C tests/rando test` to exist and exit 0, so that every later step has a fast loop to run.
2. As `git-commit`, I want `make -C tests/rando test` to pass with zero unit tests, so that this step's own commits are not blocked by an empty suite.
3. As `check-e2e`, I want `make -C tests/rando e2e` to exist and report unclaimed cases as pending, not `DIVERGENT`, so that it never sends an unclaimed case into `fix-bug`.
4. As `check-e2e`, I want the e2e run to exit 0 when there is no `DIVERGENT` line, so that I can read green from the exit status alone.
5. As `check-e2e`, I want `tests/rando/run_e2e.sh <case>…` to run only the named cases, so that the reproduction in a bug report targets one case.
6. As a developer, I want `make -C tests/rando test` and `e2e` to run in well under a second today, with no PHP and no network, so that the fast loop stays fast as CLAUDE.md requires.
7. As a developer, I want `make -C tests/rando` with no target to run `test`, so that the CI of Phase 2 step 1 can call it bare.
8. As a later implementer, I want adding a unit test to be one line in the `UNIT_TESTS` list plus a `unit/test_<module>.c` file, so that every step registers its tests the same way.
9. As a developer, I want `e2e` to say clearly when `tests/rando/cases/` is missing or empty, so that "no cases" is never mistaken for "all green".
10. As the oracle maintainer, I want VT's `get_random_int()` to use xoshiro128\*\* seeded through splitmix32, with 32-bit masked arithmetic, so that the C port can reproduce every draw.
11. As the oracle maintainer, I want the PHP PRNG checked against known-answer vectors from the reference algorithm before any generation, so that a PHP overflow-to-float bug is caught instead of frozen into the goldens.
12. As the oracle maintainer, I want `get_random_int()` to throw on `min > max` or on a range wider than 2^32, so that a call site the PRNG contract does not cover fails loudly instead of drawing silently wrong values.
13. As a C-port implementer, I want `setTexts()`, `randomizeCredits()` and `HintService` disabled by the patch, so that the goldens hold no draws the C port will never make.
14. As a C-port implementer, I want every draw traced as `attempt draw# stage min max -> r`, so that a divergence reads as "attempt N, draw #K, in stage S".
15. As a C-port implementer, I want the `stage` label to come from a mechanical rule I can mirror in C, so that the traces of both sides use the same names.
16. As a C-port implementer, I want draw numbers to restart at 1 on each attempt, so that each attempt's trace starts clean, as the plan's retry contract says.
17. As a C-port implementer, I want the wrapper to reseed with `splitmix32(seed + attempt)` and rebuild the world from scratch on every attempt, so that each attempt is independent, exactly as the C port will do it.
18. As a C-port implementer, I want `attempts` to hold the index of the attempt that succeeded (`0` = first try), so that "the C port succeeds on the same attempt" is a direct comparison.
19. As a C-port implementer, I want a VT exception (`Exception`) and a `Game Unwinnable` result both counted as a failed attempt, so that the retry loop matches the plan.
20. As the oracle maintainer, I want a PHP `Error` (a type error, an undefined method) to abort the run, not count as a failed attempt, so that a harness bug never passes for an unlucky seed.
21. As the plan pilot, I want the wrapper's watchdog to stop a case that never succeeds, with a non-zero exit naming the case, so that an unattended run never hangs.
22. As the plan pilot, I want `spoiler.json` written already normalized (keys sorted, `meta.build` dropped, fixed JSON flags), so that two runs compare byte for byte with no post-processing.
23. As the plan pilot, I want the golden to hold only the fields VT produces after a randomize and the winnability check, without `writeToRom()`, so that it holds no ROM-side draws and needs no ROM.
24. As a developer, I want `gen_golden.sh` to need no ROM, no database, no `.env` committed and no web server, so that regenerating the goldens needs only PHP, composer and the network.
25. As a developer, I want the VT clone to live in a git-ignored directory, so that VT is never vendored or committed by accident.
26. As a developer, I want `gen_golden.sh` to reset the clone to the pinned commit and reapply the patch on every run, so that a stale or hand-edited clone can never leak into a golden.
27. As the plan pilot, I want a fresh-run mode that starts from an empty clone directory, so that "generated twice in fresh runs" means no state is carried between runs.
28. As the step 2 implementer, I want `gen_golden.sh` to take a subset of case names and an output directory, so that I can generate into scratch and scan retry seeds without touching `tests/rando/cases/`.
29. As the step 2 implementer, I want a case defined by its committed `config.json` and `seed` alone, so that adding a case means adding one directory with two files.
30. As a reviewer, I want `config.json` to use the plan's matrix option names and values verbatim, so that a case's settings read the same as the plan's table.
31. As a reviewer, I want the baseline golden's five files (`config.json`, `seed`, `spoiler.json`, `trace.txt`, `attempts`) committed, so that the tests need only gcc.
32. As the plan pilot, I want the step note to record the hand check, the double-run result and the versions, so that the determinism claim can be audited without PHP.
33. As the end-of-phase `plan-review`, I want the step note to list the baseline's RNG consumers in draw order with draw counts, so that Phase 2 step 3 can be split into one step per consumer.
34. As a future maintainer, I want the PHP and composer versions recorded next to the pinned VT commit in the plan, so that a later regeneration can tell whether its environment matches.
35. As a future maintainer, I want `gen_golden.sh` to print the PHP and composer versions it ran with, so that a mismatch shows up in its own output.
36. As the plan pilot, I want any spoiler field that turns out non-deterministic (other than hash, date and build) to stop the step and go to `guideline`, so that determinism is never bought by dropping data quietly.

## Implementation Decisions

### Harness

- **`tests/rando/Makefile`**, host gcc only, never devkitPro. Default target `test`. `UNIT_TESTS` is a list of test names, empty for now. Each `unit/test_<module>.c` builds against the `rando/` sources into a binary under a git-ignored build directory, and `test` runs each one and fails on the first non-zero exit. With an empty list, `test` prints that 0 unit tests ran and exits 0. `-std=c99 -Wall -Wextra -Werror`. The rules must not depend on `rando/` existing yet.
- **`e2e`** runs `run_e2e.sh`. Contract, which later steps extend without changing it:
  - Arguments are case names. With none, every directory under `tests/rando/cases/` that has a `config.json`, in sorted order.
  - Output is one line per case: `PENDING <case>` today. Later steps add `OK <case>` and `DIVERGENT <case>` (the latter followed by the first differing location), then a summary line `e2e: <ok> ok, <divergent> divergent, <pending> pending`.
  - Exit 0 when no case is `DIVERGENT`. A named case that does not exist is a usage error (exit 2).
  - With no cases: print `e2e: no cases` and exit 0.
  - It reads only `tests/rando/cases/`. It never calls PHP or `gen_golden.sh`.
  - The claim mechanism (which case a finished step claims, up to which stage) is Phase 2 step 1. Until then every case is pending. No claim file is invented here.
- **`.gitignore`** gains the test build directory and the VT clone directory.

### Shared PRNG (in `vt.patch`, `app/Helpers/number.php`)

The exact definition, which Phase 2 step 1 will mirror in C. Every operation is mod 2^32:

- `splitmix32(x)`: `z = x + 0x9E3779B9; z = (z ^ (z >> 16)) * 0x85EBCA6B; z = (z ^ (z >> 13)) * 0xC2B2AE35; return z ^ (z >> 16)`.
- `prng_seed(v)`: `s[k] = splitmix32(v + k * 0x9E3779B9)` for `k = 0..3`. An all-zero state is an error.
- `next()` (xoshiro128\*\*): `r = rotl(s1 * 5, 7) * 9; t = s1 << 9; s2 ^= s0; s3 ^= s1; s1 ^= s2; s0 ^= s3; s2 ^= t; s3 = rotl(s3, 11); return r`.
- `get_random_int(min, max)`: throws unless `min <= max` and `max - min + 1 <= 2^32`. Returns `min + next() % (max - min + 1)`.
- **PHP trap:** PHP integers are signed 64-bit. `z * 0x85EBCA6B` and `z * 0xC2B2AE35` can exceed `PHP_INT_MAX` and silently become floats. Those two products must be computed in 16-bit halves (or another overflow-free form) and masked. The `* 5` and `* 9` products fit and only need masking. `>>` operates on values already masked to 32 bits, so it is a logical shift.
- **Known-answer vectors.** A small table committed under the oracle: for a few seeds, `splitmix32` outputs, the first 8 `next()` outputs after `prng_seed`, and a few `get_random_int` results over small ranges. The vectors are computed once from the reference algorithm (Vigna's xoshiro128\*\* and the splitmix32 above) by a throwaway host-gcc program, never by the PHP code under test. `gen_golden.sh` runs the PHP PRNG against the table before any generation and refuses to continue on a mismatch. Phase 2 step 1 can reuse the table for the C PRNG.

### `vt.patch`

- A `git diff` against the pinned commit `219fcafd029dab597b8db400efafd8f56f8b4edb`, applied with `git apply` (a failure to apply is fatal).
- `get_random_int()` uses the PRNG above. The patch adds three small global functions the wrapper calls: one to seed, one to start an attempt's trace (it sets the attempt index and resets the draw counter to 1), and one to read the trace lines. Their exact names are the implementer's choice.
- **Trace line:** `<attempt> <draw#> <stage> <min> <max> -> <r>`, decimal, one space between fields, attempts from 0, draws from 1 within each attempt.
- **Stage rule** (mechanical, so C can mirror it with hard-coded labels): walk the backtrace (`debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)`) from the outermost frame. The stage is the bare function name of the frame called directly by `Randomizer::prepareWorld` or `Randomizer::randomize`, for example `setShops`, `setMedallions`, `placeBosses`, `fillPrizes`, `setFountains`, `shufflePrizePacks`, `getAdvancementItems` or `fill`. A draw made directly in `prepareWorld`'s own body (for example its closing `fy_shuffle` calls) gets `prepareWorld`. A draw made outside `randomize()` gets the label the wrapper set for it. The only such draw is the `random` crystal count, labelled `crystals`. Labels contain no spaces.
- `setTexts()`, `randomizeCredits()` and the `HintService` call in `Randomizer::randomize()` are commented out, nothing else in that method. They run after the fill.
- Nothing else in VT changes. In particular, `writeToRom()` and its post-fill draws (digging game, …) are left alone. The wrapper simply never calls them.

### Retry wrapper (PHP, in our repo under `tests/rando/oracle/`)

- Invoked by `gen_golden.sh` as one PHP process per case: VT clone path, case input directory, output directory.
- Boots VT's Laravel console kernel from the clone (`vendor/autoload.php`, `bootstrap/app.php`), with `memory_limit` raised as VT's own CLI does. It needs no database, ROM, `.env` file or network. Any environment it needs (for example a log channel that writes nothing) is set by the wrapper or `gen_golden.sh`, never committed into the clone.
- **`config.json`** keys are the matrix option names: `glitches`, `item_placement`, `dungeon_items`, `accessibility`, `goal`, `crystals_tower`, `crystals_ganon`, `state`, `weapons`, `item_pool`, `item_functionality`. Values are strings, as VT spells them. All 11 keys are required, and an unknown key is an error. `seed` is a decimal `uint32` on one line.
- **Mapping to VT** mirrors the alttpr.com path, `RandomizerController::prepSeed`, minus the ROM, the name and notes, entrance shuffle and the enemizer. `glitches` goes to `logic` (`none` → `NoGlitches`, …). `state` is `World::factory`'s type. The `crowd_control` item pool forces `item_placement = advanced`. Fixed values: `entrances = none`, all enemizer options off or default, `tournament = false`, `spoil.Hints = on` (the `default` preset), `spoilers`, `allow_quickswap` and `pseudoboots` left at VT's defaults. Spoiler meta is passed as `prepSeed` does: `entry_crystals_ganon`, `entry_crystals_tower` and `worlds = 1`.
- **Retry loop**, exactly the plan's contract. For `attempt = 0, 1, …`:
  1. Clear VT's static caches (`Item::clearCache()`, `Boss::clearCache()`).
  2. Reset `World`'s protected static world counter to 1, so `meta.world_id` and the per-world caches do not depend on the attempt number.
  3. Start the trace for this attempt, then `prng_seed(splitmix32((seed + attempt) mod 2^32))`.
  4. Resolve `random` crystal counts with `get_random_int(0, 7)` (stage `crystals`), tower first and then ganon, the order `prepSeed` uses.
  5. Build the world, `new Randomizer([$world])`, `randomize()`, then `WorldCollection::isWinnable()`.
  6. Success → stop. An `\Exception` from VT, or not winnable → next attempt. Any `\Error` → abort with a non-zero exit naming the case.
- **Watchdog:** 1000 attempts. Reaching it aborts with a non-zero exit naming the case and writes nothing for that case. It bounds the oracle only, never the generator's contract.
- **Outputs**, all written only on success, into the output directory:
  - `spoiler.json`: `World::getSpoiler()`, normalized. Associative arrays are sorted by key recursively. Lists (`array_is_list`) keep their order. `meta.build` is dropped. Then `json_encode` with `JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE`, plus a trailing newline. No other field is dropped. A field found non-deterministic goes to the step note and `guideline` (story 36).
  - `trace.txt`: the trace of every attempt, failed ones included, in order.
  - `attempts`: the index of the successful attempt, decimal, newline.
  - `config.json` and `seed` are copied unchanged when the output directory differs from the input.

### `gen_golden.sh`

- Usage: `gen_golden.sh [--fresh] [--out <dir>] [<case>…]`. With no cases, every case directory under `tests/rando/cases/`. Without `--out`, it writes back into each case's own directory. With `--out`, it writes `<dir>/<case>/` and never touches `tests/rando/cases/`.
- Clone directory: `tests/rando/oracle/vt/` (git-ignored), overridable with `VT_DIR`. `--fresh` deletes it first. Each run: clone or fetch the pinned commit if needed, `git reset --hard` to it, `git clean -fd` (keeping VT's own ignored `vendor/` unless `--fresh`), `git apply vt.patch`, then `composer install` from VT's `composer.lock` (`--no-dev --no-interaction`, `COMPOSER_ALLOW_SUPERUSER=1`).
- `--ignore-platform-req=ext-bcmath`: this container's PHP 8.3 has no bcmath. VT's `composer.json` requires it, but VT's own code never calls it. Only libraries that have non-bcmath fallbacks (`brick/math`, `phpseclib`, HTMLPurifier's unit converter) reference it. This was checked by grep while writing the PRD. The determinism check confirms the choice in practice.
- Prints the PHP, composer and VT commit it runs with. If PHP's major.minor differs from the version recorded in the plan, it warns but does not refuse.
- Runs the PRNG known-answer check, then the wrapper once per case, each in its own PHP process. Stops at the first failing case with a non-zero exit.
- It is never called by `make test` or `make e2e`, and it adds no Makefile target. Regenerating goldens is a deliberate act (CLAUDE.md).

### Baseline case and records

- `tests/rando/cases/baseline/config.json` is the `default` preset in matrix terms: `glitches none`, `item_placement advanced`, `dungeon_items standard`, `accessibility items`, `goal ganon`, `crystals_tower 7`, `crystals_ganon 7`, `state open`, `weapons randomized`, `item_pool normal`, `item_functionality normal`. `seed` is `1`.
- **Hand check**, recorded in the step note: the spoiler has an item in every non-event location, Ganon holds the Triforce, the playthrough reaches the end, and `attempts` exists and is written down.
- **Determinism:** two further `gen_golden.sh --fresh --out <scratch-N> baseline` runs. `diff -r` of the two scratch outputs and of each against the committed golden is empty.
- **Plan updates** (`docs/randomizer.md`, which the plan allows a step to write): in § 2 Reference, next to the pinned commit, the PHP and composer versions (expected 8.3.6 and 2.8.12) and the bcmath note. In § 6 Step notes, a Phase 1 step 1 note with: the hand check, `attempts`, the double-run result, and the baseline's RNG consumers in draw order. For that last item, the successful attempt's trace is read as consecutive runs of the same stage, each with its draw count, plus failed attempts summarized the same way if any.
- **Phase 2 step 3 follow-up:** if the consumer list shows a stage not in the plan's list (for example `getAdvancementItems` or `prepareWorld`'s own shuffles), the step note says so. `plan-review` folds it in at the end of the phase.

## Implementation Chunks

1. **Bare harness.** `tests/rando/Makefile` (`test`, `e2e`, empty `UNIT_TESTS`), `run_e2e.sh` with the pending-only contract, `.gitignore` entries. Verified by running both targets with no cases (`e2e: no cases`, exit 0) and with a throwaway case directory holding only a `config.json` (`PENDING <case>`, exit 0, removed afterwards). Commit as `test:`.
2. **Patched VT and its PRNG.** `vt.patch` (PRNG, trace hooks, stage rule, texts, credits and hints commented out), the known-answer vector table and its check, and `gen_golden.sh`'s clone, patch, install and self-test path, with no case generation yet. Verified by a `--fresh` run that applies the patch cleanly and passes the known-answer check.
3. **Retry wrapper, baseline golden, determinism.** The wrapper, `gen_golden.sh`'s per-case generation, the `baseline` case (its `config.json` and `seed` first, then the generated golden), the hand check, the two fresh runs compared byte for byte, and the plan updates (versions in § 2, step note in § 6). `make -C tests/rando e2e` then prints `PENDING baseline` and exits 0. The tooling and the baseline golden go in separate commits: generating a golden is its own commit (CLAUDE.md).

## Testing Decisions

- A good test checks behaviour visible from outside: a target's exit status and output lines, the bytes of a generated file. It never checks how a script works inside.
- No C unit test: this step writes no C. `UNIT_TESTS` stays empty, and `make -C tests/rando test` is checked to pass with zero tests.
- **Harness:** checked by running the two targets on no cases and on one pending case (chunk 1). The output line formats are the contract later steps build on, so they are checked literally.
- **PHP PRNG:** checked by the known-answer table, whose vectors come from the reference algorithm compiled with host gcc, not from the PHP. The check is wired into `gen_golden.sh`, so it runs on every regeneration. It is not part of `make test`: that would need PHP.
- **Oracle determinism:** checked by the two `--fresh` runs and `diff -r` (chunk 3). This is a one-off procedure recorded in the step note, not a committed target. Step 2 repeats it on every case.
- **Hand check of the baseline:** recorded in the step note (chunk 3).
- `make -C tests/rando test` and `e2e` must stay well under a second. Nothing in this step may add PHP or network use to them.
- Prior art: none in the repository. `tests/` does not exist yet. The `fix-bug` and `check-e2e` skills fix the expected command names and the `DIVERGENT <case>` line.

## Out of Scope

- Any C code: PRNG, `fy_shuffle`, world, fill, spoiler output (Phase 2).
- The claim mechanism and the stage-prefix check of the convergence gate (Phase 2 step 1). This step only reports every case as pending.
- CI (`.github/workflows/rando-tests.yml`, Phase 2 step 1).
- Every case other than `baseline`, including the retry seeds (Phase 1 step 2).
- Entrance shuffle, the enemizer, hints, texts, credits and ROM writing.
- Expanding Phase 2 step 3 into numbered steps: the end-of-phase `plan-review` does that, from this step's note.

## Further Notes

- **Settled from the code, no `guideline` call needed.** Each point was read in VT at the pinned commit while writing this PRD.
  - `World::factory` increments a static `$max_world` on every call. Without the reset in the retry loop, `meta.world_id` and the per-world `Item` cache would depend on the attempt count.
  - `World::writeToRom()` draws from the RNG (digging game, lines ~1237-1240) after the fill, and needs a ROM. The web controller calls it before `isWinnable()`, but it changes no location, so the wrapper skips it.
  - `random` crystal counts are drawn in the controller before `World::factory`. They go inside each attempt so that every attempt starts from a fresh seed.
  - `PlaythroughService` and `getSpoiler()` make no draws.
- **Environment.** `composer install` without `--ignore-platform-req=ext-bcmath` fails in this container (checked). Running VT's PHP code was refused by this session's auto-mode permission classifier while the PRD was being written, so nothing here has been run end to end. The implementing session may need the user's approval to run PHP on the VT clone. If it is refused, stop and report instead of working around it.
- `tests/rando/oracle/vt/` is the clone and stays git-ignored. Only `vt.patch`, the wrapper, the vectors and `gen_golden.sh` are committed.
- The step 2 PRD (`docs/prd/next/phase1-step2-all-goldens.md`) relies on this step's choices. This PRD settles its hypotheses: H-1-2 (a case is its own `config.json` + `seed`), H-1-5 (`attempts` = 0-based index of the successful attempt), H-1-7 (`--out` and case subsets) and H-1-8 (`--fresh`).
