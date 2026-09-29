#!/usr/bin/env python3
"""Certify HTTP limits and clean listener/worker teardown on loopback."""

from concurrent.futures import ThreadPoolExecutor
import http.client
import os
from pathlib import Path
import ssl
import socket
import subprocess
import sys
import tempfile
import time


def available_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def request(port, method="GET", path="/slow", body=None, headers=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    connection.request(method, path, body=body, headers=headers or {})
    response = connection.getresponse()
    result = response.status, response.read().decode("utf-8")
    connection.close()
    return result


def segmented_limit_request(port, context=None):
    raw = socket.create_connection(("127.0.0.1", port), timeout=5)
    connection = context.wrap_socket(raw, server_hostname="localhost") if context else raw
    try:
        connection.sendall(b"POST /missing HTTP/1.1\r\nHost: localhost\r\nContent-Length: 64\r\nConnection: close\r\n\r\n" + b"x" * 16)
        received = b""
        while b"\r\n\r\n" not in received:
            chunk = connection.recv(4096)
            if not chunk:
                raise RuntimeError("server closed before sending complete response headers")
            received += chunk
        header, body = received.split(b"\r\n\r\n", 1)
        lines = header.split(b"\r\n")
        status = int(lines[0].split()[1])
        headers = {key.strip().lower(): value.strip() for key, value in (line.split(b":", 1) for line in lines[1:])}
        length = int(headers[b"content-length"])
        while len(body) < length:
            chunk = connection.recv(4096)
            if not chunk:
                raise RuntimeError("server closed before sending the complete response body")
            body += chunk
        connection.sendall(b"x" * 48)
        if context:
            raw = connection.unwrap()
            raw.close()
            connection = None
        else:
            connection.shutdown(socket.SHUT_WR)
            if connection.recv(1) != b"":
                raise RuntimeError("plaintext server sent bytes beyond Content-Length")
        return status, body[:length].decode("utf-8")
    finally:
        if connection is not None:
            connection.close()


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    source = """function main() -> int : (NetworkError, TimeError) {
    app := http_server();
    app.timeouts(3000, 3000, 3000, 2000);
    app.limits(32, 4096, 16, 8);
    app.get("/slow", (http_request request) => {
        sleep_ms(100);
        return http_text("done");
    });
    app.listen("127.0.0.1", %d, 6);
    if (app.running()) {
        return 1;
    }
    return 0;
}
""" % port
    with tempfile.TemporaryDirectory(prefix="strut-http-lifecycle-") as temporary:
        root = Path(temporary)
        program = root / "server.p"
        executable = root / ("server.exe" if sys.platform == "win32" else "server")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
        server = subprocess.Popen([executable], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    probe = http.client.HTTPConnection("127.0.0.1", port, timeout=0.2)
                    probe.connect(); probe.close()
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("HTTP server did not start")
                    time.sleep(0.03)
            # The readiness connection is deliberately malformed and consumes
            # one of the six finite accepts.
            with ThreadPoolExecutor(max_workers=3) as pool:
                results = list(pool.map(lambda _: request(port), range(3)))
            if results != [(200, "done")] * 3:
                raise RuntimeError(f"concurrent lifecycle requests failed: {results!r}")
            if request(port, "POST", "/slow") != (405, "Method Not Allowed"):
                raise RuntimeError("method mismatch did not return 405")
            if segmented_limit_request(port) != (413, "Payload Too Large"):
                raise RuntimeError("segmented oversized request did not return 413")
            stdout, stderr = server.communicate(timeout=8)
            if server.returncode != 0:
                raise RuntimeError(f"server returned {server.returncode}\n{stdout}\n{stderr}")
        finally:
            if server.poll() is None:
                server.kill(); server.wait()
        stop_port = available_port()
        stop_program = root / "explicit-stop.p"
        stop_executable = root / ("explicit-stop.exe" if sys.platform == "win32" else "explicit-stop")
        stop_program.write_text(f"""function main() -> int : (NetworkError, ThreadError, TimeError) {{
    app := http_server();
    listener := thread(() => {{ app.listen("127.0.0.1", {stop_port}); }});
    while (!app.running()) {{
        sleep_ms(5);
    }}
    app.stop();
    app.stop();
    listener.join();
    if (app.running()) {{
        return 1;
    }}
    return 0;
}}
""", encoding="utf-8")
        subprocess.run([compiler, stop_program, "-o", stop_executable], check=True, cwd=root)
        stopped = subprocess.run([stop_executable], cwd=root, text=True, capture_output=True, timeout=8)
        if stopped.returncode != 0:
            raise RuntimeError(f"explicit stop returned {stopped.returncode}\n{stopped.stdout}\n{stopped.stderr}")
        tls_port = available_port()
        tls_program = root / "tls-server.p"
        tls_executable = root / ("tls-server.exe" if sys.platform == "win32" else "tls-server")
        tls_program.write_text(f"""function main(string command, string[] args) -> int : (NetworkError, TlsError, ThreadError, TimeError, SqliteError) {{
    database := sqlite_open(args[2]);
    database.exec("CREATE TABLE IF NOT EXISTS status(message TEXT)");
    database.exec("DELETE FROM status");
    database.exec("INSERT INTO status(message) VALUES ('secure')");
    app := http_server();
    app.limits(32, 4096, 16, 8);
    app.get("/secure", (http_request request) => {{ return http_json_response(database.query("SELECT message FROM status")); }});
    listener := thread(() => {{ app.listen_tls("127.0.0.1", {tls_port}, args[0], args[1]); }});
    while (!app.running()) {{ sleep_ms(5); }}
    sleep_ms(15000);
    app.stop();
    listener.join();
    return 0;
}}
""", encoding="utf-8")
        subprocess.run([compiler, tls_program, "-o", tls_executable], check=True, cwd=root)
        client_program = root / "tls-client.p"
        client_executable = root / ("tls-client.exe" if sys.platform == "win32" else "tls-client")
        client_program.write_text(f"""function main(string command, string[] args) -> int : HttpError {{
    if (args.length == 0) {{
        http_request_stream("GET", "https://localhost:{tls_port}/secure", {{}}, null, null);
        return 1;
    }}
    int_64 initial := 0;
    streamed_bytes := new(initial);
    streamed := http_request_stream("GET", "https://localhost:{tls_port}/secure", {{"ca_file": args[0]}}, null, (bytes chunk) => {{
        *streamed_bytes = *streamed_bytes + chunk.length();
        return true;
    }});
    if (streamed.status != 200 || *streamed_bytes == 0) {{ return 2; }}
    response := http_get_ca("https://localhost:{tls_port}/secure", args[0]);
    println(response.body);
    return 0;
}}
""", encoding="utf-8")
        subprocess.run([compiler, client_program, "-o", client_executable], check=True, cwd=root)
        fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "tls"
        certificate, private_key = fixture / "localhost-cert.pem", fixture / "localhost-key.pem"
        trusted_ca = root / "trusted ca.pem"
        trusted_ca.write_bytes(certificate.read_bytes())
        tls_server = subprocess.Popen([tls_executable, certificate, private_key, root / "service.db"], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    unverified = ssl.create_default_context()
                    with unverified.wrap_socket(socket.socket(), server_hostname="localhost") as sock:
                        sock.settimeout(0.2); sock.connect(("127.0.0.1", tls_port))
                    raise AssertionError("fixture certificate unexpectedly trusted")
                except ssl.SSLCertVerificationError:
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("TLS server did not start")
                    time.sleep(0.03)
            client_environment = os.environ.copy()
            client_environment["NO_PROXY"] = "127.0.0.1,localhost"
            client_environment["no_proxy"] = "127.0.0.1,localhost"
            rejected = subprocess.run([client_executable], cwd=root, text=True, capture_output=True, env=client_environment)
            if rejected.returncode == 0:
                raise RuntimeError("untrusted TLS certificate was accepted")
            if tls_server.poll() is not None:
                stdout, stderr = tls_server.communicate()
                raise RuntimeError(f"TLS server exited before trusted client ({tls_server.returncode})\n{stdout}\n{stderr}")
            trusted = subprocess.run([client_executable, trusted_ca], cwd=root, text=True, capture_output=True, timeout=5, env=client_environment)
            if trusted.returncode != 0 or "secure" not in trusted.stdout:
                raise RuntimeError(f"trusted Strut TLS client failed ({trusted.returncode})\n{trusted.stdout}\n{trusted.stderr}")
            context = ssl.create_default_context(cafile=str(trusted_ca))
            tls_limit_result = segmented_limit_request(tls_port, context)
            if tls_limit_result != (413, "Payload Too Large"):
                raise RuntimeError(f"TLS oversized request did not return 413: {tls_limit_result!r}")
            stdout, stderr = tls_server.communicate(timeout=20)
            if tls_server.returncode != 0:
                raise RuntimeError(f"TLS server returned {tls_server.returncode}\n{stdout}\n{stderr}")
        finally:
            if tls_server.poll() is None:
                tls_server.kill(); tls_server.wait()
    print("HTTP lifecycle certification: concurrent drain, Strut TLS client/server JSON, explicit stop, segmented plaintext/TLS 413 and TLS close_notify passed")


if __name__ == "__main__":
    main()
