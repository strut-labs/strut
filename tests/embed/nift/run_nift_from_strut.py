#!/usr/bin/env python3
"""FFI-10B: Strut-origin Nift call. Compiles tests/embed/nift/nift_from_strut.p with the narrow
int64 adapter + libnift_c through the strut CLI's native-link options (--lib-path/--static-lib),
runs the produced executable, and requires the Nift-evaluated value 42 to be printed (exit 0)."""
import os, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("usage: run_nift_from_strut.py <strut_bin_dir> <nift_lib_dir> <source_dir>")
strut_bin = Path(sys.argv[1]).resolve()
nift_lib = Path(sys.argv[2]).resolve()
srcdir = Path(sys.argv[3]).resolve()
strut = strut_bin / ("strut.exe" if os.name == "nt" else "strut")
libnift = nift_lib / ("nift_c.lib" if os.name == "nt" else "libnift_c.so")

with tempfile.TemporaryDirectory(prefix="strut-nift-from-strut-") as td:
    td = Path(td)
    adapter = td / "nift_adapter.o"
    exe = td / "nift_from_strut"
    cc = os.environ.get("CXX", "g++")
    env = dict(os.environ, LD_LIBRARY_PATH=str(nift_lib))
    a = subprocess.run([cc, "-std=c++20", "-I" + str(nift_lib.parent / "include"),
                        "-c", str(srcdir / "nift_adapter.cpp"), "-o", str(adapter)],
                       capture_output=True, text=True)
    if a.returncode != 0:
        print("adapter build FAILED:", (a.stdout + a.stderr)[-1200:]); sys.exit(1)
    c = subprocess.run([str(strut), str(srcdir / "nift_from_strut.p"), "-o", str(exe),
                        "--lib-path", str(nift_lib), "--static-lib", str(adapter),
                        "--static-lib", str(libnift)], env=env, capture_output=True, text=True)
    if c.returncode != 0:
        print("strut compile FAILED:", (c.stdout + c.stderr)[-1500:]); sys.exit(1)
    r = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    out = r.stdout.strip()
    expected = "42\n25\n0\n125\n-103\n42\nFAILED\n42\nRECOVERED\nPARSE-OK\nhi!\n!\nhéllo!"
    if r.returncode != 0 or out != expected:
        print("nift_from_strut FAILED rc=%d out=%r" % (r.returncode, out)); sys.exit(1)
print("nift from strut ok")