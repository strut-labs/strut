#!/usr/bin/env python3
"""FFI-10: genuine Strut-source binary round trip through Nift's byte engine.

Strut constructs bytes [97, 0, 98, 255, 128], passes payload.data() (raw_ptr<uint_8>) and an
explicit length over the extern "C" boundary, the adapter routes the exact bytes through
nift_engine_set_bytes -> evaluate "b" -> nift_script_result_value_bytes, copies the result into
Strut-owned storage, and Strut verifies every byte plus output length (plus the empty-buffer
case). Build and execution exit codes and the exact 2-line output are enforced."""
import os, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("usage: run_nift_bytes_rt.py <strut_bin_dir> <nift_lib_dir> <source_dir>")
strut_bin = Path(sys.argv[1]).resolve()
nift_lib = Path(sys.argv[2]).resolve()
srcdir = Path(sys.argv[3]).resolve()
strut = strut_bin / ("strut.exe" if os.name == "nt" else "strut")
nift_inc = nift_lib.parent / "include"
libnift = nift_lib / ("nift_c.lib" if os.name == "nt" else "libnift_c.so")
cc = os.environ.get("CXX", "g++")

with tempfile.TemporaryDirectory(prefix="strut-nift-brt-") as td:
    td = Path(td)
    adapter = td / "nift_adapter.o"
    exe = td / "nift_bytes_rt"
    env = dict(os.environ, LD_LIBRARY_PATH=str(nift_lib))
    a = subprocess.run([cc, "-std=c++20", "-I" + str(nift_inc), "-c",
                        str(srcdir / "nift_adapter.cpp"), "-o", str(adapter)],
                       capture_output=True, text=True)
    if a.returncode != 0:
        print("adapter build FAILED:", (a.stdout + a.stderr)[-1200:]); sys.exit(1)
    c = subprocess.run([str(strut), str(srcdir / "nift_bytes_rt.p"), "-o", str(exe),
                        "--lib-path", str(nift_lib), "--static-lib", str(adapter),
                        "--static-lib", str(libnift)], env=env, capture_output=True, text=True)
    if c.returncode != 0:
        print("strut compile FAILED:", (c.stdout + c.stderr)[-1500:]); sys.exit(1)
    r = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    out = r.stdout.strip()
    expected = "BYTES-RT-OK\nEMPTY-OK"
    if r.returncode != 0 or out != expected:
        print("nift_bytes_rt FAILED rc=%d out=%r" % (r.returncode, out)); sys.exit(1)
print("nift bytes rt ok")