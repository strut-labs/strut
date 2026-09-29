#!/usr/bin/env python3
"""Certify bounded HTTP worker admission, shutdown, and TLS handshakes."""

import http.client
from concurrent.futures import ThreadPoolExecutor
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


def connect_when_ready(port):
    deadline = time.monotonic() + 5
    while True:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            if time.monotonic() >= deadline:
                raise RuntimeError(f"server on port {port} did not start")
            time.sleep(0.02)


def wait_until_listening(port):
    connection = connect_when_ready(port)
    connection.close()


def request(port, path="/"):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    connection.request("GET", path)
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
            try:
                chunk = connection.recv(4096)
            except ConnectionResetError:
                break
            if not chunk:
                break
            response.extend(chunk)
        return bytes(response)


def compile_program(compiler, root, name, source):
    program = root / f"{name}.p"
    executable = root / (f"{name}.exe" if sys.platform == "win32" else name)
    program.write_text(source, encoding="utf-8")
    subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
    return executable


def require_clean_exit(process, timeout=10):
    stdout, stderr = process.communicate(timeout=timeout)
    if process.returncode != 0 or stderr:
        raise RuntimeError(
            f"server failed: exit={process.returncode} stdout={stdout!r} stderr={stderr!r}"
        )
    return stdout


def native_thread_count(process):
    tasks = Path(f"/proc/{process.pid}/task")
    if tasks.is_dir():
        return len(list(tasks.iterdir()))
    return None


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-workers-") as temporary:
        root = Path(temporary)

        admission_port = available_port()
        admission = compile_program(
            compiler,
            root,
            "admission",
             f'''function main() -> void : NetworkError {{
    app := http_server();
    app.timeouts(2000, 2000, 2000, 3000);
    app.limits(1024, 4096, 16, 2);
    app.get("/", (http_request request) => {{ return http_text("ok"); }});
    app.listen("127.0.0.1", {admission_port}, 4);
}}
''',
        )
        server = subprocess.Popen(
            [admission], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        blockers = []
        try:
            wait_until_listening(admission_port)
            for _ in range(2):
                blocked = socket.create_connection(("127.0.0.1", admission_port), timeout=5)
                blocked.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
                blockers.append(blocked)
                time.sleep(0.2)
            rejected = raw_request(
                admission_port, b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
            )
            if b" 503 " not in rejected:
                raise RuntimeError(f"saturated connection was not rejected: {rejected!r}")
            blockers.pop().close()
            deadline = time.monotonic() + 3
            while True:
                try:
                    recovered = request(admission_port)
                    if recovered == (200, b"ok"):
                        break
                except (ConnectionResetError, OSError):
                    pass
                if time.monotonic() >= deadline:
                    raise RuntimeError("worker capacity was not reclaimed")
                time.sleep(0.02)
            blockers.pop().close()
            require_clean_exit(server)
        finally:
            for blocked in blockers:
                blocked.close()
            if server.poll() is None:
                server.kill()
                server.wait()

        handler_stop_port = available_port()
        handler_stop = compile_program(
            compiler,
            root,
            "handler-stop",
            f'''function main() -> void : NetworkError {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(2000, 2000, 2000, 3000);
    app.get("/stop", (http_request request) => {{
        app_ref->stop();
        return http_text("stopped");
    }});
    app.listen("127.0.0.1", {handler_stop_port});
}}
''',
        )
        server = subprocess.Popen(
            [handler_stop], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(handler_stop_port)
            started = time.monotonic()
            if request(handler_stop_port, "/stop") != (200, b"stopped"):
                raise RuntimeError("handler-initiated stop lost its response")
            require_clean_exit(server)
            if time.monotonic() - started > 1.5:
                raise RuntimeError("handler-initiated stop waited for its own shutdown deadline")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        async_stop_port = available_port()
        async_stop = compile_program(
            compiler,
            root,
            "async-handler-stop",
             f'''function main() -> void : NetworkError {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(2000, 2000, 2000, 3000);
    app.get_async("/stop", async (http_request request) => {{
        app_ref->stop();
        return http_text("async-stopped");
    }});
    app.listen("127.0.0.1", {async_stop_port});
}}
''',
        )
        server = subprocess.Popen(
            [async_stop], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(async_stop_port)
            started = time.monotonic()
            if request(async_stop_port, "/stop") != (200, b"async-stopped"):
                raise RuntimeError("async handler-initiated stop lost its response")
            require_clean_exit(server)
            if time.monotonic() - started > 1.5:
                raise RuntimeError("async handler-initiated stop waited for its own deadline")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        shutdown_port = available_port()
        shutdown = compile_program(
            compiler,
            root,
            "shutdown",
            f'''function main() -> int : (NetworkError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(5000, 5000, 5000, 500);
    app.limits(1024, 4096, 16, 1);
    app.get("/", (http_request request) => {{ return http_text("ok"); }});
    listener := thread(() => {{ app.listen("127.0.0.1", {shutdown_port}); }});
    while (!app.running()) {{ sleep_ms(1); }}
    sleep_ms(750);
    int_64 started := now_ms();
    app.stop();
    listener.join();
    println(now_ms() - started);
    return 0;
}}
''',
        )
        server = subprocess.Popen(
            [shutdown], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        blocker = None
        try:
            blocker = connect_when_ready(shutdown_port)
            blocker.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
            stdout = require_clean_exit(server)
            elapsed_ms = int(stdout.strip())
            if elapsed_ms > 850:
                raise RuntimeError(f"shutdown exceeded one deadline: {elapsed_ms} ms")
        finally:
            if blocker is not None:
                blocker.close()
            if server.poll() is None:
                server.kill()
                server.wait()

        retired_port = available_port()
        retired = compile_program(
            compiler,
            root,
            "retired-handler",
            f'''function main() -> int : (NetworkError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(5000, 5000, 5000, 200);
    app.get("/slow", (http_request request) => {{
        sleep_ms(1500);
        return http_text("late");
    }});
    listener := thread(() => {{ app.listen("127.0.0.1", {retired_port}); }});
    while (!app.running()) {{ sleep_ms(1); }}
    sleep_ms(750);
    app.stop();
    listener.join();
    if (!app.running()) {{ return 1; }}
    while (app.running()) {{ sleep_ms(5); }}
    app.limits(1024, 4096, 16, 1);
    return 0;
}}
''',
        )
        server = subprocess.Popen(
            [retired], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(retired_port)
            slow = socket.create_connection(("127.0.0.1", retired_port), timeout=5)
            slow.sendall(b"GET /slow HTTP/1.1\r\nHost: localhost\r\n\r\n")
            slow.settimeout(3)
            try:
                while slow.recv(4096):
                    pass
            except (ConnectionResetError, OSError):
                pass
            finally:
                slow.close()
            require_clean_exit(server, timeout=4)
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        race_port = available_port()
        startup_race = compile_program(
            compiler,
            root,
            "startup-stop-race",
            f'''function main() -> int : (NetworkError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(1000, 1000, 1000, 200);
    app.stop();
    initial := thread(() => {{ app.listen("127.0.0.1", {race_port}); }});
    while (!app.running()) {{ sleep_ms(1); }}
    app.stop();
    initial.join();
    int cycle := 0;
    while (cycle < 20) {{
        listener := thread(() => {{ app.listen("127.0.0.1", {race_port}); }});
        first := thread(() => {{
            int attempt := 0;
            while (attempt < 50) {{ app.stop(); sleep_ms(1); attempt++; }}
        }});
        second := thread(() => {{
            int attempt := 0;
            while (attempt < 50) {{ app.stop(); sleep_ms(1); attempt++; }}
        }});
        sleep_ms(10);
        app.stop();
        first.join();
        second.join();
        app.stop();
        listener.join();
        if (app.running()) {{ return 1; }}
        cycle++;
    }}
    return 0;
}}
''',
        )
        raced = subprocess.run(
            [startup_race], cwd=root, text=True, capture_output=True, timeout=20
        )
        if raced.returncode != 0 or raced.stdout or raced.stderr:
            raise RuntimeError(
                f"startup/stop race failed: exit={raced.returncode} "
                f"stdout={raced.stdout!r} stderr={raced.stderr!r}"
            )

        sustained_port = available_port()
        sustained = compile_program(
            compiler,
            root,
            "sustained-workers",
             f'''function main() -> void : (NetworkError, TimeError) {{
    app := http_server();
    app_ref := ref(app);
    app.timeouts(5000, 5000, 5000, 2000);
    app.limits(1024, 4096, 16, 4);
    app.get("/", (http_request request) => {{ return http_text("ok"); }});
    app.get("/batch", (http_request request) => {{ sleep_ms(20); return http_text("batch"); }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    app.listen("127.0.0.1", {sustained_port});
}}
''',
        )
        server = subprocess.Popen(
            [sustained], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        try:
            wait_until_listening(sustained_port)
            for index in range(500):
                if request(sustained_port) != (200, b"ok"):
                    raise RuntimeError(f"sequential worker reuse failed at request {index}")
            peak_threads = native_thread_count(server)
            for batch in range(25):
                with ThreadPoolExecutor(max_workers=4) as pool:
                    futures = [pool.submit(request, sustained_port, "/batch") for _ in range(4)]
                    time.sleep(0.005)
                    observed = native_thread_count(server)
                    if observed is not None:
                        peak_threads = max(peak_threads or 0, observed)
                    results = [future.result() for future in futures]
                if results != [(200, b"batch")] * 4:
                    raise RuntimeError(f"concurrent worker reuse failed in batch {batch}: {results!r}")
            if peak_threads is not None and peak_threads > 6:
                raise RuntimeError(f"worker thread bound exceeded: observed {peak_threads}, expected at most 6")
            if request(sustained_port, "/stop") != (200, b"stopped"):
                raise RuntimeError("sustained server did not stop through its handler")
            require_clean_exit(server, timeout=15)
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()

        tls_port = available_port()
        tls_server_path = compile_program(
            compiler,
            root,
            "tls-handshake",
            f'''function main(string command, string[] args) -> void : (NetworkError, TlsError) {{
    app := http_server();
    app.timeouts(300, 300, 300, 1000);
    app.limits(1024, 4096, 16, 1);
    app.get("/", (http_request request) => {{ return http_text("secure"); }});
    app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1], 2);
}}
''',
        )
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        server = subprocess.Popen(
            [
                tls_server_path,
                fixture / "localhost-cert.pem",
                fixture / "localhost-key.pem",
            ],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        silent = None
        try:
            silent = connect_when_ready(tls_port)
            silent.settimeout(2)
            started = time.monotonic()
            if silent.recv(1) != b"":
                raise RuntimeError("silent TLS peer received unexpected data")
            elapsed = time.monotonic() - started
            if elapsed > 1.0:
                raise RuntimeError(f"TLS handshake timeout was not bounded: {elapsed:.3f}s")
            silent.close()
            silent = None
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            with socket.create_connection(("127.0.0.1", tls_port), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as secure:
                    secure.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
                    response = bytearray()
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        response.extend(chunk)
            if b" 200 " not in response or not response.endswith(b"secure"):
                raise RuntimeError(f"worker was not reusable after TLS timeout: {response!r}")
            require_clean_exit(server)
        finally:
            if silent is not None:
                silent.close()
            if server.poll() is None:
                server.kill()
                server.wait()

        tls_shutdown_port = available_port()
        tls_shutdown = compile_program(
            compiler,
            root,
            "tls-shutdown",
            f'''include <filesystem>;

function main(string command, string[] args) -> int : (FilesystemError, NetworkError, TlsError, ThreadError, TimeError) {{
    app := http_server();
    app.timeouts(5000, 5000, 5000, 500);
    app.limits(1024, 4096, 16, 1);
    app.get("/", (http_request request) => {{ return http_text("secure"); }});
    listener := thread(() => {{ app.listen_tls("127.0.0.1", {tls_shutdown_port}, args[0], args[1]); }});
    while (!app.running()) {{ sleep_ms(1); }}
    while (!exists(args[2])) {{ sleep_ms(1); }}
    int_64 started := now_ms();
    app.stop();
    listener.join();
    println(now_ms() - started);
    return 0;
}}
''',
        )
        tls_shutdown_ready = root / "tls-shutdown-ready"
        server = subprocess.Popen(
            [
                tls_shutdown,
                fixture / "localhost-cert.pem",
                fixture / "localhost-key.pem",
                tls_shutdown_ready,
            ],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        secure = None
        try:
            raw = connect_when_ready(tls_shutdown_port)
            secure = context.wrap_socket(raw, server_hostname="localhost")
            secure.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
            tls_shutdown_ready.touch()
            stdout = require_clean_exit(server)
            elapsed_ms = int(stdout.strip())
            if elapsed_ms > 850:
                raise RuntimeError(f"TLS shutdown exceeded one deadline: {elapsed_ms} ms")
        finally:
            if secure is not None:
                secure.close()
            if server.poll() is None:
                server.kill()
                server.wait()

    print(
        "HTTP worker certification: bounded admission and reuse, finite accounting, "
        "single-deadline shutdown, retired handlers, startup races, 600 sustained requests, "
        "sync/async handler stop, TLS handshake timeout, and TLS shutdown passed"
    )


if __name__ == "__main__":
    main()
