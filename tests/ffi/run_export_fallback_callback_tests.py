#!/usr/bin/env python3
"""FFI-6: synchronous fallible callbacks (native -> exported Strut). The C callback returns a
status + caller-owned borrowed error descriptor; the Strut export converts it to the declared
checked error and propagates it through the ordinary opaque error handle."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_fallback_callback_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "fcb ok\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-fcb-") as td:
    td = Path(td)
    header = td / "export_fcb.h"
    if os.name == "nt":
        lib = td / "export_fcb.dll"; implib = td / "export_fcb.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_fcb.p"], check=True, cwd=str(root))
        text = header.read_text()
        assert "typedef strut_ffi_status (*" in text and "out_error);" in text, "fallible cb typedef missing"
        assert "std::function" not in text, "C++ closure leaked"
        for cc, src, exe, extra in [("cl", "export_fcb_host.c", "host_c.exe", []),
                                    ("cl", "export_fcb_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_fcb" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_fcb.p"], check=True, cwd=str(root))
        text = header.read_text()
        assert "typedef strut_ffi_status (*" in text and "out_error);" in text
        assert "std::function" not in text
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_fcb_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_fcb_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_fcb", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export fallible callback passed (status + borrowed descriptor -> Strut checked error -> host handle)")
