#!/usr/bin/env python3
"""Build lttp-ds.nds on Windows, macOS, or Linux."""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def fail(message):
    sys.stderr.write(message.rstrip() + "\n")
    return 1


def posix(path):
    text = str(path)
    if os.name == "nt":
        return text.replace("\\", "/")
    return text


def devkitpro():
    candidates = []
    if os.environ.get("DEVKITPRO"):
        candidates.append(Path(os.environ["DEVKITPRO"]))
    if os.name == "nt":
        candidates.append(Path(r"C:\devkitPro"))
        home = os.environ.get("USERPROFILE")
        if home:
            candidates.append(Path(home) / "devkitPro")
    else:
        candidates.append(Path("/opt/devkitpro"))
        candidates.append(Path.home() / "devkitpro")

    seen = set()
    for path in candidates:
        key = os.path.normcase(str(path))
        if key in seen:
            continue
        seen.add(key)
        if (path / "devkitARM" / "ds_rules").is_file():
            return path
    return None


def pause_if_windows_console():
    """Keep a double-clicked Windows console open long enough to read errors."""
    if os.name != "nt":
        return
    try:
        import ctypes
        from ctypes import wintypes
        kernel32 = ctypes.windll.kernel32
        processes = (wintypes.DWORD * 16)()
        count = kernel32.GetConsoleProcessList(processes, len(processes))
    except (AttributeError, OSError):
        return
    if count != 1:
        return
    try:
        input("Press Enter to close.")
    except EOFError:
        pass


def main(argv):
    dk = devkitpro()
    if dk is None:
        code = fail(
            "error: devkitPro with devkitARM was not found.\n"
            "Install the nds-dev package from https://devkitpro.org/\n"
            "Windows looks in C:\\devkitPro. macOS and Linux look in /opt/devkitpro.\n"
            "Or set DEVKITPRO to that directory."
        )
        pause_if_windows_console()
        return code

    env = os.environ.copy()
    env["DEVKITPRO"] = posix(dk)
    env["DEVKITARM"] = posix(dk / "devkitARM")
    env["PYTHON"] = posix(sys.executable)
    extra_path = [posix(dk / "devkitARM" / "bin"), posix(dk / "tools" / "bin")]
    make = "make"
    if os.name == "nt":
        make_exe = dk / "msys2" / "usr" / "bin" / "make.exe"
        if not make_exe.is_file():
            code = fail(
                "error: %s is missing. Reinstall devkitPro, including msys2."
                % make_exe
            )
            pause_if_windows_console()
            return code
        make = str(make_exe)
        extra_path.insert(0, posix(dk / "msys2" / "usr" / "bin"))
    env["PATH"] = os.pathsep.join(extra_path + [env.get("PATH", "")])

    try:
        result = subprocess.call([make] + argv, cwd=str(ROOT), env=env)
    except OSError as exc:
        code = fail("error: could not run make: %s" % exc)
        pause_if_windows_console()
        return code

    cleaning = "clean" in argv
    if result == 0 and not cleaning and (ROOT / "lttp-ds.nds").is_file():
        print("")
        print("Built %s" % (ROOT / "lttp-ds.nds"))
    elif result != 0 and not cleaning:
        print("")
        print("Build failed. lttp-ds.nds was not created.")
        print("Put your USA ROM at vendor/zelda3/assets/zelda3.sfc.")
    pause_if_windows_console()
    return result


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
