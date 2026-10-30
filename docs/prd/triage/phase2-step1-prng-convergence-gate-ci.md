---
title: "Rando C PRNG, fy_shuffle, convergence-gate claims and CI"
description: "The C port has no random source shared with VT, the e2e gate cannot check a claim or the spoiler entries a claimed stage determines, the two harness scripts discover cases separately, and nothing runs the rando tests on GitHub."
status: needs-triage
---

## Problem Statement

Phase 1 left a deterministic VT oracle and 44 committed goldens (the baseline plus 43 matrix cases, every one succeeding on attempt 0). VT's randomness all goes through one PRNG (xoshiro128\*\* seeded through splitmix32, `get_random_int(min, max) = min + next() % (max - min + 1)`). The C port has no PRNG yet, so it cannot make a single draw that matches VT. Every later Phase 2 step needs one, along with VT's `fy_shuffle`.

The convergence gate (plan § 2, Convergence gate) is also only on paper:

- `run_e2e.sh` prints `PENDING` for every case.
- Nothing records which cases a finished step has claimed.
- Nothing checks that a claimed case's C trace matches VT's up to the claimed stage's last draw.
- Nothing checks that the spoiler entries those stages determine match the golden. For example, a correct trace with the medallions assigned to the wrong dungeon would pass. The plan now requires this check (Phase 2 step 1), and step 3's sub-steps name the entries:
  - medallions and `Shops`;
  - dungeon prizes and `Bosses`;
  - the two fountain bottles;
  - the item pool as a multiset.

Phase 1's `warn-review` also left two warnings on files this step rewrites:

- `run_e2e.sh` and `gen_golden.sh` each carry their own copy of case discovery.
- The header of `prng_vectors.txt` names a generator ("a host-gcc build of Vigna's xoshiro128\*\*") that is not in the repository, and does not say what actually verifies the vectors.

Finally, no CI runs `make -C tests/rando test` and `e2e`. A push can break the PRNG or the gate without anyone noticing.

## Solution

1. **C PRNG.** A module in the `rando/` library that is bit-identical to the patched VT. It provides `splitmix32`, seeding, `next`, `get_random_int` and `fy_shuffle`, plus an optional per-draw observer so the future generator can write `trace.txt`.
   - Its unit test checks every known answer of the oracle's vector file, which VT reproduces (`prng_check.php`).
   - It also replays every draw of every committed golden trace: about 37 000 draws VT really made, across all 44 cases.
   - VT-checked `fy_shuffle` vectors are added to the vector file.
2. **Claims.** A committed **claims file** lists the claimed cases, one per line, each with either a stage name or `all`.
3. **Stage table.** A committed **stage table** lists VT's stages in draw order and the spoiler checks each stage owes. There are three kinds of check:
   - a whole top-level spoiler section;
   - the lines of every section whose key matches a pattern;
   - the remaining item pool as a multiset against the golden's placed items.

   Each Phase 2 step 3 sub-step adds the checks it names.
4. **`run_e2e.sh`** reads both files:
   - An unclaimed case is `PENDING`.
   - A stage claim compares the C trace with the golden up to that stage's last draw. It then runs every check owed by that stage and the stages before it, against the **partial spoiler** (and pool) the C generator writes when it stops.
   - An `all` claim compares the full trace, `attempts` and `spoiler.json`.
   - Every failed claim prints exactly one `DIVERGENT <case>` line, then indented detail.
   - The generator's command-line contract is fixed now. The generator itself lands in Phase 2 step 3.1.
5. **Shared case discovery and vector header.** One shared POSIX `sh` case-discovery function, sourced by both `run_e2e.sh` and `gen_golden.sh`. The vector file's header is rewritten to name what verifies it: VT through `prng_check.php`, and C through `test_prng`.
6. **No case is claimed in this step.** The claims file is committed with only its format comment, and the stage table with only the stage order and no checks. A claim needs a C trace and a partial spoiler, and no C generator exists before step 2.3.1. Every case stays `PENDING`, and `make e2e` stays green and as fast as now. A fast self-test proves the claim and check machinery on throwaway fixture cases with a fake generator.
7. **CI.** A GitHub Actions workflow runs `make -C tests/rando test` and `make -C tests/rando e2e` on Ubuntu, with gcc only and no PHP.

## User Stories

1. As a C port developer, I want a C `splitmix32` that returns exactly what VT's `prng_splitmix32` returns, so that seeds derived on both sides agree.
2. As a C port developer, I want `prng_seed(v)` in C to fill the four xoshiro128\*\* state words exactly as VT does (`splitmix32(v + k * 0x9E3779B9)`), so that both PRNGs start from the same state.
3. As a C port developer, I want `next()` in C to return VT's 32-bit outputs, so that every draw can match.
4. As a C port developer, I want `get_random_int(min, max)` in C to compute `min + next() % (max - min + 1)` over VT's full contract (span up to 2^32, negative bounds), so that every VT call site can be ported as is.
5. As a C port developer, I want `get_random_int(k, k)` to still consume one `next()`, as VT's does, so that degenerate ranges do not shift later draws.
6. As a C port developer, I want a contract violation (`min > max`, a span over 2^32, a never-seeded or all-zero state) to stop with an `assert` and a message, so that a porting mistake fails loudly instead of drifting silently.
7. As a C port developer, I want `fy_shuffle` in C to reproduce VT's Fisher-Yates exactly: from the last index down to 0 inclusive, one `get_random_int(0, i)` per index, swapping `i` and `r`. That way every VT shuffle can be ported.
8. As a C port developer, I want `fy_shuffle` of `n` elements to consume exactly `n` draws, including the final `get_random_int(0, 0)`, so that the draw count matches VT's trace.
9. As a C port developer, I want `fy_shuffle` to work on arrays of any element type without `malloc`, so that it can shuffle item ids, location ids or structs in the fixed-size `RandoWorld`.
10. As a C port developer, I want `fy_shuffle` of 0 elements to make no draw, and of 1 element to make one draw, so that edge cases match VT.
11. As a C port developer, I want a per-draw observer on the PRNG, called with `(min, max, r)` for every `get_random_int`, so that the future generator can write `trace.txt` lines without the PRNG knowing about stages.
12. As a C port developer, I want the observer to be optional and absent on the DS, so that the shipped generator pays nothing for tracing.
13. As a C port developer, I want the PRNG to keep no global state (its state lives in a struct the caller owns), so that it fits the one-`RandoWorld` rule and the retry loop can reseed it freely.
14. As a C port developer, I want the PRNG unit test to check every line of the oracle's vector file, so that the C and PHP PRNGs are held to the same known answers.
15. As a C port developer, I want the PRNG unit test to replay every draw of every committed golden trace, reseeding with `splitmix32(seed + attempt)` at each attempt and checking every `r` from its `min` and `max`. Then the C PRNG is proven against tens of thousands of draws VT really made, over all ranges the 44 configs use.
16. As a C port developer, I want `fy_shuffle` vectors in the oracle's vector file, checked against VT's real `fy_shuffle` by `prng_check.php`, so that the C shuffle is proven against VT and not only against my reading of its source.
17. As a maintainer, I want `prng_check.php` to fail on any `fy_shuffle` vector VT does not reproduce, so that a wrong vector cannot survive the next golden generation.
18. As a maintainer, I want the vector file's header to say what verifies its lines (VT via `prng_check.php`, C via `test_prng`) instead of naming a generator that is not in the repository, so that Phase 1's warning is resolved and a reader knows which check to trust.
19. As a step author in Phase 2, I want a committed claims file naming each claimed case and its stage (or `all`), so that "this step claims the baseline up to `fillPrizes`" is a one-line, reviewable change.
20. As a step author, I want to move a claim forward by editing its one line, so that a later stage supersedes an earlier one. A later stage's prefix and checks contain the earlier ones, so nothing that held before can break silently.
21. As a step author, I want a stage claim's trace check to mean "the C trace's first K lines equal the golden's first K lines", where K is the line number of the last golden draw of that stage in attempt 0. Then "match VT up to that stage's last draw" has one exact meaning.
22. As a step author, I want a C trace longer than K to be fine for a stage claim, so that a generator already drawing past the claimed stage does not fail the claim.
23. As a step author, I want a C trace shorter than K to be `DIVERGENT`, with the draw number where it ended, so that a generator that stops early is caught.
24. As a step author, I want a claim on a stage that makes no draw in that case's attempt 0 rejected as a malformed claims file (exit 2), so that I always claim a stage that draws. Zero-draw stages (`setShops` and `placeBosses` in the baseline) are covered by the next drawing stage's claim through the stage table's order.
25. As a step author, I want a claim on an unknown case, a duplicate case, an unknown stage or a malformed line rejected with exit 2, so that a typo cannot make a case silently pending.
26. As a step author, I want a committed stage table listing VT's stages in draw order, so that "the checks owed up to stage S" is defined even for stages that draw nothing in a given case.
27. As a step author, I want a golden whose attempt-0 stage sequence contradicts the table's order to be rejected with exit 2, so that a wrong table cannot make a check silently fire too early or too late.
28. As the author of step 2.3.2, I want to add a `section Shops` check and a key-pattern check for the medallion entries to the stage table, so that the gate compares `Shops` and the two medallions of the partial C spoiler with the golden.
29. As the author of step 2.3.3, I want to add a `section Bosses` check and a key-pattern check for the `* - Prize:1` entries, so that the gate compares the bosses and dungeon prizes.
30. As the author of step 2.3.4, I want to add a key-pattern check for the fountain bottles (`Waterfall Bottle:1`, `Pyramid Bottle:1`), so that the gate compares them.
31. As the author of step 2.3.6, I want a `pool` check that compares the multiset of items the C generator still has to place with the multiset of golden location values not already covered by a key-pattern check, so that "the pool equals the golden's placed items minus the pre-filled ones" is enforced without me writing a new gate.
32. As the author of step 2.3.6, I want any location that VT's `prepareWorld` pre-fills to be declared with a key-pattern check of its own, so that it is compared by value and drops out of the pool multiset.
33. As a step author, I want a check that is due but whose entry is missing from the C partial spoiler to be `DIVERGENT`, naming the missing section or key, so that a generator cannot pass by omitting an entry.
34. As a step author, I want checks owed by stages after the claimed one to be ignored, so that a claim does not demand entries its stages do not determine yet.
35. As a step author, I want an `all` claim to compare the full trace, the `attempts` file and `spoiler.json` byte for byte, so that Phase 2 step 4's "full spoiler equality and the same attempts" is enforced.
36. As a `check-e2e` run, I want exactly one `DIVERGENT <case>` line per failed claim, so that I can count and pick failures as my skill says.
37. As a `check-e2e` run, I want the first differing draw printed under the `DIVERGENT` line, as the golden line and the C line, so that I can hand "attempt N, draw #K, stage S" to `fix-bug` without diffing by hand.
38. As a `check-e2e` run, I want a failed spoiler check to print the stage that owes it, the check, and the first differing golden and C lines (which name the location), so that the plan's "prints the first location that differs" holds for partial claims too.
39. As a `check-e2e` run, I want the trace check to run before the spoiler checks, and only the first failure to be reported, so that the earliest cause comes first.
40. As a `check-e2e` run, I want an unclaimed case reported as `PENDING`, never `DIVERGENT`, so that I am never sent into `fix-bug` for work no finished step has claimed.
41. As a `check-e2e` run, I want `run_e2e.sh` to exit 0 when no case is `DIVERGENT`, 1 when one is, and 2 on a usage, claims-file or stage-table error, so that the make target's status means what it says.
42. As a step author, I want a claimed case with no C generator to be `DIVERGENT` with "no C generator", so that a claim is never satisfied by default.
43. As a step author, I want the generator's command-line contract fixed now, so that step 2.3.1 only has to implement it:
    - it writes `trace.txt` as far as it got;
    - it writes a partial `spoiler.json` and `pool.txt` when it stops before the fill;
    - it writes the full `spoiler.json` and `attempts` when it finishes.
44. As a step author, I want the generator path, the cases directory, the claims file and the stage table to be overridable by environment variables, so that the self-test can use fixtures and a fake generator.
45. As a developer, I want `run_e2e.sh <case>...` to keep working, so that I can check one case.
46. As a developer, I want a one-line summary at the end of `e2e` (pass, divergent and pending counts), so that I can see the gate's state at a glance.
47. As a maintainer, I want `run_e2e.sh` and `gen_golden.sh` to share one case-discovery function (all directories holding `config.json`, sorted; named cases checked to exist), so that the two scripts can never disagree on which cases exist. That also resolves Phase 1's warning.
48. As a maintainer, I want `gen_golden.sh`'s behavior unchanged after the refactor, so that the goldens can still be regenerated. To prove it, run `gen_golden.sh --out <scratch> baseline`, which never writes into `cases/`, and check that it produces files byte-identical to the committed baseline.
49. As a developer, I want the claim logic, the spoiler checks and the shared discovery proven by a self-test on fixture cases run by `make -C tests/rando test`, so that the gate is trusted before any real claim relies on it.
50. As a developer, I want `make -C tests/rando test` and `e2e` to stay in milliseconds, so that the loop stays fast (CLAUDE.md).
51. As a maintainer, I want a GitHub Actions workflow that runs `make -C tests/rando test` and `make -C tests/rando e2e` on every push and pull request, gcc only and without PHP, so that a broken PRNG or gate shows up on the PR.
52. As a maintainer, I want CI not to check out the `vendor/zelda3` submodule or install devkitPro, so that it stays fast and independent of the ROM.
53. As a maintainer, I want CI to build with the same strict flags as the local loop (`-std=c99 -Wall -Wextra -Werror`), so that a warning cannot hide on GitHub.
54. As a DS integrator (roadmap item 2), I want the PRNG to be pure C99 with no libnds and no `malloc`, so that the same object links into the ARM9 build.
55. As a DS integrator, I want the PRNG to use only 32-bit arithmetic in its hot path, so that it costs little on the ARM9. The 64-bit parameter type exists only to honour VT's full range contract.

## Implementation Decisions

All decisions below are mine. This run could not spawn a `guideline` agent, so none was consulted; each decision rests on the plan's Rules and on CLAUDE.md.

**PRNG module (new, in the `rando/` library, `prng.c` per the plan's Layout)**
- Interface, declared in its own header in the library's public include directory, which the unit tests already reach. It is not in `rando.h`, which stays the future `rando_generate` API.
  - A `RandoPrng` struct, owned by the caller (it will later sit inside `RandoWorld`). It holds the four `uint32_t` state words and an optional draw observer: a function pointer taking `(void *ctx, int64_t min, int64_t max, int64_t r)` plus its `ctx`. A NULL observer means no tracing.
  - `rando_splitmix32(uint32_t) -> uint32_t`.
  - `rando_prng_seed(RandoPrng *, uint32_t v)`, which leaves the observer untouched.
  - `rando_prng_next(RandoPrng *) -> uint32_t`.
  - `rando_random_int(RandoPrng *, int64_t min, int64_t max) -> int64_t`.
  - `rando_fy_shuffle(RandoPrng *, void *base, size_t count, size_t size)`.
- `int64_t` bounds and result: the vector file holds `range 2 0 4294967295` and `range 2 -2147483648 2147483647`, which VT accepts. The span is computed in 64 bits. `next() % span` is computed in 32 bits when the span is below 2^32, and the full-span case returns `min + next()`.
- Contract violations are `assert`s with a message, mirroring the Errors the patched VT throws: `min > max`, a span above 2^32, an all-zero state after seeding, and `next` on a never-seeded state. They are not unit-tested, because an `assert` aborts the binary.
- `fy_shuffle` swaps byte by byte, with no VLA or `malloc` (ARM9 stack). It draws `get_random_int(0, i)` for `i = count-1` down to `0`. The `i = 0` draw is real and must stay: it is a trap someone would "optimize" away, worth one comment of 40 words at most (CLAUDE.md § Comments).
- Not ported here: `mt_shuffle`, `Collection::random`, `randomCollection` and `weighted_random_pick`. Each is ported by the first sub-step of step 2.3 that needs it.

**Vector file and `prng_check.php` (oracle files; no golden changes)**
- A new vector kind: `fy_shuffle <v> <n> <the result of fy_shuffle([0, 1, …, n-1]) after prng_seed(v)>`. Add at least 5 lines: `n` of 0, 1, 2, 8 and 43, with at least two seeds. 43 is the length of the shuffle starting at baseline draw 80.
- `prng_check.php` gains the matching case. It also `require`s VT's `app/Helpers/array.php`, where `fy_shuffle` lives, and checks against VT's real function.
- The expected values are computed by the new C port, then confirmed against patched VT by running `gen_golden.sh --out <scratch dir> baseline`. That run executes `prng_check.php` before generating, and also proves the discovery refactor (below). PHP is installed in this container, and VT's clone is kept at the oracle's git-ignored `vt/` directory. It never writes into `cases/`, and no golden is regenerated (CLAUDE.md). The commit message records the confirmation.
- **Header (resolves Phase 1's warning).** Replace the line naming the uncommitted host-gcc generator with what verifies the lines: every line is checked against VT by `prng_check.php` (run by `gen_golden.sh`), and against the C port by `test_prng`. Keep the per-kind format lines. No generator is committed.

**Shared case discovery (resolves Phase 1's warning)**
- One POSIX `sh` file next to `run_e2e.sh`, sourced by both `run_e2e.sh` and `gen_golden.sh`, defining one function. Its arguments are the cases directory, a program name for messages, then optional case names.
  - It prints one case name per line: every sub-directory holding `config.json`, in `LC_ALL=C` glob order, or exactly the names given.
  - It exits 2 with `<program>: no such case: <name>` on stderr when a named case lacks `config.json`.
  - With no cases found, it prints nothing and exits 0. Each caller keeps its own empty-set policy: `run_e2e.sh` prints `e2e: no cases` and exits 0, and `gen_golden.sh` exits 2.
- `gen_golden.sh` is verified only with `--out <scratch>`, never writing `cases/`. The `baseline` output must be byte-identical to the committed `cases/baseline/`. The discovery function itself is covered by the self-test (no PHP).

**Claims file (new, committed, next to `run_e2e.sh`)**
- One claim per line, `<case> <stage>` or `<case> all`. A line starting with `#` is a comment, and blank lines are ignored. A case appears at most once.
- This step commits it with only the format comment: no claim.

**Stage table (new, committed, next to the claims file)**
- Ordered lines of two kinds:
  - `stage <name>`, in VT's draw order. The order is fixed by the goldens (step 1.2 note): `crystals`, `getAdvancementItems`, `setShops`, `setMedallions`, `placeBosses`, `fillPrizes`, `setFountains`, `shufflePrizePacks`, `prepareWorld`, `fill`. `placeBosses` sits after `fillPrizes`, as in `Randomizer::randomize()`. If reading that function shows another order, VT wins and the table follows it.
  - `check <stage> <kind> [<arg>]`, owed by the stage it names. The kinds are:
    - `section <Name>`: the whole top-level section `<Name>` of the spoiler.
    - `keys <glob>`: every entry line, in every top-level section except `playthrough`, whose key (without quotes) matches the shell glob. The glob is the rest of the line and may contain spaces.
    - `pool`: the multiset check below.
- This step commits the ten stage lines and no `check` line. Each Phase 2 step 3 sub-step adds the checks the plan names for it:
  - 2.3.2: `section Shops` under `setShops`, and `keys *Medallion:1` under `setMedallions`;
  - 2.3.3: `section Bosses` under `placeBosses`, and `keys * - Prize:1` under `fillPrizes`;
  - 2.3.4: `keys *Bottle:1` under `setFountains`;
  - 2.3.6: `pool` under `prepareWorld`, plus a `keys` check for every location it pre-fills.

  These examples are checked against the baseline spoiler at `HEAD`. Each sub-step owns its exact patterns.
- **Why the gate and not unit tests (the plan left the choice open).** The checks run for every claimed case on every `e2e`, so a later step (or a Phase 3 option) that breaks a medallion is caught there, the way a trace drift is. Adding a check is one table line, not a test that parses golden JSON. And `check-e2e`'s rule keeps unit tests from reading `cases/`.

**Partial spoiler and pool (generator side, contract only)**
- The generator writes `spoiler.json` even when it stops before the end, holding only the sections and entries it has determined, in the oracle's normalized format:
  - pretty-printed with 4-space indentation;
  - keys sorted;
  - one `"key": value` per line inside a section.

  The C `spoiler_json.c` must match that format byte for byte anyway, for the `all` claim.
- When it stops before the fill, it also writes `pool.txt`: the items still to be placed, one per line, in the spoiler's item notation (`Name:1`), in any order.

**Spoiler check semantics (in `run_e2e.sh`)**
- **Due checks.** A check is due for a stage claim when its stage is at or before the claimed stage in the table's order, whether or not that stage draws in the case.
- **`section`.** Equal when the golden and C spoilers' line blocks for that top-level section are identical. A section missing on the C side is `DIVERGENT`.
- **`keys`.** The golden's matching entry lines and the C side's matching entry lines must be identical, as sorted sets of lines tagged with their top-level section. When the golden has a match the C side lacks, the detail names the missing key.
- **`pool`.** The golden multiset is built from the values of every entry line in every top-level section except `Bosses`, `Equipped`, `Shops`, `Special`, `meta` and `playthrough`, minus the lines matched by any due `keys` check. It is compared with the sorted lines of the C `pool.txt`, and the first differing item is printed with its count on each side. A missing `pool.txt` is `DIVERGENT`.
- **Order.** The trace prefix is checked first, then the due checks in table order. Only the first failure is reported.
- **Tools.** Extraction is done with `awk` on the pretty-printed format, with POSIX `sh`, `sort`, `cmp` and `diff`, and `LC_ALL=C`. No PHP and no `jq`.

**`run_e2e.sh` (modified)**
- **Inputs:**
  - the cases directory (override `RANDO_CASES`);
  - the claims file (`RANDO_CLAIMS`);
  - the stage table (`RANDO_STAGES`);
  - the generator command (`RANDO_GEN`, default the host build's `rando_gen` binary in the build directory).

  The defaults are the committed files.
- **Validation, exit 2.** All of it runs before any case, with a message on stderr. The claims file is rejected for:
  - a malformed line;
  - a claimed case without `config.json`;
  - a duplicate case;
  - a stage not in the table;
  - a stage with no line in the case's attempt 0.

  The stage table is rejected for:
  - a malformed line;
  - a duplicate stage;
  - a `check` naming an unknown stage or kind.

  A claimed case is also rejected when its golden's attempt-0 stage sequence is not in table order. An unknown case named on the command line is rejected through the shared discovery function.
- **Per case:**
  - **Unclaimed:** `PENDING <case>`. The generator is not run.
  - **Claimed:** run `<generator> <case_dir> <out_dir>` into a fresh temporary directory, removed on exit. A missing or non-executable generator gives `DIVERGENT <case>` and `  no C generator: <path>`.
  - **Stage claim:**
    - The generator's exit status is ignored, because stopping after the last ported stage is normal during step 2.3.
    - Trace check: K is the 1-based line number of the golden's last attempt-0 line with that stage. The first K lines of both traces must be equal.
    - On a difference, print `DIVERGENT <case>` and `  first differing draw:` with indented `golden:` and `c:` lines.
    - On a C trace that is too short, print `  c trace ends after <n> draws, claim needs <K>`.
    - Then run the due checks. A failure prints `DIVERGENT <case>` and `  <stage>: <check>:` with the first differing `golden:` and `c:` lines, or the missing key or section.
    - If everything holds: `PASS <case> (<stage>)`.
  - **`all` claim:**
    - The generator must exit 0.
    - Compare `attempts`, the full `trace.txt` and `spoiler.json` byte for byte, in that order, with the same detail formats.
    - Stage checks are not run, since the full spoiler subsumes them.
    - If everything holds: `PASS <case> (all)`.
- **Output.** Exactly one `DIVERGENT <case>` line per failed case. Detail lines are indented and never start with `DIVERGENT`. The last line is `e2e: <p> pass, <d> divergent, <n> pending`. Exit 1 if `d > 0`, otherwise 0.

**Generator contract (fixed now, implemented in step 2.3.1)**
- Usage: `<generator> <case_dir> <out_dir>`.
- It reads `config.json`, `seed` and `attempts`. The golden's `attempts` caps the retry loop.
- It writes `out_dir/trace.txt` in the oracle's exact line format (`<attempt> <draw#> <stage> <min> <max> -> <r>`, draw numbers restarting at 1 each attempt, VT's stage names), as far as generation got.
- It writes the partial `spoiler.json` and `pool.txt` (above) when it stops early.
- On success it writes the full `spoiler.json` and `attempts`, and exits 0.
- Step 2.3.1 adds its build rule to the `e2e` target. This step adds none.

**Self-test of the gate (new)**
- A POSIX shell test script in the rando tests directory, run by `make -C tests/rando test` after the C unit tests and counted in its summary line.
- In a temporary directory it builds fixture cases, a claims file, a stage table and a fake generator (a shell script copying prepared outputs). It runs `run_e2e.sh` with the four overrides and checks stdout lines and exit status.
- It also calls the discovery function directly: sorted listing, named-case error, empty directory.
- It is a shell script, not a `unit/test_<module>.c`, because the modules under test are shell scripts. `UNIT_TESTS` stays the list of C tests.

**Makefile (modified)**
- Register `test_prng` in `UNIT_TESTS`, and add the self-test to the `test` recipe.
- The C test opens the vector file and the golden traces by paths relative to the rando tests directory, where `make` runs the binaries. It finds the cases by reading the directory with POSIX `opendir`, since host-only test code may use POSIX.

**CI (new workflow, per the plan's Layout)**
- Trigger: `push` (all branches) and `pull_request`. One job on `ubuntu-latest`:
  - `actions/checkout` at a pinned major version, without submodules;
  - `make -C tests/rando test`;
  - `make -C tests/rando e2e`.
- No PHP, no devkitPro and no caches.

**Claims in this step:** none. This step's own stage is the PRNG, which the unit tests verify. The first claim, the baseline up to `getAdvancementItems`, comes with step 2.3.1 and the generator.

**No retry-seed case exists** (step 1.2 note). Nothing in this step depends on one. The attempt-0 rule for stage claims and the per-attempt reseeding in the replay test are still implemented and covered by fixtures: they cost nothing, and step 2.4's injected-failure check produces `attempts` 1.

## Implementation Chunks

1. **C PRNG.** Build `splitmix32`, seed, `next`, `random_int` and the observer, with `test_prng` registered in `UNIT_TESTS`. The test checks:
   - every `splitmix32`, `next` and `range` line of the vector file;
   - every golden trace, replayed;
   - the observer, called once per draw with `(min, max, r)`;
   - that reseeding resets the sequence.
2. **`fy_shuffle` and the vector file.** Add the C function, the `fy_shuffle` vector lines, the new header, and the `prng_check.php` case. Extend `test_prng` to cover:
   - the `fy_shuffle` vectors;
   - draws consumed equal `n`;
   - the final `(0, 0)` draw seen by the observer;
   - other element sizes (`uint16_t`, a small struct) giving the same permutation.

   VT confirmation waits for chunk 3's `gen_golden.sh --out` run, so chunks 2 and 3 can land in either order. The run happens once both are in.
3. **Shared case discovery.** Add the sourced discovery file and switch both scripts to it, with no behavior change. Add the discovery part of the self-test. Verify with the self-test, plus `gen_golden.sh --out <scratch> baseline` byte-identical to `cases/baseline/` (which also confirms chunk 2's vectors through `prng_check.php`).
4. **Claims and trace-prefix gate.** Add the empty claims file, the stage table (ten stage lines), validation, `PENDING`/`PASS`/`DIVERGENT` for trace-prefix and `all` claims, and the summary line. Add the self-test fixtures:
   - pending;
   - stage pass;
   - stage pass with a longer C trace;
   - first differing draw;
   - short C trace;
   - a prefix ending at the last attempt-0 line, with an attempt-1 fixture;
   - `all` pass;
   - `attempts` mismatch;
   - spoiler mismatch;
   - missing generator;
   - each exit-2 validation.

   The real `make e2e` prints `PENDING` for all 44 cases and exits 0.
5. **Spoiler checks.** Add the `section`, `keys` and `pool` kinds and the due-check rule, with self-test fixtures:
   - each kind passing;
   - each kind failing, checked for its exact detail;
   - an entry missing on the C side;
   - a missing `pool.txt`;
   - a check owed by a zero-draw stage (`setShops`-like) becoming due under the next drawing stage's claim;
   - a check owed by a later stage being ignored.
6. **CI.** Add the GitHub Actions workflow. Check it locally by running its two commands from a clean checkout.

## Testing Decisions

- Good tests check external behavior only:
  - the values a PRNG call returns;
  - the permutation `fy_shuffle` leaves;
  - the draws the observer sees;
  - the lines and exit status `run_e2e.sh` and the discovery function print.

  They never check internal state words or temporary files.
- **PRNG, `test_prng` (C, one per module, CLAUDE.md):**
  - Known answers from the vector file. VT reproduces them, as `prng_check.php` proves at each golden generation.
  - A replay of every committed golden trace. For each line, reseed with `splitmix32(seed + attempt)` when the attempt changes, then check that `random_int(min, max)` returns `r`. That is 773 to 924 draws per case, about 37 000 VT draws in all. A Python prototype of exactly this replay matched all 805 baseline lines while this PRD was written.
  - It reads only each case's `seed` and `trace.txt`, discovering the cases from the directory without hard-coding names or counts. It runs well under 50 ms.
  - `check-e2e`'s rule against reading `cases/` applies to `fix-bug` regression tests. Here, the golden traces are the plan's "against VT on N draws".
- **`fy_shuffle`, also in `test_prng` (same module):** the VT-confirmed vectors, the draw count, the `(0, 0)` final draw, and element-size independence.
- **Gate and discovery, the shell self-test:** one fixture per outcome listed in chunks 3–5. Each `DIVERGENT` detail is checked line by line, since `check-e2e` reads it.
- **`gen_golden.sh` refactor:** checked by one manual `--out <scratch> baseline` run, byte-identical to the committed baseline, and recorded in the commit message. It is not added to `make test` (PHP, network, seconds).
- **Not tested:** `assert` contract violations (they abort), and the workflow file, which is checked by running its commands locally and by its first run on the PR.
- **Prior art:** the existing harness only (`Makefile` `test` loop, `run_e2e.sh` argument checks) and `prng_check.php`, whose vector parsing the C test mirrors. `test_prng` is the first C unit test and sets the pattern: plain `main`, and a nonzero exit with a message on the first failure, with no framework.
- **Speed:** everything added to `test` and `e2e` runs in milliseconds (CLAUDE.md).

## Hypotheses

**Phase 1 step 2 is done.** The hypotheses the first draft made about it were checked against the committed step 1.2 note and goldens:

- **H-2-1 — failed.** There are exactly 44 cases and no retry-seed case. The PRD now relies on neither the count nor a retry case: the replay test discovers cases, and the attempt-0 rule is covered by fixtures.
- **H-2-2 — held.** `baseline`, seed 1, is unchanged.
- **H-2-3 — held.** The trace format is unchanged.
- **H-2-4 — moot.** No case has `attempts` > 0.
- **H-2-5 — held.** Case names are `<option>-<value>`, with no whitespace and no leading `#`.
- **H-2-6 — held.** `prng_vectors.txt` and `prng_check.php` are unchanged by step 1.2.
- **H-2-7 — held.** `run_e2e.sh` still prints only `PENDING`.
- **H-2-8 — failed in part.** The stages are the baseline's, plus `crystals` first (the two `*-random` cases) and `setShops` after `getAdvancementItems` (`state-retro` only). The stage table now encodes this order.

**Remaining hypotheses, about steps not yet written:**

- **H-2-9.** Every spoiler entry that Phase 2 step 3's sub-steps name can be expressed as a whole top-level section (`Shops`, `Bosses`), a key glob (medallions and bottles in `Special`, prizes as `* - Prize:1` in region sections), or the `pool` multiset. This is checked against the baseline and `state-retro` spoilers at `HEAD`. If a sub-step needs another kind, it adds it to the gate, with fixtures.
- **H-2-10.** VT's `getSpoiler()` lists every location in a region section, so "the golden's placed items" is the set of entry values outside `Bosses`, `Equipped`, `Shops`, `Special`, `meta` and `playthrough`. Step 2.3.6 confirms this and declares every pre-filled location with a `keys` check.
- **H-2-11.** `placeBosses` runs after `fillPrizes` in `Randomizer::randomize()`, matching the plan's pairing of the two in step 2.3.3. It never draws in any golden, so the traces cannot confirm this. Chunk 4 reads VT's source and orders the table by it.

## Out of Scope

- Any C generation beyond the PRNG and `fy_shuffle`: the world, regions, rules, consumers, fill, retry loop, `rando_generate`, `spoiler_json.c` and the `rando_gen` driver (step 2.3.1).
- Claiming any case, and adding any `check` line to the stage table. Step 2.3's sub-steps do both.
- The other VT RNG primitives (`mt_shuffle`, `Collection::random`, `randomCollection`, `weighted_random_pick`).
- Changing or regenerating any golden in `tests/rando/cases/`, `vt.patch` or `gen_case.php`, and running `gen_golden.sh` without `--out`.
- Finding a retry-seed case.
- PHP in CI, devkitPro or the ARM9 build in CI, and the `.nds` build.
- Host measurements (Phase 2 step 5, Phase 4).

## Further Notes

- The `range`/`next`/`splitmix32` vectors are proven against VT by `prng_check.php` (step 1.1 note), so C equality with them is equality with VT. The golden replay adds VT's actual output streams over every range the 44 configs draw.
- `fy_shuffle`'s trailing `get_random_int(0, 0)` and `randomCollection`'s last pick make the same draw pattern (`0 n-1`, …, `0 0`). A trace alone cannot tell them apart, which is why `fy_shuffle` gets explicit, VT-checked permutation vectors.
- Keep the gate's output stable. `check-e2e` parses the `DIVERGENT <case>` lines and reads the first differing draw to pick the earliest-diverging failure.
