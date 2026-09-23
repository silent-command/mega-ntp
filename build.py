#!/usr/bin/env python3
"""Build driver for mega-ntp: Python, so that Windows, Linux and macOS
are all first-class development hosts.

    python3 build.py            build the client and bin/NTP.D81
    python3 build.py test       the host suite for the date and offset arithmetic
    python3 build.py clean

It needs llvm-mos (mos-mega65-clang), CMake, c1541 from VICE, and the two
sibling checkouts ../mega-net and ../mega65-libc; it builds mega65-libc
and mega-net's image itself when they are missing. The test needs a host
C compiler (cc, clang or gcc).

Overrides: LLVM_MOS_DIR, MEGANET (path to the mega-net checkout),
LIBC_SRC, C1541, CC.
"""
import os, platform, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
BIN = ROOT / "bin"
IS_WINDOWS = platform.system() == "Windows"
MEGANET = Path(os.environ.get("MEGANET", ROOT.parent / "mega-net")).resolve()
LIBC_SRC = Path(os.environ.get("LIBC_SRC", ROOT.parent / "mega65-libc")).resolve()
LIBC_BUILD = BUILD / "libc"


def die(msg):
    print(f"error: {msg}", file=sys.stderr); sys.exit(1)


def run(cmd, **kw):
    print("  " + " ".join(Path(c).name if os.sep in str(c) else str(c) for c in cmd))
    if subprocess.run([str(c) for c in cmd], **kw).returncode != 0:
        die("command failed")


def find_tool(exe, roots=(), env=None):
    if env and os.environ.get(env):
        return os.environ[env]
    name = exe + (".exe" if IS_WINDOWS else "")
    for r in roots:
        cand = Path(r) / "bin" / name
        if cand.is_file():
            return str(cand)
    found = shutil.which(name)
    if found:
        return found
    for cand in ("/opt/homebrew/bin/" + name, "/usr/local/bin/" + name):
        if Path(cand).is_file():
            return cand
    die(f"{exe} not found")


def mos_clang():
    roots = [os.environ["LLVM_MOS_DIR"]] if os.environ.get("LLVM_MOS_DIR") else []
    roots += [Path.home() / "llvm-mos", "/opt/llvm-mos", "/usr/local/llvm-mos"]
    return find_tool("mos-mega65-clang", roots)


def check_rmw(elf):
    """ssh REQUIREMENTS.md 5.6: the compiler once decremented the wrong
    zero-page word in a loop; tools/orphan_rmw.py finds that shape in a
    linked ELF."""
    if not elf.exists():
        return
    objdump = find_tool("llvm-objdump", [Path(mos_clang()).parent.parent])
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "orphan_rmw.py"), str(elf), objdump])
    if r.returncode:
        die(f"{elf.name}: the loop miscompile of ssh 5.6 is back; rewrite the loop it names")


def ensure_libc():
    lib = LIBC_BUILD / "src" / "libmega65libc.a"
    if lib.is_file():
        return lib
    if not LIBC_SRC.is_dir():
        die(f"mega65-libc not found at {LIBC_SRC}; set LIBC_SRC")
    print("building mega65-libc for llvm-mos:")
    prefix = Path(mos_clang()).parent.parent
    run(["cmake", f"-DCMAKE_PREFIX_PATH={prefix}", "-B", LIBC_BUILD, "-S", LIBC_SRC])
    run(["cmake", "--build", LIBC_BUILD])
    return lib


def ensure_meganet():
    image = MEGANET / "build" / "m65" / "meganet.bin"
    tramp = MEGANET / "build" / "gen" / "meganet_tramp.c"
    if not (image.is_file() and tramp.is_file()):
        if not MEGANET.is_dir():
            die(f"mega-net not found at {MEGANET}; set MEGANET")
        print("building mega-net:")
        run([sys.executable, "build.py", "abi"], cwd=MEGANET)
    return image, tramp


def cflags():
    return ["-Oz", "-I", str(LIBC_SRC / "include"), "-I", str(MEGANET / "src" / "abi"),
            "-I", str(MEGANET / "build" / "gen"), "-I", str(ROOT / "src" / "platform"), "-I", str(ROOT / "src"),
            "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter"]


def build_client():
    clang = mos_clang(); lib = ensure_libc(); image, tramp = ensure_meganet()
    BIN.mkdir(exist_ok=True)
    srcs = sorted(str(p) for p in (ROOT / "src").glob("*.c"))
    platform_srcs = sorted(str(p) for p in (ROOT / "src" / "platform").glob("*.c"))
    prg = BIN / "ntp.prg"
    print("client:")
    run([clang] + cflags() + srcs + platform_srcs +
        [str(tramp), str(MEGANET / "src" / "abi" / "meganet_vectors.c"), str(lib),
         f"-Wl,-Map={BIN / 'ntp.map'}", "-o", str(prg)])
    print(f"  {prg.name}: {prg.stat().st_size} bytes")
    check_rmw(prg.with_suffix(".prg.elf"))
    c1541 = find_tool("c1541", env="C1541")
    d81 = BIN / "NTP.D81"
    shutil.copy(image, BIN / "meganet")
    if d81.exists():
        d81.unlink()
    run([c1541, "-format", "ntp,nt", "d81", d81, "-write", prg, "ntp", "-write", BIN / "meganet", "meganet"],
        stdout=subprocess.DEVNULL)
    run([c1541, "-attach", d81, "-dir"])
    return 0


def build_test():
    cc = os.environ.get("CC") or next((c for c in ("cc", "clang", "gcc") if shutil.which(c)), None)
    if not cc:
        die("no host C compiler (cc, clang or gcc); set CC")
    out = BUILD / "host"; out.mkdir(parents=True, exist_ok=True)
    exe = out / ("test_timecalc" + (".exe" if IS_WINDOWS else ""))
    print("host suite:")
    run([cc, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-I", ROOT / "src",
         ROOT / "tests" / "test_timecalc.c", ROOT / "src" / "timecalc.c", "-o", exe])
    return subprocess.run([str(exe)]).returncode


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "client"
    if target == "client":
        return build_client()
    if target == "test":
        return build_test()
    if target == "clean":
        shutil.rmtree(BUILD, ignore_errors=True); shutil.rmtree(BIN, ignore_errors=True); return 0
    print(__doc__); die(f"unknown target '{target}'")


if __name__ == "__main__":
    sys.exit(main())
