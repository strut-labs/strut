#!/usr/bin/env python3
"""FFI-4 closure: the SAME borrowed primitive pointer ABI serves Strut -> native too.

An `extern "C"` declaration taking `raw_ptr<int_32>` / `ref<int_32>` calls a native C library
whose parameters are `int32_t*`. Proves addressability, in-place mutation, and that the
internal representation is not lowered by value. Expected output: "42" then "43".
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_pointer_bidir_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "42\n43\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-ptr-bidir-") as td:
    td = Path(td)
    src = root / "export_pointer_native.c"
    prog = root / "export_pointer_extern.p"
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/c", str(src), "/Fo:" + str(td / "native.obj")], check=True)
        subprocess.run(["lib", "/nologo", "/OUT:" + str(td / "ffi_ptnative.lib"), str(td / "native.obj")], check=True)
        exe = td / "prog.exe"
        subprocess.run([str(compiler), "--lib", "ffi_ptnative", "--lib-path", str(td), "-o", str(exe), str(prog)], check=True)
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-c", str(src), "-o", str(td / "native.o")], check=True)
        subprocess.run(["ar", "rcs", str(td / "libffi_ptnative.a"), str(td / "native.o")], check=True)
        exe = td / "prog"
        subprocess.run([str(compiler), "--lib", "ffi_ptnative", "--lib-path", str(td), "-o", str(exe), str(prog)], check=True)
    out = subprocess.check_output([str(exe)], text=True)
    if out != expected:
        raise SystemExit(f"unexpected bidir pointer output: {out!r}")

print("bidirectional pointer ABI passed (Strut->native extern \"C\" raw_ptr/ref -> int32_t*)")
