#!/usr/bin/env python3
"""FFI-10A: Nift-origin call into a Strut-exported C function.

Builds tests/embed/nift/export_ffi10.p into a shared library with the strut CLI, compiles the
small engine-driver host (tests/embed/nift/nift_to_strut.c) against libnift_c, and requires the
Nift SCRIPT (ffi_open/ffi_call/ffi_close inside the evaluated Nift source) to call the Strut add
symbol and yield 42. Exit code and expected output are both enforced."""
import os, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("usage: run_nift_to_strut.py <strut_bin_dir> <nift_lib_dir> <source_dir>")
strut_bin = Path(sys.argv[1]).resolve()
nift_lib = Path(sys.argv[2]).resolve()
srcdir = Path(sys.argv[3]).resolve()
strut = strut_bin / ("strut.exe" if os.name == "nt" else "strut")
nift_inc = nift_lib.parent / "include"
nift_impl = nift_lib / ("nift_c.lib" if os.name == "nt" else "nift_c")

with tempfile.TemporaryDirectory(prefix="strut-nift-to-strut-") as td:
    td = Path(td)
    libso = td / ("strut_ffi10." + ("dll" if os.name == "nt" else "so"))
    env = dict(os.environ, LD_LIBRARY_PATH=str(nift_lib))
    s = subprocess.run([str(strut), str(srcdir / "export_ffi10.p"), "--shared", "-o", str(libso)],
                       env=env, capture_output=True, text=True)
    if s.returncode != 0:
        print("strut export build FAILED:", (s.stdout + s.stderr)[-1500:]); sys.exit(1)
    host = td / "nift_to_strut"
    h = subprocess.run([os.environ.get("CC", "gcc"), "-I" + str(nift_inc),
                        str(srcdir / "nift_to_strut.c"), "-L" + str(nift_lib),
                        "-l" + str(nift_impl.name), "-Wl,-rpath," + str(nift_lib), "-o", str(host)],
                       env=env, capture_output=True, text=True)
    if h.returncode != 0:
        print("host build FAILED:", (h.stdout + h.stderr)[-1500:]); sys.exit(1)
    r = subprocess.run([str(host), str(libso)], env=env, capture_output=True, text=True)
    out = r.stdout.strip()
    if r.returncode != 0 or out != "nift strut ok":
        print("nift_to_strut FAILED rc=%d out=%r stderr=%r" % (r.returncode, out, r.stderr.strip()[-800:]))
        sys.exit(1)
print("nift strut ok")