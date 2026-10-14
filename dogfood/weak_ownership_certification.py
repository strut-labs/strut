#!/usr/bin/env python3
"""Explicit weak-ownership certification (current runtime).

Certifies that Strut's documented explicit weak ownership works for the cases that the
HTTP server's self-reference needs, without changing by-value capture semantics:

  - weak_ptr<ordinary user struct>          : compiles; lock()/expired() correct
  - weak_ptr<native http_server>            : compiles; lock() live while owned
  - weak_ptr<struct containing http_server> : compiles; member access through lock()
  - strong by-value self-capture            : still compiles and runs (semantics unchanged)

Compilation failures, wrong stdout, or a nonzero exit propagate as a nonzero exit. A running
server case binds a dynamically selected loopback port and asserts the response body.
"""
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")

PLAIN = '''struct Box { int n; }
function main() -> void {
    b := new(Box{ n: 7 });
    w := weak(b);
    print(w.expired());
    locked := w.lock();
    if (locked != null) { print(locked->n); } else { print("null"); }
    locked = null;
    b = null;
    print(w.expired());
    gone := w.lock();
    if (gone == null) { print("gone"); } else { print("alive"); }
}
'''

SERVER = '''function main() -> void : NetworkError {
    app := new(http_server());
    w := weak(app);
    print(w.expired());
    locked := w.lock();
    if (locked != null) { print("live"); } else { print("null"); }
}
'''

HOLDER_MEMBER = '''struct Holder { http_server app; }
function main() -> int : NetworkError {
    h := new(Holder{ app: http_server() });
    w := weak(h);
    locked := w.lock();
    if (locked != null) {
        locked->app.get("/", (http_request request) => { return http_text("holder-ok"); });
        locked->app.listen("127.0.0.1", %PORT%);
    }
    return 0;
}
'''

STRONG_CAPTURE = '''function main() -> int : NetworkError {
    app := new(http_server());
    app->get("/", (http_request request) => { app->stop(); return http_text("strong-ok"); });
    app->listen("127.0.0.1", %PORT%);
    return 0;
}
'''


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def compile_program(td, name, src):
    po = td / (name + ".p")
    po.write_text(src)
    exe = td / name
    b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True, timeout=180)
    if b.returncode:
        print("[%s] compile FAILED:\n%s%s" % (name, b.stdout, b.stderr))
        return None
    return exe


def compile_run(td, name, src, expected):
    exe = compile_program(td, name, src)
    if exe is None:
        return False
    r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    ok = r.returncode == 0 and r.stdout == expected
    print("[%s] stdout=%r exit=%s -> %s" % (name, r.stdout, r.returncode, "PASS" if ok else "FAIL"))
    if not ok:
        if r.stderr:
            print("  stderr:", r.stderr[-400:])
        print("  expected:", repr(expected))
    return ok


def run_server(td, name, src, path, expected_body):
    port = free_port()
    exe = compile_program(td, name, src.replace("%PORT%", str(port)))
    if exe is None:
        return False
    p = subprocess.Popen([str(exe)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 8
        while True:
            try:
                probe = socket.create_connection(("127.0.0.1", port), timeout=2)
                probe.close()
                break
            except OSError:
                if time.monotonic() > deadline:
                    print("[%s] server never listened -> FAIL" % name)
                    return False
        time.sleep(0.4)
        c = socket.create_connection(("127.0.0.1", port), timeout=5)
        c.settimeout(5)
        c.sendall(("GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" % path).encode())
        data = b""
        while True:
            v = c.recv(4096)
            if not v:
                break
            data += v
        c.close()
        body = data.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in data else b""
        head = data.split(b"\r\n", 1)[0]
        ok = head == b"HTTP/1.1 200 OK" and body == expected_body
        print("[%s] %s body=%r -> %s" % (name, head.decode(errors="replace"), body, "PASS" if ok else "FAIL"))
        return ok
    finally:
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()


def main():
    with tempfile.TemporaryDirectory(prefix="strut-weakexp-") as td:
        td = Path(td)
        results = [
            compile_run(td, "plain_struct_weak", PLAIN, "0\n7\n1\ngone\n"),
            compile_run(td, "native_server_weak", SERVER, "0\nlive\n"),
            run_server(td, "holder_member_access", HOLDER_MEMBER, "/", b"holder-ok"),
            run_server(td, "strong_capture_unchanged", STRONG_CAPTURE, "/", b"strong-ok"),
        ]
    ok = all(results)
    print("weak ownership certification -> %s" % ("PASS" if ok else "FAIL"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
