#!/bin/sh
# Firmware ueber WLAN aufspielen (HTTP-Push an /update).
#
# espota/ArduinoOTA laesst das Geraet zum Host zurueckverbinden - das scheitert,
# wenn der ESP32 in einem IoT-VLAN haengt, das nicht ins LAN initiieren darf.
# Dieser Weg laeuft in der Richtung Host -> Geraet und funktioniert dort.
#
#   ./tools/flash-wifi.sh [host]        default: juma-pa.local
set -e
cd "$(dirname "$0")/.."

HOST="${1:-juma-pa.local}"
# Ohne JUMA_OTA_PASS aus platformio_local.ini lesen - dieselbe Quelle, aus der
# die Firmware gebaut wurde, also nie ein zweiter Ort zum Pflegen.
PASS="${JUMA_OTA_PASS:-$(sed -n "s/.*-DOTA_PASSWORD='\"\(.*\)\"'.*/\1/p" \
        platformio_local.ini 2>/dev/null | head -1)}"
: "${PASS:?kein Passwort - JUMA_OTA_PASS setzen oder platformio_local.ini anlegen}"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"

"$PIO" run -e esp32dev
BIN=.pio/build/esp32dev/firmware.bin

echo "vorher:  $(curl -fsS -m 5 "http://$HOST/api/state" | sed -n 's/.*"version":"\([^"]*\)".*/\1/p')"
echo "-> $HOST  ($(wc -c < "$BIN" | tr -d ' ') Bytes)"
curl -fsS -m 180 -u "admin:$PASS" -F "firmware=@$BIN" "http://$HOST/update"

printf 'Neustart'
i=0
while [ $i -lt 20 ]; do
    sleep 1; printf '.'
    V=$(curl -fsS -m 2 "http://$HOST/api/state" 2>/dev/null | sed -n 's/.*"version":"\([^"]*\)".*/\1/p') || V=""
    [ -n "$V" ] && { echo; echo "nachher: $V"; exit 0; }
    i=$((i+1))
done
echo; echo "Geraet meldet sich nicht zurueck - per USB pruefen." >&2
exit 1
