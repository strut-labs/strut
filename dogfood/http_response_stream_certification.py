#!/usr/bin/env python3
"""Certify incremental HTTP response writing and its single framing state machine."""

from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time


def available_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def compile_program(compiler, root, name, source):
    program = root / f"{name}.p"
    executable = root / (f"{name}.exe" if sys.platform == "win32" else name)
    program.write_text(source, encoding="utf-8")
    subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
    return executable


def wait_until_listening(port, process):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            stdout, stderr = process.communicate()
            raise RuntimeError(
                f"streaming server exited during startup: {process.returncode} {stdout!r} {stderr!r}"
            )
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.02)
    raise RuntimeError("streaming server did not start")


def request_bytes(port, path, version="HTTP/1.1"):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
        connection.sendall(f"GET {path} {version}\r\nHost: localhost\r\nConnection: close\r\n\r\n".encode())
        response = bytearray()
        while True:
            chunk = connection.recv(65536)
            if not chunk:
                return bytes(response)
            response.extend(chunk)


def split_response(response):
    head, separator, body = response.partition(b"\r\n\r\n")
    if not separator:
        raise RuntimeError(f"response has no complete head: {response!r}")
    lines = head.split(b"\r\n")
    fields = []
    for line in lines[1:]:
        if b":" not in line:
            raise RuntimeError(f"malformed response field: {line!r}")
        name, value = line.split(b":", 1)
        fields.append((name.lower(), value.strip()))
    return lines[0], fields, body


def field_values(fields, name):
    return [value for field, value in fields if field == name.lower()]


def decode_chunked(body, require_terminal=True):
    decoded = bytearray()
    position = 0
    terminals = 0
    while position < len(body):
        line_end = body.find(b"\r\n", position)
        if line_end < 0:
            if require_terminal:
                raise RuntimeError(f"truncated chunk size: {body!r}")
            return bytes(decoded), terminals
        size_text = body[position:line_end]
        try:
            size = int(size_text, 16)
        except ValueError as error:
            raise RuntimeError(f"invalid chunk size: {size_text!r}") from error
        position = line_end + 2
        if size == 0:
            terminals += 1
            if body[position:position + 2] != b"\r\n":
                raise RuntimeError("invalid terminal chunk")
            position += 2
            if position != len(body):
                raise RuntimeError(f"bytes follow terminal chunk: {body[position:]!r}")
            return bytes(decoded), terminals
        end = position + size
        if end + 2 > len(body) or body[end:end + 2] != b"\r\n":
            if require_terminal:
                raise RuntimeError("truncated chunk payload")
            return bytes(decoded), terminals
        decoded.extend(body[position:end])
        position = end + 2
    if require_terminal:
        raise RuntimeError("missing terminal chunk")
    return bytes(decoded), terminals


def require_status(line, status):
    if not line.startswith(f"HTTP/1.".encode()) or f" {status} ".encode() not in line:
        raise RuntimeError(f"expected status {status}, received {line!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    with tempfile.TemporaryDirectory(prefix="strut-http-response-stream-") as temporary:
        root = Path(temporary)
        server_path = compile_program(
            compiler,
            root,
            "response-stream-server",
            f'''function main() -> void : (NetworkError, TimeError, ThreadError) {{
    app := http_server();
    http_response_writer saved_value;
    saved := new(saved_value);
    app.timeouts(3000, 1000, 3000, 500);
    app.limits(1024, 4096, 32, 4);
    app.get("/health", (http_request request) => {{ return http_text("healthy"); }});
    app.get_stream("/small", (http_request request, http_response_writer writer) => {{
        int index := 0;
        while (index < 100) {{ writer.write("x"); index++; }}
    }});
    app.get_stream("/binary", (http_request request, http_response_writer writer) => {{
        data := bytes(65536);
        writer.write_bytes(data);
    }});
    app.get_stream("/flush", (http_request request, http_response_writer writer) => {{
        writer.flush();
        sleep_ms(300);
        writer.write("later");
    }});
    app.get_stream("/known", (http_request request, http_response_writer writer) => {{
        writer.content_length(5);
        writer.write("he");
        writer.write("llo");
    }});
    app.get_stream("/empty", (http_request request, http_response_writer writer) => {{ writer.finish(); }});
    app.get_stream("/before", (http_request request, http_response_writer writer) => {{
        writer.header("X-Test", "safe\\r\\nInjected: yes");
        writer.write("unsafe");
    }});
    app.get_stream("/after", (http_request request, http_response_writer writer) => {{
        writer.write("first");
        writer.status(201);
    }});
    app.get_stream("/mutation", (http_request request, http_response_writer writer) => {{
        writer.write("ok");
        try {{ writer.header("X-Late", "bad"); }} catch (NetworkError caught) {{ if (caught.message == "") {{ writer.write("bad"); }} }}
    }});
    app.get_stream("/write-after-finish", (http_request request, http_response_writer writer) => {{
        writer.content_length(2);
        writer.write("ok");
        writer.finish();
        try {{ writer.write("bad"); }} catch (NetworkError caught) {{ if (caught.message == "") {{ writer.write("bad"); }} }}
    }});
    app.get_stream("/finish-twice", (http_request request, http_response_writer writer) => {{
        writer.write("ok");
        writer.finish();
        writer.finish();
    }});
    app.get_stream("/forbidden", (http_request request, http_response_writer writer) => {{
        writer.status(204);
        writer.write("unsafe");
    }});
    app.get_stream("/underfill", (http_request request, http_response_writer writer) => {{
        writer.content_length(5);
    }});
    app.get_stream("/underfill-after", (http_request request, http_response_writer writer) => {{
        writer.content_length(5);
        writer.write("x");
    }});
    app.get_stream("/save", (http_request request, http_response_writer writer) => {{
        *saved = writer;
        writer.write("saved");
    }});
    app.get_stream("/use-saved", (http_request request, http_response_writer writer) => {{
        try {{ saved->write("unsafe"); }} catch (NetworkError caught) {{ if (caught.message != "") {{ writer.write("inactive"); }} }}
    }});
    app.get_stream("/concurrent", (http_request request, http_response_writer writer) => {{
        first := thread(() => {{ writer.write("x"); }});
        second := thread(() => {{ writer.write("x"); }});
        third := thread(() => {{ writer.write("x"); }});
        fourth := thread(() => {{ writer.write("x"); }});
        first.join(); second.join(); third.join(); fourth.join();
    }});
    app.get_stream("/disconnect", (http_request request, http_response_writer writer) => {{
        block := bytes(8192);
        int index := 0;
        while (index < 10000) {{ writer.write_bytes(block); index++; }}
    }});
    app.listen("127.0.0.1", {port}, 21);
}}
''',
        )
        server = subprocess.Popen(
            [server_path], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(port, server)

            small = request_bytes(port, "/small")
            line, fields, body = split_response(small)
            require_status(line, 200)
            if field_values(fields, b"transfer-encoding") != [b"chunked"]:
                raise RuntimeError(f"unknown HTTP/1.1 stream was not chunked: {small!r}")
            decoded, terminals = decode_chunked(body)
            if decoded != b"x" * 100 or terminals != 1:
                raise RuntimeError("many-write stream framing was not exact")

            binary = request_bytes(port, "/binary")
            _, binary_fields, binary_body = split_response(binary)
            decoded, terminals = decode_chunked(binary_body)
            if decoded != bytes(65536) or terminals != 1:
                raise RuntimeError("binary stream lost byte values or terminal framing")
            if field_values(binary_fields, b"content-length"):
                raise RuntimeError("unknown stream emitted Content-Length")

            known = request_bytes(port, "/known")
            _, known_fields, known_body = split_response(known)
            if field_values(known_fields, b"content-length") != [b"5"] or known_body != b"hello":
                raise RuntimeError(f"known-length stream framing failed: {known!r}")
            if field_values(known_fields, b"transfer-encoding"):
                raise RuntimeError("known-length stream was also chunked")

            empty = request_bytes(port, "/empty")
            _, empty_fields, empty_body = split_response(empty)
            if field_values(empty_fields, b"transfer-encoding") != [b"chunked"]:
                raise RuntimeError("empty unknown-length stream was not chunked")
            if decode_chunked(empty_body) != (b"", 1):
                raise RuntimeError("empty stream did not contain exactly one terminal chunk")

            http10 = request_bytes(port, "/small", "HTTP/1.0")
            line, fields, body = split_response(http10)
            if not line.startswith(b"HTTP/1.0 200 ") or body != b"x" * 100:
                raise RuntimeError(f"HTTP/1.0 close-delimited stream failed: {http10!r}")
            if field_values(fields, b"content-length") or field_values(fields, b"transfer-encoding"):
                raise RuntimeError("HTTP/1.0 unknown stream emitted length or chunk framing")

            for path in ("/before", "/forbidden", "/underfill"):
                rejected = request_bytes(port, path)
                line, _, body = split_response(rejected)
                require_status(line, 500)
                if body != b"Internal Server Error" or b"unsafe" in rejected or b"Injected" in rejected:
                    raise RuntimeError(f"unsafe precommit response escaped at {path}: {rejected!r}")

            underfill_after = request_bytes(port, "/underfill-after")
            line, fields, body = split_response(underfill_after)
            require_status(line, 200)
            if field_values(fields, b"content-length") != [b"5"] or body != b"x":
                raise RuntimeError("committed length underrun attempted a second response")

            saved = request_bytes(port, "/save")
            _, _, body = split_response(saved)
            if decode_chunked(body) != (b"saved", 1):
                raise RuntimeError("writer save fixture did not complete")
            escaped = request_bytes(port, "/use-saved")
            _, _, body = split_response(escaped)
            if decode_chunked(body) != (b"inactive", 1):
                raise RuntimeError("escaped response writer remained usable after handler return")
            concurrent = request_bytes(port, "/concurrent")
            _, _, body = split_response(concurrent)
            if decode_chunked(body) != (b"xxxx", 1):
                raise RuntimeError("concurrent response writes corrupted framing")

            partial = request_bytes(port, "/after")
            line, fields, body = split_response(partial)
            require_status(line, 200)
            decoded, terminals = decode_chunked(body, require_terminal=False)
            if decoded != b"first" or terminals != 0 or b"HTTP/1.1 500" in body:
                raise RuntimeError(f"post-commit failure emitted invalid recovery: {partial!r}")

            for path in ("/mutation", "/finish-twice"):
                response = request_bytes(port, path)
                _, _, body = split_response(response)
                decoded, terminals = decode_chunked(body)
                if decoded != b"ok" or terminals != 1:
                    raise RuntimeError(f"writer terminal-state behavior failed at {path}")

            after_finish = request_bytes(port, "/write-after-finish")
            _, fields, body = split_response(after_finish)
            if field_values(fields, b"content-length") != [b"2"] or body != b"ok":
                raise RuntimeError("write-after-finish changed the completed response")

            with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
                connection.sendall(b"GET /flush HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                connection.settimeout(0.2)
                first = connection.recv(4096)
                if b"\r\n\r\n" not in first or b"later" in first:
                    raise RuntimeError(f"flush did not expose committed metadata promptly: {first!r}")
                connection.settimeout(2)
                rest = bytearray()
                while True:
                    chunk = connection.recv(4096)
                    if not chunk:
                        break
                    rest.extend(chunk)
                _, _, body = split_response(first + bytes(rest))
                if decode_chunked(body) != (b"later", 1):
                    raise RuntimeError("flushed response body framing failed")

            disconnected = socket.create_connection(("127.0.0.1", port), timeout=5)
            disconnected.sendall(b"GET /disconnect HTTP/1.1\r\nHost: localhost\r\n\r\n")
            disconnected.recv(512)
            disconnected.close()
            time.sleep(0.1)
            health = request_bytes(port, "/health")
            line, _, body = split_response(health)
            require_status(line, 200)
            if body != b"healthy":
                raise RuntimeError("server did not recover after streaming client disconnect")

            with socket.create_connection(("127.0.0.1", port), timeout=5) as malformed:
                malformed.sendall(
                    b"GET /health HTTP/1.0\r\nX-Test: one\r\nX-Test: two\r\n\r\n"
                )
                malformed_response = bytearray()
                while True:
                    chunk = malformed.recv(4096)
                    if not chunk:
                        break
                    malformed_response.extend(chunk)
            line, _, _ = split_response(bytes(malformed_response))
            if not line.startswith(b"HTTP/1.0 400 "):
                raise RuntimeError(f"HTTP/1.0 error used wrong protocol version: {line!r}")

            stdout, stderr = server.communicate(timeout=15)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(
                    f"streaming server failed: exit={server.returncode} stdout={stdout!r} stderr={stderr!r}"
                )
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        blocked_port = available_port()
        blocked_path = compile_program(
            compiler,
            root,
            "blocked-stream-server",
    f'''function main() -> void : NetworkError {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(5000, 5000, 5000, 300);
    app.limits(1024, 4096, 16, 2);
    app.get_stream("/blocked", (http_request request, http_response_writer writer) => {{
        block := bytes(65536);
        int index := 0;
        while (index < 10000) {{ writer.write_bytes(block); index++; }}
    }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    app.listen("127.0.0.1", {blocked_port});
}}
''',
        )
        blocked_server = subprocess.Popen(
            [blocked_path], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        blocked_client = None
        try:
            wait_until_listening(blocked_port, blocked_server)
            blocked_client = socket.create_connection(("127.0.0.1", blocked_port), timeout=5)
            blocked_client.sendall(b"GET /blocked HTTP/1.1\r\nHost: localhost\r\n\r\n")
            blocked_client.recv(512)
            stopped = request_bytes(blocked_port, "/stop")
            line, _, body = split_response(stopped)
            require_status(line, 200)
            if body != b"stopped":
                raise RuntimeError("handler stop response was lost during blocked stream shutdown")
            stdout, stderr = blocked_server.communicate(timeout=5)
            if blocked_server.returncode != 0 or stdout or stderr:
                raise RuntimeError(
                    f"blocked streaming shutdown failed: exit={blocked_server.returncode} "
                    f"stdout={stdout!r} stderr={stderr!r}"
                )
        finally:
            if blocked_client is not None:
                blocked_client.close()
            if blocked_server.poll() is None:
                blocked_server.kill()
                blocked_server.wait()

        tls_port = available_port()
        tls_path = compile_program(
            compiler,
            root,
            "tls-stream-server",
            f'''function main(string command, string[] args) -> int : (NetworkError, TlsError) {{
    if (args.length != 2) {{ return 2; }}
    app := http_server();
    app.get_stream("/tls", (http_request request, http_response_writer writer) => {{
        writer.write("tls-");
        writer.write("stream");
    }});
    app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1], 2);
    return 0;
}}
''',
        )
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate = fixture / "localhost-cert.pem"
        key = fixture / "localhost-key.pem"
        tls_server = subprocess.Popen(
            [tls_path, certificate, key],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            wait_until_listening(tls_port, tls_server)
            context = ssl.create_default_context(cafile=str(certificate))
            with socket.create_connection(("127.0.0.1", tls_port), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as secure:
                    secure.sendall(b"GET /tls HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                    response = bytearray()
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        response.extend(chunk)
            line, fields, body = split_response(bytes(response))
            require_status(line, 200)
            if field_values(fields, b"transfer-encoding") != [b"chunked"]:
                raise RuntimeError("TLS stream did not use the shared chunked framing path")
            if decode_chunked(body) != (b"tls-stream", 1):
                raise RuntimeError("TLS streaming body or terminal chunk was incorrect")
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0 or stdout or stderr:
                raise RuntimeError("TLS streaming server did not exit cleanly")
        finally:
            if tls_server.poll() is None:
                tls_server.kill()
                tls_server.wait()

    print(
        "HTTP response streaming certification: incremental binary/text writes, framing, flush, "
        "terminal states, disconnect, blocked shutdown, and TLS parity passed"
    )


if __name__ == "__main__":
    main()
