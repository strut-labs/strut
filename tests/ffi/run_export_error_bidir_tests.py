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
expected = "30\nbad\n7\none\n1\ntwo\n2\n"

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

    # unknown native error type (not in the declared set) is a contract violation -> fatal boundary
    exe2 = td / ("prog_unknown.exe" if os.name == "nt" else "prog_unknown")
    subprocess.run([str(compiler), "--lib", "ffi_errnative", "--lib-path", str(td), "-o", str(exe2), "export_error_unknown.p"], check=True, cwd=str(root))
    r = subprocess.run([str(exe2)], capture_output=True, text=True)
    if r.returncode == 0:
        raise SystemExit("unknown native error type did not trigger the fatal boundary")
    if "undeclared error 'Mystery'" not in (r.stderr + r.stdout):
        raise SystemExit(f"unknown-error diagnostic missing: {r.stderr!r} {r.stdout!r}")

print("bidirectional checked-error ABI passed (multi-error identity; native status/error -> Strut; unknown-error fatal boundary)")
