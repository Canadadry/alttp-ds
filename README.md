# LttP DS

A Nintendo DS (Phat / Lite) port of [snesrev/zelda3](https://github.com/snesrev/zelda3), a C reimplementation of *The Legend of Zelda: A Link to the Past*.

This repo does **not** include the game ROM or `zelda3_assets.dat`. You must extract assets from a USA SNES cartridge dump you own.

## Phase 4 (current)

The top screen is the game (256×224 cropped to 256×192). The bottom screen is a status HUD.

**B** sword, **A** lifts, opens chests, and dashes, **Y** uses the equipped item. **X** / **L** / **R** match the SNES X and shoulder buttons, **Start** opens the menu. Select+Start pauses. L+R+Select resets and keeps the save. L+R+Start+Select quits.

SRAM is an 8 KB `lttp-ds.sav` on the SD card. In melonDS, turn on DLDI (Emu settings → DLDI), enable folder sync, and point it at a folder. The save appears there as `lttp-ds.sav`. Naming a file returns to the file screen at once; the `.sav` is written on the next frame. Assets still come from NitroFS.

`build.bat` copies `zelda3_assets.dat` from the project root into NitroFS when that file exists (gitignored).

### Build

You need [devkitPro](https://devkitpro.org/) with **devkitARM** and **libnds** (this project is tested with libnds 2.0 on Windows).

```bat
build.bat
```

Or from an MSYS2 / Unix shell with `DEVKITARM` set:

```sh
make
```

That produces `lttp-ds.nds`.

### Run

- Emulator: [melonDS](https://melonds.melonemu.org/) (or DeSmuME).
- Hardware: flashcart with DLDI. NitroFS is inside the `.nds`. FAT only works when the cart’s DLDI driver is patched in (many loaders do this).

The save is `lttp-ds.sav` in the DLDI folder (or beside the ROM on a flashcart). The asset blob is inside the ROM via NitroFS.

### Assets (your dump)

On desktop, zelda3 already extracted:

- `zelda3.sfc` — USA ROM, SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`
- `python assets/restool.py --extract-from-rom` → `zelda3_assets.dat`

Do not commit those files.

### Submodule

```sh
git submodule update --init
```

Game logic lives in `vendor/zelda3` and is linked into the ROM.

The full porting plan is in [PORTING.md](PORTING.md).
