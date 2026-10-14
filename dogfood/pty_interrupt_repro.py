#!/usr/bin/env python3
"""Focused ConPTY interrupt/ETX reproducer (temporary, v0.0.6 investigation).

Exercises only the interrupt transition repeatedly to raise the number of relevant ConPTY
interrupt events without running the full P8 scenarios:

  spawn -> child installs SIGINT handler -> child reports 'ready' -> parent observes readiness
  -> parent interrupt() -> child prints 'caught=INT' -> child exits 0 -> parent drains EOF and
  waits -> terminal closed.

Each attempt uses a fresh PTY child. Any attempt that fails prints a 'FAIL P8-INT-*' marker and
the process exits nonzero (after completing all attempts). Lifecycle/cleanup assertions are the
same as the certification fixture; nothing is weakened.

Usage: python3 dogfood/pty_interrupt_repro.py <compiler> [attempts] [timeout_seconds]
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
        print("usage: pty_interrupt_repro.py <compiler> [attempts] [timeout_seconds]")
        return 2
    compiler = sys.argv[1]
    attempts = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0
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
            print("COMPILE FAILED:", b.stdout, b.stderr)
            return 2
        t0 = time.monotonic()
        try:
            r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            print(f"TIMEOUT after {timeout}s (attempts={attempts})")
            return 1
        elapsed = time.monotonic() - t0
        fails = [ln for ln in r.stdout.splitlines() if ln.startswith("FAIL")]
        done = "DONE" in r.stdout.splitlines()
        print(f"attempts={attempts} elapsed={elapsed:.1f}s exit={r.returncode} done={done} failures={len(fails)}")
        for f in fails[:20]:
            print("  ", f)
        if r.stderr.strip():
            print("stderr:", r.stderr[-400:])
        return 0 if (r.returncode == 0 and done and not fails) else 1


if __name__ == "__main__":
    raise SystemExit(main())
