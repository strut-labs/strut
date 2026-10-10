#!/usr/bin/env python3
"""FFI-9 C# consumer integration test: builds AND runs the .NET consumer (P/Invoke) against the
shared embedding library, propagating build/run exit codes. Skips (CTest SKIP_RETURN_CODE 77)
when the .NET SDK is unavailable."""
import os, shutil, subprocess, sys
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit("usage: run_csharp_consumer.py <libdir> <jsonic>")
libdir = Path(sys.argv[1]).resolve()
jsonic = Path(sys.argv[2]).resolve()
srcdir = Path(__file__).resolve().parent

dotnet = os.environ.get("DOTNET", "dotnet")
if shutil.which(dotnet) is None:
    print("SKIP: dotnet SDK not available")
    sys.exit(77)

libname = "strut_embed.dll" if os.name == "nt" else ("libstrut_embed.dylib" if sys.platform == "darwin" else "libstrut_embed.so")
env = dict(os.environ, LD_LIBRARY_PATH=str(libdir), DYLD_LIBRARY_PATH=str(libdir),
           STRUT_EMBED_LIB=str((libdir / libname).resolve()),
           STRUT_JSONIC_INCLUDE_DIR=str(jsonic))
sep = ";" if os.name == "nt" else ":"
env["PATH"] = str(libdir) + sep + env.get("PATH", "")

b = subprocess.run([dotnet, "build", "-c", "Release", "-v", "q"], env=env, capture_output=True,
                   text=True, cwd=str(srcdir))
if b.returncode != 0:
    print("dotnet build FAILED:", b.stderr[-2000:]); sys.exit(1)

bin_dir = srcdir / "bin" / "Release"
projs = list(bin_dir.glob("net*")) + list(bin_dir.glob("*/"))
bin_path = None
for d in sorted(bin_dir.iterdir(), key=lambda p: len(str(p))):
    if d.is_dir() and bin_path is None:
        exe = d / "strut_consumer"
        if not os.name == "nt" and exe.exists():
            bin_path = exe
        elif os.name == "nt":
            exe = d / "strut_consumer.exe"
            if exe.exists():
                bin_path = exe
if bin_path is None:
    print("dotnet output binary not found"); sys.exit(1)

r = subprocess.run([str(bin_path)], env=env, capture_output=True, text=True, cwd=str(srcdir))
out = r.stdout.strip()
if r.returncode != 0 or "csharp consumer ok" not in out:
    print("csharp consumer FAILED rc=%d out=%r" % (r.returncode, out))
    if r.stderr.strip():
        print("csharp consumer stderr:\n" + r.stderr.strip()[-2000:])
    sys.exit(1)
print("csharp consumer ok")