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
if shutil.which(node) is None or shutil.which(nodegyp) is None:
    print("SKIP: node/node-gyp not available")
    sys.exit(77)  # CTest SKIP_RETURN_CODE

sep = ";" if os.name == "nt" else ":"
extra_ldflags = "-Wl,--no-as-needed -L%s -lstrut_embed -Wl,--as-needed -Wl,-rpath,%s" % (libdir, libdir)
if os.name == "nt":
    extra_ldflags = "/LIBPATH:%s strut_embed.lib" % libdir

env = dict(os.environ,
           CXXFLAGS="-I" + str(inc) + " -std=c++17",
           LDFLAGS=extra_ldflags,
           LD_LIBRARY_PATH=str(libdir),
           STRUT_JSONIC_INCLUDE_DIR=str(jsonic))
env["PATH"] = str(libdir) + sep + env.get("PATH", "")

b = subprocess.run([nodegyp, "rebuild"], env=env, capture_output=True, text=True, cwd=str(srcdir))
if b.returncode != 0:
    print("node addon build FAILED:", b.stderr[-2000:]); sys.exit(1)
r = subprocess.run([node, "index.js"], env=env, capture_output=True, text=True, cwd=str(srcdir))
out = r.stdout.strip()
if r.returncode != 0 or out != "node consumer ok":
    print("node consumer FAILED rc=%d out=%r" % (r.returncode, out)); sys.exit(1)
print("node consumer ok")