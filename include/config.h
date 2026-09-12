#pragma once
#include <stdint.h>

// Nur die Zahl hochzaehlen - so laesst sich nach einem OTA-Update pruefen,
// was wirklich laeuft.
#define FW_VERSION "1.7.1"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
// UART2 zum MAX3232-Modul. UART0 (GPIO1/3) bleibt die USB-Konsole.
// GPIO16/17 sind auf ESP32-WROOM frei; auf WROVER belegt das PSRAM diese Pins,
// dort z.B. 25/26 nehmen.
static const int  JUMA_RX_PIN = 16;   // <- MAX3232 R1OUT
static const int  JUMA_TX_PIN = 17;   // -> MAX3232 T1IN
static const long JUMA_BAUD   = 115200;

// Nur true, wenn OHNE MAX3232 gearbeitet wird (Clamp-Variante, RS-232 ist
// gegenueber TTL invertiert). Mit MAX3232 muss das false bleiben.
static const bool JUMA_INVERT = false;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// Die PA faellt 5 s nach '=O' selbstaendig auf STANDBY zurueck, wenn keine
// Nachrichten kommen (Manual, Annex D "Remote Timeout"). Das Manual empfiehlt
// ~1 s; 500 ms sind reichlich und halten das Dashboard fluessig.
static const uint32_t POLL_INTERVAL_MS = 500;

// Ab wann gilt die Statusanzeige als veraltet / PA als offline.
static const uint32_t STALE_AFTER_MS   = 3000;

// Mindestabstand zwischen zwei Kommandos an die PA.
static const uint32_t CMD_GAP_MS       = 40;

// Karenzzeit, bevor bei TCI-Verlust '=A' rausgeht. Muss deutlich ueber dem
// 5-s-Reconnect liegen, damit ein kurzer Aussetzer die PA nicht umstellt.
static const uint32_t TCI_LOST_GRACE_MS = 15000;

// Beruhigungszeit nach einer QRG-Aenderung, bevor ein Bandwechsel rausgeht.
// Verhindert Bandkommandos beim Drehen ueber eine Bandgrenze.
static const uint32_t BAND_SETTLE_MS   = 150;

// Task-Watchdog: startet neu, falls die Hauptschleife je haengenbleibt.
// Grosszuegig bemessen, weil ein Firmware-Upload lange in einem einzigen
// handleClient() steckt - der Upload-Handler fuettert den Watchdog zusaetzlich.
static const uint32_t WDT_TIMEOUT_S = 20;

// ---------------------------------------------------------------------------
// Netz
// ---------------------------------------------------------------------------
static const char*    AP_SSID   = "JUMA-PA";

// Passwoerter fuer den Fallback-AP und fuer OTA-Updates.
// UNBEDINGT eigene setzen - entweder hier, oder ohne die Datei anzufassen in
// der platformio.ini:
//   build_flags = -DAP_PASSWORD='"..."' -DOTA_PASSWORD='"..."'
#ifndef AP_PASSWORD
#define AP_PASSWORD  "changeme01"     // mindestens 8 Zeichen
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "changeme"
#endif
static const char*    AP_PASS   = AP_PASSWORD;
static const uint16_t HTTP_PORT = 80;
static const uint16_t WS_PORT   = 81;

// Netzname fuer mDNS, OTA und DHCP - Vorgabe, im Web-UI aenderbar:
// http://<hostname>.local/
static const char*    HOSTNAME_DEFAULT = "juma-pa";
static const char*    OTA_PASS    = OTA_PASSWORD;
static const uint16_t TELNET_PORT = 23;

// TCI-Default. TCI ist nicht auf ExpertSDR beschraenkt - deskHPSDR und andere
// sprechen es ebenfalls, und die Ports unterscheiden sich (ExpertSDR3 40001,
// hier 50002). Host und Port werden im Web-UI gesetzt und in NVS gehalten.
static const uint16_t TCI_DEFAULT_PORT = 50002;
