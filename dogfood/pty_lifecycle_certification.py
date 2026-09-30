#!/usr/bin/env python3
"""Certify the P8 PTY control surface, lifecycle races, and resources."""

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
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline and not seen_done:
        try:
            line = lines.get(timeout=0.01)
            observed.append(line)
            if line == "RESOURCE_BASELINE" and sys.platform.startswith("linux"):
                baseline = linux_sample(process.pid)
            elif line == "RESOURCE_AFTER" and sys.platform.startswith("linux"):
                time.sleep(0.05)
                after = linux_sample(process.pid)
                after_fds = linux_fd_targets(process.pid)
            elif line == "PTY lifecycle certification passed":
                seen_done = True
        except queue.Empty:
            pass
        if sys.platform.startswith("linux") and process.poll() is None:
            sample = linux_sample(process.pid)
            if sample is not None:
                peak = maxima(peak, sample)
    try:
        returncode = process.wait(timeout=max(1, deadline - time.monotonic()))
    except subprocess.TimeoutExpired:
        process.kill()
        reader.join()
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
    else:
        print("P8 PTY resources: FD/child/zombie/RSS/thread counters unsupported on this platform; functional fixture executed")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    compile_flags = ["--release"] if os.environ.get("STRUT_PTY_RELEASE") == "1" else []
    with tempfile.TemporaryDirectory(prefix="strut-pty-p8-") as temporary:
        root = Path(temporary)
        program = root / "pty-lifecycle.p"
        executable = root / ("pty-lifecycle.exe" if sys.platform == "win32" else "pty-lifecycle")
        if sys.platform == "win32":
            program.write_text(
                'function inspect(pty terminal) -> void : PtyError { terminal.resize(24, 80); terminal.interrupt(); terminal.terminate(); terminal.kill(); terminal.hangup(); terminal.wait(); return; } function main() -> int { return 0; }\n',
                encoding="utf-8",
            )
            subprocess.run([compiler, program, "-o", executable, *compile_flags], cwd=root, check=True)
            print("P8 PTY Windows compile-only API/stub certification passed; ConPTY remains deferred to P9")
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
    while (!output.contains(expected)) {{ output = output + terminal.read_bytes(4096).to_string(); }}
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
    try {{ invalid_resize.resize(24, 65536); }} catch (PtyError caught) {{ bad_resize = true; }}
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
