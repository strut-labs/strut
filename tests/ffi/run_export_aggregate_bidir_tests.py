#!/usr/bin/env python3
"""FFI-3 closure: the SAME ABI aggregate representation serves BOTH directions.

This test covers Strut -> native (an `extern "C"` declaration calling a native C library that
takes/returns a struct) and reuses the exact same primitive-only POD aggregate rule and
generated ABI name that the native -> Strut `export "C"` tests use. The native C library's
struct layout must match the generated ABI POD; Strut must not leak its internal struct across
the boundary. Expected output: "7" then "11".
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_aggregate_bidir_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "7\n11\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-agg-bidir-") as td:
    td = Path(td)
    src = root / "export_agg_native.c"
    prog = root / "export_agg_extern.p"
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/c", str(src), "/Fo:" + str(td / "native.obj")], check=True)
        subprocess.run(["lib", "/nologo", "/OUT:" + str(td / "ffi_nativeagg.lib"), str(td / "native.obj")], check=True)
        exe = td / "prog.exe"
        subprocess.run([str(compiler), "--lib", "ffi_nativeagg", "--lib-path", str(td), "-o", str(exe), str(prog)], check=True)
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-c", str(src), "-o", str(td / "native.o")], check=True)
        subprocess.run(["ar", "rcs", str(td / "libffi_nativeagg.a"), str(td / "native.o")], check=True)
        exe = td / "prog"
        subprocess.run([str(compiler), "--lib", "ffi_nativeagg", "--lib-path", str(td), "-o", str(exe), str(prog)], check=True)
    out = subprocess.check_output([str(exe)], text=True)
    if out != expected:
        raise SystemExit(f"unexpected bidir output: {out!r}")

print("bidirectional aggregate ABI passed (Strut->native extern \"C\" uses the same ABI POD)")
