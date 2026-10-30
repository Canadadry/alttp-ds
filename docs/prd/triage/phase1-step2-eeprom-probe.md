---
title: "ARM9 EEPROM probe"
description: "Before the save code moves to the cartridge EEPROM, prove on melonDS 0.9.5 that the ARM9 can read and write that EEPROM through libnds and sysSetCardOwner, and learn what a missing chip reads."
status: needs-triage
---

## Problem Statement

Plan 1 moves the save from an SD-card file to the cartridge's 8 KB EEPROM, so that Delta, RetroArch's `melonds` core and any melonDS without DLDI can save. Plan § 2 locks how the ROM reaches that EEPROM: libnds `cardReadEeprom`/`cardWriteEeprom` with `addrtype` 2, the slot taken with `sysSetCardOwner(BUS_OWNER_ARM9)` and given back with `sysSetCardOwner(BUS_OWNER_ARM7)`, and a status of `off` when "every byte reads `0xFF` and the status register reads `0xFF`".

None of that has run yet. Three things could make Phase 2 fail after `zelda_save.c` has already been rewritten:

- calico brokers the slot between the CPUs with `ntrcardOpen`/`ntrcardClose` and a cross-CPU `exmemcnt_mirror`. A direct `sysSetCardOwner` bypasses that broker. If the ARM7 holds the slot (an ARM7 slot-1 DLDI driver declares it in the mirror), the ARM9 takes it from under the ARM7.
- melonDS 0.9.5 drops every ARM9 read and write of `0x040001A0`/`0x040001A2` while EXMEMCNT bit 11 is set (the ARM7 owns the slot): reads return `0`, writes vanish. If the ownership switch does not land, every EEPROM byte silently reads `0x00`.
- In melonDS 0.9.5, `CartRetail::SPIWrite` returns `0` for every byte when `SRAMType == 0`. A missing chip there reads `0x00`, not the `0xFF` that § 2's `off` rule assumes. On real hardware an unpulled bus reads `0xFF`. One rule has to cover both.

The developer needs these facts measured, on the exact path Phase 2 will ship, before Phase 2 starts.

## Solution

A throwaway probe is added to the current ROM, with the old FAT save code still in place. Once per boot, before `ZeldaSaveMount` does anything else, it:

1. takes the slot for the ARM9 with `sysSetCardOwner(BUS_OWNER_ARM9)`;
2. reads the EEPROM status register, sets and clears the write-enable latch and reads the status after each;
3. reads one 32-byte page with `cardReadEeprom` (`addrtype` 2);
4. writes one 32-byte page with `cardWriteEeprom` (`addrtype` 2), filled with what it observed, then reads it back;
5. gives the slot back with `sysSetCardOwner(BUS_OWNER_ARM7)`.

Instrumentation in the scratchpad melonDS 0.9.5 logs every ARM9 access to the slot-1 SPI registers, every EXMEMCNT write and every byte the EEPROM emulation receives and returns. Two headless runs answer the step:

- **Chip present** (`SaveMemType = 2`, the default for `ZLTP`): the read and write commands arrive, the EEPROM returns what was asked, and melonDS writes `lttp-ds.sav` with the probe page in it.
- **No chip** (`SaveMemType` forced to 0): the log shows what the status register and data reads return.

The results go into plan § 6 Step notes, § 2's slot sequence and `off`/`new` rule are checked against them (and amended where the evidence contradicts them), and the probe is removed. The shipped ROM ends the step byte-for-byte equivalent in behavior to where it started.

## Hypotheses on step 1

Phase 1 step 1 (game code `ZLTP` through `ndstool`) is not done when this PRD is written. Everything this step assumes about its outcome is listed here. Chunk 1 checks each one before any probe code is written; if one is false, the step stops and reports it as blocked by step 1 (see Implementation Decisions, "Gate").

- **H-1-1** — The built `lttp-ds.nds` has `ZLTP` at header bytes `0x0C..0x0F`, and headless melonDS 0.9.5 prints `Game code: ZLTP` and, through the existing `NDSCart.cpp` printf, `SaveMemType=2 homebrew=0`. melonDS therefore builds a `CartRetail` (not a `CartHomebrew`, `CartRetailIR` or `CartRetailNAND`) with `SRAMType = 2` and an 8192-byte EEPROM image filled with `0xFF`.
- **H-1-2** — The `ZLTP` ROM still boots on melonDS 0.9.5: the `MARK9 main` and `MARK9 ZeldaHostTick x<n>` lines appear. In particular `fatInitDefault()` and `ZeldaSavePrepare()` return (they do not hang) when the cart is retail, and `ZeldaSaveMount` is reached (`MARK9 ZeldaSaveMount`).
- **H-1-3** — Step 1 changed only the `.nds` packaging (the root `Makefile`'s `$(OUTPUT).nds` rule with `-g $(GAME_CODE)`); `build.py` and the Docker build command are unchanged, and the ELF built from the step-1 tree has the same symbol addresses as the instrumentation's hardcoded `MARK9` table.
- **H-1-4** — Because melonDS treats the `ZLTP` ROM as retail, it applies no DLDI patch. The ARM9 binary keeps its DLDI stub, calico's ARM7 loads no slot-1 driver, `g_envExtraInfo->dldi_features` has no `DLDI_FEATURE_SLOT_NDS` bit, `dldi_io_type` is not melonDS's `MELN` (`0x4E4C454D`), and calico's `exmemcnt_mirror` does not mark slot 1 as held by the ARM7. Nothing on the ARM7 touches slot 1 while the probe runs.
- **H-1-5** — `g_envBootParam->boot_src` reads `EnvBootSrc_Card` on melonDS 0.9.5 direct boot of the `ZLTP` ROM (recorded for the step notes; the probe does not depend on it).

## User Stories

1. As a player on Delta, I want the save code that ships in Phase 2 to have been proven against the emulator's EEPROM first, so that saving works the first time I try it.
2. As a player on RetroArch's `melonds` core, I want the ROM to talk to the EEPROM the way that core emulates it, so that my `.sav` holds my game.
3. As a player on any melonDS without DLDI, I want the ROM to find its EEPROM with no setting to change, so that I configure nothing.
4. As a player whose emulator exposes no save memory, I want the ROM to see that no chip answers, so that a later build reports `off` instead of corrupting memory or hanging.
5. As a player on real hardware, I want the no-chip rule to hold for a floating bus (`0xFF`) as well as for melonDS's `0x00`, so that a cart without EEPROM never hangs a save.
6. As a player, I want this step to leave the shipped ROM behaving as before, so that the probe never touches my real saves after the step.
7. As a developer, I want the probe to use exactly the libnds calls and slot sequence plan § 2 locks, so that what the probe proves is what Phase 2 ships.
8. As a developer, I want any change the probe needs to work (a different ownership call, a critical section, a wait) to be the step's recorded result, so that Phase 2 starts from facts, not guesses.
9. As a developer, I want the probe to run once per boot at the start of `ZeldaSaveMount`, so that it runs at the call site where Phase 2's Mount will read the EEPROM.
10. As a developer, I want the probe to read the status register before and after setting and clearing the write-enable latch, so that I learn whether that latch tells a chip from no chip.
11. As a developer, I want the probe to read one full 32-byte page, so that I see what blank memory and missing memory each read as.
12. As a developer, I want the probe to write one full 32-byte page, so that I see the page-write command arrive with its 2-byte address.
13. As a developer, I want the page the probe writes to carry what the probe observed (status bytes, read bytes, EXMEMCNT values, DLDI info), so that the ARM9's own view reaches the `.sav` and the SPI log without a console or a screen.
14. As a developer, I want the probe page to sit at `0x1FE0`, past the settings block, so that it never overlaps the slots, their mirrors or the settings an old `.sav` holds.
15. As a developer, I want the probe to read its page back after writing it, so that the log shows the write was stored, not just sent.
16. As a developer, I want EXMEMCNT recorded before the take, while held and after the give-back, so that I know the ownership switch landed and was undone.
17. As a developer, I want melonDS to log every ARM9 access to `0x040001A0`/`0x040001A2` with the current EXMEMCNT bit 11, so that a dropped access is visible instead of silently reading `0`.
18. As a developer, I want melonDS to log every ARM7 access to the slot-1 SPI registers, so that I know whether the ARM7 used the slot while the ARM9 held it.
19. As a developer, I want melonDS to log every byte the EEPROM emulation receives and returns, with its position and the current command, so that read status, write enable, read and page write are each visible.
20. As a developer, I want the no-chip run to come from the same melonDS build through an environment variable, so that both runs use identical emulator code.
21. As a developer, I want the chip-present run to end only after `SaveManager: Written` (bounded wait), so that the `.sav` I read is the one melonDS flushed.
22. As a developer, I want the `.sav` checked to be 8192 bytes, all `0xFF` except the probe page, so that the write landed at the right address and nowhere else.
23. As a developer, I want the emulator runs to happen in a scratchpad copy of the ROM, so that no `.sav` lands in the repository.
24. As a developer, I want each step-1 hypothesis checked before the probe is written, so that a failure of step 1 is not mistaken for a failure of the EEPROM path.
25. As a developer, I want the boot markers re-derived from `arm-none-eabi-nm` after adding the probe, so that the instrumentation marks the new addresses.
26. As a developer, I want to know whether `cardWriteEeprom`'s wait for the write-in-progress bit can hang when no chip answers, so that Phase 2 never calls it in that state.
27. As a developer, I want § 6 Step notes to record how the slot must be taken, what a chip and a missing chip read, and whether § 2 holds, so that Phase 2 step 1 is written from them.
28. As a developer, I want § 2 amended only where the evidence contradicts it, with the reason in the step notes, so that the plan stays true and every change is traceable.
29. As a developer, I want the probe committed once with its evidence and removed in a second commit, so that git keeps the exact code that produced the evidence while the branch ends clean.
30. As a developer, I want the build's tracked ARM7 objects restored after every build, so that no build artifact reaches a commit.
31. As a reviewer, I want the probe's source to follow `CLAUDE.md` § Comments, so that even throwaway code in history reads cleanly.
32. As a reviewer, I want the commit messages to carry the observed log lines, so that the evidence survives the probe's removal.

## Implementation Decisions

- **Gate on step 1.** Chunk 1 builds the current tree and checks H-1-1 to H-1-5 on headless melonDS 0.9.5 before any probe code exists. If H-1-1, H-1-2 or H-1-3 is false, the step stops: nothing is committed and the report says step 1 is not delivered as assumed. If H-1-4 is false (the ARM7 holds slot 1 on melonDS for a `ZLTP` ROM), the step goes on: that is exactly the case the probe must measure, and the step notes record it. H-1-5 is only recorded.
- **Where the probe lives.** One throwaway translation unit in the DS host sources (the root `Makefile` globs `platform/nds/source/*.c`, so no build change), with one function and a one-line header. `ZeldaSaveMount` calls it as its very first statement, before the `g_zenv.sram` check, so it runs once per boot after `fatInitDefault`/`ZeldaSavePrepare` and inside `ZeldaHostInit`, where Phase 2's Mount reads the EEPROM. Rejected: calling it from `main` (a call site Phase 2 does not use) or from `ZeldaSaveFlush` (it needs a dirty image, which needs a save).
- **Exactly § 2's path.** The probe calls `sysSetCardOwner(BUS_OWNER_ARM9)` once before its first access and `sysSetCardOwner(BUS_OWNER_ARM7)` once after its last, `cardReadEeprom(addr, buf, 32, 2)`, `cardWriteEeprom(addr, buf, 32, 2)` and, for the status register and the write-enable latch, `cardEepromCommand` with `SPI_EEPROM_RDSR`, `SPI_EEPROM_WREN` and `SPI_EEPROM_WRDI` (`nds/card.h`). No `ntrcardOpen`, no critical section, no raw register access beyond reading `REG_EXMEMCNT`. `sysSetCardOwner` is deprecated in libnds 2.0.2; the build already passes `-Wno-deprecated-declarations`, so it compiles silently. If this path fails (any EEPROM access dropped, wrong data, a hang), the implementer changes the smallest thing that makes it work (for example `ntrcardOpen`/`ntrcardClose`, or an `enterCriticalSection` around the access), reruns, and that change is the result recorded in the step notes and carried into § 2.
- **What the probe reads, in order.**
  1. `REG_EXMEMCNT` before the take (`exm_before`).
  2. Take the slot; `REG_EXMEMCNT` again (`exm_held`).
  3. `RDSR` (`sr0`); `WREN`; `RDSR` (`sr_wren`); `WRDI`; `RDSR` (`sr_wrdi`).
  4. `cardReadEeprom` of the 32 bytes at `0x1FE0` into a buffer; keep the first byte, the AND and the OR of all 32 (`rd_first`, `rd_and`, `rd_or`).
- **What the probe writes.** One 32-byte page at `0x1FE0`, the last page of the 8 KB image, past the settings block (`kSetOff = 0x1E00`, about 0xB8 bytes used). Its layout, all little-endian:

  | Offset | Bytes | Content |
  |---|---|---|
  | 0 | 4 | `P` `R` `B` `1` |
  | 4 | 1 | `sr0` |
  | 5 | 1 | `sr_wren` |
  | 6 | 1 | `sr_wrdi` |
  | 7 | 1 | `rd_first` |
  | 8 | 1 | `rd_and` |
  | 9 | 1 | `rd_or` |
  | 10 | 2 | `exm_before` |
  | 12 | 2 | `exm_held` |
  | 14 | 4 | `g_envExtraInfo->dldi_io_type` |
  | 18 | 2 | `g_envExtraInfo->dldi_features` |
  | 20 | 2 | `g_envBootParam->boot_src` |
  | 22 | 10 | `0xA5` |

  It then reads the page back with `cardReadEeprom` (the log shows those bytes), gives the slot back, and reads `REG_EXMEMCNT` a last time. That last value cannot go into the page; the EXMEMCNT write log shows it.
- **The probe writes even when it believes no chip answers.** It is a measurement, not the shipped logic: the no-chip run must show what `cardWriteEeprom` sends and whether it returns. In melonDS a missing chip returns `0x00`, so the write-in-progress poll ends; that is recorded.
- **Hang risk recorded for Phase 2.** libnds's `cardWriteEeprom` polls `RDSR` until bit 0 (write in progress) clears, with no timeout. On real hardware with no chip the bus reads `0xFF` and that poll never ends. The implementer confirms the loop with `arm-none-eabi-objdump -d` on `cardEeprom.o` from `libnds9.a` inside the `dkp-lttp` image and records in the step notes that Phase 2 must decide `off` before any `cardWriteEeprom`.
- **Candidate presence rule, to confirm or reject.** Expected from the melonDS source: chip present gives `sr0 = 0x00`, `sr_wren = 0x02`, `sr_wrdi = 0x00`, blank data `0xFF`; no chip in melonDS gives `0x00` everywhere; no chip on real hardware gives `0xFF` everywhere. The rule "a chip answers if and only if `RDSR` after `WREN` has bit 1 set and `RDSR` after `WRDI` has it clear" separates all three. If both runs confirm it, the step replaces § 2's "every byte reads `0xFF` and the status register reads `0xFF`" with that rule, and keeps "every byte `0xFF` with a chip that answers is a blank EEPROM: `new`". If the runs contradict it, the step notes say what does separate the cases, and § 2 is amended to that.
- **§ 2 amendments are limited** to the "EEPROM access" bullet (slot sequence and `off`/`new` rule). Any other locked decision the evidence contradicts is written in § 6 as an open problem for the user; it is not changed by this step.
- **melonDS instrumentation** lives only in the scratchpad tree `.../scratchpad/src/m095`, never in the repository, and is rebuilt with `cmake --build build -j16`. It adds:
  - in `NDS.cpp`, ARM9 8/16/32-bit I/O writes to `0x040001A0`, `0x040001A1`, `0x040001A2` and 8/16-bit reads of `0x040001A2`: one line `A9 <R|W> <addr> <val> own7=<ExMemCnt[0] bit 11>`, printed whether or not the access is dropped. `AUXSPICNT` reads are not logged (busy polls would flood).
  - ARM7 I/O accesses to the same registers: one line `A7 ...`, same format.
  - ARM9 writes to `0x04000204`: `EXMEMCNT9 <old> -> <new>`.
  - in `CartRetail::SPIWrite`, after the result is known: `EEP pos=<n> in=<val> out=<ret> cmd=<SRAMCmd> st=<SRAMStatus> type=<SRAMType>`. The `SRAMType == 0` early return is logged too.
  - in `NDSCart::LoadROM`, right after the existing `SaveMemType=` printf: if the environment variable `M095_NOSAVE` is set, force `romparams.SaveMemType = 0` and print `SaveMemType forced to 0`.
  - the `MARK9` address table in `ARM.cpp`, updated from `arm-none-eabi-nm lttp-ds.elf` for `main`, `ZeldaHostInit`, `ZeldaSaveMount`, `ZeldaHostTick` and the probe function, since adding the probe moves code.
- **The run.** The ROM is copied to a fresh scratchpad directory (one per run), so the `.sav` lands there. `QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy stdbuf -oL -eL <melonDS> <copy>.nds > out.txt 2>&1 &`; wait (bounded, 60 s) for `MARK9 ZeldaHostTick` and, in the chip run, `SaveManager: Written`; then `pkill -9 melonDS`. Read stdout and files only; never the screen; never a button.
- **The commit sequence.** Commit 1 adds the probe, its message quoting the chip-run log lines that prove the path (the `EEP` lines for `05`, `06`, `03`, `02`, the `EXMEMCNT9` lines, the `.sav` probe page in hex). Commit 2 removes the probe and the call, and writes § 6 Step notes (plus any § 2 amendment), its message quoting the no-chip run. Neither commit includes `platform/nds/arm7/build/`.
- **Step notes content** (plan § 6, under "Phase 1 step 2"): the slot sequence that worked (or what had to change); `exm_before`/`exm_held`/after values; `dldi_io_type`, `dldi_features`, `boot_src`; the status bytes and data bytes for chip and no chip; whether any `A7` access appeared; the `cardWriteEeprom` hang risk; the verdict on § 2's sequence and `off`/`new` rule; the H-1-k outcomes.
- **Comments.** Per `CLAUDE.md` § Comments, the probe carries no explanatory comments; the reasoning goes in the commit messages.

## Implementation Chunks

1. **Gate.** Build the current tree in `dkp-lttp`, restore `platform/nds/arm7/build`, check H-1-1 to H-1-5 with the existing instrumentation (header bytes, `Game code`, `SaveMemType=2 homebrew=0`, `MARK9` lines, `nm` addresses). Add the melonDS instrumentation listed above and rebuild melonDS; a run of the unchanged ROM must show no `EEP`/`A9`/`A7` slot-1 SPI lines (nothing touches the EEPROM yet). Stop here if H-1-1, H-1-2 or H-1-3 fails. No commit.
2. **Probe, chip present.** Add the probe and its call, build, update the `MARK9` table from `nm`, run with the default `SaveMemType = 2`. Pass conditions below. Commit 1.
3. **No chip, notes, removal.** Run the same ROM with `M095_NOSAVE=1`. Write § 6 Step notes and any § 2 amendment. Remove the probe and its call, build, check boot once more (`MARK9 ZeldaHostTick`, no `EEP` line), restore `platform/nds/arm7/build`. Commit 2.

## Testing Decisions

- **No host tests.** This branch has no `tests/` directory; `make -C tests/rando test`/`e2e` do not exist, and `check-e2e` has nothing to run. The probe is throwaway and has no lasting interface to test. Verification is external behavior only: what melonDS 0.9.5 logs and writes when it runs the ROM.
- **Build.** `docker run --rm -v "$PWD":/src -w /src dkp-lttp bash -c 'python3 build.py -j8; chown -R <uid>:<gid> /src'`, then `git checkout -- platform/nds/arm7/build && git clean -f platform/nds/arm7/build`. The ROM `vendor/zelda3/assets/zelda3.sfc` and the image `dkp-lttp` are already present.
- **Chip-present run passes when**, in order in stdout:
  - `SaveMemType=2 homebrew=0` and `MARK9 ZeldaSaveMount`;
  - an `EXMEMCNT9` line clearing bit 11 (`0x0800`), then `A9` lines with `own7=0` only (no dropped access);
  - `EEP` lines for `RDSR` (`cmd=05`), `WREN` (`in=06`), `WRDI` (`in=04`), a read (`cmd=03`, 2 address bytes `1F E0`, 32 data bytes `out=FF`), a page write (`cmd=02`, address `1F E0`, 32 data bytes matching the table layout, `st` bit 1 set before the data), and a read-back returning those 32 bytes;
  - an `EXMEMCNT9` line setting bit 11 again;
  - no `A7` slot-1 SPI line between the two `EXMEMCNT9` lines;
  - `MARK9 ZeldaHostTick` (no hang, no boot regression) and `SaveManager: Written`;
  - the run directory's `.sav` is 8192 bytes, `0xFF` everywhere except `0x1FE0..0x1FFF`, which starts with `PRB1` and matches the bytes in the `EEP` write lines.
- **No-chip run passes when** stdout has `SaveMemType forced to 0`, the `EEP` lines show `type=0` returns, the `A9` reads show what the ARM9 actually got for status and data, `MARK9 ZeldaHostTick` still appears (the probe did not hang), and no `.sav` is written (or, if one exists, it is not 8192 bytes of probe data; whatever is observed is recorded). This run has no "expected value" to match: it measures.
- **Removal check.** After commit 2's build, a run shows `MARK9 ZeldaHostTick` and no `EEP` line, and `arm-none-eabi-nm lttp-ds.elf` no longer lists the probe function.
- **Log only.** Per plan § 2: stdout and files, never the screen, never a button. The instrumentation never enters the repository.
- **Prior art.** Commits `3d533cb` and `861b690` were verified the same way (headless melonDS 0.9.5 reaching `main` and `ZeldaHostTick`). The current `melon_write_sector` in `zelda_save.c` already borrows slot 1 for the ARM9 by clearing EXMEMCNT bit 11 inside a critical section; it is the in-tree precedent that melonDS 0.9.5 honors an ARM9 slot take.

## Out of Scope

- Rewriting `zelda_save.c` for the EEPROM, removing FAT/DLDI/TWiLight code, `ZeldaSavePrepare`, `fatInitDefault` or `-lfat` (Phase 2 step 1).
- The page-diff flush, the shadow copy, `Sram_RepairSlots` ordering (Phase 2 step 1).
- A save triggered through `ZeldaWriteSram`, and a relaunch that loads a seeded `.sav` (Phase 2 steps 2 and 3).
- Any `ZeldaSaveStatus` change; the probe does not touch `g_status`.
- README changes (Phase 3).
- The game code itself (Phase 1 step 1).
- Real hardware, flashcarts, nds-bootstrap, Delta, recent melonDS: the human tests those at the end of the plan.
- Other EEPROM sizes, FLASH or NAND save types, DSi mode.

## Further Notes

- The probe writes to page `0x1FE0` on every boot of the commit-1 ROM. That commit must not be released; commit 2 removes it in the same step. On a real cart or flashcart that maps an EEPROM, commit 1 would overwrite that page; it is past every byte the save format uses, so an existing save stays valid.
- If H-1-4 turns out false (the ARM7 declares slot 1 in `exmemcnt_mirror`), `sysSetCardOwner` steals the slot from the ARM7 driver. The probe then measures whether that still works on melonDS; the step notes say whether Phase 2 needs `ntrcardOpen` semantics instead, and the flashcart case (an ARM7 slot-1 DLDI driver with save emulation) is written down as a risk for the human test.
- The melonDS `.sav` file is written by a background thread at least 2 s after the last flush request; `pkill -9` skips the destructor that would write it otherwise. That is why the chip run waits for `SaveManager: Written`.
- `SRAMStatus` resets to `0x00` in `CartRetail::Reset`, and melonDS never sets the write-in-progress bit, so a write returns at once in the emulator. Real EEPROMs take a few milliseconds per page; Phase 2's flush must budget for that on hardware, which this step cannot measure.
