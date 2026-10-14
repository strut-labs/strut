#!/usr/bin/env python3
"""Focused ConPTY interrupt/ETX reproducer (temporary, v0.0.6 investigation).

Exercises only the interrupt transition repeatedly to raise the number of relevant ConPTY
interrupt events without running the full P8 scenarios:

  spawn -> child installs SIGINT handler -> child reports 'ready' -> parent observes readiness
  -> parent interrupt() -> child prints 'caught=INT' -> child exits 0 -> parent drains EOF and
  waits -> terminal closed.

The program is compiled once, then run `--parallel N` times concurrently to add self-generated
load (without recompiling concurrently). Each attempt uses a fresh PTY child. Any failure prints
a 'FAIL P8-INT-*' marker; the driver exits nonzero if any attempt or run failed. Lifecycle and
cleanup assertions match the certification fixture; nothing is weakened.

Usage: python3 dogfood/pty_interrupt_repro.py <compiler> [attempts] [timeout_seconds] [parallel]
"""
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HELPER = """import signal, sys, time
def interrupted(signum, frame):
    print('caught=INT', flush=True)
    raise SystemExit(0)
signal.signal(signal.SIGINT, interrupted)
print('ready', flush=True)
while True: time.sleep(1)
"""

PROGRAM = r'''function drain(pty terminal) -> string : PtyError {
    string output := "";
    while (!terminal.eof()) { bytes chunk := terminal.read_bytes(4096); if (!chunk.empty()) { output = output + chunk.to_string(); } }
    return output;
}
function read_until(pty terminal, string expected) -> string : PtyError {
    string output := "";
    while (!output.contains(expected) && !terminal.eof()) { output = output + terminal.read_bytes(4096).to_string(); }
    return output;
}
function main() -> int : (PtyError, ThreadError, TimeError, StreamError) {
    int attempts := __ATTEMPTS__;
    for (int i := 0; i < attempts; i++) {
        try {
            terminal := pty_spawn("__PYTHON__", ["__HELPER__", "interrupt"]);
            ready := read_until(terminal, "ready");
            if (!ready.contains("ready")) {
                print("FAIL P8-INT-READY");
            } else {
                terminal.interrupt();
                out := drain(terminal);
                code := terminal.wait();
                terminal.close();
                if (!out.contains("caught=INT")) { print("FAIL P8-INT-DELIVERY"); }
                else if (code != 0) { print("FAIL P8-INT-EXIT"); }
            }
        } catch (PtyError e) {
            print("FAIL P8-INT-EXCEPTION");
        }
    }
    print("DONE");
    return 0;
}
'''


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def main():
    if len(sys.argv) < 2:
        print("usage: pty_interrupt_repro.py <compiler> [attempts] [timeout_seconds] [parallel]")
        return 2
    compiler = sys.argv[1]
    attempts = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0
    parallel = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    with tempfile.TemporaryDirectory(prefix="pty-int-repro-") as td:
        td = Path(td)
        helper = td / "helper.py"
        helper.write_text(HELPER, encoding="utf-8")
        source = td / "p.p"
        program = (PROGRAM
                   .replace("__ATTEMPTS__", str(attempts))
                   .replace("__PYTHON__", escaped(sys.executable))
                   .replace("__HELPER__", escaped(str(helper))))
        source.write_text(program, encoding="utf-8")
        exe = td / "p"
        b = subprocess.run([compiler, str(source), "-o", str(exe)], capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("COMPILE FAILED:", b.stdout[-500:], b.stderr[-500:])
            return 2
        t0 = time.monotonic()
        try:
            if parallel <= 1:
                r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=timeout)
                runs = [(r.returncode, r.stdout, r.stderr)]
            else:
                procs = [subprocess.Popen([str(exe)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for _ in range(parallel)]
                runs = []
                for p in procs:
                    out, err = p.communicate(timeout=timeout)
                    runs.append((p.returncode, out, err))
        except subprocess.TimeoutExpired:
            print(f"TIMEOUT after {timeout}s (attempts={attempts}, parallel={parallel})")
            return 1
        elapsed = time.monotonic() - t0
        total_fails = 0
        total_expected = attempts * max(1, parallel)
        done_runs = 0
        for idx, (rc, out, err) in enumerate(runs):
            fails = [ln for ln in out.splitlines() if ln.startswith("FAIL")]
            done = "DONE" in out.splitlines()
            total_fails += len(fails)
            if done:
                done_runs += 1
            print(f"run[{idx}] exit={rc} done={done} failures={len(fails)}")
            for f in fails[:10]:
                print("   ", f)
            if err.strip():
                print("    stderr:", err[-200:])
        print(f"SUMMARY attempts={total_expected} parallel={parallel} elapsed={elapsed:.1f}s done_runs={done_runs}/{len(runs)} failures={total_fails}")
        ok = all(rc == 0 and "DONE" in out for rc, out, _ in runs) and total_fails == 0
        return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
