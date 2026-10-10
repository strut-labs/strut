#!/usr/bin/env python3
"""FFI-9 Go consumer integration test: builds and RUNS the cgo consumer against the shared
embedding library, propagating build/run exit codes (no piped masking)."""
import os, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) != 5:
    raise SystemExit("usage: run_go_consumer.py <libdir> <include> <jsonic> <source_dir>")
libdir = Path(sys.argv[1]).resolve()
inc = Path(sys.argv[2]).resolve()
jsonic = Path(sys.argv[3]).resolve()
srcdir = Path(sys.argv[4]).resolve()

go = os.environ.get("GO", "go")
with tempfile.TemporaryDirectory(prefix="strut-embed-go-") as td:
    td = Path(td)
    exe = td / "go_consumer"
    env = dict(os.environ, CGO_ENABLED="1",
               CGO_CFLAGS="-I" + str(inc),
               CGO_LDFLAGS="-L" + str(libdir) + " -lstrut_embed")
    b = subprocess.run([go, "build", "-buildvcs=false", "-o", str(exe), str(srcdir)],
                       env=env, capture_output=True, text=True, cwd=str(srcdir))
    if b.returncode != 0:
        print("go build FAILED:", b.stderr[:2000]); sys.exit(1)
    run_env = dict(os.environ, LD_LIBRARY_PATH=str(libdir),
                   STRUT_JSONIC_INCLUDE_DIR=str(jsonic))
    sep = ";" if os.name == "nt" else ":"
    run_env["PATH"] = str(libdir) + sep + run_env.get("PATH", "")
    r = subprocess.run([str(exe)], env=run_env, capture_output=True, text=True)
    out = r.stdout.strip()
    if r.returncode != 0 or "go consumer ok" not in out:
        print("go consumer FAILED rc=%d out=%r" % (r.returncode, out)); sys.exit(1)
print("go consumer ok")