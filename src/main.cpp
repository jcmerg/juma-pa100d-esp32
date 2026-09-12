// JUMA PA-100D Controller auf ESP32.
//
//   - RS-232 zur PA ueber MAX3232 an UART2 (115200 8N1, Kommandos "=X\n\r")
//   - Web-Dashboard per WLAN (HTTP :80, WebSocket :81)
//   - Bandwahl per TCI-Client (ExpertSDR, deskHPSDR, Thetis)
//
// OPERATE/STANDBY kommt strikt aus dem Geraetestatus, gesendet wird explizit
// =O oder =S statt eines Toggles - so muss der Zustand nie erraten werden.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "juma.h"
#include "tci.h"
#include "bands.h"
#include "web.h"
#include "console.h"

static uint32_t lastBandCmd  = 0;

// ---------------------------------------------------------------------------
// WLAN-Ueberwachung
// ---------------------------------------------------------------------------
static uint32_t wifiLastOk   = 0;
static uint32_t wifiRetryAt  = 0;
static uint32_t wifiCheckAt  = 0;
static uint32_t wifiDrops_   = 0;
static bool     wifiWasUp    = false;

uint32_t wifiDropCount() { return wifiDrops_; }
uint32_t wifiDownSecs()  {
    return (WiFi.status() == WL_CONNECTED) ? 0 : (millis() - wifiLastOk) / 1000;
}

static void wifiSupervise() {
    if (millis() - wifiCheckAt < WIFI_CHECK_MS) return;
    wifiCheckAt = millis();

    // Ohne konfigurierte SSID ist der AP-Modus der gewollte Zustand.
    if (!cfg.ssid.length()) return;

    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasUp) log_i("WLAN wieder da: %s", WiFi.localIP().toString().c_str());
        wifiWasUp  = true;
        wifiLastOk = millis();
        return;
    }

    if (wifiWasUp) { wifiDrops_++; wifiWasUp = false; log_w("WLAN weg"); }

    if (WiFi.getMode() == WIFI_STA && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        // Neu verbinden statt reconnect(): reconnect() nimmt den zuletzt
        // benutzten AP, begin() sucht mit den Einstellungen oben den staerksten.
        WiFi.disconnect();
        WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    }

    // Kommt es laenger nicht zurueck, hilft nur ein Neustart - unerreichbar
    // nuetzt das Geraet niemandem, und ein Neustart versucht es sauber neu.
    if (millis() - wifiLastOk > WIFI_REBOOT_AFTER_MS) {
        log_e("WLAN seit %lu s weg - Neustart", (unsigned long)wifiDownSecs());
        Serial.flush();
        delay(100);
        ESP.restart();
    }
}
static bool     tciWasUp     = false;   // war TCI ueberhaupt schon mal da?
static uint32_t tciLostAt    = 0;
static bool     autoSelSent  = false;   // '=A' nach dem Verlust schon raus?
static uint32_t lastForceM   = 0;       // letzter Versuch, die PA nach M zu holen
static uint8_t  forceMTries  = 0;

// ---------------------------------------------------------------------------
// Automatische Bandwahl: TCI-QRG -> "=Bn"
// ---------------------------------------------------------------------------
static void bandControl() {
    const JumaStatus& s = juma.status();

    // Der Hinweis im Dashboard zeigt immer den AKTUELLEN Grund, nicht ein
    // einmaliges Ereignis - sonst steht nach dem Umschalten eine veraltete
    // Meldung da, und direkt nach dem Start gar keine.
    if (!cfg.autoband)     { webSetNote("aboff");     return; }
    if (!tci.enabled())    { webSetNote("tcioff");    return; }
    if (!tci.connected()) {
        // '=Bn' ist laut Manual eine MANUELLE Bandwahl - die PA springt dabei
        // von A nach M und bleibt bei einem TCI-Ausfall deshalb auf dem
        // zuletzt kommandierten Band stehen. '=A' gibt ihr die automatische
        // Bandwahl zurueck; welche Methode sie dann nutzt (F-Sense, FT-817,
        // CAT, ...), steht in ihrer eigenen Konfiguration.
        if (!tciLostAt) tciLostAt = millis();
        bool due = cfg.tciLostAuto && tciWasUp && !autoSelSent &&
                   (millis() - tciLostAt >= TCI_LOST_GRACE_MS) &&
                   juma.online() && !s.tx;
        if (due) {
            juma.setAutoSelect();
            autoSelSent = true;
        }
        webSetNote(autoSelSent ? "tciauto" : "tcidis");
        return;
    }
    tciWasUp    = true;
    tciLostAt   = 0;
    autoSelSent = false;

    uint32_t hz = tci.freqHz();
    if (!hz)               { webSetNote("tcinofreq"); return; }

    uint8_t target = bandFromHz(hz);
    if (target == BAND_NONE) { webSetNote("unsupported", bandNameFromHz(hz)); return; }

    // Beruhigungszeit: erst senden, wenn die QRG stabil steht. Sonst feuert
    // jedes Drehen ueber eine Bandgrenze ein Bandkommando.
    if (millis() - tci.freqSetAt() < BAND_SETTLE_MS) return;

    // Niemals waehrend TX umschalten - weder laut TCI noch laut PA. Ohne
    // Meldung: dass man beim Senden nicht das Band wechselt, ist selbstver-
    // staendlich, und der Hinweis stand nur im Weg.
    if (tci.tx() || s.tx)  { return; }
    if (!juma.online())    { webSetNote("paoff");     return; }

    if (s.band == target) {
        // Solange der ESP32 das Band bestimmt, darf die PA nicht gleichzeitig
        // selbst waehlen - sonst zieht F-Sense sie irgendwann woanders hin.
        // Ein '=Bn' auf das laufende Band holt sie von A nach M, ohne das Band
        // zu aendern. Begrenzt oft versuchen: nimmt die PA es nicht an, wird
        // gemeldet statt endlos gefeuert.
        if (s.autoSel) {
            if (forceMTries < 3 && millis() - lastForceM >= 5000) {
                juma.setBand(target);
                lastForceM = millis();
                forceMTries++;
            }
            webSetNote(forceMTries >= 3 ? "selstuck" : "bandok", bandName(target));
        } else {
            forceMTries = 0;
            webSetNote("bandok", bandName(target));
        }
        return;
    }
    if (millis() - lastBandCmd < 1000) return;      // nicht dauerfeuern

    juma.setBand(target);
    lastBandCmd = millis();
    webSetNote("bandset", bandName(target));
}

// ---------------------------------------------------------------------------
static void wifiBegin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(cfg.hostname.c_str());

    if (cfg.ssid.length()) {
        // Der Default ist WIFI_FAST_SCAN: damit nimmt der ESP32 den ERSTEN
        // gefundenen Zugangspunkt der SSID, nicht den staerksten. Bei mehreren
        // APs auf derselben SSID landet er so leicht auf dem schwaechsten -
        // mit Paketverlust, der dann nach einem Firmwarefehler aussieht.
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        // Ohne WLAN einen eigenen AP aufspannen, damit das Web-UI zum
        // Konfigurieren immer erreichbar ist.
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        log_w("Kein WLAN - AP '%s' auf %s", AP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
        // NACH dem Verbinden - vorher gesetzt wird es von WiFi.begin() wieder
        // verworfen. Mit Modem-Sleep wartet jeder Roundtrip auf das naechste
        // Beacon (~100 ms); die 23-kB-Seite brauchte dadurch 6-20 s.
        WiFi.setSleep(false);
        wifiWasUp = true;
        log_i("WLAN verbunden: %s, Sleep aus", WiFi.localIP().toString().c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("JUMA PA-100D Controller " FW_VERSION);

    settingsLoad();
    juma.begin();
    wifiBegin();
    // OTA: flashen ueber WLAN, damit am Verstaerker kein USB-Kabel mehr
    // haengen muss. Vor dem Update die PA auf STANDBY - waehrend des Flashens
    // laeuft loop() nicht, die PA wuerde ohnehin nach 5 s selbst zurueckfallen,
    // aber so ist der Zustand definiert statt abgelaufen.
    ArduinoOTA.setHostname(cfg.hostname.c_str());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {
        if (cfg.otaStandby) {
            juma.sendNow("=S");
            Serial.println("OTA-Update startet, PA auf STANDBY");
        }
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA-Fehler %u\n", e); });
    ArduinoOTA.begin();

    if (MDNS.begin(cfg.hostname.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        MDNS.addService("telnet", "tcp", TELNET_PORT);
    }

    tci.begin();
    tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
    webBegin();

    // Watchdog erst hier scharf schalten - der WLAN-Verbindungsversuch oben
    // darf bis zu 15 s dauern.
    esp_task_wdt_init(WDT_TIMEOUT_S, true);   // true = Reboot bei Ablauf
    esp_task_wdt_add(NULL);                   // loopTask ueberwachen
    Serial.printf("Watchdog aktiv (%lu s)\n", (unsigned long)WDT_TIMEOUT_S);

    wifiLastOk = millis();
    consoleBegin();
}

void loop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    juma.loop();
    tci.loop();
    webLoop();
    consoleLoop();
    wifiSupervise();
    bandControl();
}
