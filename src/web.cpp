#include "web.h"
#include "console.h"
#include "config.h"
#include "juma.h"
#include "tci.h"
#include "bands.h"
#include "index_html_gz.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_task_wdt.h>

Settings cfg;

// mDNS and DHCP only tolerate lower-case letters, digits and hyphens.
// Anything else is dropped rather than rejected - an empty name falls back to
// the default so the device is never nameless on the network.
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

// Fixed buffer instead of a String: the state goes out twice a second, and a
// growing String reallocates every time. Over hours that fragments the heap -
// free memory stays high, the largest contiguous block shrinks, and eventually
// an allocation fails.
static char             stateJson[1280];
static size_t           stateLen = 0;


void webSetNote(const char* code, const char* arg) {
    // bandControl() calls this on every pass - without the comparison that
    // would be needless String assignment on the heap.
    if (noteCode == code && noteArg == arg) return;
    noteCode = code;
    noteArg  = arg;
}

// --- Settings -------------------------------------------------------------

void settingsLoad() {
    // Open writable so the namespace is created on first start - read-only
    // otherwise logs a confusing "nvs_open failed"
    prefs.begin("juma", false);
    // isKey() first, otherwise getString() logs an [E] nvs_get_str ...
    // NOT_FOUND per key on first start - that looks like a fault but is merely
    // the still-empty namespace.
    cfg.hostname = prefs.isKey("host") ? prefs.getString("host") : String(HOSTNAME_DEFAULT);
    if (prefs.isKey("ssid"))     cfg.ssid     = prefs.getString("ssid");
    if (prefs.isKey("pass"))     cfg.pass     = prefs.getString("pass");
    if (prefs.isKey("tcihost"))  cfg.tciHost  = prefs.getString("tcihost");
    cfg.tciPort  = prefs.getUShort("tciport", TCI_DEFAULT_PORT);
    cfg.tciEn    = prefs.getBool("tcien", false);
    cfg.autoband = prefs.getBool("autoband", false);
    cfg.otaStandby = prefs.getBool("otastdby", true);
    cfg.tciLostAuto = prefs.getBool("tcilosta", false);
    cfg.tempWarn  = prefs.getUChar("tempwarn", 50);
    cfg.tempHigh  = prefs.getUChar("temphigh", 60);
    cfg.tempAlarm = prefs.getBool("tempalarm", true);
    cfg.swrWarnX10 = prefs.getUChar("swrwarn", 20);
    cfg.swrHighX10 = prefs.getUChar("swrhigh", 25);
    cfg.swrAlarm   = prefs.getBool("swralarm", true);
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
    prefs.putUChar("tempwarn", cfg.tempWarn);
    prefs.putUChar("temphigh", cfg.tempHigh);
    prefs.putBool("tempalarm", cfg.tempAlarm);
    prefs.putUChar("swrwarn", cfg.swrWarnX10);
    prefs.putUChar("swrhigh", cfg.swrHighX10);
    prefs.putBool("swralarm", cfg.swrAlarm);
    prefs.end();
}

// --- State as JSON --------------------------------------------------------

static void buildState() {
    const JumaStatus s = juma.status();
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
    d["tciHz"]   = tci.freqHz();
    d["tciBandName"] = bandNameFromHz(tci.freqHz());
    d["tciHost"] = cfg.tciHost;
    d["tciPort"] = cfg.tciPort;
    d["tciEnabled"] = cfg.tciEn;
    d["otaStandby"] = cfg.otaStandby;
    d["tciLostAuto"] = cfg.tciLostAuto;
    d["tempWarn"]  = cfg.tempWarn;
    d["tempHigh"]  = cfg.tempHigh;
    d["tempAlarm"] = cfg.tempAlarm;
    d["swrWarn"]   = cfg.swrWarnX10 / 10.0f;
    d["swrHigh"]   = cfg.swrHighX10 / 10.0f;
    d["swrAlarm"]  = cfg.swrAlarm;
    d["ssid"]    = cfg.ssid;
    d["hostname"] = cfg.hostname;
    d["note"]    = noteCode;
    d["noteArg"] = noteArg;
    d["version"] = FW_VERSION;

    stateLen = serializeJson(d, stateJson, sizeof(stateJson));
    if (stateLen >= sizeof(stateJson) - 1) log_e("state does not fit in the buffer");
}

// --- Commands from the browser ("name:value") -----------------------------

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
        // Only meaningful while the ESP32 is not choosing the band itself -
        // otherwise the next '=Bn' would undo the change immediately. The UI
        // locks this too; here it guards against stale browser state.
        if (cfg.autoband) {
            log_w("band select change rejected: TCI band selection is active");
            return;
        }
        // For "automatic" there is '=A'. For "manual" there is no command of
        // its own - '=Bn' IS the manual band selection. So send the band the
        // PA is already on: no band change, but A becomes M.
        if (v) {
            juma.setAutoSelect();
        } else {
            uint8_t b = juma.status().band;
            if (b >= 1 && b <= 9) juma.setBand(b);
            else log_w("manual band select: PA reports band %u, no =Bn possible", b);
        }
    }
    // These need no reboot and therefore take effect at once - a switch that
    // only becomes effective via "save & restart" looks like it does nothing.
    else if (name == "tempwarn" || name == "temphigh") {
        if (v >= 20 && v <= 120) {
            if (name == "tempwarn") cfg.tempWarn = (uint8_t)v;
            else                    cfg.tempHigh = (uint8_t)v;
            if (cfg.tempHigh < cfg.tempWarn) cfg.tempHigh = cfg.tempWarn;
            settingsSave();
        }
    }
    else if (name == "tempalarm") { cfg.tempAlarm  = (v != 0); settingsSave(); }
    // SWR arrives in tenths so the command stays integral
    else if (name == "swrwarn" || name == "swrhigh") {
        if (v >= 10 && v <= 100) {
            if (name == "swrwarn") cfg.swrWarnX10 = (uint8_t)v;
            else                   cfg.swrHighX10 = (uint8_t)v;
            if (cfg.swrHighX10 < cfg.swrWarnX10) cfg.swrHighX10 = cfg.swrWarnX10;
            settingsSave();
        }
    }
    else if (name == "swralarm") { cfg.swrAlarm = (v != 0); settingsSave(); }
    else if (name == "otastby")   { cfg.otaStandby = (v != 0); settingsSave(); }
    else if (name == "autoband") {
        cfg.autoband = (v != 0);
        settingsSave();
        // bandControl() sets the hint itself on the next pass
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
    if (http.hasArg("tempalarm")) cfg.tempAlarm = (http.arg("tempalarm") == "1");
    if (http.hasArg("tempwarn")) {
        int v = http.arg("tempwarn").toInt();
        if (v >= 20 && v <= 120) cfg.tempWarn = (uint8_t)v;
    }
    if (http.hasArg("temphigh")) {
        int v = http.arg("temphigh").toInt();
        if (v >= 20 && v <= 120) cfg.tempHigh = (uint8_t)v;
    }
    if (cfg.tempHigh < cfg.tempWarn) cfg.tempHigh = cfg.tempWarn;
    settingsSave();
    // language neutral - the dashboard supplies the wording, otherwise German
    // would suddenly appear in the English UI
    http.send(200, "text/plain", "saved");
    delay(1000);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// OTA by HTTP push.
//
// ArduinoOTA/espota has the DEVICE connect back to the host - that fails as
// soon as the ESP32 sits in an IoT VLAN which may not initiate into the LAN.
// This endpoint runs in the direction that works anyway:
//   curl -u admin:<pass> -F firmware=@firmware.bin http://<ip>/update
// ---------------------------------------------------------------------------
static bool otaAuthOk = false;

static void onUpdateEnd() {
    if (!http.authenticate("admin", OTA_PASS)) { http.requestAuthentication(); return; }
    bool ok = otaAuthOk && !Update.hasError();
    http.sendHeader("Connection", "close");
    http.send(ok ? 200 : 500, "text/plain; charset=utf-8",
              ok ? "OK - restarting\n" : "FAILED\n");
    if (ok) { delay(300); ESP.restart(); }
}

static void onUpdateChunk() {
    HTTPUpload& up = http.upload();

    if (up.status == UPLOAD_FILE_START) {
        otaAuthOk = http.authenticate("admin", OTA_PASS);
        if (!otaAuthOk) { log_w("OTA: authentication failed"); return; }
        // Put the PA into a defined idle state - loop() no longer runs while
        // writing. Switchable, because otherwise every development flash takes
        // away the operating mode unasked.
        if (cfg.otaStandby) juma.sendNow("=S");
        log_i("OTA: '%s' starting", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            log_e("OTA: begin failed");
            otaAuthOk = false;
        }
        return;
    }
    if (!otaAuthOk) return;

    // The whole upload runs inside a single handleClient() - without this the
    // watchdog would fire in the middle of writing.
    esp_task_wdt_reset();

    if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            log_e("OTA: write error at %u bytes", (unsigned)up.totalSize);
            otaAuthOk = false;
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) log_i("OTA: %u bytes written", (unsigned)up.totalSize);
        else                  log_e("OTA: end failed (%s)", Update.errorString());
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaAuthOk = false;
    }
}

void webBegin() {
    // authenticate() only reads the Authorization header when it is
    // collected. Core 2.0.x takes an array plus count, not variadic args.
    static const char* otaHeaders[] = { "Authorization" };
    http.collectHeaders(otaHeaders, 1);
    http.on("/update", HTTP_POST, onUpdateEnd, onUpdateChunk);
    // The check in the upload handler only takes effect once the body is
    // through - the browser would upload a megabyte, get a 401, prompt, and
    // upload again. This GET makes it ask beforehand.
    http.on("/update", HTTP_GET, []() {
        if (!http.authenticate("admin", OTA_PASS)) { http.requestAuthentication(); return; }
        http.send(204, "text/plain", "");
    });

    http.on("/", HTTP_GET, []() {
        // Serve compressed: a third of the bytes, so the script at the end of
        // the document runs correspondingly sooner - before, the page sat
        // there for seconds without buttons or gauges.
        http.sendHeader("Cache-Control", "no-store");
        http.sendHeader("Content-Encoding", "gzip");
        // The server writes the body in chunks of 1436 bytes and waits up to
        // HTTP_MAX_SEND_WAIT (5 s) for each one to be ACKed - and the whole
        // loop waits with it. On a link with retransmissions two such chunks
        // are enough to exceed the 5 s after which the PA leaves remote mode
        // and falls back to STANDBY. Measured: handleClient() blocked for
        // 10.06 s, the PA had had no command for 13.5 s, STANDBY. One second
        // is still generous for a LAN; setTimeout works on the socket, so
        // the copy client() returns is enough.
        http.client().setTimeout(1);
        uint32_t t0 = millis();
        http.send_P(200, "text/html; charset=utf-8",
                    (PGM_P)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
        // Time spent *here* is the device pushing bytes into the socket. If
        // this is short and the browser still waits, the delay is on the
        // radio link, not in the firmware.
        if (dbgOn()) {
            uint32_t ms = millis() - t0;
            dbg("web: / %u B in %lu ms (%lu kB/s), RSSI %d dBm",
                (unsigned)INDEX_HTML_GZ_LEN, (unsigned long)ms,
                (unsigned long)(INDEX_HTML_GZ_LEN / (ms ? ms : 1)), (int)WiFi.RSSI());
        }
    });
    http.on("/api/state", HTTP_GET, []() {
        uint32_t t0 = millis();
        buildState();
        http.send(200, "application/json", stateJson);
        if (dbgOn()) dbg("web: /api/state %u B in %lu ms", (unsigned)stateLen,
                         (unsigned long)(millis() - t0));
    });
    http.on("/api/config", HTTP_POST, onConfig);
    // otherwise the core logs an [E] "handler not found" on every page load
    http.on("/favicon.ico", HTTP_GET, []() { http.send(204, "image/x-icon", ""); });
    http.onNotFound([]() { http.send(404, "text/plain", "not found"); });
    http.begin();

    wsSrv.begin();
    // Without this, connections the browser did not close cleanly linger
    // (tab gone, Wi-Fi gone, reload mid-frame). Once all slots are taken by
    // such corpses no new browser gets through and the dashboard appears
    // frozen. Tighter than the default so dead connections disappear quickly
    // instead of blocking a slot for half a minute.
    wsSrv.enableHeartbeat(6000, 2000, 2);
    wsSrv.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
        if (type == WStype_TEXT) {
            payload[len] = 0;
            handleCmd((const char*)payload);
        } else if (type == WStype_CONNECTED) {
            if (dbgOn()) dbg("ws: client %u connected (%u total)", num,
                             wsSrv.connectedClients());
            buildState();
            wsSrv.sendTXT(num, stateJson, stateLen);
        } else if (type == WStype_DISCONNECTED) {
            // The browser reports "connection lost" either way - this says
            // whether the socket went down while Wi-Fi stayed up, which is
            // what the heartbeat does when a reply takes too long.
            if (dbgOn()) dbg("ws: client %u gone (%u left, Wi-Fi %s)", num,
                             wsSrv.connectedClients(),
                             WiFi.status() == WL_CONNECTED ? "up" : "DOWN");
        }
    });
}

uint8_t webClients() { return wsSrv.connectedClients(); }

void webLoop() {
    uint32_t t0 = millis();
    http.handleClient();
    // A client that opens a socket and then says nothing holds handleClient()
    // for HTTP_MAX_DATA_WAIT - seconds, in the middle of the loop.
    if (dbgOn() && millis() - t0 > 250)
        dbg("slow: http.handleClient took %lu ms", (unsigned long)(millis() - t0));

    t0 = millis();
    wsSrv.loop();
    if (dbgOn() && millis() - t0 > 250)
        dbg("slow: wsSrv.loop took %lu ms", (unsigned long)(millis() - t0));

    if (millis() - lastPush >= POLL_INTERVAL_MS) {
        lastPush = millis();
        if (wsSrv.connectedClients()) {
            buildState();
            uint32_t t0 = millis();
            wsSrv.broadcastTXT(stateJson, stateLen);
            // A client that does not read blocks the write for up to
            // WEBSOCKETS_TCP_TIMEOUT - and with it the whole main loop.
            uint32_t ms = millis() - t0;
            if (dbgOn() && ms > 20)
                dbg("ws: broadcast to %u clients took %lu ms",
                    wsSrv.connectedClients(), (unsigned long)ms);
        }
    }
}
