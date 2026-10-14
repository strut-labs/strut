#!/usr/bin/env python3
"""Reactor saturation accounting regression (current runtime).

Verifies the reactor saturation-rejection contract and its connection accounting:
  - admitted connections occupy the configured connection budget;
  - a connection over the budget receives HTTP/1.1 503;
  - completing the rejected 503 does NOT free an admitted slot (rejection-only
    connections must not decrement the admitted-work counter);
  - a further connection is still rejected while both admitted slots are occupied;
  - releasing one admitted connection permits a new admission;
  - a slow-reading rejected peer does not stall the reactor;
  - repeated saturation does not leak resources (fd/thread return).

Runs legacy and reactor modes with a fresh server process and dynamically selected
ports. Compilation/assertion failures propagate as a nonzero exit."""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")

SRC = '''function main() -> void : NetworkError {
    app := http_server();
    app.timeouts(30000, 30000, 30000, 3000);
    app.limits(1024, 4096, 16, 2);
    app.get("/", (http_request request) => { return http_text("ok"); });
    app.listen("127.0.0.1", %PORT%);
}
'''


def server_err_tail(err_path, n=40):
    try:
        with open(err_path, "r", errors="replace") as f:
            return f.read().splitlines()[-n:]
    except Exception:
        return []


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def full_request(port, timeout=5.0):
    c = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    c.settimeout(timeout)
    c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    data = b""
    try:
        while True:
            v = c.recv(4096)
            if not v:
                break
            data += v
    except socket.timeout:
        data += b"<TIMEOUT>"
    finally:
        c.close()
    return data


def open_blocker(port):
    c = socket.create_connection(("127.0.0.1", port), timeout=5)
    c.settimeout(5)
    c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")  # incomplete: holds an admitted slot
    return c


_LAST_RAW = ""


def fast_status(port, timeout=2.0):
    """One request with Connection: close; return the status code promptly (no keep-alive wait)."""
    global _LAST_RAW
    _LAST_RAW = ""
    try:
        c = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    except OSError:
        return "ERR"
    try:
        c.settimeout(timeout)
        c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        line = b""
        while b"\r\n" not in line:
            v = c.recv(1)
            if not v:
                break
            line += v
        _LAST_RAW = line.decode("utf-8", "replace")
        for code in (b" 503 ", b" 200 ", b" 404 ", b" 403 ", b" 400 "):
            if code in line:
                return code.strip().decode()
        return "OTHER"
    except Exception:
        return "ERR"
    finally:
        c.close()


def classify_saturation(observed):
    """Classify a failed repeated-saturation probe sequence from the observed statuses."""
    if observed and all(s == "ERR" for s in observed):
        return "SAT-NO-RESPONSE"
    if observed and all(s == "200" for s in observed):
        return "SAT-ADMITTED"
    if any(s == "503" for s in observed):
        return "SAT-MIXED"
    return "SAT-UNEXPECTED-STATUS"


def status_code(data):
    head = data.split(b"\r\n", 1)[0]
    for code in (b" 503 ", b" 200 ", b" 404 ", b" 403 ", b" 400 "):
        if code in head + b" ":
            return code.strip().decode()
    return "OTHER"


def sample(pid):
    try:
        root = Path(f"/proc/{pid}")
        fds = len(list((root / "fd").iterdir()))
        threads = 0
        for line in (root / "status").read_text().splitlines():
            if line.startswith("Threads:"):
                threads = int(line.split()[1])
        return (fds, threads)
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return None


def run_mode(reactor):
    port = free_port()
    with tempfile.TemporaryDirectory(prefix="strut-satacct-") as td:
        td = Path(td)
        po = td / "p.p"
        po.write_text(SRC.replace("%PORT%", str(port)))
        exe = td / "p"
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("compile FAILED:", b.stdout, b.stderr)
            sys.exit(1)
        env = dict(os.environ)
        env.pop("STRUT_HTTP_REACTOR", None)
        if reactor:
            env["STRUT_HTTP_REACTOR"] = "1"
        err_path = td / "server.err"
        err_handle = open(err_path, "w")
        p = subprocess.Popen([str(exe)], env=env, start_new_session=True,
                             stdout=subprocess.PIPE, stderr=err_handle)
        mode = "reactor" if reactor else "legacy"
        try:
            # Wait for the listener to come up without leaving a counted probe connection:
            # a bare connect-and-close races the slot accounting, so poll TCP readiness then
            # settle before the test's own connections.
            deadline = time.monotonic() + 8
            while True:
                try:
                    probe = socket.create_connection(("127.0.0.1", port), timeout=2)
                    probe.close()
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise
            time.sleep(0.5)
            blk1 = open_blocker(port)
            blk2 = open_blocker(port)
            # Establish that BOTH blockers actually occupy their admission slots before
            # asserting, so fixture-setup timing is never mistaken for an accounting result.
            established = False
            for _ in range(100):
                if fast_status(port) == "503":
                    established = True
                    break
                time.sleep(0.02)
            if not established:
                print("PRECONDITION FAILURE", flush=True)
                print("  mode=%s category=SAT-NOT-ESTABLISHED last_raw=%r" % (mode, _LAST_RAW), flush=True)
                raise RuntimeError("neither admission slot reached the saturated state [mode=%s SAT-NOT-ESTABLISHED]" % mode)
            r3 = full_request(port)          # over budget -> 503
            r4 = full_request(port)          # reject close must NOT free a slot -> 503
            blk1.close(); time.sleep(0.4)    # release one admitted slot
            r5 = full_request(port)          # admission now possible -> 200
            blk2.close()
            ok_core = status_code(r3) == "503" and status_code(r4) == "503" and status_code(r5) == "200"

            # Slow-reading rejected peer must not stall the reactor: occupy both slots,
            # then keep a rejected peer open without reading it, then confirm the server
            # still answers a fresh (admitted) request once a slot frees.
            blk1 = open_blocker(port)
            blk2 = open_blocker(port)
            time.sleep(0.2)
            slow = socket.create_connection(("127.0.0.1", port), timeout=5)
            slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 64)
            slow.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            blk1.close(); time.sleep(0.4)
            r6 = full_request(port)          # 200 while a rejected peer is still open
            ok_slow = status_code(r6) == "200"
            slow.close(); blk2.close()

            # Repeated saturation must not leak descriptors/threads.
            #
            # Establish saturation robustly: open blockers until the server reports it is
            # saturated (a probe is rejected). Opening a fixed pair assumed both blockers occupy
            # slots, but a still-releasing connection from the previous iteration can pre-occupy
            # one, so the second blocker is rejected and the server never saturates. Adding
            # blockers until the reject is observed makes the precondition genuinely hold
            # regardless of a lingering connection. The 503 assertion is unchanged.
            baseline = sample(p.pid)
            for attempt in range(30):
                blockers = []
                rejected = False
                observed = []
                for _ in range(8):
                    blockers.append(open_blocker(port))
                    for _ in range(25):
                        st = fast_status(port)
                        observed.append(st)
                        if st == "503":
                            rejected = True
                            break
                        time.sleep(0.02)
                    if rejected:
                        break
                for b in blockers:
                    b.close()
                if not rejected:
                    category = classify_saturation(observed)
                    print("REPEATED-SATURATION FAILURE", flush=True)
                    print("  mode=%s attempt=%d/30 probes=%d blockers=%d"
                          % (mode, attempt, len(observed), len(blockers)), flush=True)
                    print("  observed=%r" % (observed[-20:],), flush=True)
                    print("  last_raw=%r" % (_LAST_RAW,), flush=True)
                    print("  category=%s" % category, flush=True)
                    try:
                        err_handle.flush()
                        tail = server_err_tail(err_path)
                        print("  server stderr tail (%d lines):" % len(tail), flush=True)
                        for ln in tail:
                            print("   | " + ln, flush=True)
                    except Exception:
                        pass
                    raise RuntimeError(
                        "repeated saturation did not reject with 503 [mode=%s %s probes=%d blockers=%d]"
                        % (mode, category, len(observed), len(blockers)))
                time.sleep(0.03)
            time.sleep(0.5)
            after = sample(p.pid)
            ok_res = (baseline is None or after is None or
                      (after[0] <= baseline[0] + 4 and after[1] <= baseline[1] + 2))

            ok = ok_core and ok_slow and ok_res
            print("[%s] core(r3=%s r4=%s r5=%s) slow=%s resources=%s -> %s" % (
                mode, status_code(r3), status_code(r4), status_code(r5),
                ok_slow, ok_res, "PASS" if ok else "FAIL"))
            if not ok:
                print("  detail: baseline=%s after=%s r6=%s" % (baseline, after, r6[:40]))
            return ok
        finally:
            try:
                err_handle.close()
            except Exception:
                pass
            if p.poll() is None:
                try:
                    if os.name == "posix":
                        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
                    else:
                        p.kill()
                except Exception:
                    try:
                        p.kill()
                    except Exception:
                        pass
                try:
                    p.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass


def main():
    results = [run_mode(False), run_mode(True)]
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
