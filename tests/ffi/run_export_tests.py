#!/usr/bin/env python3
"""FFI-1 primitives + FFI-2 strings/bytes: exported C ABI round-trip from independent C and
C++ hosts.

Builds a shared library + generated C header with the Strut compiler, then compiles
independent C and C++ hosts against the generated header, links the library, runs them, and
checks the results. Runs on Linux/macOS (cc/c++) and Windows (cl/MSVC).

FFI-2 ownership contract exercised here:
  * string/bytes INPUT  -> borrowed const data + explicit length (host owns the buffer)
  * string/bytes OUTPUT -> owned buffer allocated by the Strut library, returned via
    out-parameters, released with strut_ffi_free_string/bytes from the same library
  * empty values        -> canonical (data == NULL, len == 0)
  * bytes               -> binary-safe (0x00..0xff, embedded NUL), never NUL-terminated
  * string length       -> BYTE length (embedded NUL permitted)
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "42 42 3.00 3.00 200 7\nffi2 ok\n"
decls = ["int32_t ff_add(int32_t a, int32_t b);",
         "int64_t ff_mul(int64_t a, int64_t b);",
         "float ff_scale(float value, float factor);",
         "double ff_scale64(double value, double factor);",
         "uint8_t ff_byte(uint8_t value);",
         "int32_t ff_zero(void);",
         "void ff_note(int32_t value);",
         "void ff_str_echo(const char* s_data, size_t s_len, char** out_data, size_t* out_len);",
         "void ff_str_dup(const char* s_data, size_t s_len, char** out_data, size_t* out_len);",
         "int32_t ff_str_len(const char* s_data, size_t s_len);",
         "void ff_bytes_echo(const uint8_t* b_data, size_t b_len, uint8_t** out_data, size_t* out_len);",
         "#define EXPORT_LIB_FFI_FREE_STRING ",
         "#define EXPORT_LIB_FFI_FREE_BYTES "]

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
        assert "#include <stdint.h>" in text and "#include <stddef.h>" in text
        assert "#ifndef STRUT_FFI_" in text and 'extern "C" {' in text
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
        assert "#include <stdint.h>" in text and "#include <stddef.h>" in text
        assert "#ifndef STRUT_FFI_" in text and 'extern "C" {' in text
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

print("export C ABI round-trip passed (independent C and C++ hosts; FFI-1 primitives + FFI-2 strings/bytes)")
