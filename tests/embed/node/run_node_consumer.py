#!/usr/bin/env python3
"""FFI-9 Node consumer integration test: builds the dependency-free N-API addon with node-gyp
against the shared embedding library and then RUNS the JavaScript consumer, requiring a
successful build AND execution (exit 0 plus expected output) before passing."""
import os, shutil, subprocess, sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("usage: run_node_consumer.py <libdir> <include> <jsonic>")
libdir = Path(sys.argv[1]).resolve()
inc = Path(sys.argv[2]).resolve()
jsonic = Path(sys.argv[3]).resolve()
srcdir = Path(__file__).resolve().parent

node = os.environ.get("NODE", "node")
nodegyp = os.environ.get("NODE_GYP", "node-gyp")
if shutil.which(node) is None and shutil.which("nodejs") is None:
    print("SKIP: node not available")
    sys.exit(77)  # CTest SKIP_RETURN_CODE
use_npx = False
if shutil.which(nodegyp) is None:
    if shutil.which("npx") is not None:
        nodegyp = "npx"
        use_npx = True
    else:
        print("SKIP: node-gyp not available")
        sys.exit(77)

sep = ";" if os.name == "nt" else ":"
if os.name == "nt":
    extra_ldflags = "/LIBPATH:%s strut_embed.lib" % libdir
elif sys.platform == "darwin":
    # Apple's linker does not support GNU --as-needed/--no-as-needed; RPATH + explicit -l.
    extra_ldflags = "-L%s -lstrut_embed -Wl,-rpath,%s" % (libdir, libdir)
else:
    extra_ldflags = "-Wl,--no-as-needed -L%s -lstrut_embed -Wl,--as-needed -Wl,-rpath,%s" % (libdir, libdir)

env = dict(os.environ,
           CXXFLAGS="-I" + str(inc) + " -std=c++17",
           LDFLAGS=extra_ldflags,
           LD_LIBRARY_PATH=str(libdir),
           DYLD_LIBRARY_PATH=str(libdir),
           STRUT_JSONIC_INCLUDE_DIR=str(jsonic))
# Generator-independent include/link discovery: gcc/clang honor CPATH for angle includes and
# MSVC honors INCLUDE/LIB, so node-gyp's MSBuild path (which ignores the CXXFLAGS/LDFLAGS env)
# still finds the Strut embedding header and import library.
env["CPATH"] = str(inc)
if os.name == "nt":
    env["INCLUDE"] = str(inc) + ";" + env.get("INCLUDE", "")
    env["LIB"] = str(libdir) + ";" + env.get("LIB", "")
env["PATH"] = str(libdir) + sep + env.get("PATH", "")

args = [x for x in ([nodegyp, "rebuild"] if not use_npx else [nodegyp, "-y", "node-gyp", "rebuild"])]
b = subprocess.run(args, env=env, capture_output=True, text=True, cwd=str(srcdir),
                   shell=(os.name == "nt"))
if b.returncode != 0:
    combined = (b.stdout or "") + "\n" + (b.stderr or "")
    detail = "\n".join(ln for ln in combined.splitlines()
                        if any(k in ln.lower() for k in ("error", "gyp err", "msb", "fatal", "lnk", "ld\'", "cl.exe", "unresolved")))
    print("node addon build FAILED (%s):\n%s" % (b.returncode, (detail or combined)[-6000:]))
    sys.exit(1)
r = subprocess.run([node, "index.js"], env=env, capture_output=True, text=True, cwd=str(srcdir))
out = r.stdout.strip()
if r.returncode != 0 or "node consumer ok" not in out:
    print("node consumer FAILED rc=%d out=%r" % (r.returncode, out)); sys.exit(1)
print("node consumer ok")