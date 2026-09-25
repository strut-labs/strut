#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile,os
root=Path(__file__).resolve().parents[2]
compiler=Path(sys.argv[1] if len(sys.argv)>1 else root/'build-target'/'strut').resolve()
source=Path(__file__).with_name('stress.p')
with tempfile.TemporaryDirectory(prefix='strut-concurrency-') as td:
    exe=Path(td)/'stress'
    c=subprocess.run([str(compiler),str(source),'-o',str(exe),'--release'],text=True,capture_output=True,timeout=60)
    if c.returncode: print(c.stderr,file=sys.stderr);raise SystemExit(c.returncode)
    for i in range(20):
        r=subprocess.run([str(exe)],text=True,capture_output=True,timeout=20)
        if r.returncode or r.stdout!='1000\n83\n' or r.stderr:
            print(f'iteration {i}: {r.stdout!r} {r.stderr!r}',file=sys.stderr);raise SystemExit(r.returncode or 1)
print('concurrency stress: 20/20 passed')
