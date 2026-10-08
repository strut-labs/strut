#!/usr/bin/env python3
"""FFI-5 closure: Strut -> native callbacks through the SAME representation.

A Strut function value (here a capturing lambda `x => x * base`, base = 3) is passed to a
native C function taking `int32_t (*)(void*, int32_t)` + context; the native side invokes it
synchronously. Expected: 21. The context (a std::function) is stack-scoped around the call.
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_callback_bidir_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "21\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-cb-bidir-") as td:
    td = Path(td)
    src = root / "export_callback_native.c"
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/c", str(src), "/Fo:" + str(td / "native.obj")], check=True)
        subprocess.run(["lib", "/nologo", "/OUT:" + str(td / "ffi_cbnative.lib"), str(td / "native.obj")], check=True)
        exe = td / "prog.exe"
        subprocess.run([str(compiler), "--lib", "ffi_cbnative", "--lib-path", str(td), "-o", str(exe), "export_callback_extern.p"], check=True, cwd=str(root))
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-c", str(src), "-o", str(td / "native.o")], check=True)
        subprocess.run(["ar", "rcs", str(td / "libffi_cbnative.a"), str(td / "native.o")], check=True)
        exe = td / "prog"
        subprocess.run([str(compiler), "--lib", "ffi_cbnative", "--lib-path", str(td), "-o", str(exe), "export_callback_extern.p"], check=True, cwd=str(root))
    out = subprocess.check_output([str(exe)], text=True)
    if out != expected:
        raise SystemExit(f"unexpected callback bidir output: {out!r}")

print("bidirectional callback ABI passed (Strut capturing lambda -> native fn-pointer + context)")
