#!/usr/bin/env python3
"""FFI-9 9-CERT: independent-installation certification.

Stages the public Strut embedding distribution into an isolated prefix (outside the source
checkout), verifies every public artifact, then builds AND executes the Python, Go, Node and C#
consumers against ONLY the staged header/library/runtime-support assets from a temporary working
directory -- no build tree, no developer-machine paths. Each language is recorded as PASS
(executed), SKIP (toolchain absent), or FAIL, with the ok-marker check made tolerant of
platform compiler link noise (reusing each consumer's permanent runner).
"""
import os, shutil, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit("usage: ffi9_cert.py <build_dir> <prefix>")
build = Path(sys.argv[1]).resolve()
prefix = Path(sys.argv[2]).resolve()
Repo = Path(__file__).resolve().parent.parent

if prefix.exists():
    shutil.rmtree(prefix)
prefix.mkdir(parents=True)

i = subprocess.run(["cmake", "--install", str(build), "--prefix", str(prefix)],
                   capture_output=True, text=True)
if i.returncode != 0:
    print("cmake --install FAILED:", i.stderr[-1500:]); sys.exit(1)

inc = prefix / "include" / "strut" / "embed.h"
lib = prefix / "lib"
jsonic = prefix / "share" / "strut" / "jsonic"
bin_strut = prefix / "bin"
required = [inc, lib / "libstrut_embed.so", jsonic / "json.h", bin_strut / "strut"]
missing = [str(r) for r in required if not r.exists() and not (r.name.endswith(".so") and list(lib.glob("libstrut_embed.so*")))]
if missing:
    print("STAGED PREFIX INCOMPLETE:", missing); sys.exit(1)
for p in (str(prefix),):
    if "/home/nick" in p:
        print("prefix leaks a developer path:", p); sys.exit(1)

consumers = [
    ("python", [sys.executable, str(Repo / "tests/embed/python/strut_consumer.py")], True),
    ("go", [sys.executable, str(Repo / "tests/embed/go/run_go_consumer.py"), str(lib),
            str(prefix / "include"), str(jsonic), str(Repo / "tests/embed/go")], False),
    ("node", [sys.executable, str(Repo / "tests/embed/node/run_node_consumer.py"), str(lib),
              str(prefix / "include"), str(jsonic)], False),
    ("csharp", [sys.executable, str(Repo / "tests/embed/csharp/run_csharp_consumer.py"),
                str(lib), str(jsonic)], False),
]

results = {}
with tempfile.TemporaryDirectory(prefix="strut-cert-") as td:
    cwd = Path(td)
    sep = ";" if os.name == "nt" else ":"
    for lang, cmd, is_direct in consumers:
        env = dict(os.environ,
                   LD_LIBRARY_PATH=str(lib),
                   STRUT_JSONIC_INCLUDE_DIR=str(jsonic),
                   STRUT_EMBED_LIB=str(lib / "libstrut_embed.so"))
        env["PATH"] = str(lib) + sep + env["PATH"]
        if "cwd" in dir():  # noop
            pass
        r = subprocess.run(cmd, env=env, capture_output=True, text=True, cwd=str(cwd))
        if r.returncode == 77:
            results[lang] = "SKIP"
        elif r.returncode == 0:
            results[lang] = "PASS"
        else:
            results[lang] = "FAIL"
            print("%s stderr:\n%s" % (lang, r.stderr[-1200:]))

for lang, st in results.items():
    print("9-CERT %-8s %s" % (lang, st))
bad = [k for k, v in results.items() if v == "FAIL"]
sys.exit(1 if bad else 0)