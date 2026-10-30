# Randomizer — C port of the ALttPR generator

## 1. Goal

An ALttP randomizer built into `lttp-ds.nds`. The DS generates the seed at runtime;
no patched ROM and no PC tool are needed.

Why not just load an alttpr.com ROM: this port runs zelda3, a C reimplementation. The ROM is
only read for data (`restool.py` checks its SHA-1), and the 65816 code a randomizer injects
would never run. So we port the generator itself to C.

Roadmap:

1. **C port of the ALttPR generator, validated by end-to-end tests** (this plan).
2. **Integration into the NDS code** (a later plan). On a new file, choose Vanilla or Randomized.
   Item gives go through a location → item table: chests (`kDungeonRoomChests`), the 28
   hardcoded `Link_ReceiveItem(<id>, 0)` calls (NPCs, bosses, pedestal…), and overworld items.
   The slot stores only the seed and options, a few bytes; the placement is regenerated on load.
3. **Options menu** for the randomizer settings (a later plan). Configs that cannot be generated
   must not be selectable.

## 2. Rules

### Reference and oracle

- **Reference:** [`sporchia/alttp_vt_randomizer`](https://github.com/sporchia/alttp_vt_randomizer)
  ("VT", after Veetorp, its author), the PHP/Laravel generator behind alttpr.com. Pinned to
  commit `219fcafd029dab597b8db400efafd8f56f8b4edb` (2024-02-18). Goldens are generated with PHP 8.3.6 and
  Composer 2.8.12.
- **bcmath:** VT's own `Seed::boot()` (Hashids) needs bcmath or gmp. Without them, `composer install`
  runs with `--ignore-platform-req=ext-bcmath` and the wrapper pre-boots `Seed` once, tolerating
  that exception: it only registers a database listener and draws nothing.
- **Why VT can be made deterministic:** all of its randomness goes through one function,
  `get_random_int()` in `app/Helpers/number.php`. `fy_shuffle()` and `randomCollection()` both
  call it, and so do about 60 other call sites. Today it calls `random_int()` (a CSPRNG), so the
  output cannot be replayed. Every call uses a small range (`0..7`, `0..count-1`, `0..0x100`, …).
- **`vt.patch`** changes VT in two ways:
  1. `get_random_int()` uses the shared deterministic PRNG (below).
  2. `setTexts()`, `randomizeCredits()` and `HintService` are commented out. They run *after* the
     fill (`Randomizer::randomize()`, `app/Randomizer.php:69-87`), so they have no effect on
     item placement, and the C port skips them.
- **What the C port must reproduce**, because it draws from the RNG before or during the fill:
  shops, medallions, boss placement, prizes (`fillPrizes`, with its 5 internal attempts),
  fountains, prize packs, the item pool, `RandomAssumed` fill, and `isWinnable()`.
- **Out of scope:** entrance shuffle (VT shells out to the Python ER for it), enemizer (boss,
  enemy and pot shuffle, enemy damage and health), hints, texts, credits; NDS integration and
  the options menu (roadmap items 2 and 3).

### Shared PRNG (PHP and C)

- **xoshiro128\*\***, seeded through splitmix32. Only 32-bit operations, masked with
  `& 0xFFFFFFFF`: PHP has no unsigned 64-bit integers, and an overflowing multiplication silently
  becomes a float.
- `get_random_int(min, max) = min + next() % (max - min + 1)`. The modulo bias is negligible and
  identical on both sides.
- The seed is a `uint32`.

### Retry contract

VT never retries on its own: it throws an exception (`RandomAssumed.php:69,113`, prize placement)
or the caller rejects the result with `Game Unwinnable`. The DS cannot show an error, so both the
oracle (a small PHP wrapper around VT) and the C port use the same loop:

```
attempt = 0
loop:
    prng_seed(splitmix32(seed + attempt))   // fresh RNG for each attempt
    world = new world
    try: randomize(); if !isWinnable() → fail
    success → stop
    failure (exception or unwinnable) → attempt++, start over
```

- There is no cap. Each attempt is independent, and in practice it converges within a few.
- The golden records `attempts`. The C port must succeed on the same attempt.
- The tests bound the loop, never the shipped generator: in e2e, the C port stops after the
  golden's `attempts`, and a case still failing then is `DIVERGENT`; the oracle wrapper has a
  generous watchdog, and a case that reaches it goes to the step note and `guideline`. An
  unattended run never hangs on a loop that cannot succeed.
- Each attempt's RNG trace starts clean, so a divergence reads as "attempt N, draw #K".

### Test matrix

- **Baseline:** the VT `default` preset (`config/alttp.php`), with seed `1`.
- **One case for each non-default value of each option**, every other option left at baseline.
- **Plus** a few seeds chosen so that at least one attempt fails first, to exercise the retry.

| Option | Baseline | Other values (1 case each) | # |
|---|---|---|---|
| `glitches` | `none` | `overworld_glitches`, `hybrid_major_glitches`, `major_glitches`, `no_logic` | 4 |
| `item_placement` | `advanced` | `basic` | 1 |
| `dungeon_items` | `standard` | `mc`, `mcs`, `full` | 3 |
| `accessibility` | `items` | `locations`, `none` | 2 |
| `goal` | `ganon` | `fast_ganon`, `dungeons`, `pedestal`, `triforce-hunt`, `ganonhunt`, `completionist` | 6 |
| `crystals_tower` | `7` | `0`…`6`, `random` | 8 |
| `crystals_ganon` | `7` | `0`…`6`, `random` | 8 |
| `state` | `open` | `standard`, `inverted`, `retro` | 3 |
| `weapons` | `randomized` | `assured`, `vanilla`, `swordless` | 3 |
| `item_pool` | `normal` | `hard`, `expert`, `crowd_control` | 3 |
| `item_functionality` | `normal` | `hard`, `expert` | 2 |
| **Total** | | | **43 + 1 baseline = 44 cases** |

Interactions between options are left for a second pass, guided by C code coverage.

### Convergence gate

Cases go green in the order of § 3, not all at once. Each step names the cases it brings to green
(its *claimed* cases). A step may claim a case only up to a named stage: the trace must then
match VT's up to that stage's last draw, and anything past it stays pending. The e2e gate is:
every claim of a finished step holds, and nothing that held before has broken. A case no finished
step has claimed yet is reported as pending, not as `DIVERGENT`, so it never sends `check-e2e`
into `fix-bug`.

Claims only ratchet up: a claim never moves down, and each step raises the earlier steps' claims
to its own stage when they hold.

`make -C tests/rando e2e` stays under 0.5 s on the host, and every step note records its time. A
step whose new claims or checks would break that budget first restructures the gate, in its own
commit, so that the gate's wall time no longer grows with what it pays for: per-claim checks
(done in 2.3.3) or, at `all`, one generator run per claim (run claims in parallel with output
kept in case order, and/or make the generator cheaper); `make selftest` proves the verdicts and
messages unchanged.

`make test` gets only hand-written cases; ported location tests replay through `make logic`
fixtures, as in step 2.2. Every step note records the `make test` time against step 2.5's
figure. Steps that change logic record `make measure`'s maximum host time per case.

### Comparison

- The output is VT's spoiler JSON (`World::getSpoiler()`). Before diffing, both sides are
  normalized: keys sorted, non-deterministic fields (hash, date, build) dropped.
- `run_e2e.sh` prints the first location that differs.
- Both sides can write `trace.txt`, one line per draw: `attempt draw# stage min max -> r`, where
  `stage` names the VT RNG consumer that made the draw. When outputs diverge, diffing the traces
  shows the first draw where they drift apart, and in which stage.
- **Unit tests:** the PRNG and `fy_shuffle` against VT on N draws, and the per-location logic
  tests ported from VT's `tests/NoGlitches`, `tests/Inverted`, `tests/OverworldGlitches`, ….

### C library constraints

- C99, no `malloc`. All state lives in one `RandoWorld` struct with fixed-size arrays (about 250
  locations, about 200 items, regions). Names are `const char*` tables. The ARM9 has 4 MB, and
  zelda3 already uses much of it.
- Platform-independent: no libnds. The same code is built by host gcc for the tests and linked
  into the ARM9 build.
- The core outputs a `location_id → item_id` table. `spoiler_json.c` is compiled only for the
  host build.

### Layout

```
rando/
  include/rando.h            public API: rando_generate(config, seed, out)
  src/                       prng.c, world.c, regions/*.c, filler.c, spoiler_json.c (host only) ...
tests/rando/
  Makefile                   host gcc only; the root Makefile needs devkitPro
  oracle/
    vt.patch                 deterministic get_random_int + texts/credits/hints commented out
    gen_golden.sh            clones VT at the pinned commit, applies the patch, composer install,
                             runs the retry wrapper and writes cases/*/
  cases/<name>/              config.json, seed, spoiler.json, trace.txt, attempts (committed)
  unit/                      prng, fy_shuffle, location rules
  run_e2e.sh                 diffs every case and prints the first divergence
.github/workflows/rando-tests.yml   runs `make -C tests/rando` on ubuntu (gcc only, no PHP)
```

- VT is not vendored. Only the patch and the script live in this repo.
- Goldens are **committed**: running the tests needs only gcc, and PHP is needed only to
  regenerate them.

## 3. Phases and steps

### Phase 1 — Oracle

No C port yet.

1. **The bare harness, then `vt.patch`, the retry wrapper and `gen_golden.sh`, proven deterministic.** Generate the
   baseline case and check it by hand (a winnable spoiler, `attempts` recorded). Then generate it
   twice in fresh runs: spoilers and traces must be byte-identical. Record the PHP and composer
   versions next to the pinned VT commit. The step note lists the baseline's RNG consumers in
   draw order, with the number of draws in each. The harness comes first, because every step's
   verify, commit and `check-e2e` run it: `make -C tests/rando test` (no unit tests yet) and
   `e2e` (every case pending, since none is claimed).
2. **Every golden committed:** the 44 matrix cases plus the retry seeds (at least one, each with
   `attempts` > 0). Every case generated twice in fresh runs, byte-identical both times.

At the end of this phase, the `plan-review` consultation expands Phase 2 step 3 into numbered
steps, one per consumer listed in step 1's note.

### Phase 2 — C port, baseline

Each step claims the baseline case up to its stage (§ Convergence gate); full spoiler equality
is required only at the last step. Stages follow the order in which
VT draws from the RNG, read from `Randomizer::randomize()`.

1. **PRNG and `fy_shuffle`**, the claim and stage-prefix checking of the convergence gate, and
   CI. A stage claim also checks the spoiler entries its stages determine (step 3 names them).
   Also resolves Phase 1's warnings on the files it rewrites: one shared case discovery for
   `run_e2e.sh` and `gen_golden.sh`, and a `prng_vectors.txt` header naming what verifies it. Verified by unit tests against VT on N draws.
2. **World, regions and location rules** for the baseline settings. Verified by the
   `tests/NoGlitches` location tests ported from VT.
3. **Pre-fill RNG consumers**, one sub-step per stage in VT's draw order (step 1.1 note). Each
   claims the baseline up to its stage and checks the spoiler entries listed. Step 3 has no note
   of its own: its sub-steps get `### Step 2.3.k` notes, and it is done when 2.3.6 has one.
   1. **`getAdvancementItems`**, plus the generator driver, per-attempt reseeding and the C
      `trace.txt` writer the gate calls.
   2. **`setMedallions`**, with `setShops`' baseline path (no draw). Checks the medallions and
      `Shops`.
   3. **`fillPrizes`** with its 5 internal attempts, with `placeBosses`' baseline path (no draw).
      Checks the dungeon prizes and `Bosses`.
   4. **`setFountains`.** Checks the two fountain bottles.
   5. **`shufflePrizePacks`.** Trace only: prize packs are not in the spoiler.
   6. **`prepareWorld`'s own work:** the rest of the item pool and its shuffles. Checks that the
      pool's items equal those the golden spoiler places, minus the pre-filled ones.
4. **`RandomAssumed` fill, `isWinnable()` and the retry loop.** Claims the baseline case: full
   spoiler equality and the same `attempts`; also claims the retry-seed cases. Every earlier
   stage claim is raised to `all` if it holds; one that does not stays at its stage and the step
   note names it for its Phase 3 row. No golden has a
   failed attempt (step 1.2 note), so the loop is also verified by the identity "attempt k of
   seed s is attempt 0 of seed s+k": seed 0 with attempt 0 forced to fail (host-only injection,
   once mid-fill, once at `isWinnable()`) must reproduce the baseline golden, with `attempts` 1.
5. **Measure on the host**, as Phase 4 step 1 does, now that the state layout and the fill are
   fixed and before options build on them.

### Phase 3 — Options

One step per option row of the test matrix. Each claims that row's cases and ports the location
tests VT has for it (`tests/Inverted`, `tests/OverworldGlitches`, …). The `glitches` and `state`
rows, whose values each bring their own logic or world, get one step per value instead. A row
step is done when its cases are claimed at `all` and its listed tests are ported.

Execution order, as listed (numbers match `progress.md`):

0. **Claim what is green.** First the e2e budget restructure (§ Convergence gate), then every
   case that holds claimed at `all`. The note lists, per row, the VT test files and methods that
   row owes, enumerated from VT's `tests/` tree. A row whose cases are all claimed and whose list
   is empty gets its `### Step 3.k` note here and is ticked, with no PRD. Then, under those
   claims, one refactor commit (`test`, `logic`, `e2e` unchanged) merging the library
   duplications Phase 2's warn-review found on Phase 3's paths: the map/compass-not-wild test,
   `randomCollection`, the collect-until-stable loop, the list find/remove logic, and the inline
   config defaults, shop prices and bottle index.
6. **`dungeon_items`**, before any other logic step: it settles the rule shape (`dungeon_items-mcs`
   needs VT's nested rules to read the shadow world, step 2.4 note) before glitch and inverted
   rules are written. It also removes the host playthrough's static state.
1–5, 7–16. The other rows, in table order.

Phase 2's other host-only warn-review warnings (`rando_gen.c` statics and duplicated file
handling, `spoiler_json.c` scratch buffers, the watchdog defined three times, `measure.sh`
constants) are left open for the pull request.

### Phase 4 — Fits on the DS

1. **Measure on the host**, outside the fast `test` and `e2e` targets: the size of the whole
   generator state, peak stack use, and the attempts and RNG draws per case (from the goldens
   and traces).
   Record the figures in the step note, against the ARM9 budget, before roadmap item 2 starts.

## 4. Done when

- Every case in `tests/rando/cases/` (the 44 matrix cases and the retry seeds) green with
  `make -C tests/rando e2e`, none pending.
- `make -C tests/rando test` green, and the CI workflow green.
- Phase 4's figures recorded.

## 5. Open questions

None.

## 6. Step notes

### Step 1.1

- Harness: `make -C tests/rando test` (empty `UNIT_TESTS`) and `e2e` (`PENDING` per case), both
  PHP-free, milliseconds.
- `vt.patch`: xoshiro128\*\*/splitmix32 PRNG checked against 25 known answers from a C reference;
  every draw traced as `attempt draw# stage min max -> r`, the stage being the method called by
  `Randomizer::randomize()` or `prepareWorld()`. `get_random_int()` takes `int` parameters, because
  VT passes a `floor()` float (`getGanonsTowerJunkFillRange`).
- `gen_case.php` (retry wrapper, 1000-attempt watchdog) and `gen_golden.sh [--fresh] [--out dir]
  [case...]`. Random crystal counts are drawn ganon first, then tower, as `prepSeed` does (the PRD
  said the reverse; VT wins).
- Baseline (seed 1): `attempts` 0, 805 draws, 230 locations filled, 12 spheres ending on Ganon.
  Two `--fresh` runs are byte-identical to each other and to the golden.
- Baseline RNG consumers in draw order: `getAdvancementItems` 4, `setMedallions` 2, `fillPrizes`
  64, `setFountains` 2, `shufflePrizePacks` 7, `prepareWorld` (its own shuffles) 153, `fill` 573.
  `setShops`, `placeBosses` and `crystals` draw nothing in this preset. `getAdvancementItems` and
  `prepareWorld` are consumers § 2 does not list.

### Step 1.2

- 43 matrix cases added (`<option>-<value>`, seed 1, config one line off baseline's) and their
  goldens generated: two `--fresh` runs byte-identical to each other and to the committed files;
  baseline unchanged.
- Every case succeeds on attempt 0. Draws: 773 (`item_placement-basic`) to 924
  (`dungeon_items-full`); baseline 805. `item_pool-hard` and `-expert` have baseline's trace (VT at
  this commit only changes shops and overflow for them).
- Stages: 41 cases have baseline's sequence. `crystals_*-random` add `crystals` first;
  `state-retro` adds `setShops` after `getAdvancementItems`. `placeBosses` draws in no case.
- **No retry-seed case** (`guideline`): bounded scan, baseline config seeds 2–8000 and each of the
  43 configs seeds 2–200 (16 556 generations), all succeed on attempt 0. VT's failure paths
  (`Cannot Place Prize`, unwinnable) exist but never fire here. The plan's "at least one retry
  seed" is unmet; Phase 2 step 4's retry-seed claim is empty.

### Step 2.1

- C PRNG (`rando/include/rando_prng.h`, `rando/src/prng.c`): caller-owned `RandoPrng`, optional
  per-draw observer, `rando_fy_shuffle`. `test_prng` checks 31 VT-verified vectors (9
  `fy_shuffle`) and replays all 35 729 draws of the 44 golden traces.
- Gate: `claims.txt` (empty), `stages.txt` (VT order: `crystals`, `getAdvancementItems`,
  `setShops`, `setMedallions`, `placeBosses`, `fillPrizes`, `setFountains`,
  `shufflePrizePacks`, `prepareWorld`, `fill` — `placeBosses` precedes `fillPrizes`, the PRD had
  it reversed). Stage claims compare the trace prefix, then the due `section`/`keys`/`pool`
  checks on the C partial `spoiler.json` and `pool.txt`; `all` claims compare everything.
  Generator contract: `$RANDO_GEN <case_dir> <out_dir>`, default `build/rando_gen`.
- Case discovery shared by `run_e2e.sh` and `gen_golden.sh` (`cases.sh`).
- Deviation: the gate fixtures run in a separate `make -C tests/rando selftest` (~2 s), not in
  `make test` (kept at ~50 ms, CLAUDE.md). CI (`rando-tests.yml`) runs `test`, `selftest`, `e2e`.
- No case claimed: e2e is 44 pending.

### Step 2.2

- `tests/rando/oracle/extract_logic.php` runs every `tests/NoGlitches` method inside VT (stub
  `TestCase`, each row replayed in a fresh VT world before writing) into three committed fixtures:
  world structure, predicate/`canBeat` vectors under 3 configs, 2 969 rows plus `collectItems`
  scenarios. VT agrees with its own expected value on every row. `gen_golden.sh` runs it.
- C: `rando_items.h` (179 items, VT's two views, 24 predicates), `rando_bosses.h`,
  `rando_config.h`, `rando_world.h` (Standard and Open: 26 regions, 236 locations, 41 shops),
  every region in `rando/src/regions/`. `RandoWorld` is 1 996 bytes, ids only; the library is
  85 KB text, 21 KB const data, no bss.
- `make logic` replays all 2 969 rows, 120 912 predicate and 32 976 `canBeat` values, and all 21
  `collectItems` steps; `make test` stays under 50 ms with hand-written cases.
- Deviations: the shops' default inventory and requirements are in world init here, not in
  2.3.2 (the PRD assumed shops empty; VT's `collectItems` collects them). Kept VT quirks:
  `heartCount` counts a Piece of Heart as a full heart; Open's escape skips its parent's
  `initalize`. Turtle Rock's middle/lower paths need glitch flags (untested until Phase 3); two
  Ganon's Tower boss slots are only distinguishable once `placeBosses` exists (2.3.3).

### Step 2.3.1

- Oracle: `oracle_case.php` shares case loading and per-attempt setup between `gen_case.php`
  and `dump_pools.php`, which writes VT's four `getAdvancementItems` lists per case
  (`pool_lists.txt`). A `--fresh --out` run of every case and fixture is byte-identical.
- C: `rando_options.h` and `rando_config_from_options` (no derivation; world init now derives
  VT's `item.pool`, `item.functionality` and hunt-goal branches), `rando_pool.h` (four lists,
  overrides, 216 cap, `getBottle`), `rando.h`/`generate.c` (attempt loop, reseeding, crystals,
  per-stage draw counting, trace hook; `RandoGen` 3.5 KB on the host). Host-only
  `rando/host/` (strict case reader, trace writer, `main/rando_gen.c`).
- `test_generate --fixtures` replays the four lists for 42 cases; `state-inverted` and
  `state-retro` return unsupported before world init and are skipped by name until Phase 3.
- Claims: `baseline`, both `crystals_*-random`, both `item_functionality-*` at
  `getAdvancementItems`; e2e is 5 pass, 39 pending, ~0.1 s (it now runs the generator).
- Not covered: the ganon-before-tower crystal order (no case has both counts random; taken
  from VT's code) and the retry path (no stage fails yet, step 2.4).

### Step 2.3.2

- World: shop kind, default prices, active flag and per-slot prices; `shops_hard_mode` and the
  shield overflow replacement item in the config. `RandoWorld` is 2 652 bytes.
- `rando_prepare.h`: `rando_set_shops` (draw-free path; retro's random path asserts until
  Phase 3) and `rando_set_medallions` (Turtle Rock, then Misery Mire). All 42 supported cases
  match their golden trace through `setMedallions`.
- Host-only `rando/host/spoiler_json.c` writes the region sections and `Shops` in the oracle's
  normalized layout; `rando_gen` writes `spoiler.json` on complete or incomplete runs.
- Gate: `check setShops section Shops`, `check setMedallions keys *Medallion:1`. Eight claims at
  `setMedallions` (the five from 2.3.1 plus `item_pool-hard`, `-expert`, `-crowd_control`): 8
  pass, 36 pending. Breaks of the medallion order and the HardMode clear flag exactly the
  expected cases.
- e2e grew from ~0.1 s to ~0.29 s: the generator takes ~2 ms per case, the rest is the gate's
  per-check awk passes over `spoiler.json` (~25 ms per claim with checks).

### Step 2.3.3

- Gate restructured first (own commit): one awk pass per claim evaluates all due checks; output
  byte-identical, `make selftest` unchanged; e2e 0.28 s → 0.07 s at 8 claims.
- Prologue: `rando_prepare_start_equipment` (VT's default 3 `BossHeartContainer`,
  `BombUpgrade10`, 3 `ArrowUpgrade10` — not in the PRD; without it `item_placement-basic` fails
  every win check), the goal's Triforce and completionist accessibility, the Boots/`RescueZelda`
  pre-collect. The pool-list edit for Boots waits for 2.3.6 (`TODO`).
- `rando_place_bosses` (no-shuffle path), `rando_fill_prizes` (14 locations and 10 prizes
  shuffled, candidates from the end, up to six passes; `Cannot Place Prize` is a false return,
  the generator's first failed-attempt path, unit-tested only). All 42 supported cases match their
  golden trace through `fillPrizes` (70 draws for baseline).
- Spoiler `Bosses`; checks `section Bosses` (placeBosses) and `keys *- Prize:1` (fillPrizes). 13
  claims at `fillPrizes`: 13 pass, 31 pending; e2e ~0.11 s; `make test` ~57 ms.

### Step 2.3.4

- `rando_set_fountains` (Waterfall, then Pyramid; filled `getBottle`; a preset fountain is
  skipped) runs after a successful `fillPrizes`. All 42 supported cases match their golden trace
  through `setFountains`.
- Check `keys *Bottle:1` (exactly the two fountain keys in every golden). The 13 claims move up to
  `setFountains`, none added: 13 pass, 31 pending; e2e ~0.11 s; `make test` ~54 ms.

### Step 2.3.5

- Prize packs in `RandoWorld` (11 packs × 8 drop ids; ROM sprite byte `0xD7 + id`), config
  `custom_prize_packs`, `rando_shuffle_prize_packs` after `setFountains`. Not in the spoiler, so
  `test_prepare` pins baseline's 56 drops against a one-off VT dump (not committed).
- All 42 supported cases match their golden trace through `shufflePrizePacks`. The 13 claims move
  up, none added: 13 pass, 31 pending; e2e ~0.11 s; `make test` ~56 ms. `RandoWorld` 2 744 bytes.

### Step 2.3.6

- Oracle: `dump_pools.php` also writes VT's four lists after `prepareWorld`
  (`prepared_pools.txt`); goldens and existing fixtures byte-identical.
- Gate: the `pool` check counts in-scope entries the C spoiler already fills as pre-fills
  (compared by value, removed from the golden multiset). H-2-10 corrected: the golden's placed
  items exclude `*- Prize:1` too, and three goals (`pedestal`, `triforce-hunt`, `ganonhunt`)
  have more pool items than free locations, which VT leaves unplaced.
- C: `rando_generate_until` (stop after a stage); the Boots edit in `rando_prepare_pre_collect`;
  `rando_prepare_pools` (every branch of VT's `prepareWorld` after `shufflePrizePacks`; four
  new region config fields); `rando_gen` writes `pool.txt`. All 42 supported cases match VT's
  prepared lists and their golden trace through `prepareWorld` (232 draws for baseline).
  `RandoWorld` 2 748 bytes, `RandoGen` 3 896.
- Claims: 17 at `prepareWorld` (six new: `item_placement-basic`, `weapons-assured`,
  `-vanilla`, `dungeon_items-mc`, `-mcs`, `-full`), `goal-pedestal` and `goal-triforce-hunt` at
  `shufflePrizePacks` (surplus pool items). 19 pass, 25 pending; e2e ~0.17 s; `make test` ~62 ms.
- Phase 2 step 3 is done.

### Step 2.4

- World queries (ordered pre-collected list, bare win condition, `collectOtherItems`,
  `isWinnable`, `World::copy`), `rando_filler.h` (`RandomAssumed` line by line, false at VT's two
  exceptions), the fill and `isWinnable` stages, the retry loop, host-only `RANDO_FAULTS`.
  `rando_gen` takes an optional stage argument; the gate passes it for stage claims.
- Host-only spoiler completion: `Equipped`, `meta`, a port of `getPlayThrough`. Baseline's
  spoiler, trace and `attempts` are byte-identical to the golden, also through the retry identity
  (seed 0, attempt 0 failed mid-fill or at `isWinnable`, reproduces baseline at attempt 1).
- Survey at `all` over the 42 supported cases: 40 byte-identical. `dungeon_items-mcs` differs only
  in the playthrough: VT's nested rules (`$this->locations[...]`) read the shadow world while
  pruning; matching it needs rules that take two worlds (~240 rule functions) — its Phase 3 row.
  `glitches-no_logic` diverges at a fill draw (map/compass fillable count) — its Phase 3 row.
- Budget: e2e generator built at -O2 (`build/o2/`). Claims: 18 at `all`, `dungeon_items-mcs` at
  `prepareWorld`; 19 pass, 25 pending; e2e ~0.29 s; `make test` ~80 ms (up from ~60: noisy host,
  but trending up). Cross-attempt state where `World::copy` keeps the collect cache is caught only
  by `make test`; the identity test guards a world not reinitialized per attempt.

### Step 2.5

- `make -C tests/rando measure` (not in CI, never run by `test` or `e2e`): library at -O2/-Os
  without `RANDO_FAULTS`, a clang ARMv5TE proxy (devkitARM absent here), and `rando_measure` on a
  painted 1 MiB thread stack.
- Budget assumptions, to restate in roadmap item 2: A-1 stack peak ≤ 8 KiB (half of a 16 KiB
  DTCM); A-2 code+rodata ≤ 128 KiB and state ≤ 32 KiB, no writable static in the library; A-3
  ≤ 2 s per generation on the DS, host factor ×150 (13 ms on the host); A-4 no separate attempt
  budget.
- Figures: `RandoGen` 3 912 B (ARM 3 892); library code+rodata 80 KB (ARM proxy) to 96 KB (host
  -O2), 0 writable, 0 bss; largest frame 2.6 KB (`rando_prepare_pools`); stack peak 6.2 KB net,
  ARM frames ≤ 1.0× host → within A-1. All 42 supported cases match the golden's attempts, draws
  and per-stage counts. Baseline 3.7 ms; 1 024-seed sweep: no retry, median 4.0 ms, max 6.9 ms
  (DS projection 0.6 s / 1.0 s).
- Over budget: `item_placement-basic` takes ~20 ms on the host (A-3 is 13 ms; ~3 s projected on
  the DS). To look at before roadmap item 2.
- Thread overhead is the entry frame depth, not a painted empty run (glibc writes below the
  entry frame before the job runs).

### Step 3.0

- Gate: `run_e2e.sh` validates claims first (exit 2 before any generator), builds one per-case
  plan, creates every output directory with one `mkdir`, launches every claim as one background
  job (generator then claim awk, outputs per case index), then one awk prints the report in case
  order with each claim's stderr before its verdict. No job cap. Report, summary and exit status
  unchanged; `make selftest` adds slow-first-case, two-divergent and generator-stderr fixtures.
  A 42-claim run: ~0.61 s serial → ~0.18 s.
- Claims: 40 at `all`, `dungeon_items-mcs` and `glitches-no_logic` (fill diverges at draw 457)
  at `prepareWorld`, `state-inverted` and `state-retro` pending. 42 pass, 0 divergent, 2
  pending; e2e ~0.16 s; `make test` ~80 ms (step 2.5: ~80 ms).
- Owed VT tests per row (an interaction between two rows goes to the later one):

  | Row | Owed | Status |
  |---|---|---|
  | 3.1 overworld_glitches | `tests/OverworldGlitches` (23 files, 41 methods) | open |
  | 3.2 hybrid_major_glitches | none (no VT directory) | closed |
  | 3.3 major_glitches | `tests/MajorGlitches` (23 files, 41 methods) | open |
  | 3.4 no_logic | none (no VT directory) | open: case at `prepareWorld` |
  | 3.5 item_placement | none | closed |
  | 3.6 dungeon_items | `WorldTest::testSetVanillaDungeonItems` (8 rows), top-level `RegionTest::testRegionLockedItems` | open |
  | 3.7 accessibility | none | closed |
  | 3.8 goal | none | closed |
  | 3.9 crystals_tower | none | closed |
  | 3.10 crystals_ganon | none | closed |
  | 3.11 state: standard | none (`tests/NoGlitches`, ported in 2.2) | closed |
  | 3.12 state: inverted | `tests/Inverted` (22, 43), `tests/InvertedOverworldGlitches` (22, 38), `tests/InvertedMajorGlitches` (22, 40) | open |
  | 3.13 state: retro | `RetroBowTest::testLocation` (32 rows) | open |
  | 3.14 weapons | `RandomizerTest::testVanillaSwordsSet`, `::testVanillaSwordsSetWithPedestalGoal` | open |
  | 3.15 item_pool | none | closed |
  | 3.16 item_functionality | none (`ItemCollectionTest`'s functionality tests in `test_items.c`) | closed |

- Owed by no row: `tests/NoGlitches` (2.2) and `Support/ItemCollectionTest` (`test_items.c`),
  already ported; `RandomizerTest`'s `test{Crystals,Pendants}NotRandomizedByConfig*` (vary
  `prize.shuffle*`, not in the matrix), `*BossShuffle` (enemizer, out of scope),
  `testCompletionistItemCount` (read only by `writeToRom`); `LightWorldTest`, `RomTest`,
  `Support/{Flips,Lz2,XXTEA}Test` (ROM and patch formats); `WorldTest::testGetRegionDoesntExist`
  (PHP exception; C uses ids); `ItemTest` (names, ROM bytes; item table checked by `make logic`);
  `HelpersTest` (PHP helpers); `Support/CollectionTest` (covered by `test_filler` and the `all`
  claims); `Unit/Http/…`, `tests/Browser`, `tests/js` (web front end).
- Nine rows closed below with no PRD. The duplications refactor (chunk 4) follows, with
  `make measure`'s figure.
- Refactor (own commit, outputs byte-identical): one dungeon-item-confined check, one public
  `rando_random_collection`, one `rando_world_collect_until_stable`, pool append/find/remove
  helpers, `derive_config` value tables, shop prices from the table, bottle bound from its table.
  `make measure` after: ARM code+rodata 79.9 KB, stack peak 6 280 B, max case
  (`item_placement-basic`) ~20 ms — +8 % from code placement, not logic (equal with aligned
  loops); `make test` ~85 ms.

### Step 3.2

- `glitches-hybrid_major_glitches` claimed at `all` in 3.0. Owes no VT test: VT has no
  HybridMajorGlitches directory. No PRD.

### Step 3.5

- `item_placement-basic` claimed at `all` in 3.0 (step 2.4). Owes no VT test. No PRD.

### Step 3.7

- `accessibility-locations`, `-none` claimed at `all` in 3.0. Owes no VT test. No PRD.

### Step 3.8

- All six goal cases claimed at `all` in 3.0. Owes no VT test: `testCompletionistItemCount` is
  ROM-only, `testVanillaSwordsSetWithPedestalGoal` goes to 3.14. No PRD.

### Step 3.9

- `crystals_tower-0` … `-6`, `-random` claimed at `all` in 3.0. Owes no VT test. No PRD.

### Step 3.10

- `crystals_ganon-0` … `-6`, `-random` claimed at `all` in 3.0. Owes no VT test. No PRD.

### Step 3.11

- `state-standard` claimed at `all` in 3.0. Owes no VT test: the Standard world's tests are
  `tests/NoGlitches`, ported in 2.2. No PRD.

### Step 3.15

- `item_pool-hard`, `-expert`, `-crowd_control` claimed at `all` in 3.0. Owes no VT test. No PRD.

### Step 3.16

- `item_functionality-hard`, `-expert` claimed at `all` in 3.0. Owes no VT test:
  `ItemCollectionTest`'s functionality tests are already in `test_items.c`. No PRD.

### Step 3.6

- Access rules take VT's `$locations` as a third parameter (`$locations` reads and region entries
  read it; `$this->locations` reads and nested `$this->locations[X]->canAccess` read the rule's own
  world); `rando_world_can_access_with`. The host playthrough checks shadow locations against the
  real world in the bubbling pass and replay, as VT does; its static state is now a local struct.
- `dungeon_items-mcs` claimed at `all`: 41 at `all`, `glitches-no_logic` at `prepareWorld`; 42
  pass, 2 pending.
- Owed VT tests ported as fixture rows replayed by `make logic`: `RegionTest::testRegionLockedItems`
  (90 rows, not 91) and `WorldTest::testSetVanillaDungeonItems` (8 rows, new `vanilla_dungeon_items`
  query); the C port already matched every row.
- `make measure`: ARM code+rodata 80.4 KB, stack peak 6 328 B, max case 19.8 ms
  (`item_placement-basic`, still over A-3), sweep max 8.0 ms; `make test` 65–82 ms; e2e ~0.17 s.
