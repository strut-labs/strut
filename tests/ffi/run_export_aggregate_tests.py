#!/usr/bin/env python3
"""FFI-3: plain POD aggregate round-trip from independent C and C++ hosts.

Builds a shared library + generated C header for exports that take/return user structs with
primitive fields, then compiles independent C and C++ hosts against the generated header,
links the library, runs them, and checks results *and* the C-compiler's own sizeof/offsetof
view of the generated ABI structs. Runs on Linux/macOS (cc/c++) and Windows (cl/MSVC).
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_aggregate_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "agg ok\n"
# ABI type names carry a module digest, so the header exposes readable macro aliases.
decls = ["typedef struct ", "#define EXPORT_AGG_Pair ", "#define EXPORT_AGG_Mixed ",
         "int32_t agg_sum(", "agg_make(int32_t x, int32_t y);",
         "double agg_mixed_weight(", "agg_mixed_make(int32_t a, double b, uint8_t c);",
         "int32_t agg_double_sum("]

with tempfile.TemporaryDirectory(prefix="strut-ffi-aggregate-") as td:
    td = Path(td)
    header = td / "export_agg.h"
    if os.name == "nt":
        lib = td / "export_agg.dll"; implib = td / "export_agg.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), str(root / "export_agg.p")], check=True)
        assert header.is_file() and lib.is_file() and implib.is_file(), "artifacts missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        for cc, src, exe, extra in [("cl", "export_agg_host.c", "host_c.exe", []),
                                    ("cl", "export_agg_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_agg" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), str(root / "export_agg.p")], check=True)
        assert header.is_file() and lib.is_file(), "artifacts missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_agg_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_agg_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_agg", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export C aggregate round-trip passed (independent C and C++ hosts; layout certified)")
