#!/usr/bin/env python3
from pathlib import Path
import os, shutil, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[2]
compiler=Path(sys.argv[1] if len(sys.argv)>1 else root/'build-target'/'strut').resolve()
packages=Path(__file__).resolve().parent
source_app=root/'dogfood'/'package-app'
with tempfile.TemporaryDirectory(prefix='strut-package-dogfood-') as td:
    td=Path(td); app=td/'app'; shutil.copytree(source_app,app)
    env=os.environ.copy();env['STRUT_HOME']=str(td/'home')
    subprocess.run([str(compiler),'init'],cwd=app,env=env,check=True)
    for name in ('http','sqlite','tls','system'):
        subprocess.run([str(compiler),'add',str(packages/name)],cwd=app,env=env,check=True)
    subprocess.run([str(compiler),'install'],cwd=app,env=env,check=True)
    exe=app/'package-app'
    subprocess.run([str(compiler),'main.p','-o',str(exe)],cwd=app,env=env,check=True)
    out=subprocess.check_output([str(exe)],cwd=app,env=env,text=True)
    if out!='package-ok\n': raise SystemExit(f'unexpected output: {out!r}')
print('clean package dogfood passed')
