#!/usr/bin/env python3
"""Certify safe, transport-owned HTTP response-head serialization."""

from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def available_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def wait_until_listening(port, process):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            stdout, stderr = process.communicate()
            raise RuntimeError(
                f"response server exited during startup: {process.returncode} {stdout!r} {stderr!r}"
            )
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                return
        except OSError:
            time.sleep(0.02)
    raise RuntimeError("response server did not start")


def raw_request(port, path, body=b""):
    payload = (
        f"POST {path} HTTP/1.1\r\nHost: localhost\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n".encode()
        + body
    )
    with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
        connection.sendall(payload)
        response = bytearray()
        while True:
            chunk = connection.recv(4096)
            if not chunk:
                break
            response.extend(chunk)
        return bytes(response)


def assert_safe_response(response, expected_status, expected_body):
    head, separator, body = response.partition(b"\r\n\r\n")
    if not separator or not head.startswith(f"HTTP/1.1 {expected_status} ".encode()):
        raise RuntimeError(f"unexpected response status or shape: {response!r}")
    lines = head.split(b"\r\n")
    lower_names = [line.split(b":", 1)[0].lower() for line in lines[1:] if b":" in line]
    for owned in (b"content-type", b"connection"):
        if lower_names.count(owned) != 1:
            raise RuntimeError(f"transport field {owned!r} was not singular: {response!r}")
    expected_lengths = 0 if expected_status in (204, 304) else 1
    if lower_names.count(b"content-length") != expected_lengths:
        raise RuntimeError(f"content length framing was invalid: {response!r}")
    length_values = [
        line.split(b":", 1)[1].strip()
        for line in lines[1:]
        if line.lower().startswith(b"content-length:")
    ]
    if expected_lengths and length_values != [str(len(expected_body)).encode()]:
        raise RuntimeError(f"content length value was invalid: {response!r}")
    if b"transfer-encoding" in lower_names:
        raise RuntimeError(f"transfer encoding escaped response validation: {response!r}")
    if body != expected_body:
        raise RuntimeError(f"unexpected response body: {body!r}, expected {expected_body!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    invalid = [
        ("/bad-name-space", b""),
        ("/bad-name-colon", b""),
        ("/bad-name-crlf", b""),
        ("/bad-value", b""),
        ("/reflect", b"safe\r\nInjected: yes"),
        ("/reflect", b"safe\x00unsafe"),
        ("/reflect", b"safe\x01unsafe"),
        ("/content-length", b""),
        ("/content-length-lower", b""),
        ("/transfer-encoding", b""),
        ("/transfer-encoding-lower", b""),
        ("/connection", b""),
        ("/connection-lower", b""),
        ("/content-type-header", b""),
        ("/bad-content-type", b""),
        ("/bad-media-type", b""),
        ("/duplicate", b""),
        ("/body-forbidden", b""),
        ("/reset-content-body", b""),
        ("/bad-status-low", b""),
        ("/bad-status-high", b""),
    ]
    admitted = 1 + 3 + len(invalid) * 2
    source = f'''include <map>;

function response_with_header(string name, string value) -> http_server_response {{
    response := http_text("unsafe");
    map<string, string> headers;
    headers.insert(name, value);
    response.headers = headers;
    return response;
}}

function main() -> void : NetworkError {{
    app := http_server();
    app.timeouts(5000, 5000, 5000, 2000);
    app.limits(1024, 4096, 16, 2);
    app.post("/health", (http_request request) => {{ return http_text("healthy"); }});
    app.post("/safe", (http_request request) => {{
        response := response_with_header("X-Test", "safe");
        response.status = 201;
        response.content_type = "application/x-strut-test";
        response.body = "safe";
        return response;
    }});
    app.post("/no-content", (http_request request) => {{
        response := http_text("");
        response.status = 204;
        return response;
    }});
    app.post("/reset-content", (http_request request) => {{
        response := http_text("");
        response.status = 205;
        return response;
    }});
    app.post("/bad-name-space", (http_request request) => {{ return response_with_header("Bad Name", "value"); }});
    app.post("/bad-name-colon", (http_request request) => {{ return response_with_header("Bad:Name", "value"); }});
    app.post("/bad-name-crlf", (http_request request) => {{ return response_with_header("X-Test\\r\\nInjected", "value"); }});
    app.post("/bad-value", (http_request request) => {{ return response_with_header("X-Test", "safe\\r\\nInjected: yes"); }});
    app.post("/reflect", (http_request request) => {{ return response_with_header("X-Reflected", request.body); }});
    app.post("/content-length", (http_request request) => {{ return response_with_header("Content-Length", "999"); }});
    app.post("/content-length-lower", (http_request request) => {{ return response_with_header("content-length", "999"); }});
    app.post("/transfer-encoding", (http_request request) => {{ return response_with_header("Transfer-Encoding", "chunked"); }});
    app.post("/transfer-encoding-lower", (http_request request) => {{ return response_with_header("transfer-encoding", "chunked"); }});
    app.post("/connection", (http_request request) => {{ return response_with_header("Connection", "keep-alive"); }});
    app.post("/connection-lower", (http_request request) => {{ return response_with_header("connection", "keep-alive"); }});
    app.post("/content-type-header", (http_request request) => {{ return response_with_header("Content-Type", "text/html"); }});
    app.post("/bad-content-type", (http_request request) => {{
        response := http_text("unsafe");
        response.content_type = "text/plain\\r\\nInjected: yes";
        return response;
    }});
    app.post("/bad-media-type", (http_request request) => {{
        response := http_text("unsafe");
        response.content_type = "not a media type";
        return response;
    }});
    app.post("/duplicate", (http_request request) => {{
        response := http_text("unsafe");
        map<string, string> headers;
        headers.insert("X-Test", "one");
        headers.insert("x-test", "two");
        response.headers = headers;
        return response;
    }});
    app.post("/body-forbidden", (http_request request) => {{
        response := http_text("unsafe");
        response.status = 304;
        return response;
    }});
    app.post("/reset-content-body", (http_request request) => {{
        response := http_text("unsafe");
        response.status = 205;
        return response;
    }});
    app.post("/bad-status-low", (http_request request) => {{
        response := http_text("unsafe");
        response.status = 199;
        return response;
    }});
    app.post("/bad-status-high", (http_request request) => {{
        response := http_text("unsafe");
        response.status = 600;
        return response;
    }});
    app.listen("127.0.0.1", {port}, {admitted});
}}
'''
    with tempfile.TemporaryDirectory(prefix="strut-http-response-") as temporary:
        root = Path(temporary)
        program = root / "response-server.p"
        executable = root / ("response-server.exe" if sys.platform == "win32" else "response-server")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
        server = subprocess.Popen(
            [executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(port, server)
            safe = raw_request(port, "/safe")
            assert_safe_response(safe, 201, b"safe")
            if b"X-Test: safe\r\n" not in safe or b"Content-Type: application/x-strut-test\r\n" not in safe:
                raise RuntimeError(f"valid response metadata was not preserved: {safe!r}")
            assert_safe_response(raw_request(port, "/no-content"), 204, b"")
            assert_safe_response(raw_request(port, "/reset-content"), 205, b"")
            for path, body in invalid:
                rejected = raw_request(port, path, body)
                assert_safe_response(rejected, 500, b"Internal Server Error")
                if b"Injected:" in rejected or b"unsafe" in rejected:
                    raise RuntimeError(f"attacker-controlled metadata reached the wire: {rejected!r}")
                assert_safe_response(raw_request(port, "/health"), 200, b"healthy")
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(
                    f"response server failed: exit={server.returncode} stdout={stdout!r} stderr={stderr!r}"
                )
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
    print(f"HTTP response certification: valid metadata and {len(invalid)} rejection cases passed")


if __name__ == "__main__":
    main()
