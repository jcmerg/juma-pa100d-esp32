#!/usr/bin/env python3
"""Try, during a TX, a series of commands that might stop the transmitter and
report which one works.

Never sends anything that turns TX ON. 'tx_enable' is always set back to true
at the end so the radio is ready to transmit normally afterwards.
"""
import base64, os, socket, struct, sys, time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 50002
wait = float(sys.argv[3]) if len(sys.argv) > 3 else 120

CANDIDATES = ["trx:0,false,tci;", "trx:0,0;", "tx_enable:0,false;"]

key = base64.b64encode(os.urandom(16)).decode()
s = socket.create_connection((host, port), timeout=6); s.settimeout(0.4)
s.sendall(("GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (host, key)).encode())
buf = b""
while b"\r\n\r\n" not in buf: buf += s.recv(4096)
acc = buf.split(b"\r\n\r\n", 1)[1]

def send(text):
    p = text.encode(); m = os.urandom(4)
    s.sendall(b"\x81" + bytes([0x80 | len(p)]) + m +
              bytes(b ^ m[i % 4] for i, b in enumerate(p)))

def pull():
    global acc
    try: acc += s.recv(65536)
    except socket.timeout: pass
    out = []
    while len(acc) >= 2:
        ln = acc[1] & 0x7F; off = 2
        if ln == 126: ln = struct.unpack(">H", acc[2:4])[0]; off = 4
        elif ln == 127: ln = struct.unpack(">Q", acc[2:10])[0]; off = 10
        if acc[1] & 0x80: off += 4
        if len(acc) < off + ln: break
        out.append(acc[off:off+ln].decode("utf-8", "replace")); acc = acc[off+ln:]
    return [c.strip() for f in out for c in f.split(";") if c.strip()]

tx = False
def refresh():
    global tx
    for c in pull():
        if c.lower().startswith("trx:"):
            tx = "true" in c.lower()

print("Waiting up to %.0f s for TX - key up and HOLD." % wait, flush=True)
t0 = time.time()
while time.time() - t0 < wait and not tx:
    refresh(); time.sleep(0.1)

if not tx:
    print("No TX seen."); s.close(); sys.exit(2)

print("TX detected.", flush=True)
winner = None
try:
    for cmd in CANDIDATES:
        print("  trying %-22s" % cmd, end="", flush=True)
        send(cmd)
        t1 = time.time()
        while time.time() - t1 < 2.5:
            refresh()
            if not tx:
                print(" -> TRANSMITTER DROPS", flush=True); winner = cmd; break
            time.sleep(0.1)
        if winner: break
        print(" -> no effect", flush=True)
finally:
    send("tx_enable:0,true;")          # always restore
    time.sleep(0.4)

print()
if winner: print("RESULT: '%s' stops the transmitter." % winner)
else:      print("RESULT: none of the commands stops a locally keyed TX.")
print("tx_enable has been set back to true.")
s.close()
sys.exit(0 if winner else 1)
