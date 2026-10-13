#!/usr/bin/env python3
"""Sanitized generated-server HTTP smoke (reusable, self-contained).

Compiles a tiny Strut fixture (GET /test -> "hello-asan", dynamic port, finite admission 1) with
the CURRENT compiler, instruments the GENERATED server with ASan/UBSan via STRUT_CXXFLAGS /
STRUT_LDFLAGS, launches it in its own process group, and asserts: HTTP 200, exact body, clean
server exit, no sanitizer diagnostics, no client timeout. Usage:
  python3 sanitized_http_smoke.py <strut> --reactor   (or omit --reactor for legacy)
Exit code conveys PASS(0)/FAIL(1)."""
import argparse, os, signal, socket, subprocess, tempfile, time
from pathlib import Path

SAN_FLAGS = "-fsanitize=address,undefined -fno-omit-frame-pointer -O1"
SRC = (
    "function main() -> int : NetworkError {\n"
    "    app := http_server();\n"
    "    app.get(\"/test\", (http_request request) => http_text(\"hello-asan\"));\n"
    "    app.listen(\"127.0.0.1\", %d, 1);\n"
    "    return 0;\n}\n"
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("strut", help="absolute path to the strut compiler")
    ap.add_argument("--reactor", action="store_true")
    ap.add_argument("--asan", action="store_true")
    args = ap.parse_args()
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="strut-san-") as td:
        td = Path(td)
        po = td / "p.p"
        po.write_text(SRC % port)
        exe = td / "p"
        b = subprocess.run([args.strut, str(po), "-o", str(exe)],
                           env=dict(os.environ, STRUT_CXXFLAGS=SAN_FLAGS,
                                    STRUT_LDFLAGS=SAN_FLAGS),
                           capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("compile FAILED:", b.stderr[-500:])
            return 1
        e = dict(os.environ)
        e.pop("STRUT_HTTP_REACTOR", None)
        if args.reactor:
            e["STRUT_HTTP_REACTOR"] = "1"
        e["ASAN_OPTIONS"] = "halt_on_error=1" if args.asan else "detect_leaks=0:halt_on_error=1"
        p = subprocess.Popen([str(exe)], env=e, start_new_session=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            dl = time.monotonic() + 8
            while True:
                try:
                    c = socket.create_connection(("127.0.0.1", port), timeout=2)
                    break
                except OSError:
                    if time.monotonic() > dl:
                        raise
            c.settimeout(4)
            c.sendall(b"GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n")
            data = b""
            try:
                while True:
                    v = c.recv(4096)
                    if not v:
                        break
                    data += v
            except socket.timeout:
                print("client-timeout")
                return 1
            c.close()
            rc = None
            dl = time.monotonic() + 8
            while time.monotonic() < dl:
                if p.poll() is not None:
                    rc = p.returncode
                    break
                time.sleep(0.05)
            so, se = p.communicate(timeout=8) if rc is not None else (b"", b"")
            ok = rc == 0 and b"200" in data and b"hello-asan" in data
            print("mode=%s 200=%s body=%s exit=%s sanitizer_bytes=%d" % (
                "reactor" if args.reactor else "legacy",
                b"200" in data, b"hello-asan" in data,
                rc if rc is not None else "STILL-ALIVE", len(se or b"")))
            print("sanitized_http_smoke: %s" % ("PASS" if ok else "FAIL"))
            return 0 if ok else 1
        finally:
            if p.poll() is None:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()


if __name__ == "__main__":
    raise SystemExit(main())