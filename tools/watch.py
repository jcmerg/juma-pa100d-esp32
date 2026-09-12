#!/usr/bin/env python3
"""Beobachtet das Geraet und protokolliert, was bei einem Ausfall wichtig ist.

Unterscheidet drei Faelle, die von aussen gleich aussehen:
  - Neustart      -> Laufzeit springt zurueck
  - Fragmentierung-> groesster Block faellt, waehrend "frei" steht
  - unerreichbar  -> gar keine Antwort

    ./tools/watch.py [host] [intervall_s] [dauer_min]
"""
import json, sys, time, urllib.request

host = sys.argv[1] if len(sys.argv) > 1 else "juma-pa.local"
iv   = float(sys.argv[2]) if len(sys.argv) > 2 else 30
dur  = float(sys.argv[3]) if len(sys.argv) > 3 else 240

url = "http://%s/api/state" % host
print("# %-19s %7s %8s %9s %8s %5s %5s %s" %
      ("Zeit", "Laufz", "frei", "gr.Block", "Minimum", "RSSI", "Drops", "Ereignis"), flush=True)

last_up = None
misses = 0
t_end = time.time() + dur * 60
while time.time() < t_end:
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    try:
        d = json.load(urllib.request.urlopen(url, timeout=5))
    except Exception as e:
        misses += 1
        print("%-21s %7s %8s %9s %8s %5s %5s UNERREICHBAR (%d in Folge) %s" %
              (ts, "-", "-", "-", "-", "-", "-", misses, type(e).__name__), flush=True)
        time.sleep(iv)
        continue

    ev = ""
    if misses:
        ev = "wieder da nach %d Fehlversuchen" % misses
        misses = 0
    up = d.get("uptime", 0)
    if last_up is not None and up < last_up:
        ev = (ev + " | " if ev else "") + "NEUSTART (vorher %d s)" % last_up
    last_up = up

    print("%-21s %7d %8d %9d %8d %5d %5d %s" %
          (ts, up, d.get("heap", 0), d.get("heapMax", 0), d.get("heapMin", 0),
           d.get("rssi", 0), d.get("wifiDrops", 0), ev), flush=True)
    time.sleep(iv)

print("# Ende", flush=True)
