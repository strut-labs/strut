#!/usr/bin/env python3
"""Certify the bounded buffered libcurl client against a local HTTP peer."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import threading
import time


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

    def dispatch(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        if self.path == "/binary":
            self.reply(200, b"A\x00B", (("X-Final", "yes"),))
        elif self.path == "/large":
            self.reply(200, b"123456789")
        elif self.path == "/large-header":
            self.reply(200, b"ok", (("X-Large", "x" * 300),))
        elif self.path == "/duplicate":
            self.reply(200, b"ok", (("X-Dup", "one"), ("x-dup", "two")))
        elif self.path == "/malformed-status":
            self.connection.sendall(
                b"HTTP/1.1 200 OK\r\nX-Value: first\r\nHTTP/x: fake\r\n"
                b"x-value: second\r\nContent-Length: 2\r\n\r\nok"
            )
            self.close_connection = True
        elif self.path == "/redirect":
            self.reply(302, b"hop-body", (("Location", "/final"), ("X-Hop", "hidden")))
        elif self.path == "/final":
            self.reply(200, b"final", (("X-Final", "visible"),))
        elif self.path in ("/r302", "/r307"):
            self.reply(int(self.path[2:]), headers=(("Location", "/echo"),))
        elif self.path == "/loop":
            self.reply(302, headers=(("Location", "/loop"),))
        elif self.path == "/slow":
            time.sleep(0.2)
            self.reply(200, b"late")
        elif self.path == "/missing":
            self.reply(404, b"missing")
        elif self.path == "/echo":
            marker = self.headers.get("X-Test", "")
            result = self.command.encode() + b":" + str(len(body)).encode() + b":" + marker.encode()
            self.reply(200, result)
        elif self.path == "/echo-bytes":
            self.reply(200, body)
        elif self.path == "/shutdown-slow":
            self.server.shutdown_started.set()
            time.sleep(0.2)
            self.reply(200, b"joined")
        else:
            self.reply(500, b"unexpected")

    do_GET = dispatch
    do_HEAD = dispatch
    do_POST = dispatch
    do_PUT = dispatch


class Server(ThreadingHTTPServer):
    def handle_error(self, *_):
        pass


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    server = Server(("127.0.0.1", 0), Handler)
    server.shutdown_started = threading.Event()
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    base = f"http://127.0.0.1:{server.server_port}"
    source = f'''function main() -> int : HttpError {{
    response := http_get("{base}/binary");
    bytes binary := bytes.from_string(response.body);
    if (response.status != 200 || binary.length() != 3 || binary[1] != 0 || response.headers["x-final"] != "yes") {{ return 1; }}
    redirected := http_get("{base}/redirect");
    if (redirected.body != "final" || redirected.headers["x-final"] != "visible" || redirected.headers.contains("x-hop")) {{ return 2; }}
    missing := http_get("{base}/missing");
    if (missing.status != 404 || missing.body != "missing") {{ return 3; }}
    empty := http_request("POST", "{base}/echo", {{"body": "", "headers": {{"X-Test": "yes"}}}});
    if (empty.body != "POST:0:yes") {{ return 4; }}
    binary_request := http_request("PUT", "{base}/echo-bytes", {{"body": response.body}});
    if (binary_request.body != response.body) {{ return 29; }}
    stopped := http_request("GET", "{base}/redirect", {{"follow_redirects": false}});
    if (stopped.status != 302 || stopped.body != "hop-body" || stopped.headers["x-hop"] != "hidden") {{ return 30; }}
    async_response := await http_request_async("GET", "{base}/final", {{"max_response_body_bytes": 16}});
    if (async_response.status != 200 || async_response.body != "final") {{ return 35; }}
    rewritten := http_request("POST", "{base}/r302", {{"body": "abc"}});
    if (rewritten.body != "GET:0:") {{ return 5; }}
    preserved := http_request("POST", "{base}/r307", {{"body": "abc"}});
    if (preserved.body != "POST:3:") {{ return 6; }}
    try {{ http_request("GET", "{base}/large", {{"max_response_body_bytes": 8}}); return 7; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 8; }} }}
    try {{ http_request("GET", "{base}/large-header", {{"max_response_header_bytes": 180}}); return 9; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 10; }} }}
    try {{ http_request("GET", "{base}/duplicate", {{}}); return 11; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 12; }} }}
    try {{ http_request("GET", "{base}/malformed-status", {{}}); return 42; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 43; }} }}
    try {{ http_request("GET", "{base}/slow", {{"timeout_ms": 50}}); return 13; }}
    catch (HttpError caught) {{ if (caught.code != 28) {{ return 14; }} }}
    try {{ http_request("GET", "{base}/loop", {{"max_redirects": 1}}); return 15; }}
    catch (HttpError caught) {{ if (caught.code != 47) {{ return 16; }} }}
    try {{ http_request("GET", "{base}/echo", {{"body": "12345", "max_request_body_bytes": 4}}); return 17; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 18; }} }}
    try {{ http_request("BAD METHOD", "{base}/echo"); return 19; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 20; }} }}
    try {{ http_request("GET", "file:///etc/passwd"); return 21; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 22; }} }}
    try {{ http_request("GET", "{base}/echo", {{"timeout_ms": 0}}); return 23; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 24; }} }}
    try {{ http_request("GET", "{base}/echo", {{"unknown_option": true}}); return 25; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 26; }} }}
    try {{ http_request("HEAD", "{base}/echo", {{"body": "wrong"}}); return 27; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 28; }} }}
    try {{ http_request("GET", "{base}/binary", {{"max_response_header_count": 2}}); return 31; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 32; }} }}
    try {{ http_request("GET", "{base}/echo", {{"headers": {{"X-Test": "one", "x-test": "two"}}}}); return 33; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 34; }} }}
    try {{ http_request("GET", "{base}/echo", {{"headers": {{"Content-Length": "0"}}}}); return 36; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 37; }} }}
    try {{ await http_request_async("GET", "{base}/large", {{"max_response_body_bytes": 8}}); return 38; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 39; }} }}
    try {{ http_request("GET", "{base}/echo", {{"max_response_body_bytes": 1e20}}); return 40; }}
    catch (HttpError caught) {{ if (caught.code != -101) {{ return 41; }} }}
    pending_shutdown := http_get_async("{base}/shutdown-slow");
    return 0;
}}
'''
    try:
        with tempfile.TemporaryDirectory(prefix="strut-http-client-") as temporary:
            root = Path(temporary)
            program = root / "client.p"
            executable = root / ("client.exe" if sys.platform == "win32" else "client")
            program.write_text(source, encoding="utf-8")
            subprocess.run([compiler, program, "-o", executable], check=True, cwd=root)
            environment = os.environ.copy()
            environment["NO_PROXY"] = "127.0.0.1,localhost"
            environment["no_proxy"] = "127.0.0.1,localhost"
            result = subprocess.run([executable], capture_output=True, text=True, timeout=20, cwd=root, env=environment)
            if result.returncode != 0:
                raise RuntimeError(
                    f"HTTP client certification returned {result.returncode}\n{result.stdout}\n{result.stderr}"
                )
            if not server.shutdown_started.is_set():
                raise RuntimeError("pending HTTP request did not start before executor shutdown")
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)
    print("HTTP client certification: limits, metadata, redirects, methods, timeout and protocol policy passed")


if __name__ == "__main__":
    main()
