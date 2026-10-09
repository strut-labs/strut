#!/usr/bin/env python3
"""FFI-7 fallible retained callbacks. Direction A: Strut-owned fallible retained callback
invoked from C; on failure the FFI-6 callback-error descriptor is backed by per-thread module
storage so it stays valid AFTER invoke() returns (copy immediately). Direction B: a native
callback reporting FFI-6 errors becomes a native-backed retained_callback; the wrapper copies
the descriptor immediately and Strut checked-error flow handles declared errors; native
retain-once/release-once is unchanged."""
from pathlib import Path
import re, os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_retained_f_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent

with tempfile.TemporaryDirectory(prefix="strut-ffi-retained-f-") as td:
    td = Path(td)
    env = os.environ.copy()
    key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    ext = ".dll" if os.name == "nt" else (".dylib" if platform.system() == "Darwin" else ".so")
    cc = os.environ.get("CC", "cc")
    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic"]
    if os.environ.get("STRUT_TEST_SANITIZE"):
        strict += ["-fsanitize=address", "-g"]
    libs = []
    for name, exe_out in [("export_retained_f", "fallibleA"),
                          ("export_retained_bf", "fallibleB")]:
        header = td / (name + ".h")
        lib = td / ("lib" + name + ext)
        subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                        "-o", str(lib), name + ".p"], check=True, cwd=str(root))
        text = header.read_text()
        assert ("out_error" in text or "callback_error*" in text or "callback_error*)") and "callback_error" in text, f"{name}: fallible descriptor missing"
        assert "std::function" not in text and "shared_ptr" not in text, f"{name}: C++ internals leaked"
        host = root / (name + "_host.c")
        dst = td / exe_out
        if os.name == "nt":
            subprocess.run(["cl", "/nologo", "/Wall", "/WX", "/wd5045", "/wd4820", "/wd4668", "/wd5105", "/I", str(td), str(host),
                            "/Fe:" + str(dst), "/link", str(td / ("lib" + name + ".lib"))], check=True)
        else:
            subprocess.run([cc, *strict, "-pthread", "-I", str(td), str(host),
                            "-L", str(td), "-l" + name, "-o", str(dst)], check=True)
        env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
        out = subprocess.check_output([str(dst)], text=True, env=env)
        want = "fallible A ok\n" if name == "export_retained_f" else "fallible B ok\n"
        if out != want:
            raise SystemExit(f"unexpected {name} output: {out!r}")

print("export C retained fallible A+B passed (TLS-backing-after-return; immediate native copy; ownership unchanged)")
