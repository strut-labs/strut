#!/usr/bin/env python3
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[2]
compiler=Path(sys.argv[1] if len(sys.argv)>1 else root/'build-sanitize'/'strut').resolve()
source=Path(__file__).with_name('runtime_stress.p').resolve()
with tempfile.TemporaryDirectory(prefix='strut-memory-') as td:
    exe=Path(td)/'runtime_stress'
    env=os.environ.copy()
    sanitizer_flags='-fsanitize=address,undefined -fno-omit-frame-pointer'
    existing_flags=env.get('STRUT_CXXFLAGS','').strip()
    env['STRUT_CXXFLAGS']=' '.join(filter(None,(existing_flags,sanitizer_flags)))
    c=subprocess.run([str(compiler),str(source),'-o',str(exe)],env=env,text=True,capture_output=True)
    if c.returncode:
        print(c.stderr,file=sys.stderr);raise SystemExit(c.returncode)
    r=subprocess.run([str(exe)],env=env,text=True,capture_output=True,timeout=30)
    if r.returncode or r.stderr:
        print(r.stdout,end='');print(r.stderr,file=sys.stderr,end='');raise SystemExit(r.returncode or 1)
    if r.stdout.strip() not in {'true','1'}:
        print('weak pointer did not expire after owner release',file=sys.stderr);raise SystemExit(1)
print('sanitized ptr/weak_ptr/thread stress passed')
