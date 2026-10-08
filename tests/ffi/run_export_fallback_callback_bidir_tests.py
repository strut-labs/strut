#!/usr/bin/env python3
"""FFI-6 closure: synchronous fallible callback (Strut -> native). A Strut function value that
can throw a declared checked error is passed to a native C function taking a fallible callback;
the generated trampoline catches the checked error, writes a structured callback error, returns
failure status; native forwards it; the extern wrapper re-throws the checked error; Strut catch
selects it. Expected: "12" then "thrown" then "5"."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_fallback_callback_bidir_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "12\nthrown\n5\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-fcb-bidir-") as td:
    td = Path(td)
    src = root / "export_fcb_native.c"
    if os.name == "nt":
        subprocess.run(["cl", "/nologo", "/c", str(src), "/Fo:" + str(td / "native.obj")], check=True)
        subprocess.run(["lib", "/nologo", "/OUT:" + str(td / "ffi_fcbnative.lib"), str(td / "native.obj")], check=True)
        exe = td / "prog.exe"
        subprocess.run([str(compiler), "--lib", "ffi_fcbnative", "--lib-path", str(td), "-o", str(exe), "export_fcb_extern.p"], check=True, cwd=str(root))
    else:
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-c", str(src), "-o", str(td / "native.o")], check=True)
        subprocess.run(["ar", "rcs", str(td / "libffi_fcbnative.a"), str(td / "native.o")], check=True)
        exe = td / "prog"
        subprocess.run([str(compiler), "--lib", "ffi_fcbnative", "--lib-path", str(td), "-o", str(exe), "export_fcb_extern.p"], check=True, cwd=str(root))
    out = subprocess.check_output([str(exe)], text=True)
    if out != expected:
        raise SystemExit(f"unexpected fallible-callback bidir output: {out!r}")

print("bidirectional fallible callback passed (Strut checked throw -> native status/descriptor -> Strut catch)")
