#!/usr/bin/env python3
"""FFI-5: synchronous borrowed C callbacks (function pointer + opaque context) from an
independent C/C++ host into an exported Strut function. Generated header is pure C (no
std::function/closure type)."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_callback_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "cb ok\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-callback-") as td:
    td = Path(td)
    header = td / "export_callback.h"
    if os.name == "nt":
        lib = td / "export_callback.dll"; implib = td / "export_callback.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_callback.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file() and implib.is_file(), "artifacts missing"
        text = header.read_text()
        assert "typedef int32_t (*" in text and "void* context" in text, "callback typedef missing"
        assert "std::function" not in text, "C++ closure type leaked into C header"
        for cc, src, exe, extra in [("cl", "export_callback_host.c", "host_c.exe", []),
                                    ("cl", "export_callback_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_callback" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_callback.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file(), "artifacts missing"
        text = header.read_text()
        assert "typedef int32_t (*" in text and "void* context" in text, "callback typedef missing"
        assert "std::function" not in text, "C++ closure type leaked into C header"
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_callback_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_callback_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_callback", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export C callback round-trip passed (sync borrowed fn-pointer + context; pure-C typedef)")
