#!/usr/bin/env python3
"""Certify the P5 RFC 6455 server frame and message runtime."""

from pathlib import Path
import os
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import time

from http_websocket_certification import KEY, ACCEPT, available_port, compile_program, opening


def receive_exact(connection, size):
    output = b""
    while len(output) < size:
        chunk = connection.recv(size - len(output))
        if not chunk:
            raise RuntimeError("WebSocket transport ended inside a frame")
        output += chunk
    return output


def receive_head(connection):
    response = b""
    while b"\r\n\r\n" not in response:
        response += receive_exact(connection, 1)
    expected = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {ACCEPT}\r\n\r\n"
    ).encode()
    if response != expected:
        raise RuntimeError(f"unexpected upgrade response: {response!r}")


def client_frame(opcode, payload=b"", fin=True, masked=True, rsv=0, length_code=None):
    first = (0x80 if fin else 0) | rsv | opcode
    mask = b"\x11\x22\x33\x44"
    length = len(payload)
    code = length if length <= 125 else 126 if length <= 65535 else 127
    if length_code is not None:
        code = length_code
    output = bytes([first, (0x80 if masked else 0) | code])
    if code == 126:
        output += struct.pack("!H", length)
    elif code == 127:
        output += struct.pack("!Q", length)
    if masked:
        output += mask
        payload = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
    return output + payload


def declared_frame(opcode, length, extended, fin=True):
    return bytes([(0x80 if fin else 0) | opcode, 0x80 | extended]) + (
        struct.pack("!H", length) if extended == 126 else struct.pack("!Q", length)
    )


def receive_frame(connection):
    first, second = receive_exact(connection, 2)
    if second & 0x80:
        raise RuntimeError("server frame was masked")
    length = second & 0x7F
    if length == 126:
        length = struct.unpack("!H", receive_exact(connection, 2))[0]
        if length < 126:
            raise RuntimeError("server used a non-minimal 16-bit length")
    elif length == 127:
        encoded = receive_exact(connection, 8)
        if encoded[0] & 0x80:
            raise RuntimeError("server used an invalid 64-bit length")
        length = struct.unpack("!Q", encoded)[0]
        if length <= 65535:
            raise RuntimeError("server used a non-minimal 64-bit length")
    return bool(first & 0x80), first & 0x0F, receive_exact(connection, length)


def connect(port, context=None, initial=b"", path="/text"):
    raw = socket.create_connection(("127.0.0.1", port), timeout=3)
    connection = context.wrap_socket(raw, server_hostname="localhost") if context else raw
    connection.settimeout(3)
    connection.sendall(opening(path=path) + initial)
    receive_head(connection)
    return connection


def close_code(connection):
    fin, opcode, payload = receive_frame(connection)
    if not fin or opcode != 8 or len(payload) < 2:
        raise RuntimeError(f"expected close frame, received {(fin, opcode, payload)!r}")
    return struct.unpack("!H", payload[:2])[0]


def require_protocol_close(port, wire, expected):
    with connect(port) as connection:
        connection.sendall(wire)
        actual = close_code(connection)
        if actual != expected:
            raise RuntimeError(f"expected close {expected}, received {actual}")


def exercise_interop(port, context=None):
    with connect(port, context, client_frame(1, b"buffered")) as connection:
        if receive_frame(connection) != (True, 1, b"text"):
            raise RuntimeError("HTTP carry bytes were not consumed as a text frame")
        connection.sendall(client_frame(8, struct.pack("!H", 1000) + b"done"))
        if receive_frame(connection) != (True, 8, struct.pack("!H", 1000) + b"done"):
            raise RuntimeError("peer close was not echoed exactly")

    with connect(port, context) as connection:
        euro = "a€z".encode()
        fragments = [client_frame(1, euro[:2], fin=False), client_frame(9, b"p"),
                     client_frame(0, euro[2:3], fin=False), client_frame(0, euro[3:])]
        for fragment in fragments:
            for byte in fragment:
                connection.sendall(bytes([byte]))
        if receive_frame(connection) != (True, 10, b"p"):
            raise RuntimeError("interleaved ping was not answered")
        received = receive_frame(connection)
        if received != (True, 1, b"text"):
            raise RuntimeError(f"fragmented UTF-8 text was not reassembled: {received!r}")

    with connect(port, context, path="/binary") as connection:
        payload = bytes(range(64))
        connection.sendall(client_frame(2, payload))
        if receive_frame(connection) != (True, 2, b"\x00\xff"):
            raise RuntimeError("binary payload was not opaque")

    for size in (126, 65536):
        with connect(port, context) as connection:
            connection.sendall(client_frame(1, b"a" * size))
            if receive_frame(connection) != (True, 1, b"text"):
                raise RuntimeError(f"canonical extended {size}-byte frame was rejected")

    for path, size in (("/extended16", 126), ("/extended64", 65536)):
        with connect(port, context, path=path) as connection:
            frame = receive_frame(connection)
            if frame != (True, 1, b"x" * size):
                raise RuntimeError(f"outgoing extended framing failed for {size} bytes")

    with connect(port, context, path="/control-limit") as connection:
        if receive_frame(connection) != (True, 9, bytes(range(125))):
            raise RuntimeError("outgoing 125-byte control frame was rejected")
        connection.sendall(client_frame(9, bytes(range(125))))
        if receive_frame(connection) != (True, 10, bytes(range(125))):
            raise RuntimeError("incoming 125-byte control frame was rejected")


def exercise_adversarial(port):
    cases = [
        (client_frame(1, b"x", masked=False), 1002),
        (client_frame(3, b"x"), 1002),
        (client_frame(1, b"x", rsv=0x40), 1002),
        (client_frame(9, b"x", fin=False), 1002),
        (client_frame(9, b"x" * 126), 1002),
        (client_frame(0, b"x"), 1002),
        (client_frame(1, b"a", fin=False) + client_frame(2, b"b"), 1002),
        (declared_frame(1, 125, 126), 1002),
        (declared_frame(1, 65535, 127), 1002),
        (bytes([0x81, 0xFF]) + struct.pack("!Q", 1 << 63), 1002),
        (client_frame(1, b"x" * 65537), 1009),
        (client_frame(1, b"a" * 65536, fin=False) + client_frame(0, b"b" * 65536, fin=False) + client_frame(0, b"c"), 1009),
        (client_frame(1, b"\xff"), 1007),
        (client_frame(1, b"\xe2", fin=False) + client_frame(0, b"x"), 1007),
        (client_frame(8, b"x"), 1002),
        (client_frame(8, struct.pack("!H", 1005)), 1002),
        (client_frame(8, struct.pack("!H", 1000) + b"\xff"), 1007),
    ]
    for wire, code in cases:
        require_protocol_close(port, wire, code)

    with connect(port) as connection:
        connection.sendall(client_frame(1, b"part", fin=False) + client_frame(8, struct.pack("!H", 1001)))
        if close_code(connection) != 1001:
            raise RuntimeError("close during fragmentation was not answered")

    with connect(port) as connection:
        connection.sendall(client_frame(1, b"a" * 65536, fin=False) + client_frame(0, b"b" * 65536))
        if receive_frame(connection) != (True, 1, b"text"):
            raise RuntimeError("exact message/frame limits were rejected")


def exercise_disconnects(port, context=None):
    partials = [b"\x81", b"\x81\xfe\x01", b"\x81\xfe\x00\x7e\x11", client_frame(1, b"body")[:-2]]
    for partial in partials:
        connection = connect(port, context)
        connection.sendall(partial)
        connection.close()
    deadline = time.monotonic() + 3
    while True:
        try:
            with connect(port, context) as connection:
                connection.sendall(client_frame(1, b"healthy"))
                if receive_frame(connection) != (True, 1, b"text"):
                    raise RuntimeError("server unhealthy after partial-frame disconnect")
            return
        except OSError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.02)


def exercise_concurrent_writes(port):
    with connect(port, path="/concurrent") as connection:
        frames = [receive_frame(connection), receive_frame(connection)]
        if any(not fin or opcode != 1 for fin, opcode, _ in frames):
            raise RuntimeError(f"concurrent writes produced invalid frames: {frames!r}")
        if sorted(payload for _, _, payload in frames) != [b"left", b"right"]:
            raise RuntimeError(f"concurrent writes interleaved payloads: {frames!r}")


def exercise_convenience_mismatch(port):
    with connect(port, path="/mismatch") as connection:
        connection.sendall(client_frame(2, b"retained"))
        if receive_frame(connection) != (True, 1, b"retained"):
            raise RuntimeError("convenience read discarded a mismatched message")
        connection.sendall(client_frame(8, struct.pack("!H", 1000)))
        if close_code(connection) != 1000:
            raise RuntimeError("mismatch route close handshake failed")


def exercise_server_closing(port, context=None):
    cases = [("/return-close", 1000), ("/explicit-close", 1001), ("/error-close", 1011)]
    for path, expected in cases:
        with connect(port, context, path=path) as connection:
            if close_code(connection) != expected:
                raise RuntimeError(f"{path} sent the wrong server close code")
            connection.sendall(client_frame(9, b"after-close"))
            if receive_frame(connection) != (True, 10, b"after-close"):
                raise RuntimeError(f"{path} did not answer Ping while awaiting peer Close")
            connection.sendall(client_frame(8, struct.pack("!H", 1000)))
            if connection.recv(1):
                raise RuntimeError(f"{path} emitted data after the peer Close response")

    with connect(port, context, path="/reject-1010") as connection:
        if receive_frame(connection) != (True, 1, b"rejected"):
            raise RuntimeError("server public close accepted code 1010")
        connection.sendall(client_frame(8, struct.pack("!H", 1000)))
        if close_code(connection) != 1000:
            raise RuntimeError("1010 rejection changed later close handling")

    with connect(port, context, path="/client-1010") as connection:
        connection.sendall(client_frame(8, struct.pack("!H", 1010)))
        if receive_frame(connection) != (True, 8, b""):
            raise RuntimeError("server echoed client-only close code 1010")

    with connect(port, context, path="/return-close-timeout") as connection:
        if close_code(connection) != 1000:
            raise RuntimeError("bounded return sent the wrong close code")
        started = time.monotonic()
        if connection.recv(1):
            raise RuntimeError("bounded return emitted bytes without a peer Close")
        if time.monotonic() - started > 2:
            raise RuntimeError("server-initiated close handshake exceeded its deadline")


def exercise_active_return_invalidation(port, context=None):
    connection = connect(port, context, path="/escaped-read")
    connection.sendall(b"\x81")
    started = time.monotonic()
    try:
        while connection.recv(4096):
            pass
    finally:
        connection.close()
    if time.monotonic() - started > 2:
        raise RuntimeError("handler return did not interrupt and drain its admitted escaped reader")


def resource_counts(process):
    pid = process.pid
    fd_path = Path(f"/proc/{pid}/fd")
    status_path = Path(f"/proc/{pid}/status")
    fds = len(list(fd_path.iterdir())) if fd_path.exists() else None
    rss = None
    threads = None
    if status_path.exists():
        for line in status_path.read_text(encoding="ascii").splitlines():
            if line.startswith("VmRSS:"):
                rss = int(line.split()[1])
            elif line.startswith("Threads:"):
                threads = int(line.split()[1])
    elif os.name == "nt":
        import ctypes
        handle = ctypes.windll.kernel32.OpenProcess(0x0400, False, pid)
        if handle:
            count = ctypes.c_ulong()
            if ctypes.windll.kernel32.GetProcessHandleCount(handle, ctypes.byref(count)):
                fds = count.value
            ctypes.windll.kernel32.CloseHandle(handle)
    return fds, rss, threads


def require_resources_return(process, baseline, peak):
    time.sleep(0.2)
    after = resource_counts(process)
    if baseline[0] is not None and peak[0] is not None and peak[0] > baseline[0] + 4:
        raise RuntimeError(f"server FD/HANDLE peak was unbounded: {baseline[0]} -> {peak[0]}")
    if baseline[2] is not None and peak[2] is not None and peak[2] > baseline[2] + 2:
        raise RuntimeError(f"server worker/thread peak was unbounded: {baseline[2]} -> {peak[2]}")
    if baseline[1] is not None and peak[1] is not None and peak[1] > baseline[1] + 32768:
        raise RuntimeError(f"server RSS peak was unbounded: {baseline[1]} KiB -> {peak[1]} KiB")
    if baseline[0] is not None and after[0] is not None and after[0] > baseline[0] + 2:
        raise RuntimeError(f"server FD/HANDLE count did not return: {baseline[0]} -> {peak[0]} -> {after[0]}")
    if baseline[2] is not None and after[2] is not None and after[2] > baseline[2] + 1:
        raise RuntimeError(f"server worker/thread count did not return: {baseline[2]} -> {peak[2]} -> {after[2]}")
    if baseline[1] is not None and after[1] is not None and after[1] > baseline[1] + 16384:
        raise RuntimeError(f"server RSS failed bounded return: {baseline[1]} KiB -> {peak[1]} KiB -> {after[1]} KiB")
    return after


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-websocket-runtime-") as temporary:
        root = Path(temporary)
        limit_source = """function main() -> int { app := http_server(); try { app.websocket_limits(124, 125); } catch (NetworkError err) { return 0; } return 1; }"""
        limit_executable = compile_program(compiler, root, "runtime-limit-boundary", limit_source)
        subprocess.run([limit_executable], cwd=root, check=True)
        port = available_port()
        extended16 = "x" * 126
        extended64 = "x" * 65536
        control_payload = ", ".join(str(value) for value in range(125))
        source = f"""function main() -> int : (NetworkError, WebSocketError, ThreadError, TimeError) {{
    app := http_server();
    channel<bool> shutdown;
    channel<thread> escaped_threads;
    channel<bool> escaped_starting;
    app.timeouts(300, 300, 300, 1000);
    app.websocket_limits(65536, 131072);
    app.websocket("/text", (http_request request, websocket socket) => {{
        socket.accept();
        socket.read_text();
        socket.write_text("text");
        socket.read();
        return;
    }});
    app.websocket("/binary", (http_request request, websocket socket) => {{
        socket.accept();
        socket.read_bytes();
        socket.write_bytes([0, 255]);
        socket.read();
        return;
    }});
    app.websocket("/concurrent", (http_request request, websocket socket) => {{
        socket.accept();
        left := thread(() => {{ socket.write_text("left"); }});
        right := thread(() => {{ socket.write_text("right"); }});
        left.join();
        right.join();
        return;
    }});
    app.websocket("/extended16", (http_request request, websocket socket) => {{ socket.accept(); socket.write_text("{extended16}"); return; }});
    app.websocket("/extended64", (http_request request, websocket socket) => {{ socket.accept(); socket.write_text("{extended64}"); return; }});
    app.websocket("/control-limit", (http_request request, websocket socket) => {{ socket.accept(); socket.ping([{control_payload}]); socket.read(); return; }});
    app.websocket("/blocked-write", (http_request request, websocket socket) => {{
        socket.accept();
        channel<bool> started;
        reader := thread(() => {{ started.send(true); try {{ socket.read(); }} catch (NetworkError err) {{ }} }});
        started.receive();
        sleep_ms(50);
        socket.write_text("released");
        reader.join();
        return;
    }});
    app.websocket("/blocked-close", (http_request request, websocket socket) => {{
        socket.accept();
        channel<bool> started;
        reader := thread(() => {{ started.send(true); try {{ socket.read(); }} catch (NetworkError err) {{ }} }});
        started.receive();
        sleep_ms(50);
        socket.close(1000, "released");
        reader.join();
        return;
    }});
    app.websocket("/return-close", (http_request request, websocket socket) => {{ socket.accept(); return; }});
    app.websocket("/return-close-timeout", (http_request request, websocket socket) => {{ socket.accept(); return; }});
    app.websocket("/explicit-close", (http_request request, websocket socket) => {{ socket.accept(); socket.close(1001, "explicit"); return; }});
    app.websocket("/error-close", (http_request request, websocket socket) => {{ socket.accept(); socket.close(1010); return; }});
    app.websocket("/reject-1010", (http_request request, websocket socket) => {{
        socket.accept();
        try {{ socket.close(1010); }} catch (WebSocketError err) {{ socket.write_text("rejected"); }}
        socket.read();
        return;
    }});
    app.websocket("/client-1010", (http_request request, websocket socket) => {{ socket.accept(); socket.read(); return; }});
    app.websocket("/escaped-read", (http_request request, websocket socket) => {{
        socket.accept();
        escaped_threads.send(thread(() => {{
            escaped_starting.send(true);
            try {{ socket.read(); }} catch (NetworkError err) {{ }}
        }}));
        escaped_starting.receive();
        sleep_ms(250);
        return;
    }});
    app.websocket("/shutdown", (http_request request, websocket socket) => {{ socket.accept(); shutdown.send(true); return; }});
    app.websocket("/mismatch", (http_request request, websocket socket) => {{
        socket.accept();
        try {{
            socket.read_text();
        }} catch (WebSocketError err) {{
            socket.read_bytes();
            socket.write_text("retained");
        }}
        socket.read();
        return;
    }});
    listener := thread(() => {{ app.listen("127.0.0.1", {port}); }});
    shutdown.receive();
    app.stop();
    listener.join();
    return 0;
}}
"""
        executable = compile_program(compiler, root, "runtime-server", source)
        server = subprocess.Popen([executable], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 8
            while True:
                try:
                    exercise_interop(port)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.03)
            exercise_disconnects(port)
            exercise_adversarial(port)
            exercise_concurrent_writes(port)
            exercise_convenience_mismatch(port)
            exercise_server_closing(port)
            exercise_active_return_invalidation(port)
            baseline = resource_counts(server)
            peak = baseline
            sanitized = "-fsanitize=" in os.environ.get("STRUT_CXXFLAGS", "")
            stress_iterations = 100 if sanitized else 2000
            for _ in range(stress_iterations):
                with connect(port) as connection:
                    connection.sendall(client_frame(8, struct.pack("!H", 1000)))
                    if close_code(connection) != 1000:
                        raise RuntimeError("stress close handshake failed")
                sample = resource_counts(server)
                peak = tuple(max(old, new) if old is not None and new is not None else old or new for old, new in zip(peak, sample))
            after = require_resources_return(server, baseline, peak)
            print(f"WebSocket resource counts after {stress_iterations} cycles (FD/HANDLE, RSS KiB, threads): {baseline} -> {peak} -> {after}")
            for path in ("/blocked-write", "/blocked-close"):
                started = time.monotonic()
                with connect(port, path=path) as connection:
                    frame = receive_frame(connection)
                    if path == "/blocked-write" and frame != (True, 1, b"released"):
                        raise RuntimeError(f"blocked write did not resume: {frame!r}")
                    if path == "/blocked-close" and (frame[1], frame[2]) != (8, struct.pack("!H", 1000) + b"released"):
                        raise RuntimeError(f"blocked close did not resume: {frame!r}")
                if time.monotonic() - started > 2:
                    raise RuntimeError(f"{path} was not bounded by the configured read timeout")
            with connect(port, path="/shutdown"):
                pass
            stdout, stderr = server.communicate(timeout=15)
            if server.returncode != 0:
                raise RuntimeError(f"runtime server failed ({server.returncode})\n{stdout}\n{stderr}")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
        tls_port = available_port()
        tls_source = source.replace(f'app.listen("127.0.0.1", {port});',
                                    f'app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1]);')
        tls_source = tls_source.replace("function main()", "function main(string command, string[] args)")
        tls_source = tls_source.replace(": (NetworkError, WebSocketError, ThreadError, TimeError)",
                                        ": (NetworkError, WebSocketError, ThreadError, TimeError, TlsError)")
        tls_executable = compile_program(compiler, root, "runtime-tls-server", tls_source)
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate = fixture / "localhost-cert.pem"
        private_key = fixture / "localhost-key.pem"
        context = ssl.create_default_context(cafile=str(certificate))
        tls_server = subprocess.Popen([tls_executable, certificate, private_key], cwd=root,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 8
            while True:
                try:
                    exercise_interop(tls_port, context)
                    break
                except OSError:
                    if tls_server.poll() is not None:
                        stdout, stderr = tls_server.communicate()
                        raise RuntimeError(f"TLS runtime server exited early ({tls_server.returncode})\n{stdout}\n{stderr}")
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.03)
            exercise_disconnects(tls_port, context)
            exercise_server_closing(tls_port, context)
            exercise_active_return_invalidation(tls_port, context)
            for path in ("/blocked-write", "/blocked-close"):
                started = time.monotonic()
                with connect(tls_port, context, path=path) as connection:
                    receive_frame(connection)
                if time.monotonic() - started > 2:
                    raise RuntimeError(f"TLS {path} was not bounded by the configured read timeout")
            with connect(tls_port, context, path="/shutdown"):
                pass
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0:
                raise RuntimeError(f"TLS runtime server failed ({tls_server.returncode})\n{stdout}\n{stderr}")
        finally:
            if tls_server.poll() is None:
                tls_server.kill()
                tls_server.wait()

        stop_port = available_port()
        stop_source = f"""function main() -> int : (NetworkError, WebSocketError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(5000, 5000, 5000, 1000);
    channel<bool> reading;
    app.websocket("/text", (http_request request, websocket socket) => {{
        socket.accept();
        notifier := thread(() => {{ sleep_ms(50); reading.send(true); }});
        socket.read();
        notifier.join();
        return;
    }});
    listener := thread(() => {{ app.listen("127.0.0.1", {stop_port}); }});
    while (!app.running()) {{ sleep_ms(5); }}
    reading.receive();
    app.stop();
    listener.join();
    return 0;
}}
"""
        stop_executable = compile_program(compiler, root, "runtime-stop-server", stop_source)
        stop_server = subprocess.Popen([stop_executable], cwd=root, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    connection = connect(stop_port)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.02)
            started = time.monotonic()
            stdout, stderr = stop_server.communicate(timeout=8)
            connection.close()
            if stop_server.returncode != 0 or time.monotonic() - started > 3:
                raise RuntimeError(f"blocked WebSocket stop failed ({stop_server.returncode})\n{stdout}\n{stderr}")
        finally:
            if stop_server.poll() is None:
                stop_server.kill()
                stop_server.wait()

    print("WebSocket P5 certification: plaintext/TLS framing, bounded close handshakes, post-close Ping, operation draining, live resources and stress passed")


if __name__ == "__main__":
    main()
