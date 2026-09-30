#!/usr/bin/env python3
"""Certify the P8 PTY control surface, lifecycle races, and resources."""

import ctypes
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def linux_sample(pid):
    root = Path(f"/proc/{pid}")
    try:
        descriptors = len(list((root / "fd").iterdir()))
        fields = {}
        for line in (root / "status").read_text(encoding="utf-8").splitlines():
            if ":" in line:
                name, value = line.split(":", 1)
                fields[name] = value.strip()
        children_text = (root / "task" / str(pid) / "children").read_text(encoding="utf-8").strip()
        children = [int(value) for value in children_text.split()] if children_text else []
        zombies = 0
        for child in children:
            try:
                child_status = Path(f"/proc/{child}/status").read_text(encoding="utf-8")
                zombies += "\nState:\tZ" in "\n" + child_status
            except (FileNotFoundError, ProcessLookupError):
                pass
        rss_kib = int(fields.get("VmRSS", "0 kB").split()[0])
        return {"fd": descriptors, "children": len(children), "zombies": zombies, "rss_kib": rss_kib, "threads": int(fields["Threads"])}
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return None


def windows_sample(pid):
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.GetProcessHandleCount.argtypes = [wintypes.HANDLE, wintypes.LPDWORD]
    kernel32.GetProcessHandleCount.restype = wintypes.BOOL
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    process = kernel32.OpenProcess(0x0400, False, pid)
    if not process:
        return None
    try:
        handles = ctypes.c_ulong()
        if not kernel32.GetProcessHandleCount(process, ctypes.byref(handles)):
            return None
    finally:
        kernel32.CloseHandle(process)

    class ThreadEntry(ctypes.Structure):
        _fields_ = [
            ("size", wintypes.DWORD),
            ("usage", wintypes.DWORD),
            ("thread_id", wintypes.DWORD),
            ("owner_pid", wintypes.DWORD),
            ("base_priority", wintypes.LONG),
            ("delta_priority", wintypes.LONG),
            ("flags", wintypes.DWORD),
        ]

    kernel32.Thread32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(ThreadEntry)]
    kernel32.Thread32First.restype = wintypes.BOOL
    kernel32.Thread32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(ThreadEntry)]
    kernel32.Thread32Next.restype = wintypes.BOOL
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000004, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        return None
    threads = 0
    try:
        entry = ThreadEntry()
        entry.size = ctypes.sizeof(entry)
        present = kernel32.Thread32First(snapshot, ctypes.byref(entry))
        while present:
            if entry.owner_pid == pid:
                threads += 1
            present = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return {"handles": handles.value, "threads": threads}


def process_exists(pid):
    if sys.platform == "win32":
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel32.WaitForSingleObject.restype = wintypes.DWORD
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL
        handle = kernel32.OpenProcess(0x00100000, False, pid)
        if not handle:
            error = ctypes.get_last_error()
            if error == 87:
                return False
            raise OSError(error, f"OpenProcess failed for descendant {pid}")
        try:
            result = kernel32.WaitForSingleObject(handle, 0)
            if result == 0:
                return False
            if result == 0x102:
                return True
            raise OSError(ctypes.get_last_error(), f"WaitForSingleObject failed for descendant {pid}")
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def wait_gone(pid, description, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not process_exists(pid):
            return
        time.sleep(0.02)
    raise RuntimeError(f"{description} PID {pid} survived PTY cleanup")


def maxima(left, right):
    if left is None:
        return dict(right)
    return {name: max(left[name], right[name]) for name in left}


def linux_fd_targets(pid):
    result = {}
    for descriptor in Path(f"/proc/{pid}/fd").iterdir():
        try:
            result[descriptor.name] = os.readlink(descriptor)
        except FileNotFoundError:
            pass
    return result


def require_pid_gone(pid, description, timeout=3):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return
        time.sleep(0.01)
    try:
        os.kill(pid, 9)
    except ProcessLookupError:
        return
    raise RuntimeError(f"{description} PID {pid} survived PTY cleanup")


def certify_resources(executable, root):
    process = subprocess.Popen([executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1)
    lines = queue.Queue()

    def consume():
        for line in process.stdout:
            lines.put(line.rstrip("\n"))

    reader = threading.Thread(target=consume)
    reader.start()
    baseline = peak = after = None
    after_fds = None
    seen_done = False
    observed = []
    deadline = time.monotonic() + (120 if sys.platform == "win32" else 60)
    while time.monotonic() < deadline and not seen_done:
        try:
            line = lines.get(timeout=0.01)
            observed.append(line)
            if line == "RESOURCE_BASELINE" and sys.platform.startswith("linux"):
                baseline = linux_sample(process.pid)
            elif line == "RESOURCE_BASELINE" and sys.platform == "win32":
                baseline = windows_sample(process.pid)
            elif line == "RESOURCE_AFTER" and sys.platform.startswith("linux"):
                time.sleep(0.05)
                after = linux_sample(process.pid)
                after_fds = linux_fd_targets(process.pid)
            elif line == "RESOURCE_AFTER" and sys.platform == "win32":
                deadline_after = time.monotonic() + 3
                after = windows_sample(process.pid)
                while time.monotonic() < deadline_after:
                    sample = windows_sample(process.pid)
                    if sample is not None:
                        after = sample if after is None else {name: min(after[name], sample[name]) for name in after}
                    time.sleep(0.05)
            elif line == "PTY lifecycle certification passed":
                seen_done = True
        except queue.Empty:
            if process.poll() is not None:
                break
        if sys.platform.startswith("linux") and process.poll() is None:
            sample = linux_sample(process.pid)
            if sample is not None:
                peak = maxima(peak, sample)
        elif sys.platform == "win32" and process.poll() is None:
            sample = windows_sample(process.pid)
            if sample is not None:
                peak = maxima(peak, sample)
    try:
        returncode = process.wait(timeout=max(1, deadline - time.monotonic()))
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        reader.join(timeout=2)
        raise RuntimeError(f"P8 PTY lifecycle fixture timed out; stdout={observed!r} stderr={process.stderr.read()!r}")
    reader.join()
    stderr = process.stderr.read()
    if returncode != 0 or not seen_done or stderr:
        raise RuntimeError(f"exit={returncode} seen_done={seen_done} stdout={observed!r} stderr={stderr!r}")
    if sys.platform.startswith("linux"):
        if baseline is None or peak is None or after is None:
            raise RuntimeError(f"missing Linux resource samples: baseline={baseline} peak={peak} after={after}")
        for field in ("fd", "children", "zombies"):
            if after[field] != baseline[field]:
                raise RuntimeError(f"PTY {field} did not return to baseline: baseline={baseline} after={after} fds={after_fds!r}")
        sanitizer_flags = os.environ.get("STRUT_CXXFLAGS", "")
        thread_allowance = 1 if "sanitize=thread" in sanitizer_flags else 0
        if after["threads"] > baseline["threads"] + thread_allowance:
            raise RuntimeError(f"PTY threads did not return within allowance: baseline={baseline} after={after} allowance={thread_allowance}")
        sanitizer = "sanitize" in sanitizer_flags
        allowance = 262144 if sanitizer else 8192
        if after["rss_kib"] > baseline["rss_kib"] + allowance:
            raise RuntimeError(f"PTY RSS did not return within allowance: baseline={baseline} after={after} allowance={allowance}")
        print(f"P8 PTY resources Linux: baseline={baseline} peak={peak} after={after} rss_allowance_kib={allowance} thread_allowance={thread_allowance}")
    elif sys.platform == "win32":
        if baseline is None or peak is None or after is None:
            raise RuntimeError(f"missing Windows resource samples: baseline={baseline} peak={peak} after={after}")
        if after["handles"] != baseline["handles"]:
            raise RuntimeError(f"PTY HANDLE count did not return to baseline: baseline={baseline} peak={peak} after={after}")
        if after["threads"] != baseline["threads"]:
            raise RuntimeError(f"PTY thread count did not return to baseline: baseline={baseline} peak={peak} after={after}")
        print(f"P9 PTY resources Windows: baseline={baseline} peak={peak} after={after}")
    else:
        print("P8 PTY resources: native counters unsupported on this platform; functional fixture executed")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    compile_flags = ["--release"] if os.environ.get("STRUT_PTY_RELEASE") == "1" else []
    with tempfile.TemporaryDirectory(prefix="strut-pty-p8-") as temporary:
        root = Path(temporary)
        program = root / "pty-lifecycle.p"
        executable = root / ("pty-lifecycle.exe" if sys.platform == "win32" else "pty-lifecycle")
        if sys.platform == "win32":
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            if not all(hasattr(kernel32, name) for name in ("CreatePseudoConsole", "ResizePseudoConsole", "ClosePseudoConsole")):
                program.write_text(
                    'function inspect(pty terminal) -> void : PtyError { terminal.resize(24, 80); terminal.interrupt(); terminal.terminate(); terminal.kill(); terminal.hangup(); terminal.wait(); return; } function main() -> int { return 0; }\n',
                    encoding="utf-8",
                )
                subprocess.run([compiler, program, "-o", executable, *compile_flags], cwd=root, check=True)
                subprocess.run([executable], cwd=root, check=True)
                print("P9 Windows ConPTY lifecycle API compile certification passed on a pre-1809 host; spawn fallback is covered by pty_certification.py")
                return
            descendant_pid_file = root / "descendant.pid"
            natural_descendant_pid_file = root / "natural-descendant.pid"
            abandoned_pid_file = root / "abandoned.pid"
            abandoned_marker = root / "abandoned-survived.txt"
            helper = root / "conpty-lifecycle-helper.py"
            helper.write_text(
                """import os, pathlib, signal, subprocess, sys, time
mode = sys.argv[1]
if mode == 'resize':
    print('ready', flush=True)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        size = os.get_terminal_size(1)
        if size.lines == 51 and size.columns == 133:
            print('resized=51x133', flush=True)
            raise SystemExit(0)
        time.sleep(0.01)
    raise SystemExit(2)
elif mode == 'interrupt':
    def interrupted(signum, frame):
        print('caught=INT', flush=True)
        raise SystemExit(0)
    signal.signal(signal.SIGINT, interrupted)
    print('ready', flush=True)
    while True: time.sleep(1)
elif mode == 'hangup':
    print('ready', flush=True)
    sys.stdin.buffer.read()
    print('eof', flush=True)
elif mode == 'sleep':
    print('ready', flush=True)
    time.sleep(30)
elif mode == 'descendant':
    child = subprocess.Popen([sys.executable, __file__, 'sleep'])
    pathlib.Path(sys.argv[2]).write_text(str(child.pid), encoding='ascii')
    time.sleep(30)
elif mode == 'leader-exit-descendant':
    child = subprocess.Popen([sys.executable, __file__, 'sleep'])
    pathlib.Path(sys.argv[2]).write_text(str(child.pid), encoding='ascii')
elif mode == 'delayed-marker':
    pathlib.Path(sys.argv[2]).write_text(str(os.getpid()), encoding='ascii')
    print('ready', flush=True)
    time.sleep(1)
    pathlib.Path(sys.argv[3]).write_text('survived', encoding='ascii')
""",
                encoding="utf-8",
            )
            source = f'''function drain(pty terminal) -> string : PtyError {{
    string output := "";
    while (!terminal.eof()) {{ bytes chunk := terminal.read_bytes(4096); if (!chunk.empty()) {{ output = output + chunk.to_string(); }} }}
    return output;
}}

function read_until(pty terminal, string expected) -> string : PtyError {{
    string output := "";
    while (!output.contains(expected) && !terminal.eof()) {{ output = output + terminal.read_bytes(4096).to_string(); }}
    return output;
}}

function abandon(string helper, string pid_file, string marker) -> void : PtyError {{
    abandoned := pty_spawn("{escaped(sys.executable)}", [helper, "delayed-marker", pid_file, marker]);
    if (!read_until(abandoned, "ready").contains("ready")) {{ return; }}
    escaped_copy := abandoned;
    return;
}}

function main() -> int : (PtyError, ThreadError, TimeError, StreamError) {{
    warmup := pty_spawn("cmd.exe", ["/D", "/Q", "/C", "exit 0"]);
    if (warmup.wait() != 0) {{ return 1; }}
    warmup.close();
    sleep_ms(100);
    print("RESOURCE_BASELINE"); out.flush();
    sleep_ms(1000);

    natural_tree := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "leader-exit-descendant", "{escaped(natural_descendant_pid_file)}"]);
    drain(natural_tree);
    if (natural_tree.wait() != 0) {{ return 1; }}
    natural_tree.close();

    resized := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "resize"]);
    if (!read_until(resized, "ready").contains("ready")) {{ return 2; }}
    resized.resize(51, 133);
    if (!drain(resized).contains("resized=51x133") || resized.wait() != 0) {{ return 2; }}
    bool bad_resize := false;
    invalid_resize := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    try {{ invalid_resize.resize(0, 80); }} catch (PtyError caught) {{ bad_resize = true; }}
    if (!bad_resize) {{ return 3; }}
    bad_resize = false;
    try {{ invalid_resize.resize(24, 32768); }} catch (PtyError caught) {{ bad_resize = true; }}
    if (!bad_resize) {{ return 3; }}
    invalid_resize.close();

    interrupted := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "interrupt"]);
    if (!read_until(interrupted, "ready").contains("ready")) {{ return 4; }}
    interrupted.interrupt();
    if (!drain(interrupted).contains("caught=INT") || interrupted.wait() != 0) {{ return 4; }}

    hung_up := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "hangup"]);
    if (!read_until(hung_up, "ready").contains("ready")) {{ return 5; }}
    hung_up.hangup();
    if (!drain(hung_up).contains("eof") || hung_up.wait() != 0) {{ return 5; }}

    terminated := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    if (!read_until(terminated, "ready").contains("ready")) {{ return 6; }}
    terminated.terminate();
    int terminated_status := terminated.wait();
    if (terminated.running() || terminated.exit_code() != terminated_status || terminated.wait() != terminated_status) {{ return 6; }}

    killed := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    if (!read_until(killed, "ready").contains("ready")) {{ return 7; }}
    killed.kill();
    if (killed.wait() != 137 || killed.running() || killed.exit_code() != 137) {{ return 7; }}

    cancellation_source wait_source;
    cancelled_wait := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], wait_source.token());
    channel<bool> wait_result;
    wait_worker := thread(() => {{ try {{ cancelled_wait.wait(); wait_result.send(false); }} catch (PtyError caught) {{ wait_result.send(caught.code == 125); }} }});
    sleep_ms(50); wait_source.cancel();
    if (!(wait_result.receive() ?? false)) {{ return 8; }}
    wait_worker.join(); cancelled_wait.close();

    resize_race := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    if (!read_until(resize_race, "ready").contains("ready")) {{ return 9; }}
    resize_worker := thread(() => {{ for (index := 0; index < 500; index++) {{ try {{ resize_race.resize(24 + index % 50, 80 + index % 100); }} catch (PtyError caught) {{ return; }} }} }});
    signal_worker := thread(() => {{ for (index := 0; index < 100; index++) {{ try {{ resize_race.interrupt(); }} catch (PtyError caught) {{ return; }} }} }});
    sleep_ms(10); resize_race.close(); resize_worker.join(); signal_worker.join();

    for (close_cancel_cycle := 0; close_cancel_cycle < 20; close_cancel_cycle++) {{
        cancellation_source source;
        raced := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], source.token());
        channel<int> result;
        waiter := thread(() => {{ try {{ result.send(raced.wait()); }} catch (PtyError caught) {{ result.send(-caught.code); }} }});
        closer := thread(() => {{ raced.close(); }});
        canceller := thread(() => {{ source.cancel(); }});
        int status := result.receive() ?? -999;
        waiter.join(); closer.join(); canceller.join();
        int cached := raced.wait();
        if ((status != -125 && status != cached) || raced.running() || raced.exit_code() != cached) {{ return 10; }}
    }}

    abandon("{escaped(helper)}", "{escaped(abandoned_pid_file)}", "{escaped(abandoned_marker)}");
    for (cycle := 0; cycle < 300; cycle++) {{
        quick := pty_spawn("cmd.exe", ["/D", "/Q", "/C", "exit 0"]);
        escaped_copy := quick;
        if (cycle % 2 == 0) {{ if (quick.wait() != 0) {{ return 11; }} }} else {{ quick.close(); }}
        escaped_copy.close();
    }}
    print("RESOURCE_AFTER"); out.flush();
    sleep_ms(500);
    print("PTY lifecycle certification passed"); out.flush();
    return 0;
}}
'''
            program.write_text(source, encoding="utf-8")
            subprocess.run([compiler, program, "-o", executable, *compile_flags], cwd=root, check=True)
            certify_resources(executable, root)
            if not natural_descendant_pid_file.exists():
                raise RuntimeError("natural-exit ConPTY descendant PID was not reported")
            wait_gone(int(natural_descendant_pid_file.read_text(encoding="ascii")), "natural-exit ConPTY descendant")
            if not abandoned_pid_file.exists():
                raise RuntimeError("abandoned ConPTY helper never reported readiness")
            wait_gone(int(abandoned_pid_file.read_text(encoding="ascii")), "abandoned ConPTY child")
            if abandoned_marker.exists():
                raise RuntimeError("abandoned live ConPTY survived last-owner cleanup")

            owner_source = root / "conpty-owner-shutdown.p"
            owner_executable = root / "conpty-owner-shutdown.exe"
            owner_source.write_text(
                f'''function main() -> int : (PtyError, TimeError) {{ terminal := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "descendant", "{escaped(descendant_pid_file)}"]); escaped_terminal := terminal; sleep_ms(250); return 0; }}\n''',
                encoding="utf-8",
            )
            subprocess.run([compiler, owner_source, "-o", owner_executable, *compile_flags], cwd=root, check=True)
            result = subprocess.run(owner_executable, cwd=root, text=True, capture_output=True, timeout=15, check=False)
            if result.returncode != 0 or result.stderr or not descendant_pid_file.exists():
                raise RuntimeError(f"ConPTY owner shutdown failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")
            wait_gone(int(descendant_pid_file.read_text(encoding="ascii")), "ConPTY descendant")
            print("P9 Windows ConPTY lifecycle certification: resize, ETX interrupt, input hangup, terminate/kill approximations, wait cancellation, close/cancel and resize/control races, exact descendant cleanup, HANDLE/thread return, owner shutdown, and 300 spawn/exit cycles passed")
            return

        helper = root / "pty-p8-helper.py"
        helper.write_text(
            """import fcntl, os, signal, struct, sys, termios, time, tty
mode = sys.argv[1]
if mode == 'resize':
    def resized(signum, frame):
        rows, columns, _, _ = struct.unpack('HHHH', fcntl.ioctl(0, termios.TIOCGWINSZ, b'\\0' * 8))
        os.write(1, ('resized=' + str(rows) + 'x' + str(columns)).encode())
        raise SystemExit(0)
    signal.signal(signal.SIGWINCH, resized)
    os.write(1, b'ready')
    while True: signal.pause()
elif mode == 'catch':
    selected = getattr(signal, 'SIG' + sys.argv[2])
    def caught(signum, frame):
        os.write(1, ('caught=' + sys.argv[2]).encode())
        raise SystemExit(0)
    signal.signal(selected, caught)
    identity = ';leader=' + str(os.getsid(0) == os.getpid() and os.getpgrp() == os.getpid()) + ';foreground=' + str(os.tcgetpgrp(0) == os.getpgrp())
    os.write(1, ('ready' + identity).encode())
    while True: signal.pause()
elif mode == 'resistant':
    signal.signal(signal.SIGTERM, lambda signum, frame: os.write(1, b'resisted'))
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    os.write(1, b'ready')
    while True: time.sleep(1)
elif mode == 'raw-copy':
    tty.setraw(0)
    os.write(1, b'ready')
    remaining = 65536
    while remaining:
        data = os.read(0, min(remaining, 4096))
        if not data: break
        remaining -= len(data)
        offset = 0
        while offset < len(data): offset += os.write(1, data[offset:])
elif mode == 'sleep':
    time.sleep(30)
elif mode == 'orphan-foreground':
    signal.signal(signal.SIGINT, lambda signum, frame: os.write(1, b'wrong-parent'))
    ready_read, ready_write = os.pipe()
    leader = os.fork()
    if leader == 0:
        os.close(ready_read)
        os.setpgid(0, 0)
        member = os.fork()
        if member == 0:
            def orphan_caught(signum, frame):
                os.write(1, b'orphan-caught')
                os._exit(0)
            signal.signal(signal.SIGINT, orphan_caught)
            signal.signal(signal.SIGHUP, signal.SIG_IGN)
            os.write(ready_write, b'1')
            os.close(ready_write)
            while True: signal.pause()
        os.close(ready_write)
        os.write(1, ('orphan-member=' + str(member) + ';').encode())
        time.sleep(0.05)
        os._exit(0)
    os.close(ready_write)
    try: os.setpgid(leader, leader)
    except ProcessLookupError: pass
    os.read(ready_read, 1)
    os.close(ready_read)
    os.tcsetpgrp(0, leader)
    os.waitpid(leader, 0)
    gone = False
    try: os.kill(leader, 0)
    except ProcessLookupError: gone = True
    os.write(1, ('orphan-ready;group=' + str(leader) + ';leader-gone=' + str(gone)).encode())
    while True: time.sleep(1)
elif mode == 'descendant':
    descendant = os.fork()
    if descendant == 0:
        while True: time.sleep(1)
    with open(sys.argv[2], 'w', encoding='utf-8') as output:
        output.write(str(descendant))
    time.sleep(30)
""",
            encoding="utf-8",
        )
        source = f'''function drain(pty terminal) -> string : PtyError {{
    string output := "";
    while (!terminal.eof()) {{ bytes chunk := terminal.read_bytes(4096); if (!chunk.empty()) {{ output = output + chunk.to_string(); }} }}
    return output;
}}

function read_until(pty terminal, string expected) -> string : PtyError {{
    string output := "";
    while (!output.contains(expected) && !terminal.eof()) {{ output = output + terminal.read_bytes(4096).to_string(); }}
    return output;
}}

function abandon(string helper) -> void : PtyError {{
    abandoned := pty_spawn("{escaped(sys.executable)}", [helper, "sleep"]);
    escaped_abandoned := abandoned;
    return;
}}

function main() -> int : (PtyError, ThreadError, TimeError, StreamError) {{
    print("RESOURCE_BASELINE");
    out.flush();
    sleep_ms(100);

    resized := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "resize"]);
    if (resized.read_bytes(5).to_string() != "ready") {{ return 1; }}
    resized.resize(51, 133);
    if (!drain(resized).contains("resized=51x133") || resized.wait() != 0) {{ return 2; }}
    resized.close();
    bool bad_resize := false;
    invalid_resize := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    try {{ invalid_resize.resize(0, 80); }} catch (PtyError caught) {{ bad_resize = true; }}
    if (!bad_resize) {{ return 3; }}
    bad_resize = false;
    try {{ invalid_resize.resize(24, 32768); }} catch (PtyError caught) {{ bad_resize = true; }}
    if (!bad_resize) {{ return 4; }}
    invalid_resize.close();

    interrupted := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "catch", "INT"]);
    if (interrupted.read_bytes(5).to_string() != "ready") {{ return 5; }}
    interrupted.interrupt();
    if (!drain(interrupted).contains("caught=INT") || interrupted.wait() != 0) {{ return 6; }}
    interrupted.close();

    terminated := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "catch", "TERM"]);
    if (terminated.read_bytes(5).to_string() != "ready") {{ return 7; }}
    terminated.terminate();
    if (!drain(terminated).contains("caught=TERM") || terminated.wait() != 0) {{ return 8; }}
    terminated.close();

    hung_up := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "catch", "HUP"]);
    if (hung_up.read_bytes(5).to_string() != "ready") {{ return 9; }}
    hung_up.hangup();
    if (!drain(hung_up).contains("caught=HUP") || hung_up.wait() != 0) {{ return 10; }}
    hung_up.close();

    killed := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    killed.kill();
    if (killed.wait() != 137) {{ return 11; }}
    killed.close();

    resistant := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "resistant"]);
    if (resistant.read_bytes(5).to_string() != "ready") {{ return 12; }}
    resistant.terminate();
    if (!read_until(resistant, "resisted").contains("resisted") || !resistant.running()) {{ return 13; }}
    resistant.kill();
    if (resistant.wait() != 137) {{ return 14; }}
    resistant.close();

    print("STEP_FOREGROUND");
    foreground := pty_spawn("/bin/sh", []);
    foreground.write_bytes(bytes.from_string("{escaped(sys.executable)} {escaped(helper)} catch INT\\n"));
    string foreground_start := read_until(foreground, "foreground=True");
    foreground.interrupt();
    string foreground_result := read_until(foreground, "caught=INT");
    foreground.write_bytes(bytes.from_string("exit 0\\n"));
    drain(foreground);
    if (!foreground_start.contains("leader=False") || !foreground_start.contains("foreground=True") || !foreground_result.contains("caught=INT") || foreground.wait() != 0) {{ return 15; }}
    foreground.close();

    orphan_foreground := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "orphan-foreground"]);
    string orphan_start := read_until(orphan_foreground, "leader-gone=True");
    orphan_foreground.interrupt();
    string orphan_result := read_until(orphan_foreground, "orphan-caught");
    if (!orphan_start.contains("orphan-member=") || !orphan_result.contains("orphan-caught") || orphan_result.contains("wrong-parent")) {{ return 23; }}
    orphan_foreground.close();

    print("STEP_WAIT_CANCEL");
    cancellation_source wait_source;
    cancelled_wait := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], wait_source.token());
    channel<bool> wait_result;
    wait_worker := thread(() => {{
        try {{ cancelled_wait.wait(); wait_result.send(false); }}
        catch (PtyError caught) {{ wait_result.send(caught.code == 125 && caught.message == "PTY wait cancelled"); }}
    }});
    sleep_ms(50);
    wait_source.cancel();
    if (!(wait_result.receive() ?? false)) {{ return 16; }}
    wait_worker.join();
    cancelled_wait.close();

    print("STEP_SIMULTANEOUS");
    simultaneous := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "raw-copy"]);
    if (simultaneous.read_bytes(5).to_string() != "ready") {{ return 17; }}
    bytes simultaneous_payload := bytes(65536);
    channel<bool> simultaneous_done;
    simultaneous_writer := thread(() => {{ try {{ simultaneous.write_bytes(simultaneous_payload); simultaneous_done.send(true); }} catch (PtyError caught) {{ simultaneous_done.send(false); }} }});
    int_64 simultaneous_read := 0;
    while (simultaneous_read < 65536) {{ simultaneous_read = simultaneous_read + simultaneous.read_bytes(65536).length(); }}
    if (!(simultaneous_done.receive() ?? false)) {{ return 18; }}
    simultaneous_writer.join();
    if (simultaneous.wait() != 0) {{ return 19; }}
    simultaneous.close();

    print("STEP_RACES");
    resize_race := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "resistant"]);
    if (resize_race.read_bytes(5).to_string() != "ready") {{ return 20; }}
    resize_worker := thread(() => {{ for (index := 0; index < 500; index++) {{ try {{ resize_race.resize(24 + index % 50, 80 + index % 100); }} catch (PtyError caught) {{ return; }} }} }});
    signal_worker := thread(() => {{ for (index := 0; index < 100; index++) {{ try {{ resize_race.interrupt(); }} catch (PtyError caught) {{ return; }} }} }});
    sleep_ms(10);
    resize_race.close();
    resize_worker.join();
    signal_worker.join();

    exited_race := pty_spawn("/bin/sh", ["-c", "exit 0"]);
    sleep_ms(20);
    exited_race.interrupt(); exited_race.terminate(); exited_race.kill(); exited_race.hangup();
    if (exited_race.wait() != 0) {{ return 20; }}
    exited_race.close();

    for (cancel_cycle := 0; cancel_cycle < 25; cancel_cycle++) {{
        cancellation_source churn_source;
        churn := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], churn_source.token());
        channel<bool> churn_result;
        churn_worker := thread(() => {{ try {{ churn.wait(); churn_result.send(false); }} catch (PtyError caught) {{ churn_result.send(caught.code == 125); }} }});
        churn_source.cancel();
        if (!(churn_result.receive() ?? false)) {{ return 22; }}
        churn_worker.join();
        churn.close();
    }}

    print("STEP_CLOSE_CANCEL"); out.flush();
    for (close_cancel_cycle := 0; close_cancel_cycle < 20; close_cancel_cycle++) {{
        cancellation_source close_cancel_source;
        close_cancel := pty_spawn("/bin/sh", ["-c", "trap '' HUP; trap 'exit 0' TERM; printf ready; while :; do sleep 1; done"], close_cancel_source.token());
        if (close_cancel.read_bytes(5).to_string() != "ready") {{ return 24; }}
        channel<int> close_cancel_status;
        channel<bool> close_cancel_done;
        channel<bool> close_cancelled;
        channel<bool> close_before_cancel;
        channel<bool> close_probe_started;
        channel<int> close_probe_result;
        close_cancel_waiter := thread(() => {{ try {{ close_cancel_status.send(close_cancel.wait()); }} catch (PtyError caught) {{ close_cancel_status.send(-caught.code); }} }});
        close_probe := thread(() => {{
            close_probe_started.send(true);
            try {{ while (true) {{ close_cancel.read_bytes(1); }} }}
            catch (PtyError caught) {{
                int probe_code := -1;
                if (caught.code == 125) {{ probe_code = 125; }}
                else if (caught.message == "PTY is closed") {{ probe_code = 1; }}
                close_probe_result.send(probe_code);
            }}
        }});
        close_probe_started.receive();
        sleep_ms(2);
        close_cancel_closer := thread(() => {{ close_cancel.close(); if (close_cancel_cycle == 0) {{ close_before_cancel.send(true); }} close_cancel_done.send(true); }});
        close_cancel_canceller := thread(() => {{ if (close_cancel_cycle == 0) {{ close_before_cancel.receive(); }} close_cancel_source.cancel(); close_cancelled.send(true); }});
        int close_probe_code := close_probe_result.receive() ?? -1;
        int close_cancel_code := close_cancel_status.receive() ?? -1;
        bool close_finished := close_cancel_done.receive() ?? false;
        bool cancel_finished := close_cancelled.receive() ?? false;
        close_cancel_waiter.join();
        close_cancel_closer.join();
        close_cancel_canceller.join();
        close_probe.join();
        int close_cancel_cached := close_cancel.wait();
        bool close_cancel_running := close_cancel.running();
        int close_cancel_exit := close_cancel.exit_code();
        if (!close_finished || !cancel_finished || (close_probe_code != 1 && close_probe_code != 125) || (close_cancel_code != -125 && close_cancel_code != close_cancel_cached) || close_cancel_cached < 0 || close_cancel_running || close_cancel_exit != close_cancel_cached) {{ return 27; }}
    }}

    print("STEP_STRESS");
    for (cycle := 0; cycle < 300; cycle++) {{
        quick := pty_spawn("/bin/sh", ["-c", "exit 0"]);
        escaped_copy := quick;
        if (cycle % 2 == 0) {{ if (quick.wait() != 0) {{ return 21; }} }}
        else {{ quick.close(); }}
        escaped_copy.close();
    }}
    abandon("{escaped(helper)}");

    print("RESOURCE_AFTER");
    out.flush();
    sleep_ms(250);
    print("PTY lifecycle certification passed");
    out.flush();
    return 0;
}}
'''
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable, *compile_flags], cwd=root, check=True)
        certify_resources(executable, root)
        descendant_pid_file = root / "descendant.pid"
        owner_source = root / "pty-owner-shutdown.p"
        owner_executable = root / "pty-owner-shutdown"
        owner_source.write_text(
            f'''function main() -> int : (PtyError, TimeError) {{ terminal := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "descendant", "{escaped(descendant_pid_file)}"]); escaped_terminal := terminal; sleep_ms(100); return 0; }}\n''',
            encoding="utf-8",
        )
        subprocess.run([compiler, owner_source, "-o", owner_executable, *compile_flags], cwd=root, check=True)
        result = subprocess.run(owner_executable, cwd=root, text=True, capture_output=True, timeout=10, check=False)
        if result.returncode != 0 or result.stdout or result.stderr or not descendant_pid_file.exists():
            raise RuntimeError(f"PTY owner shutdown failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")
        descendant_pid = int(descendant_pid_file.read_text(encoding="utf-8"))
        require_pid_gone(descendant_pid, "same-group descendant")
    print("P8 PTY lifecycle certification: resize/SIGWINCH, fixed signals, live and leaderless foreground groups, resistant termination, wait cancellation/churn, close-over-cancel races, simultaneous I/O, close/resize/signal races, exact descendant cleanup, escaped copies, owner shutdown, and 300 spawn/exit cycles passed")


if __name__ == "__main__":
    main()
