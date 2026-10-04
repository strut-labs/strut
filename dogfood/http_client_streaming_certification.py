#!/usr/bin/env python3
"""Certify bounded outbound HTTP streaming against deterministic local peers."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import threading
import time


DOWNLOAD_BYTES = 5 * 1024 * 1024
RESOURCE_BYTES = 64 * 1024 * 1024
BLOCK = bytes(range(256)) * 64


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.stable_count = 0
        self.stable_lock = threading.Lock()
        self.stable_events = {value: threading.Event() for value in (1, 100, 200, 300)}
        self.stable_releases = {value: threading.Event() for value in (1, 100, 200, 300)}
        self.download_events = {value: threading.Event() for value in (8, 56)}
        self.download_releases = {value: threading.Event() for value in (8, 56)}
        self.upload_events = {value: threading.Event() for value in (8, 56)}
        self.upload_releases = {value: threading.Event() for value in (8, 56)}

    def handle_error(self, *_):
        pass


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def body(self):
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            result = bytearray()
            while True:
                line = self.rfile.readline()
                if not line:
                    raise ConnectionError("chunked request ended before terminator")
                size = int(line.split(b";", 1)[0], 16)
                if size == 0:
                    while self.rfile.readline() not in (b"\r\n", b"\n", b""):
                        pass
                    return bytes(result)
                result.extend(self.rfile.read(size))
                if self.rfile.read(2) != b"\r\n":
                    raise ConnectionError("malformed chunked request")
        return self.rfile.read(int(self.headers.get("Content-Length", "0")))

    def reply(self, status=200, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD" and body:
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

    def stream_blocks(self, count, pause=False):
        self.send_response(200)
        self.send_header("Content-Length", str(count * len(BLOCK)))
        self.send_header("X-Stream", "final")
        self.end_headers()
        for _ in range(count):
            try:
                self.wfile.write(BLOCK)
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                break
            if pause:
                time.sleep(0.002)

    def resource_download(self):
        self.send_response(200)
        self.send_header("Content-Length", str(RESOURCE_BYTES))
        self.end_headers()
        sent = 0
        while sent < RESOURCE_BYTES:
            try:
                self.wfile.write(BLOCK)
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                return
            sent += len(BLOCK)
            mib = sent // (1024 * 1024)
            if mib in self.server.download_events and sent % (1024 * 1024) == 0:
                self.server.download_events[mib].set()
                if not self.server.download_releases[mib].wait(10):
                    raise RuntimeError("download resource checkpoint was not released")

    def resource_upload(self):
        remaining = int(self.headers.get("Content-Length", "0"))
        received = 0
        while remaining:
            chunk = self.rfile.read(min(16384, remaining))
            if not chunk:
                raise ConnectionError("resource upload ended early")
            remaining -= len(chunk)
            received += len(chunk)
            mib = received // (1024 * 1024)
            if mib in self.server.upload_events and received % (1024 * 1024) == 0:
                self.server.upload_events[mib].set()
                if not self.server.upload_releases[mib].wait(10):
                    raise RuntimeError("upload resource checkpoint was not released")
        self.reply(headers=(("X-Length", str(received)),))

    def dispatch(self):
        try:
            if self.path == "/download-large":
                self.stream_blocks(DOWNLOAD_BYTES // len(BLOCK))
            elif self.path == "/resource-download":
                self.resource_download()
            elif self.path == "/resource-upload":
                self.resource_upload()
            elif self.path == "/download-slow":
                self.stream_blocks(256, pause=True)
            elif self.path == "/download-small":
                self.reply(body=b"small")
            elif self.path == "/download-limit":
                self.reply(body=b"123456789")
            elif self.path == "/redirect-get":
                self.reply(302, b"HOP", (("Location", "/redirect-final"), ("X-Hop", "hidden")))
            elif self.path == "/redirect-final":
                self.reply(200, b"FINAL", (("X-Final", "visible"),))
            elif self.path == "/redirect-auth":
                self.reply(302, headers=(("Location", "/auth-final"),))
            elif self.path == "/redirect-cross":
                self.reply(302, headers=(("Location", self.server.cross_base + "/auth-final"),))
            elif self.path == "/auth-final":
                self.reply(headers=(
                    ("X-Authorization", "yes" if self.headers.get("Authorization") else "no"),
                    ("X-Cookie", "yes" if self.headers.get("Cookie") else "no"),
                ))
            elif self.path in ("/post-301", "/post-302", "/post-303", "/post-307", "/post-308"):
                body = self.body()
                code = int(self.path[-3:])
                target = "/method" if code in (301, 302, 303) else "/must-not-replay"
                self.reply(code, b"HOP", (("Location", target), ("X-Read", str(len(body)))))
            elif self.path == "/method":
                body = self.body()
                self.reply(headers=(("X-Method", self.command), ("X-Length", str(len(body)))))
            elif self.path == "/must-not-replay":
                self.reply(500, b"replayed")
            elif self.path in ("/upload-known", "/upload-unknown", "/upload-slow"):
                body = self.body()
                self.reply(headers=(("X-Length", str(len(body))), ("X-Sum", str(sum(body)))))
            elif self.path == "/timeout":
                time.sleep(0.15)
                self.reply(body=b"late")
            elif self.path == "/empty-location":
                self.reply(302, b"terminal", (("Location", ""),))
            elif self.path == "/redirect-large":
                self.reply(302, b"123456789", (("Location", "/redirect-final"),))
            elif self.path == "/stable":
                with self.server.stable_lock:
                    self.server.stable_count += 1
                    count = self.server.stable_count
                if count in self.server.stable_events:
                    self.server.stable_events[count].set()
                    if not self.server.stable_releases[count].wait(10):
                        raise RuntimeError("resource checkpoint was not released")
                self.reply(body=b"x")
            else:
                self.reply(500, b"unexpected")
        except (BrokenPipeError, ConnectionResetError, ConnectionError):
            self.close_connection = True

    do_GET = dispatch
    do_POST = dispatch
    do_PUT = dispatch


def compile_program(compiler, root, name, source):
    program = root / f"{name}.p"
    executable = root / (f"{name}.exe" if sys.platform == "win32" else name)
    program.write_text(source, encoding="utf-8")
    subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
    return executable


def environment():
    result = os.environ.copy()
    result.update(
        {
            "HTTP_PROXY": "http://127.0.0.1:1",
            "HTTPS_PROXY": "http://127.0.0.1:1",
            "ALL_PROXY": "http://127.0.0.1:1",
            "NO_PROXY": "127.0.0.1,localhost",
            "no_proxy": "127.0.0.1,localhost",
        }
    )
    return result


def resource_snapshot(pid):
    if sys.platform != "linux":
        return None
    proc = Path("/proc") / str(pid)
    try:
        descriptors = len(list((proc / "fd").iterdir()))
        status = (proc / "status").read_text(encoding="utf-8")
        rss = next(int(line.split()[1]) for line in status.splitlines() if line.startswith("VmRSS:"))
        return descriptors, rss
    except (FileNotFoundError, ProcessLookupError, StopIteration):
        return None


def rss_growth_limit():
    flags = os.environ.get("STRUT_CXXFLAGS", "")
    if "sanitize=address" in flags:
        return 128 * 1024
    if "-fsanitize" in flags:
        return 32 * 1024
    return 16 * 1024


def core_source(base):
    expected_sum = (sum(range(256)) * (DOWNLOAD_BYTES // 256))
    return f'''function main() -> int : (Error, HttpError, ThreadError, TimeError) {{
    int_64 initial_64 := 0;
    int initial_int := 0;
    bool initial_bool := false;
    downloaded := new(initial_64);
    download_sum := new(initial_64);
    max_download_chunk := new(initial_64);
    large := http_request_stream("GET", "{base}/download-large", {{"max_response_body_bytes": {DOWNLOAD_BYTES}}}, null, (bytes chunk) => {{
        if (chunk.length() > *max_download_chunk) {{ *max_download_chunk = chunk.length(); }}
        int_64 index := 0;
        while (index < chunk.length()) {{ *download_sum = *download_sum + chunk[index]; index++; }}
        *downloaded = *downloaded + chunk.length();
        return true;
    }});
    if (large.status != 200 || large.headers["x-stream"] != "final" || *downloaded != {DOWNLOAD_BYTES} || *download_sum != {expected_sum} || *max_download_chunk <= 0 || *max_download_chunk > 65536) {{ return 1; }}

    known_calls := new(initial_int);
    known_sent := new(initial_64);
    known_max := new(initial_64);
    known := http_request_stream("PUT", "{base}/upload-known", {{"request_body_length": 3145851, "max_request_body_bytes": 3145851}}, (int_64 requested) => {{
        *known_calls = *known_calls + 1;
        if (requested > *known_max) {{ *known_max = requested; }}
        int_64 amount := requested;
        if (amount > 8192) {{ amount = 8192; }}
        chunk := bytes(amount);
        if (amount > 0) {{ chunk[0] = 255; }}
        *known_sent = *known_sent + amount;
        return chunk;
    }}, null);
    if (known.status != 200 || *known_sent != 3145851 || *known_max <= 0 || *known_max > 65536 || known.headers["x-length"] != "3145851" || known.headers["x-sum"] != to_string(*known_calls * 255)) {{ return 2; }}

    unknown_calls := new(initial_int);
    unknown_sent := new(initial_64);
    unknown := http_request_stream("POST", "{base}/upload-unknown", {{"max_request_body_bytes": 3200000}}, (int_64 requested) => {{
        if (*unknown_sent >= 3146003) {{ return bytes(); }}
        int_64 amount := requested;
        if (amount > 7001) {{ amount = 7001; }}
        if (amount > 3146003 - *unknown_sent) {{ amount = 3146003 - *unknown_sent; }}
        chunk := bytes(amount);
        chunk[0] = 255;
        *unknown_calls = *unknown_calls + 1;
        *unknown_sent = *unknown_sent + amount;
        return chunk;
    }}, null);
    if (unknown.status != 200 || unknown.headers["x-length"] != "3146003" || unknown.headers["x-sum"] != to_string(*unknown_calls * 255)) {{ return 3; }}

    redirect_bytes := new(initial_64);
    redirected := http_request_stream("GET", "{base}/redirect-get", {{}}, null, (bytes chunk) => {{
        *redirect_bytes = *redirect_bytes + chunk.length();
        if (chunk.to_string() != "FINAL") {{ return false; }}
        return true;
    }});
    if (redirected.status != 200 || *redirect_bytes != 5 || redirected.headers["x-final"] != "visible" || redirected.headers.contains("x-hop")) {{ return 4; }}
    same_auth := http_request_stream("GET", "{base}/redirect-auth", {{"headers": {{"Authorization": "Bearer secret", "Cookie": "session=secret"}}}}, null, null);
    if (same_auth.headers["x-authorization"] != "yes" || same_auth.headers["x-cookie"] != "yes") {{ return 39; }}
    cross_auth := http_request_stream("GET", "{base}/redirect-cross", {{"headers": {{"Authorization": "Bearer secret", "Cookie": "session=secret"}}}}, null, null);
    if (cross_auth.headers["x-authorization"] != "no" || cross_auth.headers["x-cookie"] != "no") {{ return 40; }}

    post_calls := new(initial_int);
    post302 := http_request_stream("POST", "{base}/post-302", {{"request_body_length": 3}}, (int_64 requested) => {{
        *post_calls = *post_calls + 1;
        bool first_post_call := *post_calls == 1;
        if (first_post_call) {{ bytes binary := [97, 0, 98]; return binary; }}
        return bytes();
    }}, null);
    if (post302.status != 200 || post302.headers["x-method"] != "GET" || post302.headers["x-length"] != "0") {{ return 5; }}
    post301 := http_request_stream("POST", "{base}/post-301", {{"request_body_length": 3}}, (int_64 requested) => {{ return bytes.from_string("abc"); }}, null);
    post303 := http_request_stream("POST", "{base}/post-303", {{"request_body_length": 3}}, (int_64 requested) => {{ return bytes.from_string("abc"); }}, null);
    if (post301.headers["x-method"] != "GET" || post303.headers["x-method"] != "GET") {{ return 43; }}
    empty_location_bytes := new(initial_64);
    empty_location := http_request_stream("GET", "{base}/empty-location", {{}}, null, (bytes chunk) => {{ *empty_location_bytes = *empty_location_bytes + chunk.length(); return true; }});
    if (empty_location.status != 302 || *empty_location_bytes != 8) {{ return 44; }}
    try {{ http_request_stream("PUT", "{base}/post-303", {{"request_body_length": 3}}, (int_64 requested) => {{ return bytes.from_string("abc"); }}, null); return 47; }}
    catch (HttpError caught) {{ if (caught.code != -105) {{ return 48; }} }}

    try {{
        http_request_stream("POST", "{base}/post-307", {{"request_body_length": 3}}, (int_64 requested) => {{ return bytes.from_string("abc"); }}, null);
        return 6;
    }} catch (HttpError caught) {{ if (caught.code != -105) {{ return 7; }} }}
    try {{
        http_request_stream("POST", "{base}/post-308", {{"request_body_length": 3}}, (int_64 requested) => {{ return bytes.from_string("abc"); }}, null);
        return 8;
    }} catch (HttpError caught) {{ if (caught.code != -105) {{ return 9; }} }}

    stop_calls := new(initial_int);
    stopped := http_request_stream("GET", "{base}/download-large", {{}}, null, (bytes chunk) => {{ *stop_calls = *stop_calls + 1; return false; }});
    if (stopped.status != 200 || *stop_calls != 1) {{ return 10; }}

    try {{ http_request_stream("PUT", "{base}/upload-known", {{"request_body_length": 9}}, (int_64 requested) => {{ return bytes(); }}, null); return 11; }}
    catch (HttpError caught) {{ if (caught.code != -104) {{ return 12; }} }}
    try {{ http_request_stream("PUT", "{base}/upload-known", {{"request_body_length": 1}}, (int_64 requested) => {{ return bytes(requested + 1); }}, null); return 13; }}
    catch (HttpError caught) {{ if (caught.code != -104) {{ return 14; }} }}

    try {{ http_request_stream("GET", "{base}/download-limit", {{"max_response_body_bytes": 8}}, null, (bytes chunk) => {{ return true; }}); return 17; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 18; }} }}
    try {{ http_request_stream("POST", "{base}/upload-unknown", {{"max_request_body_bytes": 4}}, (int_64 requested) => {{ return bytes.from_string("12345"); }}, null); return 19; }}
    catch (HttpError caught) {{ if (caught.code != -104 && caught.code != -100) {{ return 20; }} }}
    try {{ http_request_stream("PUT", "{base}/upload-known", {{"request_body_length": 5, "max_request_body_bytes": 4}}, (int_64 requested) => {{ return bytes(); }}, null); return 21; }}
    catch (HttpError caught) {{ if (caught.code != -100) {{ return 22; }} }}

    channel<bool> entered;
    channel<bool> release;
    gate := thread(() => {{ entered.receive(); release.send(true); }});
    gated := new(initial_bool);
    backpressure := http_request_stream("GET", "{base}/download-small", {{}}, null, (bytes chunk) => {{
        if (!*gated) {{ *gated = true; entered.send(true); release.receive(); }}
        return true;
    }});
    gate.join();
    if (backpressure.status != 200 || !*gated) {{ return 23; }}

    cancellation_source pre_source;
    pre_source.cancel(); pre_source.cancel();
    try {{ http_request_stream("GET", "{base}/download-small", {{}}, null, null, pre_source.token()); return 24; }}
    catch (HttpError caught) {{ if (caught.code != -103) {{ return 25; }} }}

    cancellation_source upload_source;
    upload_token := upload_source.token();
    channel<bool> upload_started;
    upload_copy := upload_source;
    upload_canceller := thread(() => {{ upload_started.receive(); upload_copy.cancel(); upload_copy.cancel(); }});
    upload_first := new(true);
    try {{
        http_request_stream("POST", "{base}/upload-slow", {{}}, (int_64 requested) => {{
            if (*upload_first) {{ *upload_first = false; upload_started.send(true); }}
            int_64 amount := requested; if (amount > 8192) {{ amount = 8192; }} return bytes(amount);
        }}, null, upload_token);
        return 26;
    }} catch (HttpError caught) {{ if (caught.code != -103) {{ return 27; }} }}
    upload_canceller.join();

    cancellation_source download_source;
    download_token := download_source.token();
    channel<bool> download_started;
    download_copy := download_source;
    download_canceller := thread(() => {{ download_started.receive(); download_copy.cancel(); }});
    download_first := new(true);
    try {{
        http_request_stream("GET", "{base}/download-slow", {{}}, null, (bytes chunk) => {{ if (*download_first) {{ *download_first = false; download_started.send(true); }} return true; }}, download_token);
        return 28;
    }} catch (HttpError caught) {{ if (caught.code != -103) {{ return 29; }} }}
    download_canceller.join();

    cancellation_source header_source;
    header_token := header_source.token();
    try {{
        http_request_stream("GET", "{base}/download-slow", {{}}, null, (bytes chunk) => {{ header_source.cancel(); return true; }}, header_token);
        return 30;
    }} catch (HttpError caught) {{ if (caught.code != -103) {{ return 31; }} }}
    cancellation_source stop_cancel_source;
    stop_cancel_token := stop_cancel_source.token();
    try {{ http_request_stream("GET", "{base}/download-small", {{}}, null, (bytes chunk) => {{ stop_cancel_source.cancel(); return false; }}, stop_cancel_token); return 51; }}
    catch (HttpError caught) {{ if (caught.code != -103) {{ return 52; }} }}

    cancellation_source async_source;
    async_token := async_source.token();
    channel<bool> async_started;
    async_copy := async_source;
    async_canceller := thread(() => {{ async_started.receive(); async_copy.cancel(); }});
    async_first := new(true);
    try {{
        await http_request_stream_async("GET", "{base}/download-slow", {{}}, null, (bytes chunk) => {{ if (*async_first) {{ *async_first = false; async_started.send(true); }} return true; }}, async_token);
        return 32;
    }} catch (HttpError caught) {{ if (caught.code != -103) {{ return 33; }} }}
    async_canceller.join();

    async_ok := await http_request_stream_async("GET", "{base}/download-small", {{}}, null, (bytes chunk) => {{ return true; }});
    if (async_ok.status != 200) {{ return 34; }}
    async_upload_calls := new(initial_int);
    async_upload := await http_request_stream_async("PUT", "{base}/upload-known", {{"request_body_length": 4}}, (int_64 requested) => {{
        *async_upload_calls = *async_upload_calls + 1;
        return bytes.from_string("data");
    }}, null);
    if (async_upload.status != 200 || async_upload.headers["x-length"] != "4" || *async_upload_calls != 1) {{ return 38; }}
    nested_calls := new(initial_int);
    nested_outer := await http_request_stream_async("GET", "{base}/download-small", {{}}, null, (bytes chunk) => {{
        bool first_nested_call := *nested_calls == 0;
        if (first_nested_call) {{ *nested_calls = 1; try {{ nested := await http_request_stream_async("GET", "{base}/download-small", {{}}, null, null); if (nested.status != 200) {{ return false; }} }} catch (HttpError e) {{ return false; }} }}
        return true;
    }});
    if (nested_outer.status != 200 || *nested_calls != 1) {{ return 53; }}

    cancellation_source race_source;
    race_token := race_source.token();
    race_copy := race_source;
    racer := thread(() => {{ try {{ sleep_ms(5); }} catch (TimeError e) {{ }} race_copy.cancel(); }});
    try {{ http_request_stream("GET", "{base}/timeout", {{"timeout_ms": 10}}, null, null, race_token); return 35; }}
    catch (HttpError caught) {{ if (caught.code != -103 && caught.code != 28) {{ return 36; }} }}
    racer.join();

    int cycle := 0;
    while (cycle < 20) {{
        cancellation_source completed_source;
        completed := http_request_stream("GET", "{base}/download-small", {{}}, null, (bytes chunk) => {{ return true; }}, completed_source.token());
        if (completed.status != 200) {{ return 37; }}
        completed_source.cancel(); completed_source.cancel();
        cycle++;
    }}
    cycle = 0;
    while (cycle < 20) {{
        cancellation_source churn_source;
        churn_token := churn_source.token();
        churn_copy := churn_source;
        channel<bool> churn_started;
        churn_first := new(true);
        churn_canceller := thread(() => {{ churn_started.receive(); churn_copy.cancel(); }});
        try {{
            http_request_stream("GET", "{base}/download-slow", {{}}, null, (bytes chunk) => {{ if (*churn_first) {{ *churn_first = false; churn_started.send(true); }} return true; }}, churn_token);
            return 41;
        }} catch (HttpError caught) {{ if (caught.code != -103) {{ return 42; }} }}
        churn_canceller.join();
        cycle++;
    }}
    print("core passed");
    return 0;
}}
'''


def stability_source(base):
    return f'''function main() -> int : HttpError {{
    int count := 0;
    while (count < 300) {{
        int_64 initial := 0;
        bytes_seen := new(initial);
        head := http_request_stream("GET", "{base}/stable", {{}}, null, (bytes chunk) => {{ *bytes_seen = *bytes_seen + chunk.length(); return true; }});
        if (head.status != 200 || *bytes_seen != 1) {{ return 1; }}
        count++;
    }}
    return 0;
}}
'''


def resource_download_source(base):
    return f'''function main() -> int : HttpError {{
    int_64 initial := 0;
    total := new(initial);
    head := http_request_stream("GET", "{base}/resource-download", {{"max_response_body_bytes": {RESOURCE_BYTES}}}, null, (bytes chunk) => {{ *total = *total + chunk.length(); return true; }});
    if (head.status != 200 || *total != {RESOURCE_BYTES}) {{ return 1; }}
    return 0;
}}
'''


def resource_upload_source(base):
    return f'''function main() -> int : HttpError {{
    int_64 initial := 0;
    sent := new(initial);
    head := http_request_stream("PUT", "{base}/resource-upload", {{"request_body_length": {RESOURCE_BYTES}, "max_request_body_bytes": {RESOURCE_BYTES}}}, (int_64 requested) => {{
        int_64 amount := requested; if (amount > 8192) {{ amount = 8192; }} *sent = *sent + amount; return bytes(amount);
    }}, null);
    if (head.status != 200 || *sent != {RESOURCE_BYTES} || head.headers["x-length"] != "{RESOURCE_BYTES}") {{ return 1; }}
    return 0;
}}
'''


def run_stability(executable, root, server):
    process = subprocess.Popen(
        [executable], cwd=root, env=environment(), text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    snapshots = []
    try:
        for checkpoint in (1, 100, 200, 300):
            if not server.stable_events[checkpoint].wait(10):
                raise RuntimeError(f"resource checkpoint {checkpoint} was not reached")
            snapshot = resource_snapshot(process.pid)
            if snapshot is not None:
                snapshots.append((checkpoint, *snapshot))
            server.stable_releases[checkpoint].set()
        stdout, stderr = process.communicate(timeout=20)
        if process.returncode != 0 or stdout or stderr:
            raise RuntimeError(f"resource stability client failed: exit={process.returncode} stdout={stdout!r} stderr={stderr!r}")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    if snapshots:
        baseline_fd, baseline_rss = snapshots[0][1:]
        if max(item[1] for item in snapshots) > baseline_fd + 3:
            raise RuntimeError(f"file descriptor growth across checkpoints: {snapshots}")
        limit = rss_growth_limit()
        if snapshots[-1][2] > baseline_rss + limit:
            raise RuntimeError(f"RSS growth exceeds {limit // 1024} MiB across checkpoints: {snapshots}")
    return snapshots


def run_bounded_transfer(executable, root, events, releases, label):
    process = subprocess.Popen(
        [executable], cwd=root, env=environment(), text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    snapshots = []
    try:
        for checkpoint in (8, 56):
            if not events[checkpoint].wait(15):
                raise RuntimeError(f"{label} checkpoint {checkpoint} MiB was not reached")
            snapshot = resource_snapshot(process.pid)
            if snapshot is not None:
                snapshots.append((checkpoint, *snapshot))
            releases[checkpoint].set()
        stdout, stderr = process.communicate(timeout=20)
        if process.returncode != 0 or stdout or stderr:
            raise RuntimeError(f"{label} client failed: exit={process.returncode} stdout={stdout!r} stderr={stderr!r}")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    if len(snapshots) == 2:
        if snapshots[1][1] > snapshots[0][1] + 3:
            raise RuntimeError(f"{label} file descriptor growth: {snapshots}")
        limit = rss_growth_limit()
        if snapshots[1][2] > snapshots[0][2] + limit:
            raise RuntimeError(f"{label} RSS grew by more than {limit // 1024} MiB with transfer size: {snapshots}")
    return snapshots


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    cross_server = Server(("127.0.0.1", 0), Handler)
    cross_server.cross_base = f"http://127.0.0.1:{cross_server.server_port}"
    cross_worker = threading.Thread(target=cross_server.serve_forever, daemon=True)
    cross_worker.start()
    server = Server(("127.0.0.1", 0), Handler)
    server.cross_base = cross_server.cross_base
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    base = f"http://127.0.0.1:{server.server_port}"
    try:
        with tempfile.TemporaryDirectory(prefix="strut-http-client-streaming-") as temporary:
            root = Path(temporary)
            core = compile_program(compiler, root, "core", core_source(base))
            stable = compile_program(compiler, root, "stability", stability_source(base))
            resource_download = compile_program(compiler, root, "resource-download", resource_download_source(base))
            resource_upload = compile_program(compiler, root, "resource-upload", resource_upload_source(base))
            result = subprocess.run(
                [core], cwd=root, env=environment(), text=True,
                capture_output=True, timeout=45, check=False,
            )
            if result.returncode != 0 or result.stdout != "core passed\n" or result.stderr:
                raise RuntimeError(
                    f"streaming certification failed: exit={result.returncode} "
                    f"stdout={result.stdout!r} stderr={result.stderr!r}"
                )
            download_snapshots = run_bounded_transfer(
                resource_download, root, server.download_events, server.download_releases, "streamed download"
            )
            upload_snapshots = run_bounded_transfer(
                resource_upload, root, server.upload_events, server.upload_releases, "streamed upload"
            )
            snapshots = run_stability(stable, root, server)
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)
        cross_server.shutdown()
        cross_server.server_close()
        cross_worker.join(timeout=5)
    resource_result = (
        f", download snapshots {download_snapshots}, upload snapshots {upload_snapshots}, cleanup snapshots {snapshots}"
        if snapshots else ", resource sampling skipped"
    )
    print(
        "HTTP client streaming certification: large bounded binary streams, redirects, callback failures, "
        "limits, backpressure, cancellation, async, proxy isolation and repeated cleanup passed" + resource_result
    )


if __name__ == "__main__":
    main()
