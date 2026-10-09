#!/usr/bin/env python3
"""FFI-7 Direction B: a native-owned callback (fn + ctx + retain_ctx + release_ctx) becomes a
native-backed retained_callback inside Strut. Exactly one retain_ctx at shared-state construction,
exactly one release_ctx at final control-block death, and no native retain/release on ordinary
Strut copies; native drops its originating owner and the Strut global remains the sole owner
that can still invoke later."""
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_export_retained_b_tests.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
expected = "native retained ok\n"

with tempfile.TemporaryDirectory(prefix="strut-ffi-retained-b-") as td:
    td = Path(td)
    header = td / "export_retained_b.h"
    ext = ".dll" if os.name == "nt" else (".dylib" if platform.system() == "Darwin" else ".so")
    lib = td / ("libexport_retained_b" + ext)
    subprocess.run([str(compiler), "--shared", "--emit-c-header", str(header),
                    "-o", str(lib), "export_retained_b.p"], check=True, cwd=str(root))
    text = header.read_text()
    assert "int32_t (*cb)(void*, int32_t), void* cb_ctx, void (*cb_retain_ctx)(void*), void (*cb_release_ctx)(void*), int32_t v" in text, "native descriptor ABI missing"
    assert "std::function" not in text and "shared_ptr" not in text and "strut_retained_callback" not in text, "C++ internals leaked"
    assert "run_stored(" in text and "clear_stored(" in text, "stored exports missing"
    env = os.environ.copy()
    key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    env[key] = str(td) + (os.pathsep + env[key] if env.get(key) else "")
    if os.name == "nt":
        dest = td / "host_c.exe"
        subprocess.run(["cl", "/nologo", "/Wall", "/WX", "/I", str(td), str(root / "export_retained_b_host.c"),
                        "/Fe:" + str(dest), "/link", str(td / "export_retained_b.lib")], check=True)
        out = subprocess.check_output([str(dest)], text=True, env=env)
    else:
        cc = os.environ.get("CC", "cc")
        strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic"]
        if os.environ.get("STRUT_TEST_SANITIZE"):
            strict += ["-fsanitize=address", "-g"]
        dest = td / "host_c"
        subprocess.run([cc, *strict, "-pthread", "-I", str(td), str(root / "export_retained_b_host.c"),
                        "-L", str(td), "-lexport_retained_b", "-o", str(dest)], check=True)
        out = subprocess.check_output([str(dest)], text=True, env=env)
    if out != expected:
        raise SystemExit(f"unexpected Direction-B host output: {out!r}")

print("export C retained Direction-B passed (native retain-once, release-once, invoke after origin drop, copies share)")
