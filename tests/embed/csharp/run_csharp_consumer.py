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
dll = None
tfm_dirs = sorted(bin_dir.glob("net*"), key=lambda p: str(p)) if bin_dir.exists() else []
if tfm_dirs:
    cand = tfm_dirs[-1] / "strut_consumer.dll"
    if cand.exists():
        dll = cand
if dll is None:
    print("dotnet output dll not found; build tree:", list(bin_dir.rglob("*.dll")) if bin_dir.exists() else "bin/Release missing")
    sys.exit(1)

r = subprocess.run([dotnet, str(dll)], env=env, capture_output=True, text=True, cwd=str(srcdir))
out = r.stdout.strip()
if r.returncode != 0 or "csharp consumer ok" not in out:
    print("csharp consumer FAILED rc=%d out=%r" % (r.returncode, out))
    if r.stderr.strip():
        print("csharp consumer stderr:\n" + r.stderr.strip()[-2000:])
    info = subprocess.run([dotnet, "--info"], env=env, capture_output=True, text=True)
    runtime_line = [ln for ln in info.stdout.splitlines() if "RID:" in ln or "Architecture:" in ln or "Version:" in ln or "RID" in ln]
    print("dotnet info:\n" + "\n".join(runtime_line[:4]))
    if info.stderr.strip():
        print("dotnet --info stderr: " + info.stderr.strip()[-500:])
    smoke = subprocess.run([dotnet, str(dll), "--smoke"], env=env, capture_output=True, text=True)
    print("smoke rc=%d out=%r stderr=%r" % (smoke.returncode, smoke.stdout.strip()[-500:], smoke.stderr.strip()[-800:]))
    sys.exit(1)
print("csharp consumer ok")