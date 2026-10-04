#!/usr/bin/env python3
"""Certify bounded streaming request bodies built on validated HTTP framing."""

from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time

from http_response_stream_certification import (
    available_port,
    compile_program,
    decode_chunked,
    field_values,
    require_status,
    split_response,
    wait_until_listening,
)


def raw_request(port, pieces, *, shutdown=False):
    pieces = list(pieces)
    for index, piece in enumerate(pieces):
        marker = piece.find(b"\r\n\r\n")
        if marker != -1 and b"\r\nConnection:" not in piece[:marker]:
            pieces[index] = piece[:marker] + b"\r\nConnection: close" + piece[marker:]
            break
    with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
        for piece in pieces:
            connection.sendall(piece)
            if len(pieces) > 1:
                time.sleep(0.002)
        if shutdown:
            connection.shutdown(socket.SHUT_WR)
        response = bytearray()
        while True:
            chunk = connection.recv(65536)
            if not chunk:
                return bytes(response)
            response.extend(chunk)


def decoded_response(response, status=200):
    line, fields, body = split_response(response)
    require_status(line, status)
    if field_values(fields, b"transfer-encoding") == [b"chunked"]:
        return decode_chunked(body)[0]
    return body


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    with tempfile.TemporaryDirectory(prefix="strut-http-request-stream-") as temporary:
        root = Path(temporary)
        executable = compile_program(
            compiler,
            root,
            "request-stream-server",
            f'''async function escaped_read(http_request_body body) -> void : NetworkError {{
    body.read_bytes(1);
    return;
}}

function main() -> void : (NetworkError, TimeError) {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(2000, 2000, 2000, 500);
    app.limits(32, 4096, 32, 4);
    app.post_request_stream("/echo", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ response.write_bytes(body.read_all_bytes()); }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/consume", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            data := body.read_all_bytes();
            response.content_length(data.length());
            response.write_bytes(data);
        }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/first", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            first := body.read_bytes(3);
            response.content_length(first.length());
            response.write_bytes(first);
        }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/zero", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            empty := body.read_bytes(0);
            if (empty.length() != 0 || body.eof()) {{ response.status(500); }}
            response.write_bytes(body.read_all_bytes());
        }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/close", (http_request request, http_request_body body, http_response_writer response) => {{
        body.close();
        try {{
            try {{ body.read_bytes(1); }} catch (NetworkError caught) {{
                if (caught.message != "") {{ response.write("closed"); }}
            }}
        }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/unread", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ response.write("unread"); }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/escaped", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            pending := escaped_read(body);
            sleep_ms(50);
            if (pending.valid()) {{ response.write("escaped"); }}
        }} catch (NetworkError caught) {{ }} catch (TimeError caught) {{ }}
    }});
    app.post_request_stream("/close-active", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            pending := escaped_read(body);
            sleep_ms(50);
            body.close();
        }} catch (NetworkError caught) {{ }} catch (TimeError caught) {{ }}
    }});
    app.post_request_stream("/catch-malformed", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            try {{ body.read_all_bytes(); }} catch (NetworkError caught) {{ response.write("wrong"); }}
        }} catch (NetworkError caught) {{ }}
    }});
    app.post("/buffered", (http_request request) => {{ return http_text(request.body); }});
    app.get("/health", (http_request request) => {{ return http_text("healthy"); }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    app.listen("127.0.0.1", {port});
}}
''',
        )
        server = subprocess.Popen(
            [executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(port, server)

            fixed_head = b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 7\r\n\r\n"
            fixed = raw_request(port, [fixed_head + b"a\x00", b"bc", b"def"])
            if decoded_response(fixed) != b"a\x00bcdef":
                raise RuntimeError("fixed-length streaming lost binary or fragmented bytes")

            with socket.create_connection(("127.0.0.1", port), timeout=5) as early:
                early.sendall(
                    b"POST /first HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\n\r\nabc"
                )
                early.settimeout(1)
                response = bytearray()
                while True:
                    chunk = early.recv(4096)
                    if not chunk:
                        break
                    response.extend(chunk)
                if decoded_response(bytes(response)) != b"abc":
                    raise RuntimeError("handler did not respond before the complete body arrived")

            chunked = raw_request(
                port,
                [
                    b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n",
                    b"3;name=value\r\na",
                    b"\x00b\r\n2\r\ncd\r\n",
                    b"0\r\n\r\n",
                ],
            )
            if decoded_response(chunked) != b"a\x00bcd":
                raise RuntimeError("fragmented chunked body or extension decoding failed")

            quoted_tab = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n",
                    b"1;x=\"a\tb\"\r\nx\r\n0\r\n\r\n",
                ],
            )
            if decoded_response(quoted_tab) != b"x":
                raise RuntimeError("valid quoted chunk extension was rejected")

            one_byte_pieces = [
                b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            ] + [bytes([byte]) for byte in b"1\r\na\r\n1\r\nb\r\n0\r\n\r\n"]
            if decoded_response(raw_request(port, one_byte_pieces)) != b"ab":
                raise RuntimeError("chunk framing split across writes was not decoded")

            zero = raw_request(
                port,
                [b"POST /zero HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ndata"],
            )
            if decoded_response(zero) != b"data":
                raise RuntimeError("zero-size read changed body EOF or data")

            closed = raw_request(
                port,
                [b"POST /close HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ndata"],
            )
            if decoded_response(closed) != b"closed":
                raise RuntimeError("closed request reader did not reject later reads")

            buffered = raw_request(
                port,
                [
                    b"POST /buffered HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n",
                    b"4\r\ncomp\r\n3\r\nat!\r\n0\r\n\r\n",
                ],
            )
            if decoded_response(buffered) != b"compat!":
                raise RuntimeError("buffered request.body did not use the shared chunk decoder")

            unread = raw_request(
                port,
                [
                    b"POST /unread HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\n\r\n",
                    b"partial",
                ],
            )
            if decoded_response(unread) != b"unread":
                raise RuntimeError("unread-body close policy did not complete the response")

            escaped = raw_request(
                port,
                [b"POST /escaped HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n"],
            )
            if escaped:
                raise RuntimeError("response committed while an escaped body read was active")

            closed_active = raw_request(
                port,
                [b"POST /close-active HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n"],
            )
            if closed_active:
                raise RuntimeError("active request body close did not terminate the connection")

            caught_malformed = raw_request(
                port,
                [
                    b"POST /catch-malformed HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n",
                    b"Z\r\n",
                ],
                shutdown=True,
            )
            if decoded_response(caught_malformed, 400) != b"Bad Request":
                raise RuntimeError("handler bypassed malformed body failure with an application response")

            malformed = [
                b"Z\r\n",
                b"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n",
                b"3\r\nabcX\n0\r\n\r\n",
                b"3\r\nab",
                b"21\r\n" + b"x" * 33 + b"\r\n0\r\n\r\n",
                b"0\r\nX-Trailer: value\r\n\r\n",
                b"1;bad =value\r\nx\r\n0\r\n\r\n",
                b"1;x=\"a\\\nb\"\r\nx\r\n0\r\n\r\n",
                b"1;x=\"a\\\rb\"\r\nx\r\n0\r\n\r\n",
            ]
            for index, encoded in enumerate(malformed):
                request = (
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                    + encoded
                )
                response = raw_request(port, [request], shutdown=index == 3)
                line, _, _ = split_response(response)
                require_status(line, 400)
                healthy = raw_request(
                    port, [b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n"]
                )
                if decoded_response(healthy) != b"healthy":
                    raise RuntimeError(f"server did not recover after malformed chunk case {index}")

            excessive_framing = b"".join(
                b"1;pad=" + b"x" * 140 + b"\r\na\r\n" for _ in range(30)
            ) + b"0\r\n\r\n"
            response = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                    + excessive_framing
                ],
            )
            require_status(split_response(response)[0], 400)

            conflict = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 1\r\n\r\n0\r\n\r\n"
                ],
            )
            require_status(split_response(conflict)[0], 400)
            unsupported = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip, chunked\r\n\r\n0\r\n\r\n"
                ],
            )
            require_status(split_response(unsupported)[0], 501)
            expectation = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\nExpect: 100-continue\r\n\r\n"
                ],
            )
            require_status(split_response(expectation)[0], 417)

            extra = raw_request(
                port,
                [
                    b"POST /consume HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\nGET /health HTTP/1.1\r\nHost: localhost\r\n\r\n"
                ],
            )
            if decoded_response(extra) != b"":
                raise RuntimeError("bytes after terminal chunk entered the decoded request body")

            stopped = raw_request(
                port, [b"GET /stop HTTP/1.1\r\nHost: localhost\r\n\r\n"]
            )
            if decoded_response(stopped) != b"stopped":
                raise RuntimeError("request streaming server did not stop cleanly")
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(
                    f"request streaming server failed: {server.returncode} {stdout!r} {stderr!r}"
                )
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        tls_port = available_port()
        tls_executable = compile_program(
            compiler,
            root,
            "request-stream-tls-server",
            f'''async function tls_escaped_read(http_request_body body) -> void : NetworkError {{
    body.read_bytes(1);
    return;
}}

function main(string command, string[] args) -> int : (NetworkError, TlsError, TimeError) {{
    if (args.length != 2) {{ return 2; }}
    app := http_server();
    app.limits(32, 4096, 16, 3);
    app.post_request_stream("/echo", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ response.write_bytes(body.read_all_bytes()); }} catch (NetworkError caught) {{ }}
    }});
    app.post_request_stream("/escaped", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            pending := tls_escaped_read(body);
            sleep_ms(50);
            if (pending.valid()) {{ response.write("escaped"); }}
        }} catch (NetworkError caught) {{ }} catch (TimeError caught) {{ }}
    }});
    app.post_request_stream("/close-active", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{
            pending := tls_escaped_read(body);
            sleep_ms(50);
            body.close();
        }} catch (NetworkError caught) {{ }} catch (TimeError caught) {{ }}
    }});
    app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1], 4);
    return 0;
}}
''',
        )
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate = fixture / "localhost-cert.pem"
        key = fixture / "localhost-key.pem"
        tls_server = subprocess.Popen(
            [tls_executable, certificate, key],
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
                    secure.sendall(
                        b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n3\r\ntls\r\n0\r\n\r\n"
                    )
                    response = bytearray()
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        response.extend(chunk)
            if decoded_response(bytes(response)) != b"tls":
                raise RuntimeError("TLS request streaming diverged from plaintext")
            with socket.create_connection(("127.0.0.1", tls_port), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as secure:
                    secure.sendall(
                        b"POST /escaped HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n"
                    )
                    escaped_response = bytearray()
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        escaped_response.extend(chunk)
            if escaped_response:
                raise RuntimeError("TLS response committed while an escaped body read was active")
            with socket.create_connection(("127.0.0.1", tls_port), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as secure:
                    secure.sendall(
                        b"POST /close-active HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n"
                    )
                    close_response = bytearray()
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        close_response.extend(chunk)
            if close_response:
                raise RuntimeError("TLS active request body close did not terminate the connection")
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0 or stdout or stderr:
                raise RuntimeError("TLS request streaming server did not exit cleanly")
        finally:
            if tls_server.poll() is None:
                tls_server.kill()
                tls_server.wait()

        shutdown_port = available_port()
        shutdown_executable = compile_program(
            compiler,
            root,
            "request-stream-shutdown-server",
            f'''function main() -> void : NetworkError {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(5000, 5000, 5000, 300);
    app.limits(32, 4096, 16, 3);
    app.post_request_stream("/blocked", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ response.write_bytes(body.read_bytes(1)); }} catch (NetworkError caught) {{ }}
    }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    app.listen("127.0.0.1", {shutdown_port});
}}
''',
        )
        shutdown_server = subprocess.Popen(
            [shutdown_executable],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        blocked = None
        try:
            wait_until_listening(shutdown_port, shutdown_server)
            blocked = socket.create_connection(("127.0.0.1", shutdown_port), timeout=5)
            blocked.sendall(
                b"POST /blocked HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n"
            )
            stopped = raw_request(
                shutdown_port, [b"GET /stop HTTP/1.1\r\nHost: localhost\r\n\r\n"]
            )
            if decoded_response(stopped) != b"stopped":
                raise RuntimeError("shutdown response was lost with a blocked body reader")
            stdout, stderr = shutdown_server.communicate(timeout=5)
            if shutdown_server.returncode != 0 or stdout or stderr:
                raise RuntimeError("shutdown did not interrupt the blocked request body reader")
        finally:
            if blocked is not None:
                blocked.close()
            if shutdown_server.poll() is None:
                shutdown_server.kill()
                shutdown_server.wait()

    print(
        "HTTP request streaming certification: fixed and chunked bodies, fragmentation, limits, "
        "malformed framing, active close, escaped reads, buffered compatibility, and TLS parity passed"
    )


if __name__ == "__main__":
    main()
