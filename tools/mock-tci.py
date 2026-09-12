#!/usr/bin/env python3
"""Minimaler TCI-Server zum Testen des ESP32-Clients ohne echte SDR-Software.

Spricht genug WebSocket, um den Handshake und Textframes zu bedienen, und
schickt eine realistische TCI-Begruessung plus vfo/trx-Nachrichten.

    ./tools/mock-tci.py [port] [freq_hz ...]
"""
import base64, hashlib, socket, struct, sys, threading, time

GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

GREETING = [
    "protocol:ExpertSynchroInterface,1.9",
    "device:MockTRX",
    "receive_only:false",
    "trx_count:2",
    "channels_count:2",
    "vfo_limits:0,65000000",
    "if_limits:-48000,48000",
    "modulations_list:lsb,usb,cw,nfm,digl,digu,am",
    "ready",
    "start",
]

def ws_send(sock, text):
    p = text.encode()
    h = b"\x81"
    n = len(p)
    if n < 126:
        h += bytes([n])
    else:
        h += b"\x7e" + struct.pack(">H", n)
    sock.sendall(h + p)

def handshake(sock):
    req = b""
    while b"\r\n\r\n" not in req:
        c = sock.recv(1024)
        if not c:
            return False
        req += c
    key = None
    for line in req.split(b"\r\n"):
        if line.lower().startswith(b"sec-websocket-key:"):
            key = line.split(b":", 1)[1].strip()
    if not key:
        return False
    acc = base64.b64encode(hashlib.sha1(key + GUID).digest())
    sock.sendall(b"HTTP/1.1 101 Switching Protocols\r\n"
                 b"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                 b"Sec-WebSocket-Accept: " + acc + b"\r\n\r\n")
    return True

def reader(sock):
    """Frames des Clients wegraeumen, damit nichts blockiert."""
    try:
        while True:
            h = sock.recv(2)
            if len(h) < 2:
                return
            ln = h[1] & 0x7F
            masked = h[1] & 0x80
            if ln == 126:
                ln = struct.unpack(">H", sock.recv(2))[0]
            elif ln == 127:
                ln = struct.unpack(">Q", sock.recv(8))[0]
            mask = sock.recv(4) if masked else b""
            data = b""
            while len(data) < ln:
                data += sock.recv(ln - len(data))
            if masked:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            if data:
                print("  <- %r" % data.decode("utf-8", "replace"), flush=True)
    except OSError:
        pass

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 50002
    freqs = [int(x) for x in sys.argv[2:]] or [14074000]

    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(4)
    print("Mock-TCI hoert auf 0.0.0.0:%d" % port, flush=True)

    while True:
        sock, addr = srv.accept()
        print("Verbindung von %s:%d" % addr, flush=True)
        if not handshake(sock):
            print("  Handshake fehlgeschlagen", flush=True)
            sock.close()
            continue
        print("  WebSocket offen", flush=True)
        threading.Thread(target=reader, args=(sock,), daemon=True).start()
        try:
            for g in GREETING:
                ws_send(sock, g + ";")
            ws_send(sock, "trx:0,false;")
            for f in freqs:
                ws_send(sock, "dds:0,%d;" % f)
                ws_send(sock, "if:0,0,0;")
                ws_send(sock, "vfo:0,0,%d;" % f)
                print("  -> vfo %.4f MHz" % (f / 1e6), flush=True)
                time.sleep(6)
            while True:
                time.sleep(5)
                ws_send(sock, "vfo:0,0,%d;" % freqs[-1])
        except OSError as e:
            print("  Verbindung weg: %s" % e, flush=True)
        finally:
            sock.close()

if __name__ == "__main__":
    main()
