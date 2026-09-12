#!/usr/bin/env python3
"""Probiert waehrend eines TX nacheinander Kommandos durch, die den Sender
stoppen koennten, und meldet welches wirkt.

Sendet nie etwas, das TX EINschaltet. 'tx_enable' wird am Ende immer wieder
auf true gesetzt, damit der Funk hinterher normal sendebereit ist.
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

print("Warte bis zu %.0f s auf TX - bitte tasten und GEDRUECKT HALTEN." % wait, flush=True)
t0 = time.time()
while time.time() - t0 < wait and not tx:
    refresh(); time.sleep(0.1)

if not tx:
    print("Kein TX gesehen."); s.close(); sys.exit(2)

print("TX erkannt.", flush=True)
winner = None
try:
    for cmd in CANDIDATES:
        print("  probiere %-22s" % cmd, end="", flush=True)
        send(cmd)
        t1 = time.time()
        while time.time() - t1 < 2.5:
            refresh()
            if not tx:
                print(" -> SENDER FAELLT AB", flush=True); winner = cmd; break
            time.sleep(0.1)
        if winner: break
        print(" -> keine Wirkung", flush=True)
finally:
    send("tx_enable:0,true;")          # immer zuruecksetzen
    time.sleep(0.4)

print()
if winner: print("ERGEBNIS: '%s' stoppt den Sender." % winner)
else:      print("ERGEBNIS: keines der Kommandos stoppt ein lokal getastetes TX.")
print("tx_enable wurde wieder auf true gesetzt.")
s.close()
sys.exit(0 if winner else 1)
