#!/usr/bin/env python3
"""Certify the RFC 6455 P4 upgrade and connection-ownership seam."""

from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time


KEY = "dGhlIHNhbXBsZSBub25jZQ=="
ACCEPT = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="


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


def opening(path="/ws/one", key=KEY, extra=b"", method="GET", version="HTTP/1.1",
            connection="Upgrade", upgrade="websocket", ws_version="13"):
    fields = [
        f"{method} {path} {version}",
        "Host: localhost",
        f"Connection: {connection}",
        f"Upgrade: {upgrade}",
        f"Sec-WebSocket-Version: {ws_version}",
        f"Sec-WebSocket-Key: {key}",
    ]
    return ("\r\n".join(fields) + "\r\n").encode() + extra + b"\r\n"


def exchange(port, request, context=None, retry=False, retry_empty=False):
    deadline = time.monotonic() + 8
    while True:
        raw = socket.socket()
        raw.settimeout(2)
        connection = None
        try:
            raw.connect(("127.0.0.1", port))
            connection = context.wrap_socket(raw, server_hostname="localhost") if context else raw
            connection.sendall(request)
            received = b""
            while True:
                chunk = connection.recv(4096)
                if not chunk:
                    break
                received += chunk
            if not received and retry_empty and time.monotonic() < deadline:
                time.sleep(0.03)
                continue
            return received
        except OSError:
            if not retry or time.monotonic() >= deadline:
                raise
            time.sleep(0.03)
        finally:
            if connection is not None:
                connection.close()
            else:
                raw.close()


def status(response):
    return int(response.split(b" ", 2)[1])


def require_version_rejection(response):
    head, body = response.split(b"\r\n\r\n", 1)
    lines = head.split(b"\r\n")
    if lines[0] != b"HTTP/1.1 426 Upgrade Required":
        raise RuntimeError(f"unexpected WebSocket version status: {lines[0]!r}")
    headers = {name.lower(): value.strip() for name, value in
               (line.split(b":", 1) for line in lines[1:])}
    if headers.get(b"sec-websocket-version") != b"13":
        raise RuntimeError(f"426 did not advertise WebSocket version 13: {response!r}")
    if headers.get(b"connection") != b"close" or b"transfer-encoding" in headers:
        raise RuntimeError(f"426 framing was ambiguous: {response!r}")
    if int(headers.get(b"content-length", b"-1")) != len(body):
        raise RuntimeError(f"426 Content-Length did not match its body: {response!r}")


def require_switch(response, protocol=None):
    expected = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {ACCEPT}\r\n"
    )
    if protocol is not None:
        expected += f"Sec-WebSocket-Protocol: {protocol}\r\n"
    expected = (expected + "\r\n").encode()
    if not response.startswith(expected):
        raise RuntimeError(f"unexpected WebSocket upgrade response: {response!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-websocket-") as temporary:
        root = Path(temporary)
        port = available_port()
        server_source = f"""error PolicyError {{ string message; }}
function main() -> int : (NetworkError, PolicyError) {{
    websocket initial;
    escaped := new(initial);
    app := http_server();
    app.websocket("/ws/:id", (http_request request, websocket socket) => {{
        println(request.params["id"]);
        socket.accept();
        return;
    }});
    app.websocket("/visible", (http_request request, websocket socket) => {{
        copy := socket;
        println(request.headers["origin"]);
        println(request.headers["sec-websocket-protocol"]);
        socket.accept("chat");
        copy.accept("chat");
        return;
    }});
    app.websocket("/reject", (http_request request, websocket socket) => {{
        token := request.cancellation;
        copy := socket;
        return;
    }});
    app.websocket("/error", (http_request request, websocket socket) => {{
        token := request.cancellation;
        copy := socket;
        throw PolicyError {{ message: "denied" }};
    }});
    app.websocket("/invalid-select", (http_request request, websocket socket) => {{
        token := request.cancellation;
        socket.accept("not-offered");
        return;
    }});
    app.websocket("/after-error", (http_request request, websocket socket) => {{
        token := request.cancellation;
        socket.accept();
        throw PolicyError {{ message: "after commit" }};
        return;
    }});
    app.websocket("/reaccept", (http_request request, websocket socket) => {{
        token := request.cancellation;
        socket.accept("chat");
        socket.accept("superchat");
        println("bad-reaccept");
        return;
    }});
    app.websocket("/escape", (http_request request, websocket socket) => {{
        token := request.cancellation;
        *escaped = socket;
        socket.accept();
        return;
    }});
    app.get("/http", (http_request request) => {{ return http_text("http"); }});
    app.listen("127.0.0.1", {port}, 32);
    try {{
        (*escaped).accept();
        return 2;
    }} catch (NetworkError err) {{
        println(err.message);
    }}
    return 0;
}}
"""
        executable = compile_program(compiler, root, "server", server_source)
        server = subprocess.Popen([executable], cwd=root, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        try:
            require_switch(exchange(port, opening(), retry=True))
            visible = opening(
                path="/visible",
                extra=(b"Origin: https://example.test\r\n"
                       b"Sec-WebSocket-Protocol: chat, superchat\r\n"),
            )
            visible_response = exchange(port, visible)
            require_switch(visible_response, "chat")

            extension_response = exchange(
                port,
                opening(extra=b"Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"),
            )
            require_switch(extension_response)
            if b"Sec-WebSocket-Extensions" in extension_response:
                raise RuntimeError("P4 unexpectedly accepted a WebSocket extension")
            require_switch(exchange(port, opening(upgrade="h2c/1.0, WebSocket")))

            invalid = [
                opening(method="POST"),
                opening(version="HTTP/1.0"),
                opening(connection="keep-alive"),
                opening(upgrade="h2c"),
                opening(key="short"),
                opening(key="dGhlIHNhbXBsZSBub25jZQ=A"),
                opening(extra=b"Content-Length: 1\r\n") + b"x",
                opening(extra=b"Transfer-Encoding: chunked\r\n"),
                opening(extra=f"Sec-WebSocket-Key: {KEY}\r\n".encode()),
                opening(upgrade="websocket,"),
                opening(upgrade="h2c/, websocket"),
                opening(upgrade="/1.0, websocket"),
                opening(upgrade="websocket/13"),
                opening(extra=b"Sec-WebSocket-Protocol: \r\n"),
                opening(extra=b"Sec-WebSocket-Protocol: chat,,superchat\r\n"),
                opening(extra=b"Sec-WebSocket-Protocol: chat, chat\r\n"),
                opening(extra=b"Sec-WebSocket-Protocol: chat/bad\r\n"),
                opening(extra=b"Sec-WebSocket-Extensions: permessage-deflate; =bad\r\n"),
                opening(extra=b"Sec-WebSocket-Extensions: permessage-deflate; mode=\"not token\"\r\n"),
            ]
            for request in invalid:
                response = exchange(port, request)
                if status(response) != 400:
                    raise RuntimeError(f"invalid opening was not rejected with 400: {response!r}")

            require_version_rejection(exchange(port, opening(ws_version="12")))

            generic = opening(path="/http", upgrade="h2c")
            if status(exchange(port, generic)) != 501:
                raise RuntimeError("generic unsupported upgrade did not retain 501")

            if status(exchange(port, opening(path="/reject"))) != 403:
                raise RuntimeError("handler return without acceptance did not produce 403")
            if status(exchange(port, opening(path="/error"))) != 500:
                raise RuntimeError("precommit handler error did not produce 500")
            invalid_selection = opening(
                path="/invalid-select",
                extra=b"Sec-WebSocket-Protocol: chat\r\n",
            )
            if status(exchange(port, invalid_selection)) != 500:
                raise RuntimeError("unoffered subprotocol selection was not rejected before commitment")
            require_switch(exchange(port, opening(path="/after-error")))
            reaccept = opening(
                path="/reaccept",
                extra=b"Sec-WebSocket-Protocol: chat, superchat\r\n",
            )
            require_switch(exchange(port, reaccept), "chat")

            disconnected = socket.create_connection(("127.0.0.1", port), timeout=2)
            disconnected.sendall(opening())
            disconnected.close()
            require_switch(exchange(port, opening(path="/escape"), retry=True, retry_empty=True))
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or "WebSocket handle is no longer active" not in stdout:
                raise RuntimeError(f"escaped WebSocket remained usable ({server.returncode})\n{stdout}\n{stderr}")
            if "bad-reaccept" in stdout:
                raise RuntimeError("different-protocol reacceptance did not throw")
            if "one" not in stdout or "https://example.test" not in stdout or "chat, superchat" not in stdout:
                raise RuntimeError(f"route params or offered headers were not visible to handlers: {stdout!r}")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        tls_port = available_port()
        tls_source = f"""function main(string command, string[] args) -> int : (NetworkError, TlsError) {{
    app := http_server();
    app.websocket("/ws", (http_request request, websocket socket) => {{
        token := request.cancellation;
        socket.accept();
        return;
    }});
    app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1], 1);
    return 0;
}}
"""
        tls_executable = compile_program(compiler, root, "tls-server", tls_source)
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate = fixture / "localhost-cert.pem"
        private_key = fixture / "localhost-key.pem"
        context = ssl.create_default_context(cafile=str(certificate))
        tls_server = subprocess.Popen([tls_executable, certificate, private_key], cwd=root,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            require_switch(exchange(tls_port, opening(path="/ws"), context=context, retry=True))
            stdout, stderr = tls_server.communicate(timeout=10)
            if tls_server.returncode != 0:
                raise RuntimeError(f"TLS server failed ({tls_server.returncode})\n{stdout}\n{stderr}")
        finally:
            if tls_server.poll() is None:
                tls_server.kill()
                tls_server.wait()

        stop_port = available_port()
        stop_source = f"""function main() -> int : (NetworkError, ThreadError, TimeError, CancellationError) {{
    app := http_server();
    app.timeouts(3000, 3000, 3000, 1000);
    app.websocket("/wait", (http_request request, websocket socket) => {{
        socket.accept();
        request.cancellation.wait();
        request.cancellation.throw_if_cancelled();
        return;
    }});
    listener := thread(() => {{ app.listen("127.0.0.1", {stop_port}); }});
    while (!app.running()) {{ sleep_ms(5); }}
    sleep_ms(1500);
    app.stop();
    listener.join();
    return 0;
}}
"""
        stop_executable = compile_program(compiler, root, "stop-server", stop_source)
        stop_server = subprocess.Popen([stop_executable], cwd=root, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
        try:
            require_switch(exchange(stop_port, opening(path="/wait"), retry=True))
            stdout, stderr = stop_server.communicate(timeout=8)
            if stop_server.returncode != 0:
                raise RuntimeError(f"upgraded stop failed ({stop_server.returncode})\n{stdout}\n{stderr}")
        finally:
            if stop_server.poll() is None:
                stop_server.kill()
                stop_server.wait()

        race_port = available_port()
        race_source = f"""function main() -> int : (NetworkError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(3000, 3000, 3000, 1000);
    app.websocket("/race", (http_request request, websocket socket) => {{
        request.cancellation.wait();
        socket.accept();
        return;
    }});
    listener := thread(() => {{ app.listen("127.0.0.1", {race_port}); }});
    while (!app.running()) {{ sleep_ms(5); }}
    sleep_ms(1500);
    app.stop();
    listener.join();
    return 0;
}}
"""
        race_executable = compile_program(compiler, root, "accept-race-server", race_source)
        race_server = subprocess.Popen([race_executable], cwd=root, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
        try:
            require_switch(exchange(race_port, opening(path="/race"), retry=True))
            stdout, stderr = race_server.communicate(timeout=8)
            if race_server.returncode != 0:
                raise RuntimeError(f"stop/accept race failed ({race_server.returncode})\n{stdout}\n{stderr}")
        finally:
            if race_server.poll() is None:
                race_server.kill()
                race_server.wait()

        first, second = available_port(), available_port()
        repeated_source = f"""function main() -> int : NetworkError {{
    app := http_server();
    app.websocket("/ws", (http_request request, websocket socket) => {{
        token := request.cancellation;
        socket.accept();
        return;
    }});
    app.listen("127.0.0.1", {first}, 1);
    app.listen("127.0.0.1", {second}, 1);
    return 0;
}}
"""
        repeated_executable = compile_program(compiler, root, "repeated-server", repeated_source)
        repeated = subprocess.Popen([repeated_executable], cwd=root, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, text=True)
        try:
            require_switch(exchange(first, opening(path="/ws"), retry=True))
            require_switch(exchange(second, opening(path="/ws"), retry=True))
            stdout, stderr = repeated.communicate(timeout=10)
            if repeated.returncode != 0:
                raise RuntimeError(f"repeated lifecycle failed ({repeated.returncode})\n{stdout}\n{stderr}")
        finally:
            if repeated.poll() is None:
                repeated.kill()
                repeated.wait()

    print("HTTP WebSocket P4 certification: explicit policy acceptance, subprotocols, plaintext/TLS validation, ownership, stop races, invalidation, disconnect and repeated lifecycle passed")


if __name__ == "__main__":
    main()
