#!/usr/bin/env python3
"""Record the bounded, buffered HTTP server baseline without changing it."""

from concurrent.futures import ThreadPoolExecutor
import http.client
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


def wait_until_listening(port):
    deadline = time.monotonic() + 5
    while True:
        try:
            probe = socket.create_connection(("127.0.0.1", port), timeout=0.2)
            probe.close()
            return
        except OSError:
            if time.monotonic() >= deadline:
                raise RuntimeError("backend baseline server did not start")
            time.sleep(0.03)


def request(port, method="GET", path="/get", body=None, headers=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    connection.request(method, path, body=body, headers=headers or {})
    response = connection.getresponse()
    result = response.status, response.read()
    connection.close()
    return result


def raw_request(port, payload):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
        connection.sendall(payload)
        connection.shutdown(socket.SHUT_WR)
        response = bytearray()
        while True:
            chunk = connection.recv(4096)
            if not chunk:
                break
            response.extend(chunk)
        return bytes(response)


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    restart_port = available_port()
    source = """function main() -> int : (NetworkError, TimeError) {
    app := http_server();
    app.timeouts(2000, 2000, 2000, 2000);
    app.limits(32, 1024, 8, 2);
    app.get("/get", (http_request request) => { return http_text("get"); });
    app.post("/post", (http_request request) => { return http_text(request.body); });
    app.listen("127.0.0.1", %d, 49);
    if (app.running()) { return 1; }
    return 0;
}
""" % port
    restart_source = """function main() -> int : (NetworkError, ThreadError, TimeError) {
    app := http_server();
    int count := 0;
    while (count < 10) {
        listener := thread(() => { app.listen("127.0.0.1", %d); });
        while (!app.running()) { sleep_ms(1); }
        app.stop();
        listener.join();
        if (app.running()) { return 1; }
        count++;
    }
    return 0;
}
""" % restart_port

    with tempfile.TemporaryDirectory(prefix="strut-backend-baseline-") as temporary:
        root = Path(temporary)
        program = root / "baseline-server.p"
        executable = root / ("baseline-server.exe" if sys.platform == "win32" else "baseline-server")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)

        server = subprocess.Popen(
            [executable], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        try:
            wait_until_listening(port)
            assert request(port) == (200, b"get")
            assert request(port, "POST", "/post", b"post") == (200, b"post")
            assert request(port, "GET", "/missing") == (404, b"Not Found")
            assert request(port, "POST", "/get") == (405, b"Method Not Allowed")
            assert request(port, "POST", "/post", b"x" * 64) == (413, b"Payload Too Large")

            oversized = raw_request(
                port,
                b"GET /get HTTP/1.1\r\nHost: localhost\r\nX-Baseline: "
                + b"x" * 1100
                + b"\r\n\r\n",
            )
            assert b" 431 " in oversized

            blockers = []
            for _ in range(2):
                blocked = socket.create_connection(("127.0.0.1", port), timeout=5)
                blocked.sendall(b"GET /get HTTP/1.1\r\nHost: localhost\r\n")
                blockers.append(blocked)
            time.sleep(0.1)
            assert request(port) == (503, b"Service Unavailable")
            for blocked in blockers:
                blocked.close()

            started = time.monotonic()
            with ThreadPoolExecutor(max_workers=1) as pool:
                sequential = list(pool.map(lambda _: request(port), range(40)))
            elapsed = time.monotonic() - started
            assert sequential == [(200, b"get")] * 40

            stdout, stderr = server.communicate(timeout=8)
            if server.returncode != 0:
                raise RuntimeError(
                    f"baseline server returned {server.returncode}\n{stdout}\n{stderr}"
                )
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        restart_program = root / "restart.p"
        restart_executable = root / ("restart.exe" if sys.platform == "win32" else "restart")
        restart_program.write_text(restart_source, encoding="utf-8")
        subprocess.run([compiler, restart_program, "-o", restart_executable], check=True, cwd=root)
        restarted = subprocess.run(
            [restart_executable], cwd=root, text=True, capture_output=True, timeout=20
        )
        if restarted.returncode != 0:
            raise RuntimeError(
                f"restart fixture returned {restarted.returncode}\n"
                f"{restarted.stdout}\n{restarted.stderr}"
            )

    print(
        "Backend baseline: GET/POST, 404/405/413/431, connection limit, "
        f"40 sequential requests in {elapsed:.3f}s, and 10 start/stop cycles passed"
    )


if __name__ == "__main__":
    main()
