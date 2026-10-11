#!/usr/bin/env python3
"""Corrected reactor/legacy WebSocket close-handshake probe: protocol-correct MASKED client
frames (reusing the certification client_frame semantics), full HTTP header termination, a
buffered frame parser feeding leftover bytes from the handshake recv, isolated ports, process
cleanup. Records each closing-handshake stage to find the FIRST divergence."""
import os, signal, socket, struct, subprocess, sys, tempfile, time
from pathlib import Path

PORT = int(os.environ.get("PORT", "0"))
STRUT = os.environ.get("STRUT") or ("build/strut" if Path("build/strut").exists()
                                   else "/home/nick/relgcc/strut")

def mask_payload(payload, mask):
    return bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

def client_frame(opcode, payload=b"", fin=True, masked=True):
    first = 0x80 | opcode if fin else opcode
    mask = b"\x11\x22\x33\x44"
    ln = len(payload)
    code = ln if ln <= 125 else (126 if ln <= 65535 else 127)
    out = bytes([first, (0x80 if masked else 0) | code])
    if code == 126:
        out += struct.pack("!H", ln)
    elif code == 127:
        out += struct.pack("!Q", ln)
    if masked:
        out += mask + mask_payload(payload, mask)
    else:
        out += payload
    return out

def read_http_upgrade(c):
    data = b""
    c.settimeout(4)
    while b"\r\n\r\n" not in data:
        v = c.recv(4096)
        if not v:
            return data, b"", False
        data += v
    head, rest = data.split(b"\r\n\r\n", 1)
    return head + b"\r\n\r\n", rest, (b"101" in head and b"Sec-WebSocket-Accept:" in head)

class FrameReader:
    def __init__(self, conn, buf=b""):
        self.c = conn; self.buf = buf; self.c.settimeout(3)
    def read(self):
        while len(self.buf) < 2:
            v = self.c.recv(4096)
            if not v:
                return None
            self.buf += v
        hdr = self.buf[:2]
        ln = hdr[1] & 0x7F
        offset = 2
        if ln == 126:
            while len(self.buf) < 4: self.buf += self.c.recv(4096)
            ln = struct.unpack("!H", self.buf[2:4])[0]; offset = 4
        elif ln == 127:
            while len(self.buf) < 10: self.buf += self.c.recv(4096)
            ln = struct.unpack("!Q", self.buf[2:10])[0]; offset = 10
        need = offset + ln
        while len(self.buf) < need:
            v = self.c.recv(4096)
            if not v:
                return None
            self.buf += v
        payload = self.buf[offset:need]
        self.buf = self.buf[need:]
        return hdr[0] & 0x0F, payload

def run():
    with tempfile.TemporaryDirectory(prefix="strut-wsclose-") as td:
        td = Path(td)
        import re
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0)); used = s.getsockname()[1]
        if PORT:
            used = PORT
        src = (Path(__file__).resolve().parent / "reactor_ws_close_probe.p").read_text()
        src = re.sub(r'\s*app\.listen\("[0-9.]+", [0-9]+, [0-9]+\);',
                     'app.listen("127.0.0.1", %d, 8);' % used, src)
        po = td / "probe.p"; po.write_text(src)
        exe = td / "probe"
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True)
        if b.returncode != 0:
            print("compile FAILED:", b.stderr[-600:]); sys.exit(1)
        results = {}
        for reactor in (False, True):
            e = dict(os.environ); e.pop("STRUT_HTTP_REACTOR", None)
            if reactor: e["STRUT_HTTP_REACTOR"] = "1"
            p = subprocess.Popen([str(exe)], env=e, start_new_session=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + 6
                while True:
                    try: c = socket.create_connection(("127.0.0.1", used), timeout=2); break
                    except OSError:
                        if time.monotonic() > deadline: raise
                with c:
                    c.sendall(b"GET /explicit-close HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\n"
                              b"Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
                              b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")
                    head, rest, ok_up = read_http_upgrade(c)
                    if not ok_up:
                        results["reactor" if reactor else "legacy"] = {"upgrade": head[:40].decode(errors="replace")}
                    else:
                        fr = FrameReader(c, rest)
                        op1, pl1 = fr.read()                      # server CLOSE
                        close_code = struct.unpack("!H", pl1[:2])[0] if pl1 and len(pl1) >= 2 else None
                        c.sendall(client_frame(9, b"after-close"))  # masked Ping
                        op2, pl2 = fr.read()                      # Pong
                        pong_ok = (op2 == 10)
                        c.sendall(client_frame(8, struct.pack("!H", 1000)))  # masked CLOSE
                        # wait for TCP EOF after close handshake
                        c.settimeout(3)
                        tail = b""
                        try:
                            while True:
                                v = c.recv(4096)
                                if not v: break
                                tail += v
                            eof = "eof"
                        except socket.timeout:
                            eof = "timeout"
                        results["reactor" if reactor else "legacy"] = {
                            "upgrade": "101+accept", "server_close_op": (op1, close_code),
                            "pong": pong_ok, "eof_after_close": eof, "tail_len": len(tail)}
                sys.stdout.flush()
            finally:
                try: os.killpg(p.pid, signal.SIGKILL)
                except Exception: pass
                p.wait()
        for mode, r in results.items():
            print("[%s] %s" % (mode, r))
        return 0

if __name__ == "__main__":
    sys.exit(run())
