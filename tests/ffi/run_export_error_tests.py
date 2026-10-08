#!/usr/bin/env python3
"""FFI-6: exported Strut checked errors -> C `strut_ffi_status` + module-owned opaque error
handle. Independent C/C++ hosts check success, structured failure (type/message/code),
release, and vacuous-checked void functions; generated header is pure C."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_error_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "err ok\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-error-") as td:
    td = Path(td)
    header = td / "export_error.h"
    if os.name == "nt":
        lib = td / "export_error.dll"; implib = td / "export_error.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_error.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file() and implib.is_file(), "artifacts missing"
        text = header.read_text()
        assert "typedef int32_t strut_ffi_status;" in text, "status type missing"
        assert "_FFI_ERROR_RELEASE" in text and "typedef struct " in text, "error handle API missing"
        assert "** out_error" in text, "checked function prototype missing out_error"
        for cc, src, exe, extra in [("cl", "export_error_host.c", "host_c.exe", []),
                                    ("cl", "export_error_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_error" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_error.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file(), "artifacts missing"
        text = header.read_text()
        assert "typedef int32_t strut_ffi_status;" in text and "** out_error" in text
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_error_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_error_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_error", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export C checked-error round-trip passed (status + opaque handle; type/message/code; release)")
