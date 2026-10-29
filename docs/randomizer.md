# Randomizer — design

Goal: an ALttP randomizer built into `lttp-ds.nds`. The DS generates the seed at runtime;
no patched ROM and no PC tool are needed.

Why not just load an alttpr.com ROM: this port runs zelda3, a C reimplementation. The ROM is
only read for data (`restool.py` checks its SHA-1), and the 65816 code a randomizer injects
would never run. So we port the generator itself to C.

## Roadmap

1. **C port of the ALttPR generator, validated by end-to-end tests** (this document).
2. **Integration into the NDS code.** On a new file, choose Vanilla or Randomized. Item gives go
   through a location → item table: chests (`kDungeonRoomChests`), the 28 hardcoded
   `Link_ReceiveItem(<id>, 0)` calls (NPCs, bosses, pedestal…), and overworld items. The slot
   stores only the seed and options, a few bytes; the placement is regenerated on load.
3. **Options menu** for the randomizer settings. Configs that cannot be generated must not be
   selectable.

## Step 1 — reference and oracle

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
  enemy and pot shuffle, enemy damage and health), hints, texts, credits.

## Shared PRNG (PHP and C)

- **xoshiro128\*\***, seeded through splitmix32. Only 32-bit operations, masked with
  `& 0xFFFFFFFF`: PHP has no unsigned 64-bit integers, and an overflowing multiplication silently
  becomes a float.
- `get_random_int(min, max) = min + next() % (max - min + 1)`. The modulo bias is negligible and
  identical on both sides.
- The seed is a `uint32`.

## Retry contract

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
- Each attempt's RNG trace starts clean, so a divergence reads as "attempt N, draw #K".

## Test matrix

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

**Convergence order:** the PRNG and `fy_shuffle` unit tests, then the baseline, then one option at
a time.

## Comparison

- The output is VT's spoiler JSON (`World::getSpoiler()`). Before diffing, both sides are
  normalized: keys sorted, non-deterministic fields (hash, date, build) dropped.
- `run_e2e.sh` prints the first location that differs.
- Both sides can write `trace.txt`, one line per draw: `attempt draw# min max -> r`. When outputs
  diverge, diffing the traces shows the first draw where they drift apart.
- **Unit tests:** the PRNG and `fy_shuffle` against VT on N draws, and the per-location logic
  tests ported from VT's `tests/NoGlitches`, `tests/Inverted`, `tests/OverworldGlitches`, ….

## C library constraints

- C99, no `malloc`. All state lives in one `RandoWorld` struct with fixed-size arrays (about 250
  locations, about 200 items, regions). Names are `const char*` tables. The ARM9 has 4 MB, and
  zelda3 already uses much of it.
- Platform-independent: no libnds. The same code is built by host gcc for the tests and linked
  into the ARM9 build.
- The core outputs a `location_id → item_id` table. `spoiler_json.c` is compiled only for the
  host build.

## Layout

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
  cases/<name>/              config.json, seed, spoiler.json, attempts (committed)
  unit/                      prng, fy_shuffle, location rules
  run_e2e.sh                 diffs every case and prints the first divergence
.github/workflows/rando-tests.yml   runs `make -C tests/rando` on ubuntu (gcc only, no PHP)
```

- VT is not vendored. Only the patch and the script live in this repo.
- Goldens are **committed**: running the tests needs only gcc, and PHP is needed only to
  regenerate them.

## First deliverable

The oracle: `vt.patch`, the retry wrapper, `gen_golden.sh`, and the 44 committed goldens.
No C yet.
