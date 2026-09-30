#!/usr/bin/env python3
"""Certify the P7 binary PTY API and owned-child cleanup."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    if sys.platform == "win32":
        with tempfile.TemporaryDirectory(prefix="strut-pty-stub-") as temporary:
            root = Path(temporary)
            program = root / "pty-stub.p"
            executable = root / "pty-stub.exe"
            program.write_text(
                'function main() -> int { cancellation_source source; bool first := false; bool second := false; bool third := false; bool fourth := false; '
                'try { terminal := pty_spawn("unused", []); terminal.close(); } catch (PtyError caught) { first = caught.message == "PTY unsupported on Windows until P9 ConPTY"; } '
                'try { terminal := pty_spawn("unused", [], {}); terminal.close(); } catch (PtyError caught) { second = caught.message == "PTY unsupported on Windows until P9 ConPTY"; } '
                'try { terminal := pty_spawn("unused", [], source.token()); terminal.close(); } catch (PtyError caught) { third = caught.message == "PTY unsupported on Windows until P9 ConPTY"; } '
                'try { terminal := pty_spawn("unused", [], {}, source.token()); terminal.close(); } catch (PtyError caught) { fourth = caught.message == "PTY unsupported on Windows until P9 ConPTY"; } '
                'if (!first || !second || !third || !fourth) { return 1; } return 0; }\n',
                encoding="utf-8",
            )
            subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
            subprocess.run([executable], cwd=root, check=True)
        print("P7 PTY Windows stub certification: all spawn overloads returned the documented unsupported error; /W4 /WX compilation passed")
        return

    with tempfile.TemporaryDirectory(prefix="strut-pty-") as temporary:
        root = Path(temporary)
        work = root / "pty-cwd"
        work.mkdir()
        invalid_executable = root / "invalid-executable"
        invalid_executable.write_text("not an executable image\n", encoding="utf-8")
        invalid_executable.chmod(0o755)
        helper = root / "pty-helper.py"
        helper.write_text(
            """import fcntl, os, pathlib, struct, sys, termios, time, tty
mode = sys.argv[1]
if mode == 'inspect':
    rows, columns, _, _ = struct.unpack('HHHH', fcntl.ioctl(0, termios.TIOCGWINSZ, b'\\0' * 8))
    pid = os.getpid()
    session = os.getsid(0)
    process_group = os.getpgrp()
    terminal_session = os.tcgetsid(0) if hasattr(os, 'tcgetsid') else session
    foreground_group = os.tcgetpgrp(0)
    values = [
        'argv=' + repr(sys.argv[2:]),
        'env=' + os.environ.get('STRUT_PTY_ENV', ''),
        'cwd=' + pathlib.Path.cwd().name,
        'tty=' + str(os.isatty(0) and os.isatty(1) and os.isatty(2)),
        'session_leader=' + str(session == pid and process_group == pid),
        'controlling_terminal=' + str(terminal_session == session),
        'foreground_group=' + str(foreground_group == process_group),
        'size=' + str(rows) + 'x' + str(columns),
    ]
    os.write(1, ('|'.join(values) + '|stdout').encode())
    os.write(2, b'|stderr')
elif mode == 'raw':
    tty.setraw(0)
    os.write(1, b'ready')
    expected = 5
    data = b''
    while len(data) < expected:
        data += os.read(0, expected - len(data))
    os.write(1, data)
elif mode == 'sleep':
    time.sleep(30)
elif mode == 'sleep_raw':
    tty.setraw(0)
    os.write(1, b'ready')
    time.sleep(30)
elif mode == 'exit':
    os.write(1, b'before-exit')
    raise SystemExit(7)
""",
            encoding="utf-8",
        )

        source = f'''function drain(pty terminal) -> string : PtyError {{
    string output := "";
    while (!terminal.eof()) {{
        bytes chunk := terminal.read_bytes(4096);
        if (!chunk.empty()) {{ output = output + chunk.to_string(); }}
    }}
    return output;
}}

function main() -> int : (PtyError, ThreadError, TimeError) {{
    shell := pty_spawn("/bin/sh", ["-c", "printf shell-ok"]);
    if (drain(shell) != "shell-ok" || shell.wait() != 0) {{ return 1; }}

    configured := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "inspect", "", "two words", "π"], {{"cwd": "{escaped(work)}", "env": {{"STRUT_PTY_ENV": "value-π"}}, "rows": 37, "columns": 111}});
    string details := drain(configured);
    if (configured.wait() != 0 || !details.contains("argv=['', 'two words', 'π']") || !details.contains("env=value-π") || !details.contains("cwd=pty-cwd") || !details.contains("tty=True") || !details.contains("session_leader=True") || !details.contains("controlling_terminal=True") || !details.contains("foreground_group=True") || !details.contains("size=37x111") || !details.contains("stdout") || !details.contains("stderr")) {{ return 2; }}

    raw := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "raw"]);
    if (raw.read_bytes(5).to_string() != "ready") {{ return 3; }}
    bytes binary := [0, 10, 13, 128, 255];
    raw.write_bytes(binary);
    bytes returned := raw.read_bytes(5);
    if (returned.length() != 5 || returned[0] != 0 || returned[1] != 10 || returned[2] != 13 || returned[3] != 128 || returned[4] != 255) {{ return 4; }}
    if (drain(raw) != "" || raw.wait() != 0 || !raw.eof()) {{ return 5; }}

    exited := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "exit"]);
    if (drain(exited) != "before-exit" || exited.wait() != 7 || exited.running() || exited.exit_code() != 7) {{ return 6; }}

    interactive := pty_spawn("/bin/sh", []);
    interactive.write_bytes(bytes.from_string("printf interactive-fixture; exit 0\\n"));
    string transcript := drain(interactive);
    if (interactive.wait() != 0 || !transcript.contains("interactive-fixture") || !transcript.contains("printf interactive-fixture")) {{ return 7; }}

    path_shell := pty_spawn("sh", ["-c", "printf path-ok"]);
    if (drain(path_shell) != "path-ok" || path_shell.wait() != 0) {{ return 8; }}

    bool missing_rejected := false;
    try {{ missing := pty_spawn("__strut_missing_pty_executable__", []); missing.close(); }}
    catch (PtyError caught) {{ missing_rejected = true; }}
    if (!missing_rejected) {{ return 9; }}
    bool invalid_rejected := false;
    try {{ invalid := pty_spawn("{escaped(invalid_executable)}", []); invalid.close(); }}
    catch (PtyError caught) {{ invalid_rejected = true; }}
    if (!invalid_rejected) {{ return 9; }}
    bool cwd_rejected := false;
    try {{ bad_cwd := pty_spawn("/bin/sh", ["-c", "exit 0"], {{"cwd": "{escaped(root / 'missing-cwd')}"}}); bad_cwd.close(); }}
    catch (PtyError caught) {{ cwd_rejected = true; }}
    if (!cwd_rejected) {{ return 10; }}

    cancellation_source read_source;
    blocked_read := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], read_source.token());
    channel<bool> read_result;
    read_worker := thread(() => {{
        try {{ blocked_read.read_bytes(1); read_result.send(false); }}
        catch (PtyError caught) {{ read_result.send(caught.code == 125 && caught.message == "PTY I/O cancelled"); }}
    }});
    sleep_ms(50);
    read_source.cancel();
    if (!(read_result.receive() ?? false)) {{ return 11; }}
    read_worker.join();
    blocked_read.close();

    cancellation_source write_source;
    blocked_write := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep_raw"], write_source.token());
    if (blocked_write.read_bytes(5).to_string() != "ready") {{ return 12; }}
    bytes payload := bytes(16777216);
    channel<bool> write_result;
    write_worker := thread(() => {{
        try {{ blocked_write.write_bytes(payload); write_result.send(false); }}
        catch (PtyError caught) {{ write_result.send(caught.code == 125 && caught.message == "PTY I/O cancelled"); }}
    }});
    sleep_ms(50);
    write_source.cancel();
    if (!(write_result.receive() ?? false)) {{ return 12; }}
    write_worker.join();
    blocked_write.close();

    close_blocked := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    channel<bool> close_result;
    close_worker := thread(() => {{
        try {{ close_blocked.read_bytes(1); close_result.send(false); }}
        catch (PtyError caught) {{ close_result.send(caught.message == "PTY is closed"); }}
    }});
    sleep_ms(50);
    close_blocked.close();
    if (!(close_result.receive() ?? false)) {{ return 13; }}
    close_worker.join();
    close_blocked.close();

    duplicate := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    channel<bool> duplicate_started;
    duplicate_worker := thread(() => {{
        duplicate_started.send(true);
        try {{ duplicate.read_bytes(1); }} catch (PtyError caught) {{ }}
    }});
    duplicate_started.receive();
    sleep_ms(25);
    bool duplicate_rejected := false;
    try {{ duplicate.read_bytes(1); }}
    catch (PtyError caught) {{ duplicate_rejected = true; }}
    if (!duplicate_rejected) {{ return 14; }}
    duplicate.close();
    duplicate_worker.join();

    cancellation_source duplicate_write_source;
    duplicate_write := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], duplicate_write_source.token());
    channel<bool> duplicate_write_started;
    duplicate_write_worker := thread(() => {{
        duplicate_write_started.send(true);
        try {{ duplicate_write.write_bytes(payload); }} catch (PtyError caught) {{ }}
    }});
    duplicate_write_started.receive();
    sleep_ms(25);
    bool duplicate_writer_rejected := false;
    try {{ duplicate_write.write_bytes([1]); }}
    catch (PtyError caught) {{ duplicate_writer_rejected = true; }}
    if (!duplicate_writer_rejected) {{ return 15; }}
    duplicate_write_source.cancel();
    duplicate_write_worker.join();
    duplicate_write.close();

    active_close := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    channel<bool> active_read_done;
    channel<bool> active_write_done;
    active_reader := thread(() => {{
        try {{ active_close.read_bytes(1); active_read_done.send(true); }}
        catch (PtyError caught) {{ active_read_done.send(true); }}
    }});
    active_writer := thread(() => {{
        try {{ active_close.write_bytes(payload); active_write_done.send(true); }}
        catch (PtyError caught) {{ active_write_done.send(true); }}
    }});
    sleep_ms(50);
    active_close.close();
    if (!(active_read_done.receive() ?? false) || !(active_write_done.receive() ?? false)) {{ return 16; }}
    active_reader.join();
    active_writer.join();

    wait_close := pty_spawn("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"]);
    channel<int> wait_close_result;
    wait_close_worker := thread(() => {{
        try {{ wait_close_result.send(wait_close.wait()); }}
        catch (PtyError caught) {{ wait_close_result.send(-1); }}
    }});
    wait_close_worker_two := thread(() => {{
        try {{ wait_close_result.send(wait_close.wait()); }}
        catch (PtyError caught) {{ wait_close_result.send(-1); }}
    }});
    sleep_ms(50);
    int_64 wait_close_start := now_ms();
    wait_close.close();
    int wait_close_status := wait_close_result.receive() ?? -1;
    int wait_close_status_two := wait_close_result.receive() ?? -1;
    wait_close_worker.join();
    wait_close_worker_two.join();
    if (now_ms() - wait_close_start > 1000 || wait_close_status < 128 || wait_close_status_two != wait_close_status || wait_close.wait() != wait_close_status || wait_close.running() || wait_close.exit_code() != wait_close_status) {{ return 17; }}

    cancellation_source resident_source;
    resident := thread(() => {{ resident_source.token().wait(); }});
    for (threaded_cycle := 0; threaded_cycle < 10; threaded_cycle++) {{
        threaded_spawn := pty_spawn("/bin/sh", ["-c", "printf threaded"]);
        if (drain(threaded_spawn) != "threaded" || threaded_spawn.wait() != 0) {{ return 18; }}
    }}
    resident_source.cancel();
    resident.join();

    for (cycle := 0; cycle < 25; cycle++) {{
        quick := pty_spawn("/bin/sh", ["-c", "exit 0"]);
        copy := quick;
        quick.close();
        copy.close();
    }}

    print("PTY certification passed");
    return 0;
}}
'''
        program = root / "pty-certification.p"
        executable = root / "pty-certification"
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
        result = subprocess.run(executable, cwd=root, text=True, capture_output=True, timeout=20, check=False)
        if result.returncode != 0 or result.stdout != "PTY certification passed\n" or result.stderr:
            raise RuntimeError(f"exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")
    print("P7 PTY certification: spawn-session and controlling-terminal identity, direct shell and ordinary executables, PATH/argv/env/cwd, TTY geometry, merged streams, raw binary I/O, interactive echo, exit/EOF, direct spawn failure, blocked read/write cancellation, active close and wait-vs-close races, duplicate readers/writers, spawning with resident threads, shared ownership, and 25 cleanup cycles passed")


if __name__ == "__main__":
    main()
