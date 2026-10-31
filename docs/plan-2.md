# Plan 2 — On-device ALttPR ROM generator for the Miyoo Mini Flip (OnionOS)

## 1. Goal

A user plays *A Link to the Past* randomizer seeds on a **Miyoo Mini Flip running OnionOS**, generated **on the console itself**, with no PC and no network:

- An Onion app (`App/ALttPR/`) reads a Japanese 1.0 ROM, generates a seed with every alttpr.com generation setting, and writes a patched `.sfc` that the user then launches from Onion's SNES list with Onion's own SNES core.
- One seed is one ROM file, so it gets its own save: many seeds run in parallel, independently. This was the user's first need ("plusieurs sav avec des seeds différentes").
- A **History** tab lists the generated seeds, shows each one's settings and spoiler, and deletes a seed's ROM, save and save states.

The goals behind every rule below:

1. **Play, not race.** Matching alttpr.com is not a feature the user asked for. It matters here only because it is the proof that the ROM is right: an agent cannot play the game, so the produced ROM is checked against VT itself.
2. **Controller-first.** The user rejected an existing project (Slashmolder/zelda3randomizer) because its randomizer UI is a PC-style ImGui panel. Every screen is driven by the d-pad and buttons, in the style of Onion's menus.
3. **Verifiable without the console and without the ROM.** No Nintendo ROM is ever in the repository or in CI; the agent sees the UI through a text renderer.

## 2. Rules

### Locked decisions

- **Approach: a ROM patcher, not a native port.** The output is a patched SNES ROM played in an emulator. Rejected:
  - *alttp-dx* (YeOldPress, plays alttpr.com seeds in an embedded SNES emulator): one ROM at a time, not one seed per save slot, and PC only.
  - *Slashmolder/zelda3randomizer* (a snesrev/zelda3 fork with an in-binary generator): PC-style ImGui UI (goal 2), no Miyoo port, and the user had no sound in it.
  - *A native zelda3 port for the Miyoo*: no such port exists (it is not in PortMaster either; checked against `PortsMaster/PortMaster-New`, 1,481 ports, Oct 2026). The randomizer's ASM patches would also have to be reimplemented in C.
  - *Generating on alttpr.com and copying to the SD card*: the user wants on-device generation.
- **The emulator side is not a risk.** The user checked that an alttpr.com ROM boots, has sound and keeps its save with Onion's default SNES core.
- **Base ROM: Zelda no Densetsu: Kamigami no Triforce, Japanese v1.0**, MD5 `03a63945398191337e896e5771f77173`. VT requires it (`resources/lang/en/start.php`); the patched game is in English. A US ROM is not supported.
- **VT is frozen** at `sporchia/alttp_vt_randomizer` `219fcafd029dab597b8db400efafd8f56f8b4edb`, the commit the oracle pins (`tests/rando/oracle/gen_golden.sh`), with no tracking of later alttpr.com updates. That commit's ROM build is `Rom::BUILD = '2024-02-18'`, `Rom::HASH = 'edc01f3db798ae4dfe21101311598d44'` (MD5 of the JP ROM, resized to 2 MiB, with the base patch applied).
- **The base patch** (z3randomizer's compiled ASM and English text, applied before any seed write) **is committed to the repository and shipped in the app.** The user accepted that it may contain data derived from the game. Its source:
  - alttpr.com serves it as a BPS at `/bps/<hash>.bps` (`routes/web.php`, `resources/js/components/VTRomLoader.vue`).
  - Otherwise it can be built with asar from `KatDevsGames/z3randomizer` `dcb0a2b42d14445f7994a0e9e4d63cbecf4b98d3` (v31.2.2, VT's `composer.json`) on the resized JP ROM: `LTTP_RND_GeneralBugfixes.asm`, `--fix-checksum=off` (`app/Console/Commands/UpdateBuildRecord.php`).
  - Either way, applying it to the JP ROM must give MD5 `edc01f3d…`.
- **Generation settings: all of alttpr.com's**, the eleven of `rando/include/rando_options.h` (glitches, item_placement, dungeon_items, accessibility, goal, crystals_tower, crystals_ganon, state, weapons, item_pool, item_functionality), plus the hints on/off toggle (`spoil.Hints`).
- **Cosmetic options: only the heart beep** (`Rom::setHeartBeepSpeed`: off, half, quarter, double, normal; one byte at `0x180033`). Rejected by the user: Link sprites (`.zspr`), heart color, menu speed, music mute, quickswap. Every other cosmetic stays at VT's default.
- **The seed is always random.** There is no seed entry and no sharing (the user does not need them). The app remembers the last settings used.
- **Files on the SD card:**
  - The base ROM is placed once at `App/ALttPR/base.sfc`; the app checks its MD5 at start and shows a clear message if it is wrong.
  - Generated ROMs go to `Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc` (e.g. `Open-FastGanon-4F2A9C.sfc`), a subfolder of Onion's SNES list.
  - History records go to `App/ALttPR/history/<code>.json`: settings, date, and the spoiler when the spoiler option was on.
- **Spoiler:** an option of the Generate screen, **off by default**. It decides only whether the spoiler is kept in the history record; it never changes the ROM and is never written next to the ROM. A seed generated without it still appears in History, with its settings but no item locations.
- **History tab:**
  - It lists the generated seeds and shows each one's settings and spoiler.
  - Deleting a seed, after a confirmation, removes the ROM, its RetroArch `.srm` save and its RetroArch save states.
  - A ROM deleted by hand with Onion's file manager simply disappears from History at the next start.
- **UI:**
  - Two tabs, **Generate** (every setting, then Generate) and **History**, in the style of Onion's menus: a clean font, lists, a cursor, no game graphics.
  - English text, every string looked up through a translation table keyed by id; only English ships.
- **UI architecture (the agent's eyes):**
  - The UI is a pure C state machine, with no SDL: a key in, a screen description out (tab, rows, cursor, values, message). Two renderers draw that description: SDL (Miyoo and PC) and text (a character grid).
  - Tests play a key sequence and compare the text screen against a snapshot.
  - A host command-line tool reads a key sequence and prints the text screen, so the agent can "look" at the app while working.
- **The app also builds for PC** (Linux and macOS, SDL, keyboard instead of the d-pad), so the user tests the SDL rendering before copying to the SD card. Only the user validates the SDL rendering and the device.
- **ROM goldens are write logs, not ROMs.** The oracle records every `Rom::write` (address and bytes) that a case makes after the base patch. The C port must reproduce that log byte for byte. Neither the oracle nor CI needs a Nintendo ROM:
  - VT's `Rom` constructor accepts no source file.
  - `writeToRom` does not read the ROM back: the only `readByte` calls are in `app/Sprite.php`, for enemizer.
  - The checksum (`Rom::updateChecksum`), which does depend on the ROM, is tested by its algorithm, not by a golden.

### Inherited guardrails (`CLAUDE.md` of this repository, apply in the new repository too)

- The rando goldens are the source of truth. Never regenerate a golden to get green: `gen_golden.sh` runs only when VT, its patch or the case list changes on purpose, in its own commit.
- Never make `make -C tests/rando test` or `make -C tests/rando e2e` slower; a slow check goes in a separate target.
- The randomizer library stays pure C99: no `malloc`, no platform library, built by host gcc and by any cross compiler.
- Never launch the game or an emulator. Never merge a pull request. Never work on or push a `claude/*` branch.
- Everything written in the repository is in English. Comments follow `CLAUDE.md`'s comment rule.

### Out of scope

- Entrance shuffle, Enemizer (boss, enemy, damage, health, pot shuffle), door shuffle: not part of VT's generator.
- Custom/customizer seeds, multiworld, tournament/race mode, seed entry or sharing, matching an alttpr.com seed.
- Any firmware other than OnionOS; PortMaster packaging.
- Translations other than English (the mechanism is in scope, the tables are not).
- What becomes of `alttp-ds` and its DS port: unrelated to this plan.

## 3. Phases and steps

### Phase 1 — The ROM writer matches VT, host only

1. **Oracle records the write log.** Extend `tests/rando/oracle/vt.patch` and `oracle_case.php` so each case also runs `World::writeToRom` on a sourceless `Rom`, with the case's heart beep (VT default `half` unless the case sets it). It records every write after the base patch as `cases/<case>/rom_writes.txt`, one `<hex address> <hex bytes>` line per write, in call order. `updateChecksum` is excluded. Regenerate the goldens in their own commit (the case list's output changes on purpose). Verified by: `gen_golden.sh` is reproducible (two runs, identical files), and `make -C tests/rando e2e` stays green on the existing outputs.
2. **Golden cases for the new settings and for combinations.** Add cases for hints off, each heart beep value, and enough multi-setting combinations to cover what the UI allows at once. At least: every state × every glitches value, dungeon_items full with every state, triforce-hunt and ganonhunt with every state. List every combination where the C generator returns `RANDO_UNSUPPORTED`, and either port it or record it as a UI restriction (Phase 4 greys it out). Commit the case list and goldens on their own. Verified by: the goldens exist and every new case is either green in `e2e` or listed as unsupported.
3. **`rando_rom` module: the C port of `World::writeToRom` and the `Rom` setters it calls** (with `Text`, `Credits`, `InitialSram`, `Dialog` as needed). It writes through a caller-supplied write callback, so the host logs writes and the app writes into a buffer; no allocation. `e2e` gains the write-log diff: one `DIVERGENT <case>` line per case whose log differs. Port by sub-area (items and locations, settings and flags, text and hints, credits, initial SRAM), each with its unit test `tests/rando/unit/test_rom_<area>.c` registered in `UNIT_TESTS`. Verified by: every case's `rom_writes.txt` matches, `e2e` timing does not regress, and `test` stays fast.
4. **Checksum and heart beep.** Port `Rom::updateChecksum` and `setHeartBeepSpeed`. Verified by: a unit test checks the checksum algorithm against a synthetic 2 MiB buffer whose expected sum is computed independently, and the heart-beep byte matches the golden cases of step 2.
5. skill: check-e2e — the whole case list, write logs included.

### Phase 2 — Base patch and full ROM assembly, host only

1. **Obtain the base patch and commit it.** Fetch `https://alttpr.com/bps/edc01f3db798ae4dfe21101311598d44.bps`. If the URL differs, find the real one from VT's `routes/web.php` and `base_rom/settings`. Commit it as `data/base.bps`, with its source URL and VT commit in the commit message. Verified by: the BPS header's target size is 2,097,152 and its source CRC32 is the JP 1.0 ROM's CRC32. The human check (apply to a real JP ROM, MD5 `edc01f3d…`) is listed in §4.
2. **BPS applier in C** (no allocation, caller buffers, source and target CRC32 checks). Verified by: unit tests on small hand-built BPS files covering every BPS action and each CRC failure.
3. **ROM assembly pipeline**: check the source MD5 (`03a63945…`), resize to 2 MiB, apply the base patch, check MD5 `edc01f3d…`, generate, run `rando_rom`, apply the heart beep, update the checksum, and write the `.sfc`. A host CLI `rando_patch --base <jp.sfc> --out <file> [settings]` does it end to end. Verified by:
   - a host test with a **synthetic** base ROM and a synthetic BPS, which checks the order of operations and each error path (wrong MD5, bad BPS, write failure);
   - a test that the written ROM equals the base-patched buffer with exactly the logged writes applied.
4. **Output naming, history record and spoiler.** Name a ROM `<Mode>-<Goal>-<code>.sfc`, where `<code>` is 6 hex digits derived from the seed and settings, with a counter suffix on collision. The history record JSON holds the settings, an ISO date and the optional spoiler (reuse `rando/host/spoiler_json.c`'s format). Verified by unit tests: naming of every mode and goal, collision handling, and a record round-trip (write, then read back identical).

### Phase 3 — The app's UI core and text renderer, host only

1. **Screen model and text renderer.** The screen description struct and a renderer to a fixed character grid. Verified by a snapshot test of a hand-built screen.
2. **String table (i18n).** Every UI string goes through an id and the English table; a test fails if the UI core contains a string literal that is shown to the user. Verified by that test.
3. **Generate tab state machine.**
   - Every setting of §2 is a row; left and right change the value, and up and down move.
   - Unsupported combinations from Phase 1 step 2 are greyed out with a reason.
   - The rows include the spoiler option (default off), the heart beep option, and the Generate action.
   - The last settings are saved and restored.

   Verified by key-sequence snapshot tests, including restoring from a saved settings file.
4. **Generation flow.** A "Generating…" screen, then success (the file name) or an error message: base ROM missing or wrong MD5, generator gave up, SD card write failed. Verified by snapshot tests with an injected generator, for success and for each failure.
5. **History tab.** The list is newest first and reconciled at start with the ROMs actually present. It has a detail view (settings, spoiler if kept, scrollable) and a delete action with confirmation, which removes the ROM, `.srm` and save states, then the record. Verified by snapshot tests on a temporary directory tree mimicking the SD card, checking the files removed and the files kept.
6. **`ui_cli`: key sequence in, text screen out**, for the agent's own checks. Verified by a smoke test in `make test`.

### Phase 4 — Onion and PC builds

1. **Confirm the Onion specifics from Onion's sources** (`OnionUI/Onion`) and record them in `docs/` with links:
   - the app folder layout (`App/<Name>/config.json`, `launch.sh`, icon);
   - the toolchain (the Miyoo Mini cross toolchain Onion builds with), and the SDL version and video setup Onion apps use;
   - the screen resolution on the Mini Flip;
   - the button mapping;
   - the exact RetroArch save and save-state paths for the default SNES core (e.g. under `Saves/CurrentProfile/`).

   Verified by: each item cites the Onion source file. Fix the delete paths of Phase 3 step 5 if they differ.
2. **SDL renderer and input**, shared by the Miyoo and PC builds: Onion-style list rendering, and key mapping from the d-pad and buttons, or the keyboard on PC. Verified by: the PC build compiles with no warnings in CI and starts, renders one frame offscreen and exits in a headless smoke test (SDL dummy video driver). The visual check is the user's (§4).
3. **Miyoo build and package**: a cross build in the Onion toolchain (a container or a pinned toolchain archive), and a release zip with the `App/ALttPR/` layout (binary, `launch.sh`, `config.json`, icon, `base.bps`). Verified by: CI produces the zip, and `file` reports an ARM 32-bit EABI hard-float executable.
4. **Performance budget**: measure the generation time of the slowest golden case on the host at `-O2`, and estimate the time on the Cortex-A7. If it exceeds about 10 s, the Generating screen gets a progress indicator per generator stage. Verified by: a measurement target outside `test` and `e2e`, with its numbers recorded in the step note.

### Phase 5 — Cleanup

1. skill: rework-comments — every file written by this plan.
2. **README**: install (copy `App/ALttPR/`, put the JP 1.0 ROM as `base.sfc`), use, and deletion behavior. Verified by review against §2.

## 4. Done when

- `make -C tests/rando test` and `make -C tests/rando e2e` are green: every case matches VT's spoiler and write log, with `e2e` no slower than before the plan, except for the per-case cost of the write-log diff.
- The UI snapshot tests cover every Generate row, the generation success and failure screens, and History list, detail and delete.
- CI produces the PC build and the Onion release zip.
- **Human checks by the user**, listed in the final step note:
  - the base patch applied to their JP ROM gives MD5 `edc01f3d…`;
  - the app runs on the Mini Flip, generates a seed, and the ROM boots and plays to at least the first dungeon;
  - two seeds keep separate saves;
  - delete from History removes the ROM, save and states;
  - the PC build looks right.

## 5. Open questions

1. **Which repository.** The user decided the code goes into a **new repository** that takes over the randomizer code (`rando/`, `tests/rando/`, the oracle, and probably `.claude/` skills and tooling). Its name and exact content are left for later ("je verrai après"). This plan is written with this repository's paths (`rando/`, `tests/rando/`) and must be moved there, and the paths adjusted, before `plan-pilot` runs.
2. Whether the app's UI core and host tools live under `rando/host/` or a new top-level `app/` directory: decide when the repository exists.

## 6. Step notes
