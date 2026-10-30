---
title: "Save through the cartridge EEPROM"
description: "Replace the SD-card file save in zelda_save with the cartridge's 8 KB SPI EEPROM, and drop libfat, so that emulators with no SD card (Delta, RetroArch melonds, melonDS without DLDI) can save."
status: needs-triage
---

## Problem Statement

The ROM saves to an 8 KB `.sav` file on an SD card, through libfat, DLDI, TWiLight Menu++'s save-location logic and a melonDS-only raw sector writer. Delta on iOS, RetroArch's `melonds` core on the Miyoo Mini Flip and any melonDS without DLDI give a homebrew no SD card, so the player there cannot save at all.

Phase 1 of plan 1 made the ROM look like a retail game (game code `ZLTP`), so melonDS now exposes an 8 KB EEPROM on the cartridge SPI bus and stores it as its own `.sav`. Step 1.2's probe proved the ARM9 can reach that EEPROM, and found that libnds `cardWriteEeprom` never reaches released melonDS's `.sav`. The save code still ignores the EEPROM.

## Solution

`zelda_save` stops being a file system client and becomes an EEPROM client:

- At boot, Mount takes the cartridge slot for the ARM9, checks that an EEPROM chip answers, reads all 8 KB into the game's SRAM image and into a shadow copy, and gives the slot back to the ARM7. No chip: status `off`, nothing is ever written. A blank chip (every byte `0xFF`): status `new`. Otherwise `loaded`.
- After each frame where the game asked to save, Flush repairs the slots as today, then writes only the 32-byte pages that differ from the shadow, with a page write that melonDS 0.9.5 to 1.1 actually commits, and updates the shadow.
- The image layout, the settings block and every public function stay as they are, so an old `.sav` can be copied as-is into an emulator's save slot.
- `ZeldaSavePrepare`, `fatInitDefault`, `<fat.h>` and `-lfat` disappear, with every FAT, DLDI, TWiLight Menu++ and melonDS folder-sync line of `zelda_save`.

The player sees no change in menus or HUD words; on an emulator without an SD card, saving now works.

## User Stories

1. As a player on Delta, I want my save to go into the emulator's own save slot, so that I can save without an SD card.
2. As a player on RetroArch's `melonds` core, I want the ROM to save through the cartridge EEPROM the core emulates, so that my `.sav` holds my game.
3. As a player on any melonDS 0.9.5 or later without DLDI, I want saving to work with no setting to change, so that I configure nothing.
4. As a player, I want my three save slots and their mirrors stored at the same offsets as before, so that a `.sav` from an older build loads when I copy it into the emulator's save slot.
5. As a player, I want my Settings (HUD position, low-health beep, carry caps, run speed, dark rooms, hair, spawn, mirror, button map) stored in the same settings block as before, so that they survive the move to the EEPROM.
6. As a player, I want a slot whose existence mark or checksum was clobbered to be repaired before it reaches the EEPROM, as it was before it reached the file, so that a bad frame cannot destroy a save.
7. As a player, I want the HUD status word to stay `off`, `new`, `loaded`, `saved` or `fail`, so that I still know whether my game was saved.
8. As a player starting on a blank cartridge EEPROM, I want the status to read `new` and the game to start from empty slots, so that I begin a fresh game.
9. As a player whose emulator exposes no save memory, I want the ROM to boot and play with status `off`, so that a missing chip never hangs or corrupts anything.
10. As a player on real hardware with no EEPROM answering (a floating bus reading `0xFF`), I want the same `off` behavior, so that a cart without save memory never hangs a save.
11. As a player, I want a save to touch only the 32-byte pages that changed, so that saving is quick and the EEPROM wears less.
12. As a player, I want a save that fails to retry once on the next frame and then report `fail`, as before, so that a transient error does not lose the save and a permanent one is visible.
13. As a player, I want a failed page to be retried by the next save, so that a page the chip did not take is not silently forgotten.
14. As a player, I want saving never to freeze the game, so that a stuck SPI bus or a chip that never finishes its write costs at most a failed save.
15. As a player, I want the save to happen after the frame, as it does today, so that saving does not cause a visible hitch in the middle of a picture.
16. As a player who holds L+R+Start+Select to quit, I want the pending save written before the loop exits, as today, so that quitting does not lose progress.
17. As a player, I want the ROM's audio and the ARM7 to keep the cartridge slot between saves, so that nothing else on the system changes behavior.
18. As a player on a flashcart or nds-bootstrap, I want the ROM to save the way a retail game with a 64 kbit EEPROM does, so that save emulation for an unknown game code has a chance to work.
19. As a developer, I want libfat and calico's block-device writer gone from the ELF, so that no code path can still reach an SD card.
20. As a developer, I want `main` free of `fatInitDefault` and `ZeldaSavePrepare`, so that boot no longer depends on DLDI.
21. As a developer, I want the public save API (`ZeldaSaveMount`, `ZeldaSaveFlush`, `ZeldaSaveStatus`, `__wrap_ZeldaWriteSram`, every settings, spawn and button-map function) unchanged, so that `zelda_host` and the bottom-screen code need no change.
22. As a developer, I want the EEPROM page write to clear chip-select hold before the last data byte, as step 1.2 proved, so that released melonDS commits the page to its `.sav`.
23. As a developer, I want reads to use libnds `cardReadEeprom` and `cardEepromCommand` with `addrtype` 2, as plan § 2 locks, so that only the write path is our own code.
24. As a developer, I want every busy and write-in-progress poll in our own code bounded, so that no hardware state can hang the ROM.
25. As a developer, I want the chip-presence rule to be "status bit 1 set after WREN and clear after WRDI", so that melonDS's no-chip `0x00` and hardware's floating `0xFF` both read as no chip.
26. As a developer, I want the slot taken with `sysSetCardOwner(BUS_OWNER_ARM9)` and given back with `sysSetCardOwner(BUS_OWNER_ARM7)` around each Mount and each Flush, as step 1.2 measured, so that the ARM7 keeps the slot it expects outside those windows.
27. As a developer, I want the header comment of `zelda_save` to state the new contract (the save is the cartridge's 8 KB EEPROM), so that nobody looks for a file.
28. As a developer, I want the build, an `nm` of the ELF and a headless melonDS 0.9.5 boot to prove the step, so that it is verified without launching the game for a human.
29. As a developer running the emulator check, I want the EEPROM SPI log at boot to show the presence check and one 8 KB read, and no write, so that Mount is seen doing exactly what it should.
30. As a developer, I want a no-chip boot (`M095_NOSAVE`) to show the presence check and no read or write command, so that the `off` path is seen to stop early.
31. As a maintainer, I want later steps (2.2 save reaches `.sav`, 2.3 relaunch loads it, 3.1 README) to build on this rewrite unchanged, so that they only add checks and docs.

## Implementation Decisions

**Modules touched**

- `zelda_save` (source and header): the only module with real change.
- `main`: loses `fatInitDefault()`, `ZeldaSavePrepare(argc, argv)` and `<fat.h>`. Nothing else in the platform code reads `argc`/`argv` (checked), so `main` becomes `int main(void)`.
- Root `Makefile`: `LIBS` loses `-lfat`. The `--wrap=ZeldaWriteSram` link flag stays.
- No other module changes. `zelda_host` keeps calling `ZeldaSaveMount` after `ZeldaInitialize` and `ZeldaSaveFlush` after the frame and in `ZeldaHostSave`.

**What leaves `zelda_save`** (plan § 2, "The file goes entirely")

- FAT geometry, directory and cluster-chain code, `touch_dir`, sector buffer and helpers.
- `melon_write_sector`, `write_sec`, the `dldi_io_type`/`MELN` detection.
- TWiLight Menu++ `settings.ini` parsing, save-location logic, argv and nds-bootstrap parsing, path helpers.
- stdio read and write, the 8192/8193 length flip, the legacy `fat:/lttp-ds.sav`, root-save and `saves/sram.dat` reads.
- `ZeldaSavePrepare` from the source and the header; the header's comments for Mount and Flush are reworded for the EEPROM ("without EEPROM access when clean").
- Includes that only served those: calico `blk.h` and `env.h`, `stdio.h`, `sys/stat.h`, `unistd.h`.

**What stays in `zelda_save` unchanged**

- The settings block at `0x1E00`, version 8, every older version's reader, the checksum, the spawn exit snapshots, the button map, every getter, setter and `apply_*` function, `settings_store` calling `ZeldaWriteSram`.
- `__wrap_ZeldaWriteSram` only marks the image dirty; Flush does the work after the frame.
- `Sram_RepairSlots(g_zenv.sram)` runs in Flush before any page is compared or written.

**EEPROM access — decision: three static functions inside `zelda_save`, no new module**

The EEPROM driver is small (presence check, full read, page write) and has exactly one caller. A separate module would add a header and an interface with no second user and no host test target to gain from (see Testing Decisions). Plan § 3 phase 2 step 1 says "the file part goes and the EEPROM part arrives" in `zelda_save`. The three are:

- *presence*: `cardEepromCommand(WREN)`, `RDSR`, `cardEepromCommand(WRDI)`, `RDSR`. Present iff bit 1 is set in the first status and clear in the second (§ 2; step 1.2 measured `02`/`00` with a chip, `00`/`00` without). Leaves the write-enable latch cleared.
- *read all*: one `cardReadEeprom(0, buf, 8192, 2)`.
- *page write*: the sequence step 1.2's probe proved on melonDS 0.9.5, kept byte for byte:
  1. `AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD`, send WREN, `AUXSPICNT = CARD_SPI_HOLD`;
  2. `AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD`, send `0x02`, address high, address low, data bytes 0..30;
  3. `AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE` (hold cleared), send data byte 31, `AUXSPICNT = CARD_SPI_HOLD`;
  4. poll `RDSR` bit 0 (write in progress) until clear.

  Each byte send writes `AUXSPIDATA` and waits for `CARD_SPI_BUSY` to clear with a **bounded** spin, not libnds `eepromWaitBusy` (unbounded). The write-in-progress poll is bounded too, by iteration count, at least 100000 `RDSR` calls (the probe's bound; far more than a hardware page write's ~5 ms). On a busy timeout the page write deselects the chip (`AUXSPICNT = CARD_SPI_HOLD`) and reports failure.
- The libnds read helpers (`cardReadEeprom`, `cardEepromCommand`) keep their own unbounded controller-busy wait. Plan § 2 mandates them for reads, and that wait is on the SPI controller, which finishes a byte whether or not a chip answers (step 1.2's no-chip run returned). The "every poll bounded" rule of § 2 applies to the code we write.

**Slot ownership**

- Mount and Flush each wrap their whole EEPROM session in one `sysSetCardOwner(BUS_OWNER_ARM9)` … `sysSetCardOwner(BUS_OWNER_ARM7)` pair, given back on every exit path, including failures. One pair per Flush, not per page.
- No critical section: no interrupt handler of the ROM touches the slot-1 SPI registers, the ARM7 never touches the slot on a `ZLTP` ROM (step 1.2: no DLDI patch, `exmemcnt_mirror` empty), and an interrupt between two SPI bytes only stretches the transfer while chip select is held. The audio and vblank IRQs keep running during a save.

**Mount**

1. `map_reset()`. If `g_zenv.sram` is NULL: status `off`, return (as today).
2. Take the slot. Presence check. No chip: give the slot back, status `off`, a flag records that the EEPROM is absent, `g_zenv.sram` stays as `ZeldaInitialize` left it (zeros), `settings_load()` applies defaults. Nothing is ever written in this boot.
3. Chip present: read all 8 KB into the shadow, give the slot back.
4. If every shadow byte is `0xFF`: blank chip, status `new`. `g_zenv.sram` is left zeroed, the way the old code started a missing file, so the game sees empty slots; the shadow keeps the `0xFF` the chip really holds. Otherwise copy the shadow into `g_zenv.sram`, status `loaded`. An image of all `0x00` from a chip that answered is `loaded` (§ 2 names only `0xFF` as blank; the old "all zero is new" rule belonged to the file).
5. `settings_load()`.

**Flush**

- Not dirty: return, no slot access.
- `g_zenv.sram` NULL or EEPROM absent: clear dirty, status `off`, no SPI traffic at all (step 2.3 will check that no WREN or page write appears).
- Otherwise `Sram_RepairSlots`, take the slot, then for each of the 256 pages whose bytes differ between `g_zenv.sram` and the shadow: page write, read the page back with `cardReadEeprom`, and on a match copy it into the shadow. A page that fails (timeout or read-back mismatch) keeps its old shadow, so the next Flush retries it. Give the slot back.
- All differing pages written (including the case of none): dirty cleared, retry counter cleared, status `saved`.
- Any page failed: dirty stays set and the counter increments; on the second consecutive failed Flush, dirty is cleared and status is `fail`. This is today's `g_tries` behavior, kept.
- The first save after a blank chip writes every page that is not `0xFF` in the zeroed image, i.e. all 256 pages. Accepted: it happens once, it is what the old code wrote to a new file, and on hardware it is about 1.3 s once.

**Static state**

- An 8192-byte shadow buffer in BSS, the status pointer, the dirty flag, the retry counter, the EEPROM-present flag. Every FAT, path and melon global goes.

**Header comment of `zelda_save`**

Rewritten to the contract only, within `CLAUDE.md` § Comments: the save is the cartridge's 8 KB EEPROM (64 kbit, 2-byte address, 32-byte pages); `ZeldaWriteSram` marks the image dirty and Flush writes the changed pages after the frame; and the one trap — the last data byte of a page goes with chip-select hold cleared, or released melonDS never commits the page (an assert-free trap, so 40 words at most). All FAT, TWiLight and melonDS-folder-sync prose goes.

**Link facts checked for this PRD**

- In the installed toolchain only libfat (`dvm_calico.o`) and `zelda_save` reference `blkDevReadSectors`/`blkDevWriteSectors`; nothing in `libcalico_ds9` or `libnds9` references `blk.o`'s symbols. Removing `-lfat` and the FAT code should therefore drop `blk.o` and both symbols from the ELF. If `nm` still shows `blkDevWriteSectors`, the link map (`-Wl,-Map` is already on) names the referencing object; that is recorded in the step notes as a deviation, not worked around.
- `fatInitDefault` is in libfat only (`fat_wrappers.o`); the ROM does not use nitroFS.

## Implementation Chunks

1. **The file goes.** Delete every file, FAT, DLDI, TWiLight and melon line of `zelda_save` listed above, `ZeldaSavePrepare` from source and header, `fatInitDefault`/`ZeldaSavePrepare`/`<fat.h>` from `main` (`int main(void)`), `-lfat` from `LIBS`. In this chunk Mount ends with status `off` after `settings_load()` and Flush clears dirty with status `off`: the ROM runs with no save, honestly reported. Verified by the build, the `nm` check and a headless boot to `ZeldaHostTick`.
2. **The EEPROM arrives.** Add presence, read-all and page write; Mount and Flush as decided above; the shadow; the new header comment. Verified by the build, the `nm` check, a headless boot to `ZeldaHostTick` with the EEPROM SPI log showing the presence check and one 8 KB read at Mount and no write command, and a `M095_NOSAVE=1` boot showing the presence check, no read and no write, and still reaching `ZeldaHostTick`.

Each chunk is one commit.

## Testing Decisions

- **No unit tests.** This branch has no `tests/` directory and no host test target (plan § 2, "No rando tests here"); `zelda_save` is ARM9-only code over libnds registers. Building a host harness for it is out of this step's scope. A good check here is external behavior seen from outside the ROM: what the link contains, what the emulator's EEPROM receives, and whether the game loop runs.
- **Build**: the `dkp-lttp` Docker image, `python3 build.py -j8` as in plan § 2, then restore the tracked `platform/nds/arm7/build/` objects (`git checkout` + `git clean`). The build must pass with no new warning in `zelda_save` or `main` (unused statics left behind are a sign the deletion is incomplete).
- **Symbols**: `arm-none-eabi-nm lttp-ds.elf` (devkitARM's `bin/` in the image) shows no `fatInitDefault`, `blkDevWriteSectors` or `melon_write_sector`. Also no `ZeldaSavePrepare`.
- **Grep**: `grep -n "fat\|dldi\|TWiLight\|melon" platform/nds/source/zelda_save.c platform/nds/source/main.c` finds nothing relevant (plan § 4). The one allowed hit is the melonDS trap in the header comment, which plan § 4's "nothing relevant" covers.
- **Header**: the `.nds` header still reads `ZLTP` at `0x0C..0x0F` (Python one-liner), since the Makefile is touched.
- **Emulator boot (headless melonDS 0.9.5, scratchpad instrumented build)**:
  - The `MARK9` addresses in the instrumented `ARMv5::JumpTo` are hard-coded. Removing code from `main` shifts every later function, so before the run the verifier reads `main`, `ZeldaHostTick` and `ZeldaSaveMount` from `nm` of the new ELF, updates the `marks[]` table and the `ZeldaHostTick` constant, and rebuilds melonDS if they changed (the `fatInitDefault` and probe entries can be set to an unused address).
  - Run through the scratchpad `tools/run.sh` (ROM passed as `./lttp-ds.nds`, shared memory cleared after `pkill -9`), waiting for `MARK9 main|MARK9 ZeldaHostTick x300`.
  - Chunk 2 additionally reads the `EEP` lines: at Mount, `06`, `05`, `04`, `05` command bytes with status `02` then `00`, then one `03 00 00` read of 8192 bytes; no `02` page-write command during the boot. If a boot path does call `ZeldaWriteSram` and a page write appears, that is recorded (not a failure of this step; step 2.2 owns the write checks).
  - Chunk 2 no-chip run: `M095_NOSAVE=1` prints `SaveMemType forced to 0`, the `EEP` lines show the presence-check bytes only (all `type=0 (no chip)`), no `03` read, no `02` write, and `MARK9 ZeldaHostTick x300` is reached.
  - Log only: stdout and files are read, the screen is never judged, no input is sent (plan § 2).
- **Prior art**: Step 1.1 and 1.2 verification in plan § 6 (same harness, same `MARK9`/`EEP`/`EXMEMCNT9` instrumentation, same `M095_NOSAVE` switch); the probe's page write in commit 23e6f8e is the reference sequence.
- `check-e2e` has nothing to run on this branch.

## Out of Scope

- Step 2.2: triggering `ZeldaWriteSram` through instrumentation and comparing melonDS's `.sav` bytes with what the ROM wrote, and the "only changed pages" check on the write path.
- Step 2.3: relaunch with a seeded `.sav` (`loaded`, settings applied), blank `.sav` (`new`), and the full no-chip save attempt (no WREN or page write after a triggered save).
- Step 3.1: README (§ Saves, libfat in § What you need, TWiLight text).
- Any change to the 8 KB image layout, the settings block or its version.
- Migrating old `.sav` files by code; DSi-mode or SD saves; any FAT path; a separate emulator build.
- The calico overrides in `platform/nds/calico/`, which stay untouched.
- Real-hardware and flashcart behavior for an unknown game code (plan § 2 "Accepted consequence"; a human checks it).

## Further Notes

- Decisions this PRD made that the plan leaves open, all from the plan's rules and step 1.2's facts (no `guideline` agent was needed):
  - EEPROM driver as static functions in `zelda_save`, not a new module.
  - Bounded own SPI byte wait; libnds read helpers kept with their internal wait.
  - One slot-ownership pair per Mount and per Flush; no critical section.
  - Read-back verify of each written page; failed pages keep their shadow and retry.
  - Blank chip: `g_zenv.sram` stays zeroed, shadow keeps `0xFF`, first save writes every page.
  - All-`0x00` image from a present chip is `loaded`.
  - `main` becomes `int main(void)`.
  - Two chunks, file out first, so the EEPROM code lands in an already-clean file.
- The status words are only reported through `ZeldaSaveStatus`; no caller outside `zelda_save` uses them today, so step 2.3 will need instrumentation (for example a `MARK9` on a status-specific path, or reading the status string's address) to see them.
- Step 2.2 should seed a non-blank `.sav` before checking "writes only on changed pages": from a blank chip the first save rewrites all 256 pages by design.
