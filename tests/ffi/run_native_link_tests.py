#!/usr/bin/env python3
from pathlib import Path
import os, platform, shutil, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_native_link_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="strut-ffi-") as td:
    td = Path(td)
    obj = td / ("fixture.obj" if os.name == "nt" else "fixture.o")
    if os.name == "nt":
        print("native-link integration helper currently requires the Unix cc/ar toolchain; Windows model is covered by backend/CLI tests")
        raise SystemExit(0)
    subprocess.run([os.environ.get("CC", "cc"), "-fPIC", "-c", str(root / "fixture.c"), "-o", str(obj)], check=True)
    static = td / "libstrutfixture.a"
    subprocess.run(["ar", "rcs", str(static), str(obj)], check=True)
    ext = ".dylib" if platform.system() == "Darwin" else ".so"
    dynamic = td / ("libstrutfixture" + ext)
    subprocess.run([os.environ.get("CC", "cc"), "-shared", str(obj), "-o", str(dynamic)], check=True)

    exe_static = td / "ffi-static"
    subprocess.run([str(compiler), str(root / "ffi.p"), "-o", str(exe_static), "--static-lib", str(static)], check=True)
    out = subprocess.check_output([str(exe_static)], text=True)
    if out != "42\n7\n11\n3\n9\n10\n": raise SystemExit(f"unexpected static FFI output: {out!r}")

    exe_dynamic = td / "ffi-dynamic"
    subprocess.run([str(compiler), str(root / "ffi.p"), "-o", str(exe_dynamic), "--dynamic-lib", str(dynamic)], check=True)
    env = os.environ.copy()
    key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
    out = subprocess.check_output([str(exe_dynamic)], text=True, env=env)
    if out != "42\n7\n11\n3\n9\n10\n": raise SystemExit(f"unexpected dynamic FFI output: {out!r}")
    print("native static/dynamic FFI linkage passed")
