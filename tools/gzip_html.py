"""Packt das Dashboard beim Bauen und legt es als Byte-Array ab.

Quelle bleibt die lesbare src/index_html.h - daraus entsteht automatisch
src/index_html_gz.h. So gibt es weiter nur eine Datei zum Bearbeiten, und der
ESP32 liefert trotzdem gzip aus: rund ein Viertel der Bytes, entsprechend
schneller steht die Seite.
"""
import gzip, os, re

SRC = os.path.join("src", "index_html.h")
DST = os.path.join("src", "index_html_gz.h")


def build():
    raw = open(SRC, encoding="utf-8").read()
    m = re.search(r'R"HTML\((.*?)\)HTML"', raw, re.S)
    if not m:
        raise SystemExit("gzip_html: Rohstring in %s nicht gefunden" % SRC)
    html = m.group(1).encode("utf-8")
    packed = gzip.compress(html, 9)

    out = ["// Erzeugt von tools/gzip_html.py - nicht von Hand aendern.",
           "// Quelle: src/index_html.h",
           "#pragma once",
           "#include <Arduino.h>",
           "",
           "static const size_t INDEX_HTML_GZ_LEN = %d;" % len(packed),
           "static const uint8_t INDEX_HTML_GZ[] PROGMEM = {"]
    for i in range(0, len(packed), 16):
        out.append("    " + ",".join("0x%02x" % b for b in packed[i:i + 16]) + ",")
    out.append("};")
    text = "\n".join(out) + "\n"

    if not os.path.exists(DST) or open(DST, encoding="utf-8").read() != text:
        open(DST, "w", encoding="utf-8").write(text)
    print("gzip_html: %d -> %d Bytes (%.0f %%)" %
          (len(html), len(packed), 100.0 * len(packed) / len(html)))


build()
