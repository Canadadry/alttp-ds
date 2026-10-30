# Plan 1 — Cartridge EEPROM save

## 1. Goal

Make saves work on emulators that give a homebrew no SD card: Delta on iOS, RetroArch's `melonds` core on the Miyoo Mini Flip, and any melonDS without DLDI. Those emulators run melonDS 0.9.5 or a close relative. The ROM saves the way a retail DS game does: it reads and writes an 8 KB EEPROM on the cartridge's SPI bus. The emulator detects that EEPROM and stores it as its own `.sav`.

This plan follows the boot fix already on branch `melonds-0.9.5-boot`, commits `3d533cb` and `861b690`. Those commits make the ROM boot on melonDS 0.9.5. Three calico 1.2.0 objects are overridden in `platform/nds/calico/`: 16-bit `IPCSYNC` in both bootstubs, plus the IRQ mask kept on the stack and no `ldm sp!, {..pc}^` in the ARM9 IRQ handler. The user confirmed that the ROM boots and has sound on melonDS 0.9.5 and on Delta. Saves failed on Delta because the ROM is a homebrew (game code `####`): melonDS gives a homebrew no save memory, and Delta has no DLDI SD card.

## 2. Rules

**Locked decisions**

- **One ROM for every target.** `lttp-ds.nds` is the only output. There is no separate emulator build.
- **Game code `ZLTP`.** `ndstool` gets `-g ZLTP`, through the `$(OUTPUT).nds` rule in the root `Makefile`, around line 147. Here is why. melonDS treats a ROM as homebrew only if its ARM9 ROM offset is below `0x4000` or its game code is `####`. That check is `NDSCart.cpp:1626` in 0.9.5 and `NDSHeader::IsHomebrew` in `src/NDS_Header.h` in 1.x. The ARM9 is already at `0x4000`. With a game code that is not in its ROM list, melonDS treats the ROM as retail and assumes `SaveMemType = 2`, a 64 kbit (8 KB) EEPROM. That is `NDSCart.cpp:1639` in 0.9.5, and the same fallback exists in 1.x. `ZLTP` was checked against both melonDS ROM lists (6778 codes): no retail code starts with `Z`.
- **Rejected: an existing retail game code.** It would give a known save type everywhere, but per-game patches in nds-bootstrap and flashcarts, per-code emulator hardware such as melonDS IR on `I…` codes, and frontend metadata would all target the other game.
- **Rejected: keeping the file as a fallback, or two ROMs.** The user chose to write as if only a cartridge EEPROM exists.
- **The file goes entirely.** In `platform/nds/source/zelda_save.c`, delete:
  - FAT geometry and directory code, the cluster chain, `touch_dir`;
  - the TWiLight Menu++ `settings.ini` and save-location logic, argv and nds-bootstrap parsing;
  - stdio read and write;
  - the legacy `fat:/lttp-ds.sav` and `saves/sram.dat` reads;
  - `melon_write_sector` and the `dldi_io_type` detection.

  Also delete `ZeldaSavePrepare` from `zelda_save.h`, `fatInitDefault()` and `ZeldaSavePrepare()` from `platform/nds/source/main.c`, `<fat.h>`, and `-lfat` from the root `Makefile`'s `LIBS`. `main` keeps `argc` and `argv` only if something else needs them.
- **The 8 KB image layout does not change.**
  - The three slots and their mirrors take bytes up to `0x1DFF`.
  - The settings block starts at `kSetOff = 0x1E00`, with `kSetVersion = 8`, the spawn exit snapshots and the button map.
  - `Sram_RepairSlots` still runs before a write.

  The EEPROM holds exactly the bytes the `.sav` file held, so an old `.sav` can be copied as-is into an emulator's save slot.
- **The public API stays.** `ZeldaSaveMount`, `ZeldaSaveFlush`, `ZeldaSaveStatus` (`off`, `new`, `loaded`, `saved`, `fail`), `__wrap_ZeldaWriteSram`, and every settings, spawn and button-map function keep their signatures and callers. `zelda_host.c` calls Mount and Flush.
- **EEPROM access.**
  - Use libnds `cardReadEeprom` and `cardWriteEeprom` (`nds/card.h`) with `addrtype` 2: 2-byte address, 32-byte pages.
  - The ARM9 takes the card slot with `sysSetCardOwner(BUS_OWNER_ARM9)` around each access, then gives it back to the ARM7 (`sysSetCardOwner(BUS_OWNER_ARM7)`). Calico leaves the slot to the ARM7.
  - Mount reads all 8 KB once into `g_zenv.sram` and into a shadow copy.
  - Flush runs after the frame, as today. It writes only the 32-byte pages that differ from the shadow, then updates the shadow.
  - If no EEPROM answers (for example, every byte reads `0xFF` and the status register reads `0xFF`), the status is `off` and nothing is written. If every byte is `0xFF` but the chip answers, it is a blank EEPROM and the status is `new`.
- **The three calico overrides in `platform/nds/calico/` stay as they are.**
- **Accepted consequence.** On real hardware, the save depends on the flashcart's or nds-bootstrap's save emulation for an unknown game code. If one of them assumes something other than a 64 kbit EEPROM, saves will not work there. Nobody can verify this in an agent session. The README says it.
- **Comments** follow `CLAUDE.md` § Comments. The header comment of `zelda_save.c` is rewritten to the new contract: the save is the cartridge's 8 KB EEPROM. All FAT, TWiLight and melonDS-folder-sync prose goes.

**How to build and check (no devkitPro on the host)**

- **The ROM.** The build needs the user's USA ROM at `vendor/zelda3/assets/zelda3.sfc`, SHA-256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`. It is not in git: ask the user to upload it in the session. Run `git submodule update --init vendor/zelda3` first.
- **The build.**
  1. Start Docker with `setsid nohup dockerd >/dev/null 2>&1 </dev/null &`. A plain `&` dies with the shell.
  2. Pull `mirror.gcr.io/devkitpro/devkitarm:latest`. Docker Hub rate-limits, and `pkg.devkitpro.org` is behind Cloudflare and refuses the proxy. That image has calico 1.2.0 and libnds 2.0.2.
  3. In a container, install `python3-pil` and `python3-yaml` with `apt-get`, then `docker commit` it as a local image.
  4. Build with `docker run --rm -v "$PWD":/src -w /src <image> bash -c 'python3 build.py -j8; chown -R <uid>:<gid> /src'`.
- **Restore tracked artifacts after each build.** `platform/nds/arm7/build/` holds objects tracked by git that every build rewrites. Run `git checkout -- platform/nds/arm7/build && git clean -f platform/nds/arm7/build` after each build. Never commit them.
- **The emulator check.**
  1. Build melonDS 0.9.5 from source: `git clone https://github.com/melonDS-emu/melonDS`, then `git worktree add ../m095 0.9.5`.
  2. Install the apt dependencies: `qtbase5-dev qtbase5-private-dev qtmultimedia5-dev libsdl2-dev libslirp-dev libarchive-dev libepoxy-dev libzstd-dev extra-cmake-modules libwayland-dev`.
  3. Configure with `cmake -B build -DENABLE_JIT=OFF` and keep the OpenGL renderer ON, or the link fails.
  4. Run it as `QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy stdbuf -oL -eL melonDS rom.nds > out.txt 2>&1 &`, then kill it with `pkill -9 melonDS`. `timeout` does not stop it. When a check reads the emulator's `.sav`, kill only after stdout shows `SaveManager: Written` (with a time limit): 0.9.5 writes the file from a background thread at least 2 s after the last flush request, or in a destructor that `pkill -9` skips. The instrumented EEPROM SPI write path is the main proof that the ROM wrote; the file is a second check.
- **Read only the console.** The user allowed melonDS as an exception to `CLAUDE.md`'s "Never launch the game", for this work only and for **log-only checks**: read stdout and files, never judge the screen, never play.
  - Instrumentation printfs added to melonDS's own source are fine. Examples: a `printf` in `ARMv5::JumpTo` when the PC hits a symbol address taken from `arm-none-eabi-nm lttp-ds.elf`, or in the EEPROM SPI handler.
  - To trigger a save without a gamepad, seed the emulator's `.sav`, or call `ZeldaWriteSram` through instrumentation. Never press buttons.
- **No rando tests here.** `make -C tests/rando test` and `e2e` do not exist on this branch (`tests/` is absent), so `check-e2e` has nothing to run. The build plus the headless melonDS checks are the verification.

**Out of scope**

- DSi-mode saves, SD saves on DSi, and any FAT path.
- Changing the save image format.
- Migrating old `.sav` files by code. Copying the file by hand is enough, and the README says how.
- Upstreaming the calico fixes.

## 3. Phases and steps

### Phase 1 — The ROM exposes an EEPROM

1. **`ndstool` writes game code `ZLTP`.**
   - The `.nds` header bytes `0x0C..0x0F` read `ZLTP`. Check with a Python one-liner.
   - On headless melonDS 0.9.5, stdout prints `Game code: ZLTP`, and instrumentation prints `romparams.SaveMemType == 2`. (`ROM entry not found` still prints: `ZLTP` is in no ROM list, so that line does not tell homebrew from retail.)
   - The ROM still boots: instrumentation shows `main` and `ZeldaHostTick` reached, as on the current branch.

2. **The ARM9 reaches the EEPROM (probe, old save code still in place).**
   - Add a minimal throwaway probe on the current code: read the EEPROM status register, read one page, write one page. It goes through exactly the libnds calls and the slot-ownership sequence of § 2, the path Phase 2 will ship; if any of them must change to work, that change is the result.
   - Verified with instrumentation on melonDS 0.9.5's EEPROM SPI handler: the read and write commands arrive, and the `.sav` changes.
   - Run the probe a second time with `romparams.SaveMemType` forced to 0, and record what the status register and data reads return with no chip. In 0.9.5, `CartRetail::SPIWrite` returns 0 when `SRAMType == 0`, so they may read `0x00`, not the `0xFF` § 2 assumes.
   - Two known doubts drive this step:
     - calico hands the slot between the CPUs through `ntrcardOpen`/`ntrcardClose` and a cross-CPU `exmemcnt_mirror` (`source/nds/ntrcard.c`), which a direct `sysSetCardOwner` bypasses;
     - melonDS 0.9.5 drops ARM9 writes to `0x040001A0`/`0x040001A2` unless EXMEMCNT bit 11 is clear.
   - Record in § 6 Step notes how the slot must be taken and what a missing chip reads, and check § 2's slot sequence and `off`/`new` rule against them before Phase 2 starts. Remove the probe once it has answered.

### Phase 2 — Save through the EEPROM only

1. **Rewrite `zelda_save.c` for the EEPROM.**
   - The file part goes and the EEPROM part arrives, as in § 2.
   - `ZeldaSavePrepare`, `fatInitDefault` and `-lfat` are removed.
   - Verified by:
     - the build passing;
     - `arm-none-eabi-nm lttp-ds.elf` showing no `fatInitDefault`, `blkDevWriteSectors` or `melon_write_sector`;
     - headless melonDS 0.9.5 reaching `main` and `ZeldaHostTick` (no boot regression).
2. **A save reaches the emulator's `.sav`.**
   - Trigger `ZeldaWriteSram` once through emulator instrumentation, or by any means that is not playing.
   - Then check that melonDS 0.9.5 wrote `<rom>.sav`, and that its bytes match what the ROM wrote. For example, seed a recognizable settings block and compare.
   - Instrumentation on melonDS's EEPROM write path shows writes only on changed 32-byte pages.
3. **A relaunch loads it.**
   - Put a known 8 KB image as `<rom>.sav`: a valid slot, or a settings block with a non-default value.
   - Relaunch, and check from stdout instrumentation that `ZeldaSaveMount` reads it: the status is `loaded`, and the setting is applied, for example the `ZeldaHudOnTop` value.
   - A blank `.sav` (all `0xFF`) gives `new`.
   - With no save memory (force `romparams.SaveMemType = 0` through instrumentation, `NDSCart.cpp:1639` in 0.9.5), the ROM still reaches `ZeldaHostTick` and the status is `off`. Then trigger a save as in step 2: the EEPROM SPI instrumentation sees no write-enable or page-write command, and the ROM keeps reaching `ZeldaHostTick` after the flush.

### Phase 3 — Docs

1. **README.**
   - § Saves becomes: the save is the cartridge's 8 KB EEPROM, stored by the emulator or flashcart as its `.sav`; an older build's `.sav` file can be copied in place of it.
   - Add a line that real-hardware saves depend on the flashcart's save emulation for an unknown game code.
   - § What you need lists melonDS 0.9.5+ (RetroArch `melonds` core, Delta) as supported.
   - Remove the TWiLight Menu++ save-location and melonDS folder-sync text, and update the `zelda_save.c` row in the layout section if one describes FAT.
   - Verified by reading the diff.

## 4. Done when

- `python3 build.py` builds in the devkitarm image, and the header game code is `ZLTP`.
- No FAT, DLDI or TWiLight save code remains: `grep -n "fat\|dldi\|TWiLight\|melon" platform/nds/source/zelda_save.c platform/nds/source/main.c` finds nothing relevant, and `-lfat` is gone.
- On headless melonDS 0.9.5, read from stdout and files only:
  - the ROM boots to `ZeldaHostTick`;
  - a save writes `<rom>.sav` (8192 bytes);
  - a relaunch loads it.
  - with no save memory exposed, the ROM still boots, the status is `off`, and a triggered save sends no EEPROM write.
- The README is updated.
- **Waits on the human:** the user plays on Delta and saves, quits and reloads. It is also checked on a recent melonDS and, if possible, on a flashcart.

## 5. Open questions

- None about the decisions. Two facts are unknown: whether Delta's melonDS core has its own save-type database that would override the 8 KB default for an unknown code, and what each flashcart assumes. Only the human test answers these.

## 6. Step notes
