#!/bin/sh
# Flash the firmware over Wi-Fi (HTTP push to /update).
#
# espota/ArduinoOTA has the device connect back to the host - that fails when
# the ESP32 sits in an IoT VLAN which may not initiate into the LAN. This path
# runs host -> device and works there.
#
#   ./tools/flash-wifi.sh [host]        default: juma-pa.local
#
# curl runs with -4: an AAAA lookup for a .local name stalls for 5 s on
# macOS before the A record is tried, which makes the poll below time out
# even though the device is already back.
set -e
cd "$(dirname "$0")/.."

HOST="${1:-juma-pa.local}"
# Without JUMA_OTA_PASS, read it from platformio_local.ini - the same source
# the firmware was built from, so never a second place to maintain.
PASS="${JUMA_OTA_PASS:-$(sed -n "s/.*-DOTA_PASSWORD='\"\(.*\)\"'.*/\1/p" \
        platformio_local.ini 2>/dev/null | head -1)}"
: "${PASS:?no password - set JUMA_OTA_PASS or create platformio_local.ini}"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"

"$PIO" run -e esp32dev
BIN=.pio/build/esp32dev/firmware.bin

echo "before: $(curl -4 -fsS -m 5 "http://$HOST/api/state" | sed -n 's/.*"version":"\([^"]*\)".*/\1/p')"
echo "-> $HOST  ($(wc -c < "$BIN" | tr -d ' ') bytes)"
curl -4 -fsS -m 180 -u "admin:$PASS" -F "firmware=@$BIN" "http://$HOST/update"

printf 'restarting'
i=0
while [ $i -lt 20 ]; do
    sleep 1; printf '.'
    V=$(curl -4 -fsS -m 3 "http://$HOST/api/state" 2>/dev/null | sed -n 's/.*"version":"\([^"]*\)".*/\1/p') || V=""
    [ -n "$V" ] && { echo; echo "after:  $V"; exit 0; }
    i=$((i+1))
done
echo; echo "Device does not come back - check over USB." >&2
exit 1
