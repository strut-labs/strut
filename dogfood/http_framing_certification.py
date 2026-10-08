#!/usr/bin/env python3
"""Certify strict HTTP/1 request syntax and unambiguous body framing."""

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


def connect_retry(port, deadline=5.0):
    # No throwaway readiness probe: the server's max_requests is an exact accept budget, and a
    # probe connection could be counted (or discarded before accept on some platforms), racing
    # the real requests. Instead we retry the first real connect until the listener is up.
    end = time.monotonic() + deadline
    while True:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=5)
        except OSError:
            if time.monotonic() >= end:
                raise
            time.sleep(0.02)


# The server's listen(...,max_requests) is an exact accept budget and its graceful stop may
# interrupt a connection that has not yet been served (a benign race near the budget edge).
# We therefore give the budget headroom and drain it with throwaway requests whose responses
# (including an empty close) are ignored -- the checked requests are never near the edge.
DRAIN = 8


def drain_plain(port, count):
    for _ in range(count):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as c:
                c.sendall(b"GET /get HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                c.shutdown(socket.SHUT_WR)
                while c.recv(4096):
                    pass
        except OSError:
            pass


def drain_tls(port, count):
    for _ in range(count):
        try:
            tls_request(port, b"GET /get HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        except Exception:
            pass


def raw_request(port, payload):
    with connect_retry(port) as connection:
        connection.sendall(payload)
        connection.shutdown(socket.SHUT_WR)
        response = bytearray()
        while True:
            try:
                chunk = connection.recv(4096)
            except ConnectionResetError:
                break
            if not chunk:
                break
            response.extend(chunk)
        return bytes(response)


def tls_raw_request(port, payload):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    with connect_retry(port) as raw:
        with context.wrap_socket(raw, server_hostname="localhost") as connection:
            connection.sendall(payload)
            half_close = socket.socket(fileno=connection.fileno())
            half_close.shutdown(socket.SHUT_WR)
            half_close.detach()
            response = bytearray()
            while True:
                try:
                    chunk = connection.recv(4096)
                except (ConnectionResetError, ssl.SSLError):
                    break
                if not chunk:
                    break
                response.extend(chunk)
            return bytes(response)


def tls_request(port, payload):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    with connect_retry(port) as raw:
        with context.wrap_socket(raw, server_hostname="localhost") as connection:
            connection.sendall(payload)
            response = bytearray()
            while True:
                chunk = connection.recv(4096)
                if not chunk:
                    break
                response.extend(chunk)
            return bytes(response)


def status(response):
    try:
        return int(response.split(b" ", 2)[1])
    except (IndexError, ValueError) as error:
        raise RuntimeError(f"invalid HTTP response: {response!r}") from error


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    valid_get = b"GET /get HTTP/1.1\r\nHost: localhost\r\n\r\n"
    cases = [
        ("http11", valid_get, 200, b"get"),
        ("http10", b"GET /get HTTP/1.0\r\n\r\n", 200, b"get"),
        ("lowercase-content-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\ncontent-length: 5\r\n\r\nhello", 200, b"hello"),
        ("mixed-content-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nCoNtEnT-LeNgTh: 4\r\n\r\ntest", 200, b"test"),
        ("duplicate-identical-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\ncontent-length: 1\r\n\r\nx", 400, None),
        ("duplicate-conflicting-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nxx", 400, None),
        ("comma-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5, 5\r\n\r\nhello", 400, None),
        ("signed-plus-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: +5\r\n\r\nhello", 400, None),
        ("negative-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n", 400, None),
        ("overflow-length", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 184467440737095516160\r\n\r\n", 413, None),
        ("whitespace-before-content-colon", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length : 5\r\n\r\nhello", 400, None),
        ("chunked", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", 200, b""),
        ("te-and-cl", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\nhello", 400, None),
        ("oversized-cl-and-te", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 17\r\nTransfer-Encoding: chunked\r\n\r\n", 400, None),
        ("oversized-duplicate-cl", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 17\r\nContent-Length: 17\r\n\r\n", 400, None),
        ("duplicate-te", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\nTransfer-Encoding: chunked\r\n\r\n", 400, None),
        ("duplicate-chunked-coding", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked, chunked\r\n\r\n", 400, None),
        ("nonfinal-chunked", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked, gzip\r\n\r\n", 400, None),
        ("nonchunked-final", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n", 400, None),
        ("valid-unsupported-coding-parameters", b"POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip; level=1, chunked\r\n\r\n", 501, None),
        ("double-space-request-line", b"GET  /get HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("tab-request-line", b"GET\t/get HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("unsupported-version", b"GET /get HTTP/2.0\r\nHost: localhost\r\n\r\n", 505, None),
        ("malformed-version", b"GET /get HTTP/x\r\nHost: localhost\r\n\r\n", 400, None),
        ("bare-lf", b"GET /get HTTP/1.1\nHost: localhost\n\n", 400, None),
        ("nul-request-target", b"GET /get\x00 HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("nul-header", b"GET /get HTTP/1.1\r\nHost: local\x00host\r\n\r\n", 400, None),
        ("whitespace-before-colon", b"GET /get HTTP/1.1\r\nHost : localhost\r\n\r\n", 400, None),
        ("folded-header", b"GET /get HTTP/1.1\r\nHost: localhost\r\n continuation\r\n\r\n", 400, None),
        ("excessive-header-count", b"GET /get HTTP/1.1\r\nHost: localhost\r\n" + b"".join(f"X-{i}: x\r\n".encode() for i in range(8)) + b"\r\n", 431, None),
        ("excessive-header-bytes", b"GET /get HTTP/1.1\r\nHost: localhost\r\nX-Large: " + b"x" * 17000 + b"\r\n\r\n", 431, None),
        ("excessive-body", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 17\r\n\r\n", 413, None),
        ("missing-host", b"GET /get HTTP/1.1\r\n\r\n", 400, None),
        ("duplicate-host", b"GET /get HTTP/1.1\r\nHost: localhost\r\nhost: localhost\r\n\r\n", 400, None),
        ("malformed-host", b"GET /get HTTP/1.1\r\nHost: bad host\r\n\r\n", 400, None),
        ("host-path-delimiter", b"GET /get HTTP/1.1\r\nHost: bad/host\r\n\r\n", 400, None),
        ("host-bad-percent", b"GET /get HTTP/1.1\r\nHost: bad%2\r\n\r\n", 400, None),
        ("host-bad-ip-literal", b"GET /get HTTP/1.1\r\nHost: [not-ip]\r\n\r\n", 400, None),
        ("host-bad-port", b"GET /get HTTP/1.1\r\nHost: localhost:99999\r\n\r\n", 400, None),
        ("host-ipv4", b"GET /get HTTP/1.1\r\nHost: 127.0.0.1:80\r\n\r\n", 200, b"get"),
        ("host-ipv6", b"GET /get HTTP/1.1\r\nHost: [::1]:80\r\n\r\n", 200, b"get"),
        ("duplicate-generic-header", b"GET /get HTTP/1.1\r\nHost: localhost\r\nX-Test: one\r\nx-test: two\r\n\r\n", 400, None),
        ("premature-eof", b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nabc", 400, None),
        ("trailing-request", b"GET /get HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\nGET /get HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, b"get"),
        ("fragment-target", b"GET /get#fragment HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("bad-percent-target", b"GET /bad%2 HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("encoded-control-target", b"GET /bad%00path HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("http10-invalid-target", b"GET /bad%00path HTTP/1.0\r\n\r\n", 400, None),
        ("head-malformed-header", b"HEAD /get HTTP/1.1\r\nHost: localhost\r\nBroken\r\n\r\n", 400, None),
        ("head-oversized-header", b"HEAD /get HTTP/1.1\r\nHost: localhost\r\nX-Large: " + b"x" * 17000 + b"\r\n\r\n", 431, None),
        ("http10-premature-head", b"GET /get HTTP/1.0\r\nX-Test: incomplete", 400, None),
        ("backslash-target", b"GET /bad\\path HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("brace-target", b"GET /bad{{path}} HTTP/1.1\r\nHost: localhost\r\n\r\n", 400, None),
        ("control-header", b"GET /get HTTP/1.1\r\nHost: localhost\r\nX-Test: a\x01b\r\n\r\n", 400, None),
        ("long-request-line", b"GET /" + b"x" * 8200 + b" HTTP/1.1\r\nHost: localhost\r\n\r\n", 414, None),
        ("head-long-request-line", b"HEAD /" + b"x" * 8200 + b" HTTP/1.1\r\nHost: localhost\r\n\r\n", 414, None),
        ("http10-long-request-line", b"GET /" + b"x" * 8200 + b" HTTP/1.0\r\n\r\n", 414, None),
        ("body-not-header-bytes", b"POST /post HTTP/1.1\r\nHost: localhost\r\nX-A: " + b"x" * 8100 + b"\r\nX-B: " + b"x" * 8100 + b"\r\nContent-Length: 16\r\n\r\n0123456789abcdef", 200, b"0123456789abcdef"),
    ]
    port = available_port()
    source = f'''function main() -> void : NetworkError {{
    app := http_server();
    app.timeouts(2000, 2000, 2000, 2000);
    app.limits(16, 16384, 8, 16);
    app.get("/get", (http_request request) => {{ return http_text("get"); }});
    app.post("/post", (http_request request) => {{ return http_text(request.body); }});
    app.listen("127.0.0.1", {port}, {len(cases) + DRAIN});
}}
'''
    with tempfile.TemporaryDirectory(prefix="strut-http-framing-") as temporary:
        root = Path(temporary)
        program = root / "framing-server.p"
        executable = root / ("framing-server.exe" if sys.platform == "win32" else "framing-server")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
        server = subprocess.Popen([executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            for name, payload, expected_status, expected_body in cases:
                response = raw_request(port, payload)
                if not response:
                    raise RuntimeError(f"{name}: server closed the connection without responding")
                observed = status(response)
                if observed != expected_status:
                    raise RuntimeError(f"{name}: status {observed}, expected {expected_status}: {response!r}")
                if expected_body is not None:
                    body = response.split(b"\r\n\r\n", 1)[1]
                    if body != expected_body:
                        raise RuntimeError(f"{name}: body {body!r}, expected {expected_body!r}")
                if name == "trailing-request" and response.count(b"HTTP/1.1 ") != 1:
                    raise RuntimeError("trailing request was interpreted as a second message")
                if name in ("http10-invalid-target", "http10-premature-head") and not response.startswith(b"HTTP/1.0 400 "):
                    raise RuntimeError(f"HTTP/1.0 parser error used the wrong version: {response!r}")
                if name == "http10-long-request-line" and not response.startswith(b"HTTP/1.0 414 "):
                    raise RuntimeError(f"HTTP/1.0 long-target error used the wrong version: {response!r}")
                if name in ("head-malformed-header", "head-oversized-header", "head-long-request-line") and response.split(b"\r\n\r\n", 1)[1]:
                    raise RuntimeError(f"malformed HEAD response included a body: {response!r}")
            drain_plain(port, DRAIN)
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"server failed: exit={server.returncode} stdout={stdout!r} stderr={stderr!r}")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        tls_port = available_port()
        tls_program = root / "framing-tls-server.p"
        tls_executable = root / ("framing-tls-server.exe" if sys.platform == "win32" else "framing-tls-server")
        tls_program.write_text(f'''function main(string command, string[] args) -> void : (NetworkError, TlsError) {{
    app := http_server();
    app.timeouts(2000, 2000, 2000, 2000);
    app.get("/get", (http_request request) => {{ return http_text("get"); }});
    app.post("/post", (http_request request) => {{ return http_text(request.body); }});
    app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1], {4 + DRAIN});
}}
''', encoding="utf-8")
        subprocess.run([compiler, tls_program, "-o", tls_executable], check=True, cwd=root)
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        tls_server = subprocess.Popen(
            [tls_executable, fixture / "localhost-cert.pem", fixture / "localhost-key.pem"],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            malformed = tls_request(tls_port, b"GET  /get HTTP/1.1\r\nHost: localhost\r\n\r\n")
            valid = tls_request(tls_port, b"GET /get HTTP/1.1\r\nHost: localhost\r\n\r\n")
            if status(malformed) != 400 or status(valid) != 200:
                raise RuntimeError(f"TLS parser parity failed: {malformed!r} {valid!r}")
            incomplete_head = tls_raw_request(tls_port, b"POST /post HTTP/1.1\r\nHost: localhost\r\n")
            incomplete_body = tls_raw_request(tls_port, b"POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nabc")
            for response in (incomplete_head, incomplete_body):
                if response and status(response) != 400:
                    raise RuntimeError(f"TLS incomplete framing was not rejected: {response!r}")
            drain_tls(tls_port, DRAIN)
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"TLS framing server failed: exit={tls_server.returncode} stdout={stdout!r} stderr={stderr!r}")
        finally:
            if tls_server.poll() is None:
                tls_server.kill()
                tls_server.wait()
    print(f"HTTP framing certification: {len(cases)} plaintext cases, TLS parser parity, and 2 TLS truncation cases passed")


if __name__ == "__main__":
    main()
