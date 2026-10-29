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
  commit `219fcafd029dab597b8db400efafd8f56f8b4edb` (2024-02-18).
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
   CI. Verified by unit tests against VT on N draws.
2. **World, regions and location rules** for the baseline settings. Verified by the
   `tests/NoGlitches` location tests ported from VT.
3. **Pre-fill RNG consumers** (shops, medallions, bosses, prizes with their internal attempts,
   fountains, prize packs, item pool), one step per consumer in VT's draw order, numbered at the
   end of Phase 1. Each claims the baseline up to its stage.
4. **`RandomAssumed` fill, `isWinnable()` and the retry loop.** Claims the baseline case: full
   spoiler equality and the same `attempts`; also claims the retry-seed cases.
5. **Measure on the host**, as Phase 4 step 1 does, now that the state layout and the fill are
   fixed and before options build on them.

### Phase 3 — Options

One step per option row of the test matrix, in table order. Each claims that row's cases and
ports the location tests VT has for it (`tests/Inverted`, `tests/OverworldGlitches`, …). The
`glitches` and `state` rows, whose values each bring their own logic or world, get one step per
value instead.

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
