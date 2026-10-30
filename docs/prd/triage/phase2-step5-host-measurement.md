---
title: "Rando host measurement: state size, stack peak, code size, attempts, draws and time, against an assumed ARM9 budget"
description: "Once the fill lands, the generator's layout and call depth are fixed, but nobody knows whether its state, its stack peak, its code or its run time fit the ARM9, and Phase 3's options are about to build on that layout."
status: needs-triage
---

## Problem Statement

Plan step (Phase 2, step 5, verbatim):

> **Measure on the host**, as Phase 4 step 1 does, now that the state layout and the fill are fixed and before options build on them.

Phase 4 step 1, which it points to:

> **Measure on the host**, outside the fast `test` and `e2e` targets: the size of the whole generator state, peak stack use, and the attempts and RNG draws per case (from the goldens and traces). Record the figures in the step note, against the ARM9 budget, before roadmap item 2 starts.

The C library must run on the ARM9 (plan § C library constraints: C99, no `malloc`, one fixed-size state, 4 MB of main RAM shared with zelda3). After Phase 2 step 4 the generator is complete for the baseline: world, pools, prize and pre-fill stages, the `RandomAssumed` fill and `isWinnable()`, under the retry loop. Today, though, the only figures are scattered:

- the step 2.3.6 note's `sizeof(RandoWorld)` (2 748 bytes) and `sizeof(RandoGen)` (3 896 bytes), host x86-64, read by hand; the step 2.4 note gives none;
- the step 2.2 note's "85 KB text, 21 KB const data, no bss", read once at `-O0` with host `size`, where pointer tables count as data;
- the step 4 PRD's estimate of the fill's working state ("under 4 KB per call", on the stack), which the step 2.4 note does not record;
- the golden draw counts of the step 1.1 and 1.2 notes (805 draws for baseline, 773 to 924 across the matrix), counted once with a one-off command.

Nobody has measured the stack peak of a whole generation. On the DS the main thread's stack is small and lives in fast memory, and the fill nests several 716-byte item multisets per call. Nobody has measured the time of one generation either, and the DS runs it on a 67 MHz ARM946E-S while the player waits on the file screen. If a figure is over budget, the fix (moving working state into `RandoGen`, splitting a frame, trimming a table) is cheap now and expensive once Phase 3 has added eleven option rows on top of the same layout.

Facts read at the step 2.4 commit (`defe4d9`), on this container, unless a line says otherwise:

- There is no devkitPro (`/opt/devkitpro` does not exist, no `arm-none-eabi-gcc`, `DEVKITPRO` unset), so no ARM9 cross build is possible here. The root `Makefile` builds the ARM9 with devkitARM gcc, `-march=armv5te -mtune=arm946e-s -marm -O2 -ffunction-sections -fdata-sections`, `ds_arm9.specs`, libnds 2.0 and calico, and writes a link map. The ARM9 build does not compile `rando/` yet (roadmap item 2).
- Host gcc 13.3 accepts `-fstack-usage` and `-fcallgraph-info`. `gcc -m32` cannot link (no 32-bit crt). `clang` 18.1 is installed and compiles every library source for `--target=arm-none-eabi -march=armv5te -mtune=arm946e-s -marm -O2 -ffreestanding`, given a two-header shim (`assert.h`, `string.h`): the library includes only `assert.h`, `stdbool.h`, `stddef.h`, `stdint.h` and `string.h`, and calls only `memcpy`, `memmove`, `strcmp` and the assert handler (`memcpy` is new since step 2.3.6; the shim already declares it).
- Library sizes, from `size` summed over the library objects (text includes read-only data on these ELF objects):
  - at `defe4d9`, host gcc `-O2` without `RANDO_FAULTS`, stack protector or fortification: 90 125 text, 22 840 data, 0 bss;
  - at `defe4d9`, clang ARM proxy `-O2`, asserts compiled out: 83 191 text, 0 data, 0 bss;
  - at `54ec1a5`, for comparison: host `-O0` 107 805 / 22 656 / 0, host `-O2` 84 814 / 22 840 / 0, host `-Os` 73 866 / 22 840 / 0, ARM proxy 79 511 / 0 / 0. The fill added about 5 KB of code.
  The host's 22 KB of data is `.data.rel.ro`: the name and rule pointer tables, relocated for a PIE. On the ARM proxy the same tables are read-only data.
- Largest library stack frames at `defe4d9` (`-fstack-usage`, host `-O2` / ARM proxy): `rando_prepare_pools` 2 640 / 2 544 (1 168 at host `-O0` at `54ec1a5`: inlining merges callees' locals), the filler's `fill_items_in_locations` 2 544 / 2 464, `rando_world_is_winnable` 2 256 / 2 184, `place_prize_phase` 1 632 / 1 560, the world's `search` 1 056 / 1 008. The ARM proxy's frames are 3 to 5 % smaller than the host's. Every frame is `static` except the draw observer in `generate.c` (32 bytes, `dynamic,bounded`). The host case reader's `rando_case_read` has a 5 216-byte frame: host-only, and it must not be counted (the driver reads the case before the measured run).
- Rules are called through function-pointer tables (`RandoRule`, `RandoAlwaysAllowRule`, `RandoFillRule` in the region tables), and the PRNG observer and trace hook are function pointers too. A static call graph built from `-fcallgraph-info` therefore has unresolved indirect edges at every rule call.
- `RandoItems` is 716 bytes (two `uint16_t` arrays of 179 items). `RandoGen` holds the options, the PRNG, `RandoPools` and `RandoWorld` by value, plus the trace hook and its context (two pointers, 16 bytes on the host, 8 on the ARM9). Without `RANDO_FAULTS`, host x86-64: `RandoGen` 3 912 bytes, `RandoWorld` 2 764, `RandoPools` 1 032. With it, `RandoGen` gains three fault fields (12 bytes).
- The fill's caller-owned `RandoFillState` (a `uint8_t` per location plus a `uint16_t` count, about 240 bytes) is a local of `run_attempt`, not a field of `RandoGen`: it is stack, and the stack peak counts it.
- `tests/rando/Makefile` puts `-DRANDO_FAULTS` in `CFLAGS`, so every host object (`build/obj`, `build/host`, the unit tests, and the e2e generator's `-O2` objects in `build/o2/`, library and host sources alike) has the fault fields.
- A quick `-O2` driver without `RANDO_FAULTS`, with a counting trace hook, best of five, at `defe4d9`: baseline `RANDO_COMPLETE` at attempt 0 with 805 draws (573 `fill`) in 3.5 ms; `dungeon_items-full` 924 draws in 4.8 ms; `goal-pedestal` 783 in 3.7 ms; `state-retro` `RANDO_UNSUPPORTED`. Under `-std=c99`, `clock_gettime` and `CLOCK_MONOTONIC` are hidden unless the driver defines `_POSIX_C_SOURCE` (200809L) before its includes.
- `make test` takes about 80 ms and `make e2e` about 0.29 s (step 2.4 note); claims are 18 at `all` and `dungeon_items-mcs` at `prepareWorld`.
- Every golden has `attempts` 0 (step 1.2 note), so per-case attempts carry no information beyond that; the retry path is reached only through step 4's host-only fault injection.

## Solution

A new, separate make target, `make -C tests/rando measure`, prints one plain-text report. It is never a prerequisite of `test`, `logic`, `selftest` or `e2e`, has its own build directory, and is not run by CI: its figures depend on the machine, and it gates nothing. The step note `### Step 2.5` in `docs/randomizer.md` records its figures against an explicit ARM9 budget, which this PRD fixes as assumptions (§ Budget assumptions) and the note restates.

The report has seven sections:

1. **Environment.** Compiler versions, the host CPU model, whether devkitPro and clang were found, and the commit.
2. **State sizes.** `sizeof` of `RandoGen` and each of its parts (`RandoOptions`, `RandoPrng`, `RandoPools`, `RandoWorld`), plus `RandoItems`, `RandoConfig` and the fill's caller-owned `RandoFillState`, which is reported apart as stack-resident state because it is a local of `run_attempt` (H-11-3). Each size is given for the host (x86-64) and, when available, for the ARM proxy and the devkitARM build. All are measured from a compiled object, so no cross target has to run.
3. **Library size.** Code, read-only data, writable data and bss of the library alone (the host-only sources excluded), summed by section class, for the host at `-O2` and `-Os`, and for each ARM build found. Any writable data or bss in the library is flagged: the library must have no mutable static state.
4. **Largest stack frames.** The ten largest `-fstack-usage` frames, per build, and every frame that is not `static`.
5. **Stack peak.** The peak stack depth of one whole generation, measured at run time on the host, for every supported case, with the maximum and the case it comes from.
6. **Per case.** For every case in `tests/rando/cases/`: from the golden, `attempts`, the draw count of each attempt and per stage; from the C generator, its status, attempts, draws, and the median time of one generation.
7. **Seed sweep.** The baseline config over a range of seeds, unbounded attempts with a watchdog: the attempt distribution, the maximum draws and the maximum and median time per generation.

Each figure that has a budget is printed next to it, with `over` when it exceeds it. The target exits 0 whatever the figures; it exits non-zero only when a measurement cannot be taken (a build fails, a case is unreadable, the stack self-check fails).

## User Stories

1. As the author of roadmap item 2, I want `sizeof(RandoGen)` and each of its parts on the host and on an ARM build, so that I know how much main RAM the generator's one static state takes on the DS.
2. As the author of roadmap item 2, I want the library's code and read-only data size with the host-only sources excluded, so that I know what linking `rando/` adds to the ARM9 binary.
3. As the author of roadmap item 2, I want the figures on an ARM32 build and not only on x86-64, so that pointer size and the ARM instruction set are reflected in the numbers I compare with the budget.
4. As the author of roadmap item 2, I want the peak stack depth of a whole generation, so that I know whether the generator can run on the ARM9's main thread or needs its own stack.
5. As the author of roadmap item 2, I want the time of one generation on the host, with the assumed host-to-ARM9 factor stated, so that I know roughly how long the new-file screen waits.
6. As a Phase 3 implementer, I want these figures recorded before the options grow the world, so that each option row can be compared with a known baseline.
7. As a maintainer, I want the measurement in its own make target, so that `make test` and `make e2e` stay exactly as fast as before (CLAUDE.md).
8. As a maintainer, I want the measurement's build in its own directory, so that it never invalidates or races the objects of `test`, `logic` or `e2e`.
9. As a maintainer, I want the measurement to build the library exactly as the DS will see it, without `RANDO_FAULTS`, so that host-only test fields do not inflate `RandoGen`.
10. As a maintainer, I want the measured library compiled at `-O2`, as the ARM9 build is, so that frame sizes reflect the inlining the DS build will do.
11. As a maintainer, I want stack protector and source fortification disabled in the measured build, so that host frames are not padded by checks the ARM9 build does not have.
12. As a maintainer, I want any writable data or bss in the library flagged in the report, so that a mutable static that slips into `rando/src` is seen.
13. As a maintainer, I want the ten largest stack frames per build listed, so that an over-budget peak points to the function to fix.
14. As a maintainer, I want every frame that is not `static` listed, so that a variable-length array or an `alloca` never enters the library unnoticed.
15. As a maintainer, I want the stack peak measured at run time on a painted stack, so that the figure follows the real call path through the rule tables' function pointers, which a static call graph cannot resolve.
16. As a maintainer, I want the stack peak measured on every supported case, so that the deepest option path is found and not only the baseline's.
17. As a maintainer, I want the stack painter checked against a known consumption before it reports, so that a broken painter cannot print a reassuring figure.
18. As a maintainer, I want the stack peak reported net of the measuring thread's own overhead, so that the figure is the generator's alone.
19. As a maintainer, I want the generator measured with a trace hook that only counts draws, so that the host's stdio is not counted in the stack peak or the time.
20. As a maintainer, I want each case's golden attempts and draws, per attempt and per stage, so that the step note restates the work VT does for each case from the committed files.
21. As a maintainer, I want the C generator's own attempts and draws per case beside the golden's, so that a case where the C run does more or less work than VT shows up in the report, even before its claim reaches `all`.
22. As a maintainer, I want a case that the C port does not support yet (inverted, retro) reported as unsupported, so that the report still covers all 44 cases.
23. As a maintainer, I want the C run of each case bounded as the e2e gate bounds it (the golden's attempts plus one), so that a case whose logic is not ported yet cannot loop.
24. As a maintainer, I want the time of one generation as the median of several runs, so that one noisy run does not set the figure.
25. As a maintainer, I want a seed sweep over the baseline config, so that the attempt distribution and the worst time over seeds are known, not only seed 1's.
26. As a maintainer, I want the seed sweep's range set by a make variable with a small default, so that `measure` stays a few seconds and can be widened by hand.
27. As a maintainer, I want the sweep bounded by a watchdog, with a seed that reaches it reported, so that an unattended run never hangs.
28. As a maintainer, I want an ARM proxy built with the installed clang when devkitPro is absent, so that ARM32 figures exist on this container.
29. As a maintainer, I want the devkitARM build used when devkitPro is found, so that a human with the toolchain gets the real ARM9 figures from the same target.
30. As a maintainer, I want a missing toolchain reported as skipped, not as a failure, so that `measure` runs on any host with gcc.
31. As a maintainer, I want each figure printed beside its assumed budget with an `over` marker, so that the step note can be written from the report without arithmetic.
32. As a maintainer, I want `measure` to exit 0 when a figure is over budget, so that it records rather than gates, as the plan asks.
33. As a maintainer, I want the budget assumptions written once in the step note, so that roadmap item 2 knows which numbers to replace with the real ARM9 link map.
34. As a reviewer, I want the report's golden figures for baseline to match the step 1.1 note (805 draws; 4, 2, 64, 2, 7, 153, 573 per stage), so that the report's parsing is checked against a known answer.
35. As a reviewer, I want `make test` and `make e2e` timed before and after this step in the step note, so that "never slower" is shown, not asserted.
36. As the author of Phase 4 step 1, I want the same target to exist and be rerun, so that Phase 4 compares its figures with these ones instead of building a new harness.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. They rest on the plan's Goal, Rules (§ C library constraints, § Retry contract, § Convergence gate), Phase 2 and Phase 4, step notes 1.1 to 2.4, the step 4 PRD, CLAUDE.md, the committed `rando/`, `rando/host/` and `tests/rando/`, and the facts measured above. Every assumption about step 4's outcome is labelled `H-11-n` (§ Hypotheses). Every assumption about the DS is labelled `A-n` (§ Budget assumptions).

**Make target**
- `make -C tests/rando measure` is a new phony target in `tests/rando/Makefile`. It depends on nothing that `test`, `logic`, `selftest` or `e2e` build, and none of them depends on it. Its objects and outputs go under the build directory's own `measure/` subdirectory.
- It builds the library and the host sources needed by the driver (the case reader) from source, at `-O2`, with `-fstack-usage`, `-fno-stack-protector` and `-U_FORTIFY_SOURCE`, and without `RANDO_FAULTS` (H-11-5). Building the host sources itself avoids linking objects compiled with a different `RandoGen` layout.
- It then runs a new shell script, `measure.sh`, beside `run_e2e.sh`, which orchestrates the sections and prints the report on stdout, and also writes it to `measure/report.txt` under the build directory. Nothing it produces is committed: the figures go into the step note by hand.
- `MEASURE_SWEEP` (default 1 024) sets the sweep's seed count. `MEASURE_REPS` (default 5) sets the repetitions per timed generation. A run with the defaults takes a few seconds on this container (one baseline generation ≈ 3.5 ms at `-O2`, H-11-6, so the sweep is about 4 s and the per-case pass under 1 s).
- Not added to CI. The figures depend on the machine, and the target gates nothing.

**Size measurement (state and library)**
- Struct sizes come from a tiny size-probe translation unit that defines one global byte array per struct, each sized by `sizeof` of that struct. `nm -S` (or `llvm-nm -S`) reads the sizes from the compiled object. This works the same for the host, the clang ARM proxy and devkitARM, and no cross target runs. Probed: `RandoGen`, `RandoOptions`, `RandoPrng`, `RandoPools`, `RandoWorld`, `RandoItems`, `RandoConfig`, and `RandoFillState` (H-11-3). Every other part of `RandoGen` is reported as the remainder (`RandoGen` minus the probed parts), so padding and the hook pointers are visible.
- Library size: per-section sizes of the library objects only (`rando/src`, never `rando/host`), read with `size -A` and summed into four classes: code (`.text*`), read-only data (`.rodata*`, `.data.rel.ro*`), writable data (`.data*` other than `.data.rel.ro*`), bss (`.bss*`, `COMMON`). Debug sections are ignored. `.data.rel.ro` is counted as read-only because on the DS (no PIE) those tables are plain read-only data, as the ARM proxy shows.
- Host builds measured: `-O2` (the ARM9 build's level) and `-Os` (what a size-optimised DS build would give). Each ARM build: `-O2` only.
- Writable data or bss in the library prints `FLAG: mutable static state in the library` with the object and section names. It does not fail the target.

**ARM builds**
- **devkitARM**: when `DEVKITPRO` is set, or `/opt/devkitpro` or `~/devkitpro` exists, and its `devkitARM/bin/arm-none-eabi-gcc` runs, the library is compiled with the root `Makefile`'s ARM9 code-generation flags (`-march=armv5te -mtune=arm946e-s -marm -O2 -ffunction-sections -fdata-sections`) plus `-std=c99 -fstack-usage`, against devkitARM's own newlib headers. Section sizes and frames as above.
- **clang ARM proxy**: when `clang` is found, the same sources are compiled with `--target=arm-none-eabi` and the same `-march`, `-mtune`, `-marm`, `-O2`, plus `-ffreestanding -std=c99 -fstack-usage`, against a two-header shim kept beside `measure.sh` (`assert.h` and `string.h`, declarations only). The shim's `assert` expands to nothing, so the proxy's code size is a lower bound (asserts compiled out); the report says so.
- Neither found: both sections print `skipped: <toolchain> not found`. On this container, expect devkitARM skipped and the clang proxy present; the step note records which ran.
- A build is never linked: the library has no `main`, and sizes and frames come from objects.

**Stack frames (static)**
- From the `.su` files of each build: the ten largest frames (function, file, bytes, qualifier), and every frame whose qualifier is not `static`. A `dynamic` frame that is not `bounded` prints `FLAG: unbounded stack frame`.
- No static call-graph maximum. The rule tables, the PRNG observer and the trace hook are indirect calls, so a call-graph sum would need hand-written edges for some 230 rule functions, which would rot. The run-time peak (below) follows the real paths; the frame list explains it.

**Stack peak (run time, host)**
- A new host-only C driver, `rando_measure`, in a new `tests/rando/measure/` directory (with `measure.sh`'s shim headers beside it). It links the `-O2` measure build of the library and the case reader. It uses `pthread`, which is host-only and confined to this driver.
- The generation runs on a thread whose stack is a static 1 MiB buffer given with `pthread_attr_setstack`. Before each run the buffer is painted with a fixed byte pattern; after the join, the driver scans from the low end for the first byte that differs. The peak is the distance from that byte to the top of the buffer.
- The thread's own overhead (thread start-up and the driver's entry function) is measured the same way with an empty body and subtracted; both the raw and the net figures are printed.
- **Self-check, before any case:** the thread runs a function that writes a local array of a known size (16 KiB) through a volatile pointer, and the net peak must be at least that size and at most that size plus 1 KiB. Otherwise the target fails with `measure: stack self-check failed`, because every later figure would be meaningless.
- `RandoGen` is a static in the driver, as it is in `rando_gen` and as it will be on the DS, so its 4 KB is never counted as stack. `RandoFillState` and `RandoConfig`, locals of `run_attempt`, are counted, as they will be on the DS.
- The case is read on the main thread before the painted thread starts, so the case reader's 5 KB frame is never counted; the painted thread runs `rando_generate` only.
- The driver defines `_POSIX_C_SOURCE` as 200809L before its includes: the measure build keeps `-std=c99`, which otherwise hides `clock_gettime`, `CLOCK_MONOTONIC` and `pthread_attr_setstack`.
- The trace hook counts draws per attempt and per stage in static counters, and calls nothing.
- Measured per supported case (the 42 that are not inverted or retro); the report prints each case's net peak, then the maximum and its case.
- The figure is the x86-64 `-O2` peak. The ARM9's will differ; the report puts the ARM proxy's frame sizes for the top functions beside the host's, so the ratio is visible. The budget comparison uses the larger of the host peak and the host peak scaled by the largest ARM-to-host frame ratio among the ten largest frames (A-1).

**Per-case figures**
- Golden side (every case in `tests/rando/cases/`, read through `cases.sh`'s discovery): `attempts`, and from `trace.txt` the draw count of each attempt, the total, and the per-stage counts of the winning attempt, in `stages.txt` order. Computed by `measure.sh` with one awk pass per case.
- C side (`rando_measure`, every case): `rando_generate` with the case's options and seed, attempts bounded at the golden's `attempts` plus one (as `rando_gen` bounds them for e2e). Printed: status (`complete`, `gave-up`, `unsupported`), attempts, total draws and per-stage draws of the last attempt, and a `same`/`differs` column against the golden's draw counts. A differing count is information, not a failure: a case can differ until its Phase 3 row claims it. Equal counts do not mean equal draws: `glitches-no_logic` has the golden's 807 draws but diverges at a fill draw's value (step 2.4 note); the report says the column compares counts only.
- Time: the median of `MEASURE_REPS` whole `rando_generate` calls, measured with `clock_gettime(CLOCK_MONOTONIC)` around the call only (no process start, no case read, no output), printed in microseconds. Run on the main thread, not the painted one, after a warm-up call.

**Seed sweep**
- Baseline's config, seeds 1 to `MEASURE_SWEEP`, unbounded attempts with a watchdog of 1 000 attempts per seed (the oracle wrapper's watchdog). Printed: the number of seeds per attempt count, the seeds that needed a retry (first ten), the maximum draws over all attempts of a seed, and the median and maximum time per generation, with the slowest seed.
- A seed that reaches the watchdog is printed as `watchdog <seed>` and counted; it does not fail the target. VT never retried on baseline seeds 2 to 8 000 (step 1.2 note), so a retry here is itself a finding for the step note.

**Report format**
- Plain text, one section per heading, one figure per line, `name value unit [budget N] [over]`, so that the step note can quote lines and a later Phase 4 run can be diffed against this one.

**Step note**
- `### Step 2.5` in `docs/randomizer.md` records: the environment (which ARM builds ran); `RandoGen` and its parts (host and ARM); the library's code, read-only data, writable data and bss (host `-O2`, host `-Os`, ARM); the three largest frames; the stack peak and its case; the time per generation (baseline median, matrix maximum, sweep maximum) and the projected DS time; the golden and C draws and attempts (the matrix range, any `differs`); the sweep's attempt distribution; the budget assumptions A-1 to A-4 restated, each figure marked within or over; the `make test` and `make e2e` times before and after; and where each `H-11-n` landed. A figure over budget is recorded with the function or struct responsible; fixing it is not this step (§ Out of Scope).

## Budget assumptions

These stand in for the ARM9 budget, which cannot be read on this container (no devkitPro, so no ARM9 link map of zelda3). The step note restates them; roadmap item 2 replaces A-1 and A-2 with figures from the real ARM9 map.

- **A-1 — Stack: 8 KiB.** calico runs the ARM9 main thread on a stack in DTCM, which is 16 KiB and also holds the interrupt stacks and anything else placed there. The generator, called from the main thread on the file screen, may use at most half of it: a peak of 8 KiB, compared against the larger of the host peak and the ARM-scaled host peak.
- **A-2 — Main RAM: 160 KiB for the randomizer.** Main RAM is 4 MiB, shared with zelda3's code, data and assets. The randomizer may take at most 128 KiB of code and read-only data (ARM build, else host `-O2`) plus 32 KiB of static state (`RandoGen` and anything else the DS host keeps for it). Writable static data inside the library has a budget of 0.
- **A-3 — Time: 2 s per generation on the DS, i.e. 13 ms on the host.** A new randomized file may wait about 2 s. The ARM946E-S runs at 67 MHz with small caches and no out-of-order execution; the host factor is assumed to be 150 (about 45× clock, about 3× per clock). Host budget: 2 s / 150 ≈ 13 ms per whole generation at `-O2`, all attempts included, applied to the sweep's maximum.
- **A-4 — Attempts: none needed.** The DS loop is unbounded (§ Retry contract); no budget beyond A-3, which already includes every attempt.

## Implementation Chunks

1. **Target and static figures** (`test:`). The `measure` target, its build directory and flags, `measure.sh` with the environment, state-size (size probe), library-size and frame sections, the clang shim headers, and the devkitARM and clang detection. Check that `make test` and `make e2e` times are unchanged.
2. **Run-time driver** (`test:`). `rando_measure`: the painted-stack thread and its self-check, the counting trace hook, the per-case C figures and times, and the seed sweep; `measure.sh` gains the golden per-case pass and the stack, per-case and sweep sections, with budget markers.
3. **Step note** (`docs:`). Run `make -C tests/rando measure` at the step's commit and write `### Step 2.5` from its report.

## Testing Decisions

- **What a good check is here.** `measure` is a measuring tool, not a test, and it gates nothing. What can be wrong is the tool itself: a painter that misses the stack, a size probe that reads the wrong symbol, a parser that miscounts draws. Each gets a check against a known answer that runs inside `measure`, not in `make test`.
- **Stack self-check** (in `rando_measure`, chunk 2): the 16 KiB known consumption must read back within 16 KiB to 17 KiB net, or the target fails. It is the only guard on every stack figure.
- **Golden parser check** (in `measure.sh`, chunk 2): baseline's golden figures must equal the step 1.1 note's (805 draws; `getAdvancementItems` 4, `setMedallions` 2, `fillPrizes` 64, `setFountains` 2, `shufflePrizePacks` 7, `prepareWorld` 153, `fill` 573). A mismatch fails the target with the differing stage. This restates committed goldens, so it can never be wrong while they are unchanged (CLAUDE.md).
- **C-side cross-check** (in `rando_measure`, chunk 2): for baseline, the C run must be `complete` with attempts 0 and 805 draws (H-11-2). A mismatch is printed loudly but does not fail `measure`: correctness belongs to e2e.
- **Size probe check** (in `measure.sh`, chunk 1): the probe's host `RandoItems` size must equal 716, the product of its two `uint16_t` arrays and `RANDO_ITEM_COUNT` (179), or the target fails.
- **No unit test.** No library code changes, so no `UNIT_TESTS` entry and no `test_<module>.c`. `make test`, `make logic`, `make selftest` and `make e2e` are unchanged; the step note records `make test` (about 80 ms at `defe4d9`) and `make e2e` (about 0.29 s) before and after (median of five) to show it.
- **Prior art.** `run_e2e.sh` and `selftest.sh` for the shell structure and case discovery (`cases.sh`), `rando/host/main/rando_gen.c` for a host driver with a static `RandoGen` and the attempts bound, `test_generate`'s counting trace hooks for the draw counter.

## Hypotheses

Phase 2 step 4 landed while this PRD was written (`1ac5a6c` to `defe4d9`, note `defe4d9`). Each item below was checked against `defe4d9`; the verdict is recorded here. The implementer re-checks each before starting and copies the verdicts into the step note.

- **H-11-1 — Generator API. Holds.** `rando_generate(gen, options, seed, max_attempts, trace, trace_ctx)` and `rando_generate_until(…, last)` keep their signatures; a full run returns `RANDO_COMPLETE`. `RandoGen` still holds the options, PRNG, pools and world by value plus the two trace-hook pointers, and nothing else by pointer.
- **H-11-2 — Baseline. Holds.** `rando_generate` on baseline's options and seed 1 returns `RANDO_COMPLETE` at attempt 0 with 805 draws, the last 573 labelled `fill` (measured with a `-O2` driver without `RANDO_FAULTS`); `baseline all` is in `claims.txt`.
- **H-11-3 — Fill state. Holds, one branch settled.** `RandoFillState` (`rando_filler.h`: a `uint8_t` order per location and a `uint16_t` count, about 240 bytes) is a local of `run_attempt`, not a field of `RandoGen`. So it is stack, counted by the stack peak, and the probe reports it apart. The filler's other working state is on the stack of `fill_items_in_locations` (2 544-byte frame at host `-O2`) and its callers; the library has no writable data or bss.
- **H-11-4 — Host-only parts. Holds.** `playthrough.c` (static shadow `RandoWorld` and static sphere arrays) and `spoiler_json.c` are in `rando/host`, so they are outside the library size and the stack peak.
- **H-11-5 — Fault injection. Holds.** `RandoGen` gains `fault`, `fault_attempts` and `fault_fired` under `#ifdef RANDO_FAULTS` (`RandoGen` 3 912 bytes without them on the host). `tests/rando/Makefile` puts `-DRANDO_FAULTS` in `CFLAGS`, so every host object has them, the `-O2` e2e objects included. The measure build therefore compiles its own library and host objects without it, and the step note gives both sizes.
- **H-11-6 — Speed. Fails, harmlessly: faster than assumed.** One whole baseline generation takes about 3.5 ms at `-O2` (best of five; `dungeon_items-full` 4.8 ms), below the assumed 5 to 20 ms. Changed: `MEASURE_SWEEP` defaults to 1 024 instead of 256 (about 4 s), and the defaults' time estimate is updated.
- **H-11-7 — Separate `-O2` object directory. Holds.** The e2e generator's library and host objects are built at `-O2` in `build/o2/` (`GEN_DIR`), with the common `CFLAGS` and so with `RANDO_FAULTS`. The measure target uses a third directory and shares no object with `build/obj`, `build/host` or `build/o2`.
- **H-11-8 — No new libc dependency. Holds.** The library still includes only `assert.h`, `stdbool.h`, `stddef.h`, `stdint.h` and `string.h`, and calls only `memcpy`, `memmove`, `strcmp` and the assert handler. The clang ARM proxy still compiles every source with the two-header shim.
- **H-11-9 — Stage labels. Holds.** Baseline's 573 fill draws are labelled `fill`, `isWinnable` draws nothing, and the per-stage counts sum to 805.
- **H-11-10 — Goldens unchanged. Holds.** `git diff 54ec1a5 defe4d9` touches no file under `tests/rando/cases/` and not `stages.txt`; every case has `attempts` 0, and the retry-seed set is empty.

## Out of Scope

- Fixing any figure that is over budget. The step note names the culprit; a fix is its own step or goes to roadmap item 2.
- An ARM9 build of the game with `rando/` linked in, any run on hardware or an emulator (CLAUDE.md: never launch the game), and reading zelda3's real ARM9 memory map. Roadmap item 2 replaces A-1 and A-2 with real figures.
- Installing devkitPro in this container.
- Measuring the host-only parts (spoiler writer, playthrough, case reader, trace writer).
- Profiling or optimising the fill, the world queries or the playthrough.
- A static call-graph maximum of the stack (see the stack frames decision).
- Adding `measure` to CI, or making any figure a pass/fail gate.
- Phase 4 step 1 itself: it reruns this target after Phase 3 and records its own note.
- Any change to the library, the goldens, `claims.txt`, `stages.txt`, `run_e2e.sh` or `selftest.sh`.

## Further Notes

- **Why run time and not static for the stack peak.** About 230 rules sit behind function pointers. A static maximum would need each indirect call site annotated with its possible targets. The painted stack measures the path actually taken, on every supported case, and the frame list shows which functions make up the peak.
- **Why clang as an ARM proxy.** The figures that matter on the DS (pointer size, the ARM instruction set, frame layout without x86-64's red zone or vector spills) need an ARM32 compiler, and devkitPro is absent here. clang produces ARMv5TE code from the same sources with a two-header shim. It is not devkitARM's gcc, so its sizes are an estimate; when a human runs `measure` with devkitPro installed, the devkitARM section replaces it.
- **Why a size probe instead of a printf program.** A cross object cannot run here. A global sized by `sizeof`, read back with `nm -S`, gives the same figure on every toolchain without running anything.
- **Why the C run's draws are reported for unclaimed cases too.** Phase 3 starts from these per-case outcomes. A case whose C draw count already equals the golden's is a cheap claim; one that differs points to its row.
- **Material for Phase 4 step 1.** The same target, rerun after Phase 3, gives a figure-by-figure diff against this note.
