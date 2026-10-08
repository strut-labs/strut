#!/usr/bin/env python3
"""FFI-2 release-symbol scoping: two independently generated Strut shared libraries in ONE
native host.

Each library exports owned string/bytes results and a module-qualified release symbol
(`<module>_ffi_free_string` / `<module>_ffi_free_bytes`). This test proves the symbols do not
collide when both libraries are linked into the same host and that each buffer is released
through its own library's release entry point (no cross-library free). Runs on Linux/macOS
(cc/c++) and Windows (cl/MSVC).
"""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_multilib_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "multilib ok\n"
libs = [("export_lib_a", "ffi_a"), ("export_lib_b", "ffi_b")]

with tempfile.TemporaryDirectory(prefix="strut-ffi-multilib-") as td:
    td = Path(td)
    if os.name == "nt":
        implibs = []
        for src, out in libs:
            header = td / (src + ".h"); lib = td / (out + ".dll"); implib = td / (out + ".lib")
            subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                            "-o", str(lib), str(root / (src + ".p"))], check=True)
            assert header.is_file() and lib.is_file() and implib.is_file(), f"artifacts missing for {src}"
            implibs.append(implib)
        exe = td / "host.exe"
        subprocess.run(["cl", "/nologo", "/I", str(td), str(root / "export_multilib_host.c"),
                        "/Fe:" + str(exe), "/link", *[str(i) for i in implibs]], check=True)
        out = subprocess.check_output([str(exe)], text=True)
    else:
        ext = ".dylib" if platform.system() == "Darwin" else ".so"
        for src, out in libs:
            header = td / (src + ".h"); lib = td / ("lib" + out + ext)
            subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                            "-o", str(lib), str(root / (src + ".p"))], check=True)
            assert header.is_file() and lib.is_file(), f"artifacts missing for {src}"
        exe = td / "host"
        cc = os.environ.get("CC", "cc")
        subprocess.run([cc, "-I", str(td), str(root / "export_multilib_host.c"),
                        "-L", str(td), "-lffi_a", "-lffi_b", "-o", str(exe)], check=True)
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        out = subprocess.check_output([str(exe)], text=True, env=env)
    if out != expected:
        raise SystemExit(f"unexpected multilib output: {out!r}")

print("two-library release-symbol scoping passed (module-qualified free symbols)")
