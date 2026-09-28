#!/usr/bin/env python3
"""Certify request-lifetime cancellation and nested operation propagation."""

from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

from http_persistence_certification import HttpConnection, check_response, request
from http_response_stream_certification import available_port, compile_program, wait_until_listening


def get(port, path):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
        connection = HttpConnection(raw)
        connection.send(request(path, extra="Connection: close\r\n"))
        return connection.response()


def wait_for(port, path, expected, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            response = get(port, path)
            if response[2] == expected:
                return
        except OSError:
            pass
        time.sleep(0.02)
    raise RuntimeError(f"{path} did not report {expected!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    port = available_port()
    python = Path(sys.executable).resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-cancellation-") as temporary:
        root = Path(temporary)
        sleeper = root / "sleeper.py"
        sleeper.write_text("import time\ntime.sleep(30)\n", encoding="utf-8")
        source = f'''function main() -> void : (NetworkError, ExecError) {{
    app := http_server();
    app.timeouts(300, 300, 1000, 1000);
    app.limits(1048576, 8192, 32, 8);
    cancellation_token saved_value;
    saved := new(saved_value);
    read_cancelled := new(false);
    write_cancelled := new(false);
    read_lock := mutex();
    write_lock := mutex();

    app.get("/save", (http_request request) => {{
        *saved = request.cancellation;
        if (saved->cancelled()) {{ return http_text("pre-cancelled"); }}
        return http_text("saved");
    }});
    app.get("/state", (http_request request) => {{
        if (!saved->cancelled()) {{ return http_text("old-active"); }}
        if (request.cancellation.cancelled()) {{ return http_text("new-cancelled"); }}
        return http_text("independent");
    }});
    app.post_request_stream("/read", (http_request request, http_request_body body, http_response_writer response) => {{
        try {{ body.read_bytes(1); }} catch (NetworkError caught) {{ read_lock.lock(); *read_cancelled = request.cancellation.cancelled(); read_lock.unlock(); }}
    }});
    app.get("/read-state", (http_request request) => {{
        read_lock.lock(); bool observed := *read_cancelled; read_lock.unlock();
        if (observed) {{ return http_text("cancelled"); }}
        return http_text("waiting");
    }});
    app.get_stream("/write", (http_request request, http_response_writer response) => {{
        block := bytes(65536);
        int count := 0;
        try {{ while (count < 10000) {{ response.write_bytes(block); count++; }} }}
        catch (NetworkError caught) {{ write_lock.lock(); *write_cancelled = request.cancellation.cancelled(); write_lock.unlock(); }}
    }});
    app.get("/write-state", (http_request request) => {{
        write_lock.lock(); bool observed := *write_cancelled; write_lock.unlock();
        if (observed) {{ return http_text("cancelled"); }}
        return http_text("waiting");
    }});
    app.get("/process", (http_request request) => {{
        child := new(process("{python}", ["{sleeper}"], request.cancellation));
        string result := "not-cancelled";
        try {{ child->out.read_bytes(1); }} catch (ExecError caught) {{
            if (caught.code == 125 && request.cancellation.cancelled()) {{ result = "cancelled"; }}
        }}
        child->terminate(); child->wait();
        return http_text(result);
    }});
    app.get("/stop", (http_request request) => {{ app.stop(); return http_text("stopped"); }});
    app.listen("127.0.0.1", {port});
}}
'''
        executable = compile_program(compiler, root, "request-cancellation-server", source)
        server = subprocess.Popen([executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            wait_until_listening(port, server)

            with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
                connection = HttpConnection(raw)
                connection.send(request("/save"))
                check_response(connection.response(), 200, b"saved")
                connection.send(request("/state"))
                check_response(connection.response(), 200, b"independent")

            read_client = socket.create_connection(("127.0.0.1", port), timeout=5)
            read_client.sendall(b"POST /read HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n")
            read_client.shutdown(socket.SHUT_RDWR)
            read_client.close()
            wait_for(port, "/read-state", b"cancelled")

            write_client = socket.create_connection(("127.0.0.1", port), timeout=5)
            write_client.sendall(request("/write"))
            write_client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            write_client.close()
            wait_for(port, "/write-state", b"cancelled")

            process_client = socket.create_connection(("127.0.0.1", port), timeout=5)
            process_connection = HttpConnection(process_client)
            process_connection.send(request("/process"))
            time.sleep(0.1)
            stop_client = socket.create_connection(("127.0.0.1", port), timeout=5)
            stop_connection = HttpConnection(stop_client)
            stop_connection.send(request("/stop", extra="Connection: close\r\n"))
            check_response(stop_connection.response(), 200, b"stopped")
            check_response(process_connection.response(), 200, b"cancelled")
            stop_client.close(); process_client.close()

            stdout, stderr = server.communicate(timeout=10)
            if server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"request cancellation server failed: {stdout!r} {stderr!r}")
        finally:
            if server.poll() is None:
                server.kill(); server.wait()

        stop_port = available_port()
        stop_source = f'''function main() -> void : (NetworkError, ThreadError) {{
    app := http_server();
    app.timeouts(1000, 1000, 5000, 2000);
    app.get("/one", (http_request request) => {{ return http_text("one"); }});
    app.get("/stop", (http_request request) => {{
        stopper := thread(() => {{ app.stop(); }});
        stopper.join();
        return http_text("stopped");
    }});
    int cycle := 0;
    while (cycle < 10) {{ app.listen("127.0.0.1", {stop_port}); cycle++; }}
}}
'''
        stop_executable = compile_program(compiler, root, "nested-stop-server", stop_source)
        stop_server = subprocess.Popen([stop_executable], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            for _ in range(10):
                wait_until_listening(stop_port, stop_server)
                idle_client = socket.create_connection(("127.0.0.1", stop_port), timeout=5)
                idle_connection = HttpConnection(idle_client)
                idle_connection.send(request("/one"))
                check_response(idle_connection.response(), 200, b"one")
                started = time.monotonic()
                check_response(get(stop_port, "/stop"), 200, b"stopped")
                if time.monotonic() - started > 1.0:
                    raise RuntimeError("handler-created stop thread waited for forced shutdown")
                idle_connection.expect_eof(timeout=1.0)
                idle_client.close()
            stdout, stderr = stop_server.communicate(timeout=10)
            if stop_server.returncode != 0 or stdout or stderr:
                raise RuntimeError(f"nested stop server failed: {stdout!r} {stderr!r}")
        finally:
            if stop_server.poll() is None:
                stop_server.kill(); stop_server.wait()
    print("HTTP request cancellation certification: normal completion, independent keep-alive requests, read/write disconnects, nested process I/O, and shutdown passed")


if __name__ == "__main__":
    main()
