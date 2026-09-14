#pragma once
#include <stdint.h>

// Bump the number on every build you want to recognise on the device - that is
// how you check what is actually running after an OTA update.
#define FW_VERSION "1.15.4"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
// UART2 to the MAX3232 module. UART0 (GPIO1/3) stays the USB console.
// GPIO16/17 are free on ESP32-WROOM; on WROVER the PSRAM occupies these pins,
// use e.g. 25/26 there.
static const int  JUMA_RX_PIN = 16;   // <- MAX3232 R1OUT
static const int  JUMA_TX_PIN = 17;   // -> MAX3232 T1IN
static const long JUMA_BAUD   = 115200;

// Only true when running WITHOUT a MAX3232 (clamp variant - RS-232 is inverted
// with respect to TTL). With a MAX3232 this must stay false.
static const bool JUMA_INVERT = false;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// The PA reverts to STANDBY by itself 5 s after '=O' when no messages arrive
// (manual, annex D "Remote Timeout"). The manual recommends ~1 s; 500 ms is
// plenty and keeps the dashboard fluid.
static const uint32_t POLL_INTERVAL_MS = 500;

// When the status display counts as stale / the PA as offline.
static const uint32_t STALE_AFTER_MS   = 3000;

// Read at most this many bytes from the PA per loop pass. Without a limit a
// noisy line - for instance when the PA is switched off and its RS-232 driver
// leaves the line undefined - can block the main loop indefinitely: at 115200
// baud bytes arrive faster than an unbounded loop gets rid of them, so the web
// server and the watchdog never get their turn. A status reply is 45 bytes, so
// this is generous.
static const uint16_t RX_MAX_PER_LOOP = 256;

// Minimum spacing between two commands to the PA.
static const uint32_t CMD_GAP_MS       = 40;

// Grace period before '=A' goes out after losing TCI. Must sit well above the
// 5 s reconnect interval so a brief dropout does not reconfigure the PA.
static const uint32_t TCI_LOST_GRACE_MS = 15000;

// Settle time after a frequency change before a band command goes out.
// Prevents band commands while tuning across a band edge.
static const uint32_t BAND_SETTLE_MS   = 150;

// Wi-Fi supervision. The watchdog does not help here: if Wi-Fi drops, the loop
// keeps running happily and keeps feeding it - the device is merely
// unreachable. From the outside that looks exactly like a crash.
static const uint32_t WIFI_CHECK_MS        = 5000;    // how often to look
static const uint32_t WIFI_RETRY_MS        = 15000;   // spacing of reconnects
static const uint32_t WIFI_REBOOT_AFTER_MS = 300000;  // then reboot (5 min)

// The ESP32 cannot roam: once associated it stays with its AP even when the
// signal collapses. With several APs on one SSID (CAPsMAN and the like) it
// then clings to the worst one. If RSSI stays below WIFI_ROAM_RSSI for longer
// than WIFI_ROAM_HOLD_MS it reconnects - and picks the strongest AP again.
static const int32_t  WIFI_ROAM_RSSI     = -75;      // dBm
static const uint32_t WIFI_ROAM_HOLD_MS  = 60000;    // must persist this long
static const uint32_t WIFI_ROAM_MIN_GAP  = 300000;   // at most every 5 min

// Task watchdog: reboots should the main loop ever get stuck. Generously
// dimensioned because a firmware upload spends a long time inside a single
// handleClient() - the upload handler feeds the watchdog as well.
static const uint32_t WDT_TIMEOUT_S = 20;

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------
static const char*    AP_SSID   = "JUMA-PA";

// Passwords for the fallback AP and for OTA updates.
// SET YOUR OWN - either here, or without touching this file, in
// platformio.ini:
//   build_flags = -DAP_PASSWORD='"..."' -DOTA_PASSWORD='"..."'
#ifndef AP_PASSWORD
#define AP_PASSWORD  "changeme01"     // at least 8 characters
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "changeme"
#endif
static const char*    AP_PASS   = AP_PASSWORD;
static const uint16_t HTTP_PORT = 80;
static const uint16_t WS_PORT   = 81;

// Network name for mDNS, OTA and DHCP - a default, changeable in the web UI:
// http://<hostname>.local/
static const char*    HOSTNAME_DEFAULT = "juma-pa";
static const char*    OTA_PASS    = OTA_PASSWORD;
static const uint16_t TELNET_PORT = 23;

// TCI default. TCI is not limited to ExpertSDR - deskHPSDR and others speak it
// too, and the ports differ (ExpertSDR3 40001, here 50002). Host and port are
// set in the web UI and kept in NVS.
static const uint16_t TCI_DEFAULT_PORT = 50002;
