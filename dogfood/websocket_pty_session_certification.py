#!/usr/bin/env python3
"""Certify bounded public-API composition of WebSocket and PTY sessions."""

import ctypes
import os
from pathlib import Path
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time

from http_websocket_certification import available_port, compile_program, opening
from pty_lifecycle_certification import linux_sample, windows_sample, windows_type_deltas
from websocket_runtime_certification import client_frame, receive_frame, receive_head


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def connect(port, path, context=None, initial=b""):
    raw = socket.create_connection(("127.0.0.1", port), timeout=4)
    connection = context.wrap_socket(raw, server_hostname="localhost") if context else raw
    connection.settimeout(4)
    connection.sendall(opening(path=path, extra=b"X-Session-Policy: allow\r\n") + initial)
    receive_head(connection)
    return connection


def send_text(connection, value):
    connection.sendall(client_frame(1, value.encode("utf-8")))


def send_binary(connection, value):
    connection.sendall(client_frame(2, value))


def close_reply(connection, payload=struct.pack("!H", 1000)):
    connection.sendall(client_frame(8, payload))


def receive_until(connection, expected, output=None):
    output = bytearray() if output is None else output
    while True:
        fin, opcode, payload = receive_frame(connection)
        if not fin:
            raise RuntimeError("P10 server fragmented an application message")
        if opcode == 2:
            if len(payload) > 4096:
                raise RuntimeError(f"P10 PTY output chunk exceeded 4096 bytes: {len(payload)}")
            output.extend(payload)
        elif opcode == 1:
            text = payload.decode("utf-8")
            if text == expected:
                return output
        elif opcode == 8:
            close_reply(connection, payload)
            raise RuntimeError(f"P10 session closed before {expected!r}")
        else:
            raise RuntimeError(f"unexpected P10 server opcode {opcode}")


def receive_output(connection, expected, output=None):
    output = bytearray() if output is None else output
    while expected not in output:
        fin, opcode, payload = receive_frame(connection)
        if not fin or opcode != 2:
            raise RuntimeError(f"unexpected frame while awaiting PTY output: {(fin, opcode, payload)!r}")
        if len(payload) > 4096:
            raise RuntimeError(f"P10 PTY output chunk exceeded 4096 bytes: {len(payload)}")
        output.extend(payload)
    return output


def finish_server_close(connection):
    while True:
        fin, opcode, payload = receive_frame(connection)
        if not fin:
            raise RuntimeError("fragmented P10 close sequence")
        if opcode == 8:
            close_reply(connection, payload)
            return


def complete_session(port, mode, expected_status, context=None):
    with connect(port, f"/session/{mode}", context) as connection:
        receive_until(connection, "ready")
        output = receive_until(connection, expected_status)
        finish_server_close(connection)
        return bytes(output)


def interactive_session(port, marker, context=None, control="kill"):
    with connect(port, "/session/interactive", context) as connection:
        receive_until(connection, "ready")
        output = receive_output(connection, b"HELPER_READY")
        send_text(connection, "resize")
        receive_until(connection, "resized", output)
        wire = marker.encode("ascii") + b"\n"
        send_binary(connection, wire)
        receive_until(connection, "input", output)
        receive_output(connection, wire.strip(), output)
        send_text(connection, control)
        receive_until(connection, control, output)
        receive_until(connection, "exit", output)
        finish_server_close(connection)


def abrupt_session(port, context=None):
    connection = connect(port, "/session/interactive", context)
    receive_until(connection, "ready")
    connection.close()


def coalesced_session(port, context=None):
    with connect(port, "/session/interactive", context, client_frame(1, b"resize")) as connection:
        output = receive_until(connection, "ready")
        receive_until(connection, "resized", output)
        send_text(connection, "close")
        finish_server_close(connection)


def reject_unknown_control(port, context=None):
    with connect(port, "/session/interactive", context) as connection:
        receive_until(connection, "ready")
        send_text(connection, "unknown")
        while True:
            _, opcode, payload = receive_frame(connection)
            if opcode == 8:
                break
        if opcode != 8 or payload[:2] != struct.pack("!H", 1003):
            raise RuntimeError(f"P10 unknown control was not rejected with close 1003: {(opcode, payload)!r}")
        close_reply(connection, payload)


def deny_before_upgrade(port, context=None):
    raw = socket.create_connection(("127.0.0.1", port), timeout=4)
    connection = context.wrap_socket(raw, server_hostname="localhost") if context else raw
    connection.settimeout(4)
    with connection:
        connection.sendall(opening(path="/denied/exit"))
        response = b""
        while b"\r\n\r\n" not in response:
            chunk = connection.recv(4096)
            if not chunk:
                break
            response += chunk
    if not response.startswith(b"HTTP/1.1 403 ") or b"101 Switching Protocols" in response:
        raise RuntimeError(f"P10 policy rejection committed an upgrade: {response!r}")


def sample(process):
    if sys.platform.startswith("linux"):
        return linux_sample(process.pid)
    if sys.platform == "win32":
        return windows_sample(process.pid, include_handle_types=True)
    return None


def require_return(baseline, after, cycles):
    if baseline is None or after is None:
        print("P10 resources: native counters unavailable; functional stress executed")
        return
    if sys.platform.startswith("linux"):
        if after["fd"] > baseline["fd"]:
            raise RuntimeError(f"P10 Linux fd count did not return: {baseline} -> {after}")
        for field in ("children", "zombies"):
            if after[field] != baseline[field]:
                raise RuntimeError(f"P10 Linux {field} did not return: {baseline} -> {after}")
        thread_allowance = 1 if "sanitize=thread" in os.environ.get("STRUT_CXXFLAGS", "") else 0
        if after["threads"] > baseline["threads"] + thread_allowance:
            raise RuntimeError(f"P10 Linux threads did not return: {baseline} -> {after}")
        rss_allowance = 262144 if "sanitize" in os.environ.get("STRUT_CXXFLAGS", "") else 16384
        if after["rss_kib"] > baseline["rss_kib"] + rss_allowance:
            raise RuntimeError(f"P10 Linux RSS exceeded allowance: {baseline} -> {after}")
        print(f"P10 Linux resources after {cycles} sessions: {baseline} -> {after}")
        return
    build = sys.getwindowsversion().build
    type_deltas = windows_type_deltas(baseline.get("handle_types", {}), after.get("handle_types", {}))
    thread_delta = after["threads"] - baseline["threads"]
    handle_delta = after["handles"] - baseline["handles"]
    if build >= 26100:
        if thread_delta != 0 or handle_delta != 0 or type_deltas:
            raise RuntimeError(f"P10 Windows resources did not return: {baseline} -> {after}, {type_deltas}")
    else:
        positive = {name: value for name, value in type_deltas.items() if value > 0}
        process_excess = positive.get("Process", 0)
        file_excess = positive.get("File", 0)
        if (set(positive) - {"Process", "File"} or process_excess > cycles + 30 or file_excess > 1
                or thread_delta not in (0, 1) or file_excess != thread_delta
                or max(0, handle_delta) != process_excess + file_excess):
            raise RuntimeError(f"P10 pre-24H2 resources exceeded allowance: {baseline} -> {after}, {type_deltas}")
    print(f"P10 Windows resources after {cycles} sessions: build={build} {baseline} -> {after} types={type_deltas}")


def require_concurrency_bound(before, after, count):
    if before is None or after is None or not sys.platform.startswith("linux"):
        return
    if after["fd"] > before["fd"] or after["children"] != before["children"] or after["zombies"] != before["zombies"]:
        raise RuntimeError(f"P10 concurrent native resources did not drain: {before} -> {after}")
    if after["threads"] > before["threads"] + count:
        raise RuntimeError(f"P10 concurrent worker bound exceeded: {before} -> {after}, count={count}")
    allowance = 262144 if "sanitize" in os.environ.get("STRUT_CXXFLAGS", "") else 16384
    if after["rss_kib"] > before["rss_kib"] + allowance:
        raise RuntimeError(f"P10 concurrent RSS exceeded allowance: {before} -> {after}")


def stop_server(port, process, context=None, timeout=12):
    try:
        with connect(port, "/shutdown", context):
            pass
        stdout, stderr = process.communicate(timeout=timeout)
    except Exception:
        if process.poll() is None:
            process.kill()
            process.wait()
        raise
    if process.returncode != 0 or stderr:
        raise RuntimeError(f"P10 server failed ({process.returncode}) stdout={stdout!r} stderr={stderr!r}")


def wait_ready(port, process, context=None):
    deadline = time.monotonic() + 10
    while True:
        try:
            deny_before_upgrade(port, context)
            return
        except OSError:
            if process.poll() is not None:
                stdout, stderr = process.communicate()
                raise RuntimeError(f"P10 server exited early ({process.returncode}) {stdout!r} {stderr!r}")
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.03)


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    if sys.platform == "win32":
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        if not all(hasattr(kernel32, name) for name in
                   ("CreatePseudoConsole", "ResizePseudoConsole", "ClosePseudoConsole")):
            print("P10 WebSocket-PTY runtime skipped: ConPTY requires Windows 10 version 1809 or newer")
            return

    with tempfile.TemporaryDirectory(prefix="strut-websocket-pty-",
                                     ignore_cleanup_errors=sys.platform == "win32") as temporary:
        root = Path(temporary)
        suffix = ".exe" if sys.platform == "win32" else ""
        helper_source = f'''function main(string command, string[] args) -> int : (StreamError, TimeError) {{
    string mode := args[0];
    if (mode == "exit") {{ out.write("final-output"); out.flush(); return 7; }}
    if (mode == "flood") {{
        bytes block := bytes.from_string("{"x" * 4096}");
        for (index := 0; index < 8192; index++) {{ out.write_bytes(block); out.flush(); }}
        return 7;
    }}
    out.write("HELPER_READY\\n");
    out.flush();
    if (mode == "no-read") {{ sleep_ms(30000); return 0; }}
    while (!in.eof()) {{
        bytes chunk := in.read_bytes(4096);
        if (!chunk.empty()) {{ out.write_bytes(chunk); out.flush(); }}
    }}
    return 0;
}}
'''
        helper = compile_program(compiler, root, "websocket-pty-helper", helper_source)
        plain_port = available_port()
        tls_port = available_port()
        if sys.platform == "win32":
            shell_program = os.environ.get("COMSPEC", r"C:\Windows\System32\cmd.exe")
            shell_args = '["/D", "/Q", "/C", "echo shell-output"]'
            blocked_spawn = f'terminal = pty_spawn("{escaped(helper)}", ["no-read"], request.cancellation);'
        else:
            shell_program = "/bin/sh"
            shell_args = '["-c", "printf shell-output"]'
            blocked_spawn = 'terminal = pty_spawn("/bin/sh", ["-c", "stty raw -echo; printf HELPER_READY; sleep 30"], request.cancellation);'
        server_source = f'''function session(http_request request, websocket socket) -> void : (NetworkError, WebSocketError, PtyError, ThreadError) {{
    socket.accept();
    pty terminal;
    string mode := request.params["mode"];
    if (mode == "shell") {{
        terminal = pty_spawn("{escaped(shell_program)}", {shell_args}, request.cancellation);
    }} else if (mode == "blocked") {{
        {blocked_spawn}
    }} else {{
        terminal = pty_spawn("{escaped(helper)}", [mode], {{"rows": 24, "columns": 80, "env": {{"TERM": "xterm-256color"}}}}, request.cancellation);
    }}
    socket.write_text("ready");
    output := thread(() => {{
        try {{
            while (!terminal.eof()) {{
                bytes chunk := terminal.read_bytes(4096);
                if (!chunk.empty()) {{ socket.write_bytes(chunk); }}
            }}
            int status := terminal.wait();
            if (status == 0) {{ socket.write_text("exit:0"); }}
            else if (status == 7) {{ socket.write_text("exit:7"); }}
            else {{ socket.write_text("exit"); }}
            socket.close();
        }} catch (PtyError caught) {{
        }} catch (NetworkError caught) {{
        }} catch (WebSocketError caught) {{
        }}
    }});
    bool finished := false;
    try {{
        while (!finished) {{
            message := socket.read();
            if (message == null) {{
                finished = true;
            }} else if ((message?.kind ?? "") == "binary") {{
                socket.write_text("input-started");
                terminal.write_bytes(message?.data ?? bytes());
                socket.write_text("input");
            }} else {{
                string control := message?.text ?? "";
                if (control == "resize") {{ terminal.resize(51, 133); socket.write_text("resized"); }}
                else if (control == "interrupt") {{ terminal.interrupt(); socket.write_text("interrupt"); }}
                else if (control == "terminate") {{ terminal.terminate(); socket.write_text("terminate"); }}
                else if (control == "kill") {{ terminal.kill(); socket.write_text("kill"); }}
                else if (control == "hangup") {{ terminal.hangup(); socket.write_text("hangup"); }}
                else if (control == "close") {{ finished = true; }}
                else {{ socket.close(1003, "unknown control"); finished = true; }}
            }}
        }}
    }} catch (PtyError caught) {{
    }} catch (NetworkError caught) {{
    }} catch (WebSocketError caught) {{
    }}
    terminal.close();
    output.join();
    return;
}}

function main(string command, string[] args) -> int : (NetworkError, WebSocketError, PtyError, ThreadError, TlsError) {{
    channel<bool> shutdown;
    app := http_server();
    app.timeouts(5000, 5000, 5000, 3000);
    app.limits(1048576, 65536, 100, 32);
    app.websocket_limits(16777216, 16777216);
    app.websocket("/denied/:mode", (http_request request, websocket socket) => {{ token := request.cancellation; copy := socket; return; }});
    app.websocket("/session/:mode", (http_request request, websocket socket) => {{ session(request, socket); return; }});
    app.websocket("/shutdown", (http_request request, websocket socket) => {{ socket.accept(); shutdown.send(true); return; }});
    listener := thread(() => {{
        if (args[0] == "tls") {{ app.listen_tls("127.0.0.1", {tls_port}, args[1], args[2]); }}
        else {{ app.listen("127.0.0.1", {plain_port}); }}
    }});
    shutdown.receive();
    app.stop();
    listener.join();
    return 0;
}}
'''
        server_executable = compile_program(compiler, root, "websocket-pty-server", server_source)
        sanitized = "-fsanitize=" in os.environ.get("STRUT_CXXFLAGS", "")
        sequential = 50 if sanitized else 1000
        concurrent_count = 8 if sanitized else 32

        plain = subprocess.Popen([server_executable, "plain"], cwd=root, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True)
        try:
            wait_ready(plain_port, plain)
            if complete_session(plain_port, "exit", "exit:7") != b"final-output":
                raise RuntimeError("P10 natural exit lost final PTY output")
            if b"shell-output" not in complete_session(plain_port, "shell", "exit:0"):
                raise RuntimeError("P10 shell executable output was not delivered")
            interactive_session(plain_port, "plain-marker")
            interactive_session(plain_port, "terminate-marker", control="terminate")
            coalesced_session(plain_port)
            reject_unknown_control(plain_port)
            abrupt_session(plain_port)
            with connect(plain_port, "/session/flood") as connection:
                receive_until(connection, "ready")
                before_pause = sample(plain)
                time.sleep(0.2)
                paused = sample(plain)
                if (not sanitized and before_pause and paused and "rss_kib" in before_pause
                        and paused["rss_kib"] > before_pause["rss_kib"] + 16384):
                    raise RuntimeError(f"P10 slow peer caused output-sized buffering: {before_pause} -> {paused}")
                flood = receive_until(connection, "exit:7")
                finish_server_close(connection)
                if len(flood) != 4096 * 8192 or flood != b"x" * len(flood):
                    raise RuntimeError(f"P10 bounded flood output mismatch: {len(flood)}")
            complete_session(plain_port, "exit", "exit:7")
            baseline = sample(plain)
            for _ in range(sequential):
                complete_session(plain_port, "exit", "exit:7")
            time.sleep(0.3)
            require_return(baseline, sample(plain), sequential)
            failures = []
            before_concurrent = sample(plain)

            def concurrent(index):
                try:
                    interactive_session(plain_port, f"concurrent-{index}")
                except Exception as error:
                    failures.append(error)

            workers = [threading.Thread(target=concurrent, args=(index,)) for index in range(concurrent_count)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join()
            if failures:
                raise failures[0]
            time.sleep(0.3)
            baseline = sample(plain)
            require_concurrency_bound(before_concurrent, baseline, concurrent_count)
            abrupt_cycles = 10 if sanitized else 50
            for _ in range(abrupt_cycles):
                abrupt_session(plain_port)
            time.sleep(0.5)
            baseline = sample(plain)
            for _ in range(abrupt_cycles):
                abrupt_session(plain_port)
            time.sleep(0.5)
            require_return(baseline, sample(plain), abrupt_cycles)
            blocked = connect(plain_port, "/session/blocked")
            receive_until(blocked, "ready")
            receive_output(blocked, b"HELPER_READY")
            send_binary(blocked, b"z" * 16777216)
            receive_until(blocked, "input-started")
            blocked.settimeout(0.2)
            try:
                receive_until(blocked, "input")
                raise RuntimeError("P10 no-read child accepted the full input without backpressure")
            except TimeoutError:
                pass
            blocked.settimeout(4)
            try:
                stop_server(plain_port, plain)
            finally:
                blocked.close()
        finally:
            if plain.poll() is None:
                plain.kill()
                plain.wait()

        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate = fixture / "localhost-cert.pem"
        private_key = fixture / "localhost-key.pem"
        context = ssl.create_default_context(cafile=str(certificate))
        tls = subprocess.Popen([server_executable, "tls", certificate, private_key], cwd=root,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_ready(tls_port, tls, context)
            interactive_session(tls_port, "tls-marker", context)
            coalesced_session(tls_port, context)
            reject_unknown_control(tls_port, context)
            if complete_session(tls_port, "exit", "exit:7", context) != b"final-output":
                raise RuntimeError("P10 TLS natural exit lost final PTY output")
            abrupt_session(tls_port, context)
            stop_server(tls_port, tls, context)
        finally:
            if tls.poll() is None:
                tls.kill()
                tls.wait()

    print(f"P10 WebSocket-PTY certification passed: public binary/control pumps, plaintext/TLS, coalesced upgrade input, policy-before-101, shell/executable, final drain, validated controls, slow peers, {sequential} sequential and {concurrent_count} bounded concurrent sessions, disconnect and cancellation-driven shutdown cleanup")


if __name__ == "__main__":
    main()
