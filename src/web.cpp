#include "web.h"
#include "config.h"
#include "juma.h"
#include "tci.h"
#include "bands.h"
#include "index_html.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_task_wdt.h>

Settings cfg;

// mDNS und DHCP vertragen nur Kleinbuchstaben, Ziffern und Bindestriche.
// Alles andere wird verworfen statt abgelehnt - ein leerer Name faellt auf
// die Vorgabe zurueck, damit das Geraet nie namenlos im Netz haengt.
String sanitizeHostname(const String& in) {
    String out;
    for (size_t i = 0; i < in.length() && out.length() < 32; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') out += c;
    }
    while (out.length() && out[0] == '-')                  out.remove(0, 1);
    while (out.length() && out[out.length() - 1] == '-')   out.remove(out.length() - 1);
    return out.length() ? out : String(HOSTNAME_DEFAULT);
}
static WebServer       http(HTTP_PORT);
static WebSocketsServer wsSrv(WS_PORT);
static Preferences      prefs;
static String           noteCode, noteArg;
static uint32_t         lastPush = 0;

void webSetNote(const char* code, const char* arg) {
    // bandControl() ruft das in jedem Durchlauf - ohne den Vergleich waere das
    // unnoetige String-Zuweisung auf dem Heap.
    if (noteCode == code && noteArg == arg) return;
    noteCode = code;
    noteArg  = arg;
}

// --- Settings -------------------------------------------------------------

void settingsLoad() {
    // beschreibbar oeffnen, damit der Namespace beim ersten Start angelegt
    // wird - readOnly loggt sonst ein irritierendes "nvs_open failed"
    prefs.begin("juma", false);
    // isKey() vorweg, sonst loggt getString() beim ersten Start je Schluessel
    // ein [E] nvs_get_str ... NOT_FOUND - das sieht nach Defekt aus, ist aber
    // nur der noch leere Namespace.
    cfg.hostname = prefs.isKey("host") ? prefs.getString("host") : String(HOSTNAME_DEFAULT);
    if (prefs.isKey("ssid"))     cfg.ssid     = prefs.getString("ssid");
    if (prefs.isKey("pass"))     cfg.pass     = prefs.getString("pass");
    if (prefs.isKey("tcihost"))  cfg.tciHost  = prefs.getString("tcihost");
    cfg.tciPort  = prefs.getUShort("tciport", TCI_DEFAULT_PORT);
    cfg.tciEn    = prefs.getBool("tcien", false);
    cfg.autoband = prefs.getBool("autoband", false);
    cfg.otaStandby = prefs.getBool("otastdby", true);
    cfg.tciLostAuto = prefs.getBool("tcilosta", false);
    prefs.end();
}

void settingsSave() {
    prefs.begin("juma", false);
    prefs.putString("host",    cfg.hostname);
    prefs.putString("ssid",    cfg.ssid);
    prefs.putString("pass",    cfg.pass);
    prefs.putString("tcihost", cfg.tciHost);
    prefs.putUShort("tciport", cfg.tciPort);
    prefs.putBool("tcien",     cfg.tciEn);
    prefs.putBool("autoband",  cfg.autoband);
    prefs.putBool("otastdby", cfg.otaStandby);
    prefs.putBool("tcilosta", cfg.tciLostAuto);
    prefs.end();
}

// --- Zustand als JSON ----------------------------------------------------

static void buildState(String& out) {
    const JumaStatus& s = juma.status();
    JsonDocument d;
    d["online"]  = juma.online();
    d["operate"] = s.operate;
    d["tx"]      = s.tx;
    d["autoSel"] = s.autoSel;
    d["celsius"] = s.celsius;
    d["band"]    = s.band;
    d["bandName"]= bandName(s.band);
    d["gain"]    = s.gain;
    d["swr"]     = s.swr;
    d["volts"]   = s.volts;
    d["amps"]    = s.amps;
    d["watts"]   = s.watts;
    d["temp"]    = s.temp;
    d["fan"]     = s.fan;
    d["alarms"]  = s.alarms;
    d["raw"]     = s.raw;
    d["autoband"]= cfg.autoband;
    d["tciConn"] = tci.connected();
    d["tciMsgs"] = tci.rxMsgs();
    d["tciDrops"] = tci.drops();
    d["tciHz"]   = tci.freqHz();
    d["tciBandName"] = bandNameFromHz(tci.freqHz());
    d["tciHost"] = cfg.tciHost;
    d["tciPort"] = cfg.tciPort;
    d["tciEnabled"] = cfg.tciEn;
    d["otaStandby"] = cfg.otaStandby;
    d["tciLostAuto"] = cfg.tciLostAuto;
    d["ssid"]    = cfg.ssid;
    d["hostname"] = cfg.hostname;
    d["note"]    = noteCode;
    d["noteArg"] = noteArg;
    d["version"] = FW_VERSION;
    serializeJson(d, out);
}

// --- Kommandos vom Browser ("name:wert") ---------------------------------

static void handleCmd(const char* text) {
    const char* colon = strchr(text, ':');
    if (!colon) return;
    String name(text, colon - text);
    long v = strtol(colon + 1, nullptr, 10);

    if (name == "band")        juma.setBand((uint8_t)v);
    else if (name == "gain")   juma.setGain((uint8_t)v);
    else if (name == "operate")juma.setOperate(v != 0);
    else if (name == "clear")  juma.clearAlarm();
    else if (name == "poweroff") juma.powerOff(false);
    else if (name == "bandsel") {
        // Nur sinnvoll, solange der ESP32 das Band nicht selbst bestimmt -
        // sonst haette das naechste '=Bn' die Umschaltung sofort wieder
        // aufgehoben. Die UI sperrt das auch, hier steht es gegen veraltete
        // Browserzustaende.
        if (cfg.autoband) {
            log_w("Bandwahl-Umschaltung abgelehnt: TCI-Bandwahl ist aktiv");
            return;
        }
        // Fuer "Automatik" gibt es '=A'. Fuer "Manuell" gibt es kein eigenes
        // Kommando - '=Bn' IST die manuelle Bandwahl. Also das Band schicken,
        // auf dem die PA ohnehin steht: kein Bandwechsel, aber A wird zu M.
        if (v) {
            juma.setAutoSelect();
        } else {
            uint8_t b = juma.status().band;
            if (b >= 1 && b <= 9) juma.setBand(b);
            else log_w("Bandwahl manuell: PA meldet Band %u, kein =Bn moeglich", b);
        }
    }
    else if (name == "autoband") {
        cfg.autoband = (v != 0);
        settingsSave();
        // den Hinweis setzt bandControl() im naechsten Durchlauf selbst
    }
}

// --- HTTP ----------------------------------------------------------------

static void onConfig() {
    if (http.hasArg("hostname")) cfg.hostname = sanitizeHostname(http.arg("hostname"));
    if (http.hasArg("ssid"))    cfg.ssid    = http.arg("ssid");
    if (http.hasArg("pass") && http.arg("pass").length()) cfg.pass = http.arg("pass");
    if (http.hasArg("tcihost")) cfg.tciHost = http.arg("tcihost");
    if (http.hasArg("tciport")) cfg.tciPort = (uint16_t)http.arg("tciport").toInt();
    if (http.hasArg("tcien"))   cfg.tciEn   = (http.arg("tcien") == "1");
    if (http.hasArg("otastby")) cfg.otaStandby = (http.arg("otastby") == "1");
    if (http.hasArg("tcilosta")) cfg.tciLostAuto = (http.arg("tcilosta") == "1");
    settingsSave();
    // sprachneutral - den Text macht das Dashboard, sonst steht im englischen
    // UI ploetzlich Deutsch
    http.send(200, "text/plain", "saved");
    delay(1000);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// OTA per HTTP-Push.
//
// ArduinoOTA/espota laesst das GERAET zum Host zurueckverbinden - das scheitert,
// sobald der ESP32 in einem IoT-VLAN haengt, das nicht ins LAN initiieren darf.
// Dieser Endpunkt laeuft in der Richtung, die ohnehin funktioniert:
//   curl -u admin:<pass> -F firmware=@firmware.bin http://<ip>/update
// ---------------------------------------------------------------------------
static bool otaAuthOk = false;

static void onUpdateEnd() {
    if (!http.authenticate("admin", OTA_PASS)) { http.requestAuthentication(); return; }
    bool ok = otaAuthOk && !Update.hasError();
    http.sendHeader("Connection", "close");
    http.send(ok ? 200 : 500, "text/plain; charset=utf-8",
              ok ? "OK - Neustart\n" : "FEHLGESCHLAGEN\n");
    if (ok) { delay(300); ESP.restart(); }
}

static void onUpdateChunk() {
    HTTPUpload& up = http.upload();

    if (up.status == UPLOAD_FILE_START) {
        otaAuthOk = http.authenticate("admin", OTA_PASS);
        if (!otaAuthOk) { log_w("OTA: Authentifizierung fehlgeschlagen"); return; }
        // Die PA definiert stillsetzen - waehrend des Schreibens laeuft loop()
        // nicht mehr. Abschaltbar, weil es sonst bei jedem Entwicklungs-Flash
        // ungefragt die Betriebsart wegnimmt.
        if (cfg.otaStandby) juma.sendNow("=S");
        log_i("OTA: '%s' startet", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            log_e("OTA: begin fehlgeschlagen");
            otaAuthOk = false;
        }
        return;
    }
    if (!otaAuthOk) return;

    // Der komplette Upload laeuft in einem handleClient() - ohne das hier
    // wuerde der Watchdog mitten im Schreiben zuschlagen.
    esp_task_wdt_reset();

    if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            log_e("OTA: Schreibfehler bei %u Bytes", (unsigned)up.totalSize);
            otaAuthOk = false;
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) log_i("OTA: %u Bytes geschrieben", (unsigned)up.totalSize);
        else                  log_e("OTA: end fehlgeschlagen (%s)", Update.errorString());
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaAuthOk = false;
    }
}

void webBegin() {
    // authenticate() liest den Authorization-Header nur, wenn er gesammelt wird.
    // Core 2.0.x nimmt Array + Anzahl, nicht variadisch.
    static const char* otaHeaders[] = { "Authorization" };
    http.collectHeaders(otaHeaders, 1);
    http.on("/update", HTTP_POST, onUpdateEnd, onUpdateChunk);

    http.on("/", HTTP_GET, []() {
        http.sendHeader("Cache-Control", "no-store");
        http.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });
    http.on("/api/state", HTTP_GET, []() {
        String s; buildState(s);
        http.send(200, "application/json", s);
    });
    http.on("/api/config", HTTP_POST, onConfig);
    // sonst loggt der Core bei jedem Seitenaufruf ein [E] "handler not found"
    http.on("/favicon.ico", HTTP_GET, []() { http.send(204, "image/x-icon", ""); });
    http.onNotFound([]() { http.send(404, "text/plain", "not found"); });
    http.begin();

    wsSrv.begin();
    // Ohne das bleiben Verbindungen stehen, die der Browser nicht sauber
    // geschlossen hat (Tab weg, WLAN weg, Reload mitten im Frame). Sind alle
    // Plaetze mit solchen Leichen belegt, kommt kein neuer Browser mehr durch
    // und das Dashboard wirkt wie eingefroren.
    wsSrv.enableHeartbeat(15000, 3000, 2);
    wsSrv.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
        if (type == WStype_TEXT) {
            payload[len] = 0;
            handleCmd((const char*)payload);
        } else if (type == WStype_CONNECTED) {
            String s; buildState(s);
            wsSrv.sendTXT(num, s);
        }
    });
}

uint8_t webClients() { return wsSrv.connectedClients(); }

void webLoop() {
    http.handleClient();
    wsSrv.loop();

    if (millis() - lastPush >= POLL_INTERVAL_MS) {
        lastPush = millis();
        if (wsSrv.connectedClients()) {
            String s; buildState(s);
            wsSrv.broadcastTXT(s);
        }
    }
}
