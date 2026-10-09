# LttP DS

A Nintendo DS (Phat / Lite) port of [snesrev/zelda3](https://github.com/snesrev/zelda3), a C reimplementation of *The Legend of Zelda: A Link to the Past*.

This repo does **not** include the game ROM or `zelda3_assets.dat`. You must extract assets from a USA SNES cartridge dump you own.

## Phase 1 (current)

Bring-up ROM: top-screen framebuffer, bottom-screen log, keypad, NitroFS, and a FAT probe for `zelda3_assets.dat`.

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

Put `zelda3_assets.dat` next to the `.nds` or in `/lttp-ds/` on the SD card when you are ready for later phases. The Phase 1 ROM only reports whether it can open that file.

### Assets (your dump)

On desktop, zelda3 already extracted:

- `zelda3.sfc` — USA ROM, SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`
- `python assets/restool.py --extract-from-rom` → `zelda3_assets.dat`

Do not commit those files.

### Submodule

```sh
git submodule update --init
```

Game logic lives in `vendor/zelda3` and is not linked yet.

The full porting plan is in [PORTING.md](PORTING.md).
