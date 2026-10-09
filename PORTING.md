# LttP DS porting plan

Port [snesrev/zelda3](https://github.com/snesrev/zelda3) to **Nintendo DS Phat and DS Lite**. zelda3 is a C reimplementation of *The Legend of Zelda: A Link to the Past*, not a ROM emulator.

Hardware target is original DS (ARM9 67 MHz, 4 MB RAM, 256×192 × 2). Vita / Switch / 3DS ports of zelda3 are useful as *workflow* references, not as something to compile as-is.

## What zelda3 actually is

- ~70–80k lines of C that reimplements ALttP’s game logic
- LakeSnes **PPU** that still draws SNES frames in software (256×224, layers, sprites, color math, Mode 7)
- LakeSnes **DSP** for audio
- **SDL2** on PC for window, input, audio, and an ARGB8888 framebuffer
- **`zelda3_assets.dat`** extracted from a USA `zelda3.sfc` (SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`)

The ROM is only for extraction. Never commit or ship the ROM or `zelda3_assets.dat`.

Upstream extras we are **not** porting: shaders, widescreen, 240-line mode, hi-res Mode 7, MSU-1 / Opus, OpenGL, CPU-compare / ROM side-by-side.

## Why not copy the Vita port

[Rinnegatamante/zelda3](https://github.com/Rinnegatamante/zelda3) is Makefile + SDL2 + path wrapping. That works on Vita because of RAM and CPU. DS has neither SDL2 nor that budget.

Keep zelda3 as `vendor/zelda3` (git submodule). All DS code lives in `platform/nds/`. **Replace** `src/main.c`, SDL keycodes in `config.h`, OpenGL, Opus, and the CPU-infra path. Game files (`player.c`, `dungeon.c`, …) stay as untouched as possible.

## Architecture

```
alttp-ds/
  vendor/zelda3/          # submodule — game + snes/ppu + snes/dsp
  platform/nds/
    source/               # DS main, video, input, FS, later audio
    nitrofiles/           # files baked into the .nds
  Makefile                # devkitARM / libnds
  data/                   # local only: zelda3_assets.dat (gitignored)
```

Do not use SDL on DS. Thin platform layer:

| SNES / zelda3 | DS |
|---|---|
| 256×224 RGB framebuffer | 256×192 RGB15, crop 32 lines |
| SDL window | libnds `MODE_FB0` (later 2D engine if needed) |
| SDL audio 44.1 kHz stereo | DSP mix at 16–22 kHz, ARM7 PCM |
| SDL keys / pad | DS keys; touch later |
| `zelda3_assets.dat` on disk | NitroFS and/or DLDI FAT (`fat:/`) |

### Screens

- **Top:** playfield, cropped 256×192
- **Bottom:** HUD (hearts, rupees, items) from the cropped strip or from game RAM; later inventory / map on touch

### Audio (later)

Disable MSU. Keep DSP on ARM9 first; ARM7 only plays the buffer. Lower rate/mono if needed.

### RAM

Compile out GL, Opus, MSU, CPU infra. Use `-ffunction-sections -fdata-sections -Wl,--gc-sections`. Prefer RGB15 PPU output (not ARGB8888). Load `zelda3_assets.dat` from SD; page it if a full load does not fit in 4 MB.

## Toolchain

Windows: **devkitPro devkitARM + libnds 2.0** (already used for Phase 1).

```bat
build.bat
```

`build.bat` sets `DEVKITPRO=C:/devkitPro` because the user env still has Unix-style `/opt/devkitpro/...`.

Test in melonDS, then on a DLDI flashcart. Keep a PC build of stock zelda3 as the behavior oracle.

## Legal

zelda3 code is MIT. Nintendo assets are not. README tells users to extract from their own USA ROM. `.gitignore` covers `.sfc`, `.smc`, `.dat`, `.nds`.

## Phases

### Phase 0 — Assets (done locally)

Extract with upstream `assets/restool.py`. Desktop copy lives in `ALTTP-DS/` (`zelda3_assets.dat` ~684 KB). Not in git.

### Phase 1 — Hello DS (done)

Bring-up `.nds`: top framebuffer, bottom console, keypad, NitroFS `hello.txt`, FAT probe for `zelda3_assets.dat`. zelda3 is a submodule but **not linked**.

### Phase 2 — Compile the game, no video yet (done)

zelda3 is linked (excluding `main.c`, OpenGL, CPU infra). SDL/Opus are stubbed in `platform/nds/include`. Host glue in `zelda_host.c` loads `zelda3_assets.dat` (NitroFS / FAT) and calls `ZeldaInitialize()`. No gameplay frame is drawn yet.

**Exit:** links, boots, does not crash before the first frame (black screen is OK).

### Phase 3 — First picture

1. `ZeldaRunFrame` (or equivalent), then blit PPU output to RGB15 VRAM.
2. Crop 224→192.
3. D-pad + ABXY + Start.
4. Frameskip if under 60 Hz.

**Exit:** title screen / file select visible in melonDS.

### Phase 4 — Make it a game

Full button map, save/load on SD, low-rate audio, dual-screen HUD, pause / reset.

**Exit:** leave the house, sword, first dungeon without a crash.

### Phase 5 — Performance

RGB15 PPU, skip off-screen lines, fast renderer only, frameskip / 30 fps if needed. Assembly only after C is correct. Hardware BG path only if software PPU cannot hold playable speed.

**Exit:** overworld + a dungeon at a stable rate on DS Phat/Lite hardware.

### Phase 6 — DS-native extras (optional)

Touch inventory, map on bottom screen. No shaders, MSU, or widescreen.

## References

| Source | Use |
|---|---|
| [snesrev/zelda3](https://github.com/snesrev/zelda3) | Game logic, PPU, DSP, asset tool |
| [Rinnegatamante/zelda3](https://github.com/Rinnegatamante/zelda3) (Vita) | Asset workflow, feature stripping — not SDL |
| [ProbablyRohan/Z3R-3ds](https://github.com/ProbablyRohan/Z3R-3ds) | Proof ARM homebrew can run zelda3 in software |
| SNEmulDS | Proof ALttP can run on DS; ARM7 audio ideas later |
