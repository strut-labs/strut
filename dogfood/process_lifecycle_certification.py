#!/usr/bin/env python3
"""Certify argv-safe process spawning, concurrent pipes, and bounded tree cleanup."""

import ctypes
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time


PAYLOAD = 2 * 1024 * 1024


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def process_exists(pid):
    if sys.platform == "win32":
        synchronize = 0x00100000
        handle = ctypes.windll.kernel32.OpenProcess(synchronize, False, pid)
        if not handle:
            return False
        try:
            return ctypes.windll.kernel32.WaitForSingleObject(handle, 0) == 0x102
        finally:
            ctypes.windll.kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def wait_gone(pid, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not process_exists(pid):
            return
        time.sleep(0.02)
    raise RuntimeError(f"descendant {pid} survived owned-tree cleanup")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-process-lifecycle-") as temporary:
        root = Path(temporary)
        work = root / "cwd-é"
        work.mkdir()
        open_file = root / "held-open.txt"
        open_file.write_text("held open", encoding="utf-8")
        helper = root / "process-helper.py"
        helper.write_text(
            f"""import ctypes, os, pathlib, signal, subprocess, sys, threading, time
SIZE = {PAYLOAD}
mode = sys.argv[1]
def write(fd, byte):
    chunk = byte * 8192
    for _ in range(SIZE // len(chunk)):
        os.write(fd, chunk)
if mode == 'argv':
    expected = ['', 'two words', 'quote\\"value', 'trailing\\\\', 'π']
    os.write(1, b'ok' if sys.argv[2:] == expected else repr(sys.argv[2:]).encode())
elif mode == 'options':
    expected = 'cwd-é|value-π'
    actual = pathlib.Path.cwd().name + '|' + os.environ.get('STRUT_UNICODE_ENV', '')
    os.write(1, b'ok' if actual == expected else actual.encode())
elif mode == 'fds':
    sentinel = int(os.environ['STRUT_SENTINEL_DESCRIPTOR'])
    if sys.platform == 'win32':
        flags = ctypes.c_ulong()
        inherited = bool(ctypes.windll.kernel32.GetHandleInformation(ctypes.c_void_p(sentinel), ctypes.byref(flags)))
    else:
        try:
            os.fstat(sentinel)
            inherited = True
        except OSError:
            inherited = False
    os.write(1, b'leaked' if inherited else b'ok')
elif mode == 'spam':
    error = threading.Thread(target=write, args=(2, b'e'))
    error.start(); write(1, b'o'); error.join()
elif mode == 'crlf':
    os.write(1, b'a\\r\\nb\\r\\n')
elif mode == 'producer':
    error = threading.Thread(target=write, args=(2, b'p'))
    error.start(); write(1, b'd'); error.join()
elif mode == 'consumer':
    error = threading.Thread(target=write, args=(2, b'c'))
    error.start(); data = sys.stdin.buffer.read(); error.join()
    os.write(1, str(len(data)).encode())
elif mode == 'resistant':
    if hasattr(signal, 'SIGTERM'): signal.signal(signal.SIGTERM, signal.SIG_IGN)
    if hasattr(signal, 'SIGBREAK'): signal.signal(signal.SIGBREAK, signal.SIG_IGN)
    time.sleep(30)
elif mode == 'resistant-ready':
    if hasattr(signal, 'SIGTERM'): signal.signal(signal.SIGTERM, signal.SIG_IGN)
    if hasattr(signal, 'SIGBREAK'): signal.signal(signal.SIGBREAK, signal.SIG_IGN)
    os.write(1, b'ready')
    time.sleep(30)
elif mode in ('tree', 'retention', 'resistant-tree'):
    descendant = subprocess.Popen([sys.executable, __file__, 'resistant'])
    pathlib.Path(sys.argv[2]).write_text(str(descendant.pid), encoding='ascii')
    os.write(1, b'ready')
    if mode == 'retention': sys.exit(0)
    if mode == 'resistant-tree':
        if hasattr(signal, 'SIGTERM'): signal.signal(signal.SIGTERM, signal.SIG_IGN)
        if hasattr(signal, 'SIGBREAK'): signal.signal(signal.SIGBREAK, signal.SIG_IGN)
    time.sleep(30)
elif mode == 'quick':
    os.write(1, b'q')
""",
            encoding="utf-8",
        )
        retention_pid = root / "retention.pid"
        exec_retention_pid = root / "exec-retention.pid"
        terminate_pid = root / "terminate.pid"
        destructor_pid = root / "destructor.pid"
        expected_crlf = "a\\nb\\n" if sys.platform == "win32" else "a\\r\\nb\\r\\n"
        source = f'''function abandon(string python, string helper, string pid_file) -> void : ExecError {{
    doomed := process(python, [helper, "resistant-tree", pid_file]);
    if (doomed.out.read(5) != "ready") {{ doomed.terminate(); doomed.wait(); }}
    return;
}}

function main() -> int : (ExecError, StreamError, ThreadError, TimeError) {{
    argv_result := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "argv", "", "two words", "quote\\\"value", "trailing\\\\", "π"]);
    if (argv_result.exit_code != 0 || argv_result.stdout != "ok" || argv_result.stderr != "") {{ return 1; }}

    options := {{"cwd": "{escaped(work)}", "env": {{"STRUT_UNICODE_ENV": "value-π"}}}};
    configured := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "options"], options);
    if (configured.exit_code != 0 || configured.stdout != "ok") {{ return 2; }}
    streamed := process("{escaped(sys.executable)}", ["{escaped(helper)}", "options"], options);
    if (streamed.out.read_all() != "ok" || streamed.wait() != 0) {{ return 3; }}

    ifstream held_open := ifstream("{escaped(open_file)}");
    descriptors := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "fds"]);
    if (descriptors.stdout != "ok" || descriptors.exit_code != 0) {{ return 4; }}
    held_open.close();

    spam := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "spam"]);
    if (spam.exit_code != 0 || spam.stdout.length() != {PAYLOAD} || spam.stderr.length() != {PAYLOAD}) {{ return 5; }}
    crlf := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "crlf"]);
    if (crlf.stdout != "{expected_crlf}") {{ return 10; }}

    exec_retained := exec("{escaped(sys.executable)}", ["{escaped(helper)}", "retention", "{escaped(exec_retention_pid)}"]);
    if (exec_retained.exit_code != 0 || exec_retained.stdout != "ready") {{ return 11; }}

    pipeline := pipe_exec("{escaped(sys.executable)}", ["{escaped(helper)}", "producer"], "{escaped(sys.executable)}", ["{escaped(helper)}", "consumer"]);
    if (pipeline.exit_code != 0 || pipeline.stdout != "{PAYLOAD}" || pipeline.stderr.length() != {PAYLOAD * 2}) {{ return 6; }}

    retained := process("{escaped(sys.executable)}", ["{escaped(helper)}", "retention", "{escaped(retention_pid)}"]);
    if (retained.wait() != 0 || retained.out.read_all() != "ready") {{ return 7; }}

    tree := process("{escaped(sys.executable)}", ["{escaped(helper)}", "tree", "{escaped(terminate_pid)}"]);
    if (tree.out.read(5) != "ready") {{ return 8; }}
    tree.terminate();
    tree.wait();

    abandon("{escaped(sys.executable)}", "{escaped(helper)}", "{escaped(destructor_pid)}");

    for (cycle := 0; cycle < 25; cycle++) {{
        quick := process("{escaped(sys.executable)}", ["{escaped(helper)}", "quick"]);
        if (quick.out.read_all() != "q" || quick.wait() != 0) {{ return 9; }}
    }}
    print("process lifecycle certification passed");
    return 0;
}}
'''
        program = root / "process-lifecycle.p"
        executable = root / ("process-lifecycle.exe" if sys.platform == "win32" else "process-lifecycle")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
        started = time.monotonic()
        sentinel_read, sentinel_write = os.pipe()
        try:
            if sys.platform == "win32":
                import msvcrt

                sentinel = msvcrt.get_osfhandle(sentinel_read)
                os.set_handle_inheritable(sentinel, True)
            else:
                sentinel = sentinel_read
                os.set_inheritable(sentinel, True)
            environment = os.environ.copy()
            environment["STRUT_SENTINEL_DESCRIPTOR"] = str(sentinel)
            result = subprocess.run([executable], cwd=root, env=environment, close_fds=False, text=True, capture_output=True, timeout=20, check=False)
        finally:
            os.close(sentinel_read)
            os.close(sentinel_write)
        elapsed = time.monotonic() - started
        if result.returncode != 0 or result.stdout != "process lifecycle certification passed\n" or result.stderr:
            raise RuntimeError(f"exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")
        if elapsed > 8:
            raise RuntimeError(f"bounded destructor/process cycles took {elapsed:.2f}s")
        for pid_file in (exec_retention_pid, retention_pid, terminate_pid, destructor_pid):
            wait_gone(int(pid_file.read_text(encoding="ascii")))
        bound_source = f'''function main() -> int : ExecError {{
    child := process("{escaped(sys.executable)}", ["{escaped(helper)}", "resistant-ready"]);
    if (child.out.read(5) != "ready") {{ return 1; }}
    return 0;
}}
'''
        bound_program = root / "process-destructor-bound.p"
        bound_executable = root / ("process-destructor-bound.exe" if sys.platform == "win32" else "process-destructor-bound")
        bound_program.write_text(bound_source, encoding="utf-8")
        subprocess.run([compiler, bound_program, "-o", bound_executable], cwd=root, check=True)
        bound_started = time.monotonic()
        bound_result = subprocess.run([bound_executable], cwd=root, capture_output=True, timeout=3, check=False)
        bound_elapsed = time.monotonic() - bound_started
        if bound_result.returncode != 0 or bound_result.stdout or bound_result.stderr or bound_elapsed > 2.5:
            raise RuntimeError(
                f"resistant direct-child destructor was not bounded: elapsed={bound_elapsed:.2f}s "
                f"exit={bound_result.returncode} stdout={bound_result.stdout!r} stderr={bound_result.stderr!r}"
            )
    print("Process lifecycle certification: Unicode/empty argv, cwd/env, CRLF compatibility, descriptor isolation, 2 MiB simultaneous streams, pipeline backpressure, exec/process descendant retention, bounded resistant cleanup, and 25 cycles passed")


if __name__ == "__main__":
    main()
