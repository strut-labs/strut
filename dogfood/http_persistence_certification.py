#!/usr/bin/env python3
"""Certify sequential persistent HTTP/1 connection ownership and framing."""

from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time

from http_response_stream_certification import available_port, compile_program, wait_until_listening


class HttpConnection:
    def __init__(self, connection):
        self.connection = connection
        self.buffer = bytearray()

    def send(self, payload):
        self.connection.sendall(payload)

    def _through(self, marker):
        while marker not in self.buffer:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise RuntimeError("connection closed before a complete HTTP message")
            self.buffer.extend(chunk)
        end = self.buffer.index(marker) + len(marker)
        value = bytes(self.buffer[:end])
        del self.buffer[:end]
        return value

    def _bytes(self, size):
        while len(self.buffer) < size:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise RuntimeError("connection closed before the framed body completed")
            self.buffer.extend(chunk)
        value = bytes(self.buffer[:size])
        del self.buffer[:size]
        return value

    def response(self, method=b"GET"):
        head = self._through(b"\r\n\r\n")[:-4]
        lines = head.split(b"\r\n")
        status = int(lines[0].split(b" ", 2)[1])
        fields = []
        for line in lines[1:]:
            name, value = line.split(b":", 1)
            fields.append((name.lower(), value.strip()))
        values = lambda name: [value for field, value in fields if field == name]
        if method == b"HEAD" or status in (204, 304):
            body = b""
        elif values(b"content-length"):
            body = self._bytes(int(values(b"content-length")[0]))
        elif values(b"transfer-encoding") == [b"chunked"]:
            decoded = bytearray()
            while True:
                size = int(self._through(b"\r\n")[:-2], 16)
                if size == 0:
                    if self._bytes(2) != b"\r\n":
                        raise RuntimeError("invalid terminal chunk")
                    break
                decoded.extend(self._bytes(size))
                if self._bytes(2) != b"\r\n":
                    raise RuntimeError("invalid chunk terminator")
            body = bytes(decoded)
        else:
            raise RuntimeError(f"persistent response is not self-delimited: {head!r}")
        return status, fields, body

    def expect_eof(self, timeout=1.0):
        self.connection.settimeout(timeout)
        if self.buffer or self.connection.recv(1):
            raise RuntimeError("connection remained open or emitted unexpected bytes")


def request(path, version="HTTP/1.1", extra=""):
    return f"GET {path} {version}\r\nHost: localhost\r\n{extra}\r\n".encode()


def check_response(value, status, body):
    if value[0] != status or value[2] != body:
        raise RuntimeError(f"unexpected response: {value!r}")


def source(port, tls=False):
    listen = (
        f'app.listen_tls("127.0.0.1", {port}, args[0], args[1]);'
        if tls
        else f'app.listen("127.0.0.1", {port});'
    )
    signature = (
        "function main(string command, string[] args) -> int : (NetworkError, TlsError, TimeError)"
        if tls
        else "function main() -> void : (NetworkError, TimeError)"
    )
    prefix = "if (args.length != 2) { return 2; }" if tls else ""
    suffix = "return 0;" if tls else ""
    return f'''{signature} {{
    {prefix}
    app := http_server();
    app_ref := ref(app);
    app.timeouts(1000, 1000, 200, 500);
    app.limits(4096, 8192, 32, 8);
    app.get("/one", (http_request request) => {{ return http_text("one"); }});
    app.get("/slow", (http_request request) => {{ try {{ sleep_ms(100); }} catch (TimeError err) {{ }} return http_text("slow"); }});
    app.post("/echo", (http_request request) => {{ return http_text(request.body); }});
    app.get_stream("/chunk", (http_request request, http_response_writer response) => {{
        try {{ response.write("chunk-"); response.write("body"); }} catch (NetworkError err) {{ }}
    }});
    app.post_request_stream("/consume", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            data := body.read_all_bytes(); response.content_length(data.length()); response.write_bytes(data);
        }} catch (NetworkError err) {{ }}
    }});
    app.post_request_stream("/partial", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ response.write_bytes(body.read_bytes(1)); }} catch (NetworkError err) {{ }}
    }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    {listen}
    {suffix}
}}
'''


def exercise(port, opener):
    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one"))
        check_response(connection.response(), 200, b"one")
        connection.send(request("/one"))
        check_response(connection.response(), 200, b"one")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/slow") + request("/one"))
        check_response(connection.response(), 200, b"slow")
        check_response(connection.response(), 200, b"one")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(
            b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ndata"
            + request("/one")
        )
        check_response(connection.response(method=b"POST"), 200, b"data")
        check_response(connection.response(), 200, b"one")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(
            b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            b"3\r\nabc\r\n0\r\n\r\n" + request("/chunk")
        )
        check_response(connection.response(method=b"POST"), 200, b"abc")
        check_response(connection.response(), 200, b"chunk-body")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one").replace(b"GET", b"HEAD", 1) + request("/one"))
        head = connection.response(method=b"HEAD")
        check_response(head, 200, b"")
        if [value for name, value in head[1] if name == b"content-length"] != [b"3"]:
            raise RuntimeError("HEAD did not preserve buffered representation length")
        check_response(connection.response(), 200, b"one")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/chunk").replace(b"GET", b"HEAD", 1) + request("/one"))
        head = connection.response(method=b"HEAD")
        if any(name in (b"content-length", b"transfer-encoding") for name, _ in head[1]):
            raise RuntimeError("unknown-length HEAD emitted body framing")
        check_response(connection.response(), 200, b"one")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one", extra="Connection: close\r\n") + request("/one"))
        closed = connection.response()
        check_response(closed, 200, b"one")
        if [value for name, value in closed[1] if name == b"connection"] != [b"close"]:
            raise RuntimeError("HTTP/1.1 close policy was not advertised")
        connection.expect_eof()

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one", version="HTTP/1.0"))
        check_response(connection.response(), 200, b"one")
        connection.expect_eof()

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one", version="HTTP/1.0", extra="Connection: keep-alive\r\n"))
        response = connection.response()
        check_response(response, 200, b"one")
        if [value for name, value in response[1] if name == b"connection"] != [b"keep-alive"]:
            raise RuntimeError("HTTP/1.0 persistence was not advertised")
        connection.send(request("/one", version="HTTP/1.0", extra="Connection: close\r\n"))
        check_response(connection.response(), 200, b"one")
        connection.expect_eof()

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(b"POST /partial HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\na")
        check_response(connection.response(method=b"POST"), 200, b"a")
        connection.expect_eof(timeout=2.0)

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/one"))
        check_response(connection.response(), 200, b"one")
        connection.expect_eof(timeout=2.0)


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
    certificate = fixture / "localhost-cert.pem"
    key = fixture / "localhost-key.pem"
    with tempfile.TemporaryDirectory(prefix="strut-http-persistence-") as temporary:
        root = Path(temporary)
        port = available_port()
        executable = compile_program(compiler, root, "persistence-server", source(port))
        server = subprocess.Popen([executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            wait_until_listening(port, server)
            exercise(port, lambda: socket.create_connection(("127.0.0.1", port), timeout=5))
            with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
                connection = HttpConnection(raw)
                connection.send(request("/stop", extra="Connection: close\r\n"))
                check_response(connection.response(), 200, b"stopped")
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"persistence server failed: {stdout!r} {stderr!r}")
        finally:
            if server.poll() is None:
                server.kill(); server.wait()

        tls_port = available_port()
        tls_executable = compile_program(compiler, root, "persistence-tls-server", source(tls_port, True))
        tls_server = subprocess.Popen([tls_executable, certificate, key], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        context = ssl.create_default_context(cafile=str(certificate))
        try:
            wait_until_listening(tls_port, tls_server)
            exercise(tls_port, lambda: context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost"))
            with context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost") as raw:
                connection = HttpConnection(raw)
                connection.send(request("/stop", extra="Connection: close\r\n"))
                check_response(connection.response(), 200, b"stopped")
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"TLS persistence server failed: {stdout!r} {stderr!r}")
        finally:
            if tls_server.poll() is None:
                tls_server.kill(); tls_server.wait()
    print("HTTP persistence certification: HTTP/1.1 and HTTP/1.0 reuse, pipelining, body carry-over, HEAD, close policy, idle timeout, and TLS parity passed")


if __name__ == "__main__":
    main()
