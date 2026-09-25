#!/usr/bin/env python3
from __future__ import annotations
import argparse, http.client, json, os, platform, shutil, statistics, subprocess, tempfile, time
from pathlib import Path

def run(cmd, cwd):
    t=time.perf_counter(); p=subprocess.run(cmd,cwd=cwd,text=True,capture_output=True,check=False); return p,time.perf_counter()-t

def wait_get(port, path='/'):
    start=time.perf_counter()
    deadline=start+10
    while time.perf_counter()<deadline:
        try:
            c=http.client.HTTPConnection('127.0.0.1',port,timeout=1); c.request('GET',path); r=c.getresponse(); r.read(); c.close(); return time.perf_counter()-start
        except OSError: time.sleep(0.005)
    raise RuntimeError('server did not become ready')

def rss_kib(pid):
    status=Path(f'/proc/{pid}/status')
    if not status.exists(): return None
    for line in status.read_text().splitlines():
        if line.startswith('VmRSS:'): return int(line.split()[1])
    return None

def percentile(values,p):
    values=sorted(values); idx=min(len(values)-1,max(0,int(round((len(values)-1)*p)))); return values[idx]

def benchmark_binary(binary, cwd, port, requests=100):
    proc=subprocess.Popen([str(binary)],cwd=cwd,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True)
    try:
        startup=wait_get(port,'/')
        rss=rss_kib(proc.pid)
        lat=[]; begin=time.perf_counter()
        for _ in range(requests):
            t=time.perf_counter(); c=http.client.HTTPConnection('127.0.0.1',port,timeout=2); c.request('GET','/api/todos'); r=c.getresponse(); r.read(); c.close(); lat.append((time.perf_counter()-t)*1000)
        elapsed=time.perf_counter()-begin
        proc.wait(timeout=5)
        return {'cold_start_ms':startup*1000,'idle_rss_kib':rss,'requests':requests,'throughput_req_s':requests/elapsed,'latency_mean_ms':statistics.mean(lat),'latency_p50_ms':percentile(lat,.5),'latency_p95_ms':percentile(lat,.95)}
    finally:
        if proc.poll() is None: proc.kill()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--compiler',type=Path,required=True); ap.add_argument('--output',type=Path,required=True); args=ap.parse_args()
    compiler=args.compiler.resolve(); source_root=Path(__file__).resolve().parents[1]/'examples'/'one-binary-todo'
    result={'platform':platform.platform(),'machine':platform.machine(),'python':platform.python_version(),'compiler':str(compiler),'modes':{}}
    with tempfile.TemporaryDirectory(prefix='strut-first-app-') as td:
        root=Path(td); shutil.copytree(source_root,root/'app'); app=root/'app'
        text=(app/'app.p').read_text(); text=text.replace('18082, 2','18082, 101'); (app/'app.p').write_text(text)
        init=subprocess.run([str(compiler),'init'],cwd=app,text=True,capture_output=True); assert init.returncode==0,init.stderr
        for mode in ('debug','release'):
            shutil.rmtree(app/'.strut'/'obj',ignore_errors=True); shutil.rmtree(app/'.strut'/'info',ignore_errors=True); shutil.rmtree(app/'.strut'/'gen',ignore_errors=True)
            binary=app/f'app-{mode}'; cmd=[str(compiler),'app.p','-o',str(binary)] + (['--release'] if mode=='release' else [])
            p,seconds=run(cmd,app)
            if p.returncode!=0: raise RuntimeError(p.stderr)
            metrics=benchmark_binary(binary,app,18082)
            metrics['cold_compile_s']=seconds; metrics['executable_bytes']=binary.stat().st_size
            result['modes'][mode]=metrics
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps(result,indent=2))
if __name__=='__main__': main()
