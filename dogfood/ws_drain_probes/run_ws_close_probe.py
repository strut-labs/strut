import socket,subprocess,time,os,struct
port = int(os.environ.get('PORT', 49324))
def frame(op,payload=b""):
    b=bytes([0x80|op,0x80|len(payload)])+payload
    return b
def probe(reactor):
    env=dict(os.environ); env.pop('STRUT_HTTP_REACTOR',None)
    if reactor: env['STRUT_HTTP_REACTOR']='1'
    p=subprocess.Popen(['/tmp/opencode/close_probe'],env=env)
    try:
        deadline=time.monotonic()+6
        while True:
            try: c=socket.create_connection(('127.0.0.1',port),timeout=2); break
            except Exception:
                if time.monotonic()>deadline: p.kill(); return "no-connect"
        with c:
            c.sendall(b'GET /explicit-close HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n')
            data=b''
            c.settimeout(3)
            while b'101' not in data:
                v=c.recv(4096)
                if not v: break
                data+=v
            if b'101' not in data:
                return "no-101 "+data[:30].decode(errors='replace')
            # read server close frame
            c.settimeout(3)
            hdr=c.recv(2); ln=hdr[1]&0x7f
            payload=c.recv(ln if ln else 1)
            # send client close
            c.sendall(frame(8, struct.pack('!H',1000)))
            # now recv(1) with short timeout: legacy => b'' (closed), reactor => hangs
            try:
                extra=c.recv(1)
                return "recv-after-close=%d bytes" % len(extra)
            except socket.timeout:
                return "TIMEOUT (socket never closed after WS close handshake)"
    finally:
        p.kill()
print("legacy :", probe(False))
print("reactor:", probe(True))
