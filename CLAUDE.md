# Project notes

## The port

- A Nintendo DS port of [snesrev/zelda3](https://github.com/snesrev/zelda3), the C reimplementation of *A Link to the Past*. The game logic is in the `vendor/zelda3` submodule; the DS host (video, HUD, bottom screen, save, audio) is in `platform/nds/`. See `README.md`.
- A randomizer is being built: a C port of the ALttPR generator (VT), validated against VT itself. The design is in `docs/randomizer.md`.
- **The rando goldens are the source of truth, not the unit tests.** Every case in `tests/rando/cases/` was written by the patched VT oracle; the C port must reproduce it byte for byte. A change that keeps every golden green may break a unit test that froze the old behavior: update that test.

## Layout and naming

- `vendor/zelda3/` is upstream code (a submodule): change it only when the port needs it, never for style.
- `platform/nds/` is the DS host. `rando/` is the randomizer library: pure C99, no libnds, no `malloc`, built both by host gcc and for the ARM9.
- `tests/rando/` holds the randomizer tests: `unit/test_<module>.c`, one per module, registered in `tests/rando/Makefile`'s `UNIT_TESTS`; the e2e cases in `cases/`; the VT oracle in `oracle/`.

## Language

Everything written in the repository is in English: code, comments, docs, commit messages, PR titles and descriptions, PRDs, skills. Older commits and PRs stay as they are.

## What we don't do

- **Never merge a pull request**, in any way (GitHub tool, `gh`, auto-merge). Merging is the user's job, always; denied in `.claude/settings.json`.
- **Never make `make -C tests/rando test` or `make -C tests/rando e2e` slower**: the loop must stay fast. A slow check goes in a separate target.
- **Never regenerate a golden to get green.** `tests/rando/oracle/gen_golden.sh` is run only when VT, its patch or the case list changes on purpose, in its own commit.
- **Never launch the game.** No emulator, no window: an agent has neither eyes nor a gamepad. Verification is the host tests; playing the `.nds` is a human's job.
- **Never work on or push a `claude/*` branch.** A branch is named after what it delivers, once the first commit shows it.

## Build

- `python3 build.py` builds `lttp-ds.nds`. It needs devkitPro and a USA `zelda3.sfc` (`README.md`).
- `make -C tests/rando test` is the fast loop: the randomizer's unit tests, host gcc only. `make -C tests/rando e2e` diffs every case of `tests/rando/cases/` against the C port and prints one `DIVERGENT <case>` line per failure. Neither needs devkitPro, PHP or the ROM.

## Docs

`docs/` only describes what is true or still to do. Anything finished — a plan, a study, an implemented PRD — is deleted: git is the history.

## Comments

A comment stays only if it carries one of two things: a trap someone would reintroduce (an `assert` with a message, otherwise 40 words at most), or a C contract the name doesn't state (who frees the memory, what the function returns when the field is missing). `TODO` is allowed. In a `Makefile`, only help for reading a command stays. Everything else goes in the commit message. This applies to code written in this repository, not to `vendor/zelda3`.

To rework a file written before these rules: skill `rework-comments`.
