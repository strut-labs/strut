#!/usr/bin/env python3
"""FFI-1: exported C ABI round-trip from independent C and C++ hosts."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
if os.name == "nt":
    print("export round-trip helper requires a Unix cc toolchain; Windows ABI is covered by MSVC CI")
    raise SystemExit(0)
with tempfile.TemporaryDirectory(prefix="strut-ffi-export-") as td:
    td = Path(td)
    header = td / "export_lib.h"
    ext = ".dylib" if platform.system() == "Darwin" else ".so"
    lib = td / ("libexport_lib" + ext)
    subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                    "-o", str(lib), str(root / "export_lib.p")], check=True)
    assert header.is_file(), "generated C header missing"
    assert lib.is_file(), "shared library missing"
    env = os.environ.copy()
    key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
    exe = td / "host_c"
    subprocess.run([os.environ.get("CC", "cc"), "-I", str(td), str(root / "export_host.c"),
                    "-L", str(td), "-lexport_lib", "-o", str(exe)], check=True)
    out = subprocess.check_output([str(exe)], text=True, env=env)
    if out != "42 42 3.0 1\n":
        raise SystemExit(f"unexpected C host output: {out!r}")
    exe_cc = td / "host_cpp"
    subprocess.run([os.environ.get("CXX", "c++"), "-I", str(td), str(root / "export_host.cpp"),
                    "-L", str(td), "-lexport_lib", "-o", str(exe_cc)], check=True)
    out = subprocess.check_output([str(exe_cc)], text=True, env=env)
    if out != "42 42 3.0 1\n":
        raise SystemExit(f"unexpected C++ host output: {out!r}")
    print("export C ABI round-trip passed (C and C++ hosts)")
