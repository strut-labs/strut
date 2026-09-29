#!/usr/bin/env python3
"""Certify bounded static-file streaming and single byte ranges."""

from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile

from http_persistence_certification import HttpConnection
from http_response_stream_certification import available_port, compile_program, wait_until_listening


def source(port, asset, empty, large, unicode_asset, directory, tls=False):
    signature = (
        "function main(string command, string[] args) -> int : (NetworkError, TlsError, FilesystemError)"
        if tls
        else "function main() -> void : (NetworkError, FilesystemError)"
    )
    prefix = "if (args.length != 2) { return 2; }" if tls else ""
    listen = (
        f'app.listen_tls("127.0.0.1", {port}, args[0], args[1]);'
        if tls
        else f'app.listen("127.0.0.1", {port});'
    )
    suffix = "return 0;" if tls else ""
    return f'''{signature} {{
    {prefix}
    app := http_server();
    app.timeouts(1000, 1000, 1000, 500);
    app.get_stream("/file", (http_request request, http_response_writer writer) => {{
        http_serve_file(request, writer, "{asset}");
    }});
    app.get_stream("/custom", (http_request request, http_response_writer writer) => {{
        http_serve_file(request, writer, "{asset}", "application/x-strut-test");
    }});
    app.get_stream("/empty", (http_request request, http_response_writer writer) => {{
        http_serve_file(request, writer, "{empty}");
    }});
    app.get_stream("/large", (http_request request, http_response_writer writer) => {{
        http_serve_file(request, writer, "{large}");
    }});
    app.get_stream("/unicode", (http_request request, http_response_writer writer) => {{
        http_serve_file(request, writer, "{unicode_asset}");
    }});
    app.get_stream("/status", (http_request request, http_response_writer writer) => {{
        writer.status(404); http_serve_file(request, writer, "{asset}");
    }});
    app.get_stream("/directory", (http_request request, http_response_writer writer) => {{
        try {{ http_serve_file(request, writer, "{directory}"); }}
        catch (FilesystemError err) {{ writer.content_length(11); writer.write("not-regular"); }}
    }});
    app.get_stream("/nul", (http_request request, http_response_writer writer) => {{
        bytes nul := [0];
        try {{ http_serve_file(request, writer, nul.to_string()); }}
        catch (FilesystemError err) {{ writer.content_length(8); writer.write("bad-path"); }}
    }});
    app.get_stream("/invalid-utf8", (http_request request, http_response_writer writer) => {{
        bytes invalid := [255];
        try {{ http_serve_file(request, writer, invalid.to_string()); }}
        catch (FilesystemError err) {{ writer.content_length(8); writer.write("bad-path"); }}
    }});
    app.get_stream("/missing", (http_request request, http_response_writer writer) => {{
        try {{ http_serve_file(request, writer, "{asset}.missing"); }}
        catch (FilesystemError err) {{ writer.content_length(7); writer.write("missing"); }}
    }});
    app.get("/stop", (http_request request) => {{ app.stop(); return http_text("stopped"); }});
    {listen}
    {suffix}
}}
'''


def request(path, method="GET", extra=""):
    return f"{method} {path} HTTP/1.1\r\nHost: localhost\r\n{extra}\r\n".encode()


def fields(response, name):
    return [value for field, value in response[1] if field == name.lower()]


def require(response, status, body, length=None):
    if response[0] != status or response[2] != body:
        raise RuntimeError(f"unexpected file response: {response!r}")
    if length is not None and fields(response, b"content-length") != [str(length).encode()]:
        raise RuntimeError(f"unexpected content length: {response!r}")


def exercise(opener, data, large_data):
    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(request("/file"))
        full = connection.response()
        require(full, 200, data, len(data))
        if fields(full, b"accept-ranges") != [b"bytes"] or fields(full, b"content-type") != [b"application/octet-stream"]:
            raise RuntimeError(f"missing file metadata: {full!r}")

        cases = [
            ("bytes=10-19", data[10:20], b"bytes 10-19/1024"),
            ("bytes=1000-", data[1000:], b"bytes 1000-1023/1024"),
            ("bytes=-5", data[-5:], b"bytes 1019-1023/1024"),
            ("bytes=1020-9999", data[1020:], b"bytes 1020-1023/1024"),
            ("bytes=-9999", data, b"bytes 0-1023/1024"),
            ("bytes=42-42", data[42:43], b"bytes 42-42/1024"),
        ]
        for header, expected, content_range in cases:
            connection.send(request("/file", extra=f"Range: {header}\r\n"))
            response = connection.response()
            require(response, 206, expected, len(expected))
            if fields(response, b"content-range") != [content_range]:
                raise RuntimeError(f"unexpected Content-Range: {response!r}")

        for header in ("bytes=1024-", "bytes=-0", "bytes=20-10"):
            connection.send(request("/file", extra=f"Range: {header}\r\n"))
            response = connection.response()
            require(response, 416, b"", 0)
            if fields(response, b"content-range") != [b"bytes */1024"]:
                raise RuntimeError(f"unexpected unsatisfied range: {response!r}")

        for header in ("items=0-1", "bytes=abc", "bytes=0-1,3-4", "bytes= 0-1"):
            connection.send(request("/file", extra=f"Range: {header}\r\n"))
            require(connection.response(), 200, data, len(data))

        connection.send(request("/file", extra="Range: bytes=18446744073709551616-\r\n"))
        require(connection.response(), 416, b"", 0)
        connection.send(request("/file", extra="Range: bytes=10-18446744073709551616\r\n"))
        require(connection.response(), 206, data[10:], len(data) - 10)
        connection.send(request("/file", extra="Range: bytes=-18446744073709551616\r\n"))
        require(connection.response(), 206, data, len(data))

        connection.send(request("/large"))
        require(connection.response(), 200, large_data, len(large_data))

        connection.send(request("/empty", extra="Range: bytes=0-0\r\n"))
        empty = connection.response()
        require(empty, 416, b"", 0)
        if fields(empty, b"content-range") != [b"bytes */0"]:
            raise RuntimeError(f"unexpected empty-file range: {empty!r}")

        connection.send(request("/custom"))
        custom = connection.response()
        require(custom, 200, data, len(data))
        if fields(custom, b"content-type") != [b"application/x-strut-test"]:
            raise RuntimeError(f"custom content type lost: {custom!r}")

        connection.send(request("/unicode"))
        require(connection.response(), 200, data, len(data))
        connection.send(request("/status"))
        require(connection.response(), 200, data, len(data))
        connection.send(request("/directory"))
        require(connection.response(), 200, b"not-regular", 11)
        connection.send(request("/nul"))
        require(connection.response(), 200, b"bad-path", 8)
        connection.send(request("/invalid-utf8"))
        require(connection.response(), 200, b"bad-path", 8)

        connection.send(request("/file", method="HEAD", extra="Range: bytes=10-19\r\n"))
        head = connection.response(method=b"HEAD")
        require(head, 200, b"", len(data))

        connection.send(request("/missing"))
        require(connection.response(), 200, b"missing", 7)

    dropped = opener()
    dropped.sendall(request("/large"))
    dropped.recv(256)
    dropped.close()
    with opener() as raw:
        healthy = HttpConnection(raw)
        healthy.send(request("/custom"))
        require(healthy.response(), 200, data, len(data))


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    certificate = Path("tests/fixtures/tls/localhost-cert.pem").resolve()
    private_key = Path("tests/fixtures/tls/localhost-key.pem").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-file-") as temporary:
        root = Path(temporary)
        data = bytes(range(256)) * 4
        large_data = bytes(range(251)) * 8193
        asset = root / "asset.bin"
        empty = root / "empty.bin"
        large = root / "large.bin"
        unicode_asset = root / ("unicode-" + chr(0xE9) + ".bin")
        asset.write_bytes(data)
        empty.write_bytes(b"")
        large.write_bytes(large_data)
        unicode_asset.write_bytes(data)

        port = available_port()
        executable = compile_program(str(compiler), root, "file-server", source(port, asset.as_posix(), empty.as_posix(), large.as_posix(), unicode_asset.as_posix(), root.as_posix()))
        process = subprocess.Popen([executable], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_until_listening(port, process)
            exercise(lambda: socket.create_connection(("127.0.0.1", port), timeout=5), data, large_data)
            with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
                connection = HttpConnection(raw)
                connection.send(request("/stop", extra="Connection: close\r\n"))
                connection.response()
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

        tls_port = available_port()
        tls_executable = compile_program(str(compiler), root, "file-server-tls", source(tls_port, asset.as_posix(), empty.as_posix(), large.as_posix(), unicode_asset.as_posix(), root.as_posix(), True))
        tls_process = subprocess.Popen([tls_executable, certificate, private_key], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        context = ssl.create_default_context(cafile=str(certificate))
        try:
            wait_until_listening(tls_port, tls_process)
            exercise(lambda: context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost"), data, large_data)
            with context.wrap_socket(socket.create_connection(("127.0.0.1", tls_port), timeout=5), server_hostname="localhost") as raw:
                connection = HttpConnection(raw)
                connection.send(request("/stop", extra="Connection: close\r\n"))
                connection.response()
            tls_process.wait(timeout=10)
        finally:
            if tls_process.poll() is None:
                tls_process.kill()
                tls_process.wait()

    print("HTTP static file certification: binary streaming, ranges, HEAD, persistence, failures, and TLS passed")


if __name__ == "__main__":
    main()
