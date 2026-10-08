#!/usr/bin/env python3
"""FFI-4: borrowed primitive pointers/references across the export C ABI.

Exports take `raw_ptr<int_32>` (borrowed, nullable) and `ref<int_32>` (borrowed, non-null);
the generated header must be pure C (`int32_t*`), never a C++ reference or shared_ptr. The
independent C and C++ hosts prove mutation in place, read, and null handling.
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_pointer_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "ptr ok\n"
decls = ["void pt_inc(int32_t* p);",
         "void rf_inc(int32_t* p);",
         "int32_t pt_read(int32_t* p);",
         "int32_t pt_is_null(int32_t* p);"]

with tempfile.TemporaryDirectory(prefix="strut-ffi-pointer-") as td:
    td = Path(td)
    header = td / "export_pointer.h"
    if os.name == "nt":
        lib = td / "export_pointer.dll"; implib = td / "export_pointer.lib"
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_pointer.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file() and implib.is_file(), "artifacts missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        assert "shared_ptr" not in text and "ref<" not in text, "C++ type leaked into C header"
        for cc, src, exe, extra in [("cl", "export_pointer_host.c", "host_c.exe", []),
                                    ("cl", "export_pointer_host.cpp", "host_cpp.exe", ["/EHsc"])]:
            out_exe = td / exe
            subprocess.run([cc, "/nologo", *extra, "/I", str(td), str(root / src),
                            "/Fe:" + str(out_exe), "/link", str(implib)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        lib = td / ("libexport_pointer" + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), "export_pointer.p"], check=True, cwd=str(root))
        assert header.is_file() and lib.is_file(), "artifacts missing"
        text = header.read_text()
        for decl in decls:
            assert decl in text, f"header missing declaration: {decl}"
        assert "shared_ptr" not in text and "ref<" not in text, "C++ type leaked into C header"
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        for cc, src, exe in [(os.environ.get("CC", "cc"), "export_pointer_host.c", "host_c"),
                             (os.environ.get("CXX", "c++"), "export_pointer_host.cpp", "host_cpp")]:
            out_exe = td / exe
            subprocess.run([cc, "-I", str(td), str(root / src), "-L", str(td), "-lexport_pointer", "-o", str(out_exe)], check=True)
            out = subprocess.check_output([str(out_exe)], text=True, env=env)
            if out != expected:
                raise SystemExit(f"unexpected {exe} output: {out!r}")

print("export C pointer/reference round-trip passed (borrowed primitive raw_ptr + ref, mutation + null)")
