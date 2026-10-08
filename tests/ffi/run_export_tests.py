#!/usr/bin/env python3
"""FFI-1: exported C ABI round-trip from independent C and C++ hosts.

Builds a shared library + generated C header with the Strut compiler, then compiles
independent C and C++ hosts against the generated header, links the library, runs them,
and checks the results. Runs on Linux/macOS (cc/c++) and Windows (cl/MSVC).
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "42 42 3.00 3.00 200 7\n"
decls = ["int32_t ff_add(int32_t a, int32_t b);",
         "int64_t ff_mul(int64_t a, int64_t b);",
         "float ff_scale(float value, float factor);",
         "double ff_scale64(double value, double factor);",
         "uint8_t ff_byte(uint8_t value);",
         "int32_t ff_zero(void);",
         "void ff_note(int32_t value);"]

with tempfile.TemporaryDirectory(prefix="strut-ffi-export-") as td:
    td = Path(td)
    header = td / "export_lib.h"
    if os.name == "nt":
        lib = td / "export_lib.dll"; implib = td / "export_lib.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), str(root / "export_lib.p")], check=True)
        assert header.is_file(), "generated C header missing"
        assert lib.is_file(), "DLL missing"
        assert implib.is_file(), "import library missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        assert "#include <stdint.h>" in text and "#ifndef STRUT_FFI_" in text and 'extern "C" {' in text
        for cc, src, exe, extra in [("cl", "export_host.c", "host_c.exe", []),
                                    ("cl", "export_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_lib" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), str(root / "export_lib.p")], check=True)
        assert header.is_file() and lib.is_file(), "artifacts missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        assert "#include <stdint.h>" in text and "#ifndef STRUT_FFI_" in text and 'extern "C" {' in text
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_lib", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export C ABI round-trip passed (independent C and C++ hosts)")
