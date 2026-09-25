#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, os, platform, statistics, subprocess, tempfile, time
from pathlib import Path

ROOT=Path(__file__).resolve().parent
PROGRAMS=["hello","arithmetic","collections_json","ptr","lambda","thread_async","sqlite"]

def timed(cmd, **kw):
    start=time.perf_counter(); p=subprocess.run(cmd,text=True,capture_output=True,check=False,**kw); return p,time.perf_counter()-start

def median_run(binary:Path, cwd:Path, count:int=5):
    vals=[]
    for _ in range(count):
        p,t=timed([str(binary)],cwd=cwd)
        if p.returncode!=0: raise RuntimeError(p.stderr)
        vals.append(t)
    return statistics.median(vals)*1000.0

def compile_one(compiler:Path, source:Path, output:Path, extra:list[str]):
    p,t=timed([str(compiler),str(source),"-o",str(output),*extra],cwd=source.parent)
    if p.returncode!=0: raise RuntimeError(f"{source.name}: {p.stderr}")
    return t,output.stat().st_size

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--compiler",type=Path,required=True); ap.add_argument("--output",type=Path,required=True); args=ap.parse_args()
    compiler=args.compiler.resolve(); result={"platform":platform.platform(),"machine":platform.machine(),"compiler":str(compiler),"programs":{},"linking":{},"first_app":None}
    with tempfile.TemporaryDirectory(prefix="strut-bench-") as td:
        tmp=Path(td)
        for name in PROGRAMS:
            src=ROOT/"programs"/(name+".p"); exe=tmp/name
            compile_s,size=compile_one(compiler,src,exe,["--release"])
            result["programs"][name]={"release_compile_s":compile_s,"release_bytes":size,"median_run_ms":median_run(exe,src.parent)}
        hello=ROOT/"programs"/"hello.p"
        for mode,flags in (("dynamic",["--release","--dynamic"]),("static",["--release","--static"])):
            exe=tmp/("hello-"+mode); p,t=timed([str(compiler),str(hello),"-o",str(exe),*flags],cwd=hello.parent)
            if p.returncode==0:
                result["linking"][mode]={"compile_s":t,"bytes":exe.stat().st_size,"median_start_ms":median_run(exe,hello.parent,10)}
            else:
                result["linking"][mode]={"supported":False,"diagnostic":p.stderr.strip()}
    prior=ROOT/"results"/"first-app.json"
    if prior.exists(): result["first_app"]=json.loads(prior.read_text())
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(json.dumps(result,indent=2)+"\n"); print(json.dumps(result,indent=2))
if __name__=="__main__": main()
