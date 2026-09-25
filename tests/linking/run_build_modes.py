#!/usr/bin/env python3
from pathlib import Path
import os, platform, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: run_build_modes.py /path/to/strut")
compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="strut-link-") as td:
    td = Path(td)
    if os.name == "nt":
        print("link-mode helper skips native fixture construction on Windows; backend emits /MT,/MD and /OPT flags")
        raise SystemExit(0)
    cc = os.environ.get("CC", "cc")
    soext = ".dylib" if platform.system() == "Darwin" else ".so"
    static_obj = td / "static.o"
    dynamic_obj = td / "dynamic.o"
    subprocess.run([cc, "-fPIC", "-c", str(root / "static_part.c"), "-o", str(static_obj)], check=True)
    subprocess.run([cc, "-fPIC", "-c", str(root / "dynamic_part.c"), "-o", str(dynamic_obj)], check=True)
    static_lib = td / "libstaticpart.a"
    dynamic_lib = td / ("libdynamicpart" + soext)
    subprocess.run(["ar", "rcs", str(static_lib), str(static_obj)], check=True)
    subprocess.run([cc, "-shared", str(dynamic_obj), "-o", str(dynamic_lib)], check=True)

    mixed = td / "mixed"
    subprocess.run([str(compiler), str(root / "mixed.p"), "-o", str(mixed), "--release", "--static-lib", str(static_lib), "--dynamic-lib", str(dynamic_lib)], check=True)
    env = os.environ.copy(); key = "DYLD_LIBRARY_PATH" if platform.system()=="Darwin" else "LD_LIBRARY_PATH"; env[key]=str(td)+(os.pathsep+env[key] if env.get(key) else "")
    out = subprocess.check_output([str(mixed)], text=True, env=env)
    if out != "42\n": raise SystemExit(f"unexpected mixed-link output: {out!r}")

    hello = td / "hello.p"; hello.write_text('function main() -> void { print("hi"); return; }\n')
    dynamic = td / "hello-dynamic"
    subprocess.run([str(compiler), str(hello), "-o", str(dynamic), "--dynamic", "--release"], check=True)
    if subprocess.check_output([str(dynamic)], text=True) != "hi\n": raise SystemExit("dynamic build failed")

    if platform.system() == "Linux":
        static = td / "hello-static"
        subprocess.run([str(compiler), str(hello), "-o", str(static), "--static", "--release"], check=True)
        if subprocess.check_output([str(static)], text=True) != "hi\n": raise SystemExit("static build failed")
    print("static/dynamic/mixed build modes passed")
