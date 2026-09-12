#pragma once
#include <stdint.h>

// Nur die Zahl hochzaehlen - so laesst sich nach einem OTA-Update pruefen,
// was wirklich laeuft.
#define FW_VERSION "1.10.2"

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

// WLAN-Ueberwachung. Der Watchdog hilft hier nicht: faellt das WLAN weg,
// laeuft die Schleife munter weiter und fuettert ihn - das Geraet ist nur
// unerreichbar. Von aussen sieht das aus wie ein Absturz.
static const uint32_t WIFI_CHECK_MS        = 5000;    // wie oft nachsehen
static const uint32_t WIFI_RETRY_MS        = 15000;   // Abstand der Reconnects
static const uint32_t WIFI_REBOOT_AFTER_MS = 300000;  // danach Neustart (5 min)

// Der ESP32 kann nicht roamen: einmal assoziiert, bleibt er an seinem AP, auch
// wenn der Pegel einbricht. Bei mehreren APs auf derselben SSID (CAPsMAN o.ae.)
// haengt er dann am schlechtesten. Faellt RSSI laenger als WIFI_ROAM_HOLD_MS
// unter WIFI_ROAM_RSSI, verbindet er neu - und sucht dabei den staerksten.
static const int32_t  WIFI_ROAM_RSSI     = -75;      // dBm
static const uint32_t WIFI_ROAM_HOLD_MS  = 60000;    // so lange muss es anliegen
static const uint32_t WIFI_ROAM_MIN_GAP  = 300000;   // hoechstens alle 5 min

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
