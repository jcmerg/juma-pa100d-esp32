#!/usr/bin/env python3
"""Prueft, ob die SDR-Software ein 'trx:0,false' von einem TCI-Client annimmt.

Wartet auf TX, schickt nach kurzer Verzoegerung das Kommando und meldet, ob der
Sender daraufhin abfaellt. Sendet NIE 'trx:0,true' - es wird nur abgeschaltet.

    ./tools/tci-trx-test.py [host] [port] [wartezeit_s]
"""
import base64, hashlib, os, socket, struct, sys, time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 50002
wait = float(sys.argv[3]) if len(sys.argv) > 3 else 90

key = base64.b64encode(os.urandom(16)).decode()
s = socket.create_connection((host, port), timeout=6)
s.settimeout(0.4)
s.sendall(("GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (host, key)).encode())
buf = b""
while b"\r\n\r\n" not in buf:
    buf += s.recv(4096)
acc = buf.split(b"\r\n\r\n", 1)[1]

def send(text):
    p = text.encode()
    m = os.urandom(4)
    s.sendall(b"\x81" + bytes([0x80 | len(p)]) + m +
              bytes(b ^ m[i % 4] for i, b in enumerate(p)))

def pull():
    global acc
    try:
        acc += s.recv(65536)
    except socket.timeout:
        pass
    out = []
    while len(acc) >= 2:
        ln = acc[1] & 0x7F; off = 2
        if ln == 126: ln = struct.unpack(">H", acc[2:4])[0]; off = 4
        elif ln == 127: ln = struct.unpack(">Q", acc[2:10])[0]; off = 10
        if acc[1] & 0x80: off += 4
        if len(acc) < off + ln: break
        out.append(acc[off:off+ln].decode("utf-8", "replace")); acc = acc[off+ln:]
    return [c.strip() for f in out for c in f.split(";") if c.strip()]

print("Verbunden mit %s:%d. Warte bis zu %.0f s auf TX - jetzt kurz tasten." % (host, port, wait), flush=True)

t0 = time.time()
state = "warte"
sent_at = None
for c in []:
    pass
while time.time() - t0 < wait:
    for c in pull():
        low = c.lower()
        if not low.startswith("trx:"):
            continue
        on = "true" in low
        print("  %6.1fs  Server meldet: %s" % (time.time() - t0, c), flush=True)
        if on and state == "warte":
            state = "tx"
            time.sleep(1.0)
            send("trx:0,false;")
            sent_at = time.time()
            print("  %6.1fs  -> trx:0,false; gesendet" % (sent_at - t0), flush=True)
        elif not on and state == "tx":
            dt = time.time() - sent_at
            print("\nERGEBNIS: Sender fiel %.2f s nach dem Kommando ab." % dt, flush=True)
            print("deskHPSDR nimmt trx:0,false von einem TCI-Client AN." if dt < 2.0 else
                  "Abfall kam spaet - vermutlich hast du selbst losgelassen.", flush=True)
            s.close(); sys.exit(0)
    if state == "tx" and sent_at and time.time() - sent_at > 4:
        print("\nERGEBNIS: 4 s nach dem Kommando sendet der Funk WEITER.", flush=True)
        print("Die SDR-Software ignoriert trx:0,false von einem TCI-Client.", flush=True)
        s.close(); sys.exit(1)

print("\nKein TX gesehen - Test nicht durchgefuehrt.", flush=True)
s.close(); sys.exit(2)
