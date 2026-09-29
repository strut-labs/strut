#!/usr/bin/env python3
"""Certify cancellation-aware NDJSON records over the shared response writer."""

from pathlib import Path
import json
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import time

from http_persistence_certification import HttpConnection
from http_response_stream_certification import available_port, compile_program, split_response, wait_until_listening


def source(port, tls=False):
    signature = (
        "function main(string command, string[] args) -> int : (HttpError, NetworkError, TlsError, TimeError)"
        if tls
        else "function main() -> void : (HttpError, NetworkError, TimeError)"
    )
    prefix = "if (args.length != 2) { return 2; }" if tls else ""
    listen = (
        f'app.listen_tls("127.0.0.1", {port}, args[0], args[1]);'
        if tls
        else f'app.listen("127.0.0.1", {port});'
    )
    suffix = "return 0;" if tls else ""
    payload = "x" * 16384
    rounds = ", ".join([str(index) for index in range(128)])
    return f'''{signature} {{
    {prefix}
    app := http_server();
    app.timeouts(1000, 1000, 1000, 500);
    app.get_stream("/records", (http_request request, http_response_writer writer) => {{
        http_write_ndjson(request, writer, {{"index": 1, "text": "line\\none"}});
        http_write_ndjson(request, writer, {{"index": 2, "ok": true}});
        http_write_ndjson(request, writer, {{"index": 3, "value": null}});
    }});
    app.get_stream("/prompt", (http_request request, http_response_writer writer) => {{
        http_write_ndjson(request, writer, {{"phase": "first"}});
        sleep_ms(1500);
        http_write_ndjson(request, writer, {{"phase": "second"}});
    }});
    app.get_stream("/many", (http_request request, http_response_writer writer) => {{
        for (round : [{rounds}]) {{
            http_write_ndjson(request, writer, {{"round": round, "payload": "{payload}"}});
        }}
    }});
    app.get_stream("/invalid-utf8", (http_request request, http_response_writer writer) => {{
        bytes invalid := [255];
        try {{ http_write_ndjson(request, writer, {{"value": invalid.to_string()}}); }}
        catch (HttpError err) {{ writer.content_length(7); writer.write("invalid"); }}
    }});
    app.get_stream("/invalid-number", (http_request request, http_response_writer writer) => {{
        try {{ http_write_ndjson(request, writer, {{"value": 0.0 / 0.0}}); }}
        catch (HttpError err) {{ writer.content_length(7); writer.write("invalid"); }}
    }});
    app.get_stream("/cancel", (http_request request, http_response_writer writer) => {{
        app.stop();
        try {{ http_write_ndjson(request, writer, {{"unexpected": true}}); }}
        catch (NetworkError err) {{ return; }}
    }});
    app.get("/health", (http_request request) => {{ return http_text("ok"); }});
    app.get("/stop", (http_request request) => {{ app.stop(); return http_text("stopped"); }});
    {listen}
    {suffix}
}}
'''


def request(path, version="HTTP/1.1", extra=""):
    return f"GET {path} {version}\r\nHost: localhost\r\n{extra}\r\n".encode()


def values(fields, name):
    return [value for field, value in fields if field == name.lower()]


def assert_records(response):
    if response[0] != 200 or values(response[1], b"content-type") != [b"application/x-ndjson"]:
        raise RuntimeError(f"unexpected NDJSON metadata: {response!r}")
    body = response[2]
    if not body.endswith(b"\n") or b"\r" in body:
        raise RuntimeError(f"invalid NDJSON delimiters: {body!r}")
    lines = body.splitlines()
    try:
        decoded = [json.loads(line) for line in lines]
    except json.JSONDecodeError as error:
        raise RuntimeError(f"invalid NDJSON record bytes: {body!r}") from error
    expected = [
        {"index": 1, "text": "line\none"},
        {"index": 2, "ok": True},
        {"index": 3, "value": None},
    ]
    if decoded != expected:
        raise RuntimeError(f"unexpected NDJSON records: {decoded!r}")


def prompt_and_disconnect(opener):
    raw = opener()
    raw.settimeout(1.0)
    started = time.monotonic()
    raw.sendall(request("/prompt"))
    received = bytearray()
    while b'"phase":"first"' not in received:
        chunk = raw.recv(4096)
        if not chunk:
            raise RuntimeError("NDJSON prompt stream closed before the first record")
        received.extend(chunk)
    if time.monotonic() - started >= 1.2:
        raise RuntimeError("first NDJSON record was not flushed promptly")
    raw.close()


def wait_success(process):
    stdout, stderr = process.communicate(timeout=30)
    if process.returncode != 0:
        raise RuntimeError(f"NDJSON server exited with {process.returncode}: {stdout!r} {stderr!r}")


def exercise(opener):
    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/records"))
        assert_records(connection.response())
        connection.send(request("/health"))
        health = connection.response()
        if health[0] != 200 or health[2] != b"ok":
            raise RuntimeError(f"NDJSON connection was not reusable: {health!r}")
        connection.send(request("/records", extra="Connection: close\r\n"))
        assert_records(connection.response())

    with opener() as raw:
        raw.sendall(request("/records", "HTTP/1.0", "Connection: close\r\n"))
        response = bytearray()
        while True:
            chunk = raw.recv(65536)
            if not chunk:
                break
            response.extend(chunk)
        status, fields, body = split_response(bytes(response))
        if status != b"HTTP/1.0 200 OK":
            raise RuntimeError(f"unexpected HTTP/1.0 NDJSON status: {status!r}")
        assert_records((200, fields, body))

    prompt_and_disconnect(opener)
    dropped = opener()
    dropped.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    dropped.sendall(request("/many"))
    dropped.recv(512)
    dropped.close()
    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/health"))
        health = connection.response()
        if health[0] != 200 or health[2] != b"ok":
            raise RuntimeError(f"server did not recover after NDJSON disconnect: {health!r}")
        connection.send(request("/invalid-utf8"))
        invalid_utf8 = connection.response()
        if invalid_utf8[0] != 200 or invalid_utf8[2] != b"invalid":
            raise RuntimeError(f"invalid UTF-8 JSON was not rejected: {invalid_utf8!r}")
        connection.send(request("/invalid-number"))
        invalid_number = connection.response()
        if invalid_number[0] != 200 or invalid_number[2] != b"invalid":
            raise RuntimeError(f"non-finite JSON number was not rejected: {invalid_number!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    certificate = Path("tests/fixtures/tls/localhost-cert.pem").resolve()
    private_key = Path("tests/fixtures/tls/localhost-key.pem").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-ndjson-") as temporary:
        root = Path(temporary)
        port = available_port()
        executable = compile_program(str(compiler), root, "ndjson-server", source(port))
        process = subprocess.Popen([executable], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_until_listening(port, process)
            exercise(lambda: socket.create_connection(("127.0.0.1", port), timeout=5))
            with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
                connection = HttpConnection(raw)
                connection.send(request("/cancel", extra="Connection: close\r\n"))
                cancelled = connection.response()
                if cancelled[2] != b"":
                    raise RuntimeError(f"cancelled NDJSON helper emitted a record: {cancelled!r}")
            wait_success(process)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

        tls_port = available_port()
        tls_executable = compile_program(str(compiler), root, "ndjson-server-tls", source(tls_port, True))
        tls_process = subprocess.Popen([tls_executable, certificate, private_key], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        context = ssl.create_default_context(cafile=str(certificate))
        try:
            wait_until_listening(tls_port, tls_process)
            exercise(lambda: context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost"))
            with context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost") as raw:
                connection = HttpConnection(raw)
                connection.send(request("/cancel", extra="Connection: close\r\n"))
                cancelled = connection.response()
                if cancelled[2] != b"":
                    raise RuntimeError(f"cancelled TLS NDJSON helper emitted a record: {cancelled!r}")
            wait_success(tls_process)
        finally:
            if tls_process.poll() is None:
                tls_process.kill()
                tls_process.wait()

    print("HTTP NDJSON certification: record framing, prompt flush, persistence, cancellation failures, and TLS passed")


if __name__ == "__main__":
    main()
