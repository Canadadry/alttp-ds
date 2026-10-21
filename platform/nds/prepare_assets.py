"""Put zelda3_assets.dat where the NDS link can incbin it.

The file has to already exist, or a USA ROM has to be present so restool
can extract one. The .nds links those bytes into the ARM9 binary.
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOME = Path.home()
EMBED = ROOT / "build" / "embed" / "zelda3_assets.dat"
CANONICAL = ROOT / "zelda3_assets.dat"
EXTRACTED = ROOT / "vendor" / "zelda3" / "zelda3_assets.dat"
SIG = b"Zelda3_v0"
USA_SHA256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"

DAT_CANDIDATES = [
    CANONICAL,
    ROOT / "data" / "zelda3_assets.dat",
    EXTRACTED,
    HOME / "Desktop" / "zelda3_assets.dat",
    HOME / "Desktop" / "ALTTP-DS" / "zelda3_assets.dat",
]

ROM_CANDIDATES = [
    ROOT / "vendor" / "zelda3" / "zelda3.sfc",
    ROOT / "vendor" / "zelda3" / "zelda3.smc",
    ROOT / "zelda3.sfc",
    ROOT / "zelda3.smc",
    ROOT / "data" / "zelda3.sfc",
    ROOT / "data" / "zelda3.smc",
]


def fail(message):
    sys.stderr.write(message.rstrip() + "\n")
    sys.exit(1)


def first_file(paths):
    for path in paths:
        if path.is_file():
            return path
    return None


def valid_dat(path):
    if not path.is_file() or path.stat().st_size < 128:
        return False
    with path.open("rb") as handle:
        return handle.read(len(SIG)) == SIG


def publish(src):
    EMBED.parent.mkdir(parents=True, exist_ok=True)
    if EMBED.exists():
        src_stat = src.stat()
        dst_stat = EMBED.stat()
        if (src_stat.st_size == dst_stat.st_size and
                int(src_stat.st_mtime) == int(dst_stat.st_mtime)):
            return
    shutil.copy2(src, EMBED)


def extract(rom):
    restool = ROOT / "vendor" / "zelda3" / "assets" / "restool.py"
    if not restool.is_file():
        fail("error: vendor/zelda3 is missing. Run: git submodule update --init")
    print("assets: extracting zelda3_assets.dat from %s" % rom)
    subprocess.check_call(
        [sys.executable, str(restool), "--extract-from-rom", "--rom", str(rom)],
        cwd=str(ROOT / "vendor" / "zelda3"),
    )
    if not valid_dat(EXTRACTED):
        fail("error: extraction did not write a valid zelda3_assets.dat")
    if EXTRACTED.resolve() != CANONICAL.resolve():
        shutil.copy2(EXTRACTED, CANONICAL)
    return CANONICAL


def main():
    found = first_file(DAT_CANDIDATES)
    if found is not None:
        if not valid_dat(found):
            fail("error: %s is not a zelda3_assets.dat extracted from a USA ROM." % found)
        print("assets: using %s" % found)
        publish(found)
        return

    rom = first_file(ROM_CANDIDATES)
    if rom is None:
        fail(
            "error: this build needs game data from a USA A Link to the Past ROM you own.\n"
            "\n"
            "Copy zelda3_assets.dat to the project root or your Desktop, or copy\n"
            "your ROM to vendor/zelda3/zelda3.sfc.\n"
            "USA ROM SHA-256: %s\n"
            "\n"
            "The ROM is only used to extract zelda3_assets.dat. Neither file is committed."
            % USA_SHA256
        )
    publish(extract(rom))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
