#!/usr/bin/env python3
"""FFI-6 closure: native C failure -> Strut checked error.

A native C function returns a status + fills a native error descriptor; the Strut `extern "C"`
wrapper converts a nonzero status into the declared Strut checked error, and Strut `try/catch`
observes the concrete type/message/code. Expected output: "30" then "bad" then "7".
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_error_bidir_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "30\nbad\n7\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-err-bidir-") as td:
    td = Path(td)
    src = root / "export_error_native.c"
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/c", str(src), "/Fo:" + str(td / "native.obj")], check=True)
        subprocess.run(["lib", "/nologo", "/OUT:" + str(td / "ffi_errnative.lib"), str(td / "native.obj")], check=True)
        exe = td / "prog.exe"
        subprocess.run([str(compiler), "--lib", "ffi_errnative", "--lib-path", str(td), "-o", str(exe), "export_error_extern.p"], check=True, cwd=str(root))
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-c", str(src), "-o", str(td / "native.o")], check=True)
        subprocess.run(["ar", "rcs", str(td / "libffi_errnative.a"), str(td / "native.o")], check=True)
        exe = td / "prog"
        subprocess.run([str(compiler), "--lib", "ffi_errnative", "--lib-path", str(td), "-o", str(exe), "export_error_extern.p"], check=True, cwd=str(root))
    out = subprocess.check_output([str(exe)], text=True)
    if out != expected:
        raise SystemExit(f"unexpected error bidir output: {out!r}")

print("bidirectional checked-error ABI passed (native status/error -> Strut checked error)")
