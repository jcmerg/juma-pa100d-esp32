#include "tci.h"
#include "config.h"
#include <WebSocketsClient.h>
#include <stdlib.h>
#include <string.h>

TciClient tci;
static WebSocketsClient ws;

void TciClient::begin() {
    ws.onEvent([this](WStype_t type, uint8_t* payload, size_t len) {
        switch (type) {
            case WStype_CONNECTED:
                connected_ = true; ready_ = false;
                log_i("TCI verbunden");
                break;
            case WStype_DISCONNECTED:
                if (connected_) { drops_++; log_w("TCI getrennt"); }
                connected_ = false; ready_ = false;
                // Den letzten Stand NICHT stehen lassen: ein eingefrorenes
                // tx_=true wuerde den Bandwechsel nach dem Reconnect dauerhaft
                // blockieren, und eine veraltete QRG koennte ein falsches Band
                // schalten. Der Server schickt beim Verbinden ohnehin den
                // kompletten Zustand neu, inklusive vfo.
                tx_ = false; freq_ = 0; haveVfo_ = false; dds_ = 0;
                break;
            case WStype_TEXT:
                onText(payload, len);
                break;
            default:
                break;
        }
    });
    ws.setReconnectInterval(5000);
}

void TciClient::configure(const String& host, uint16_t port, bool enabled) {
    bool changed = (host != host_) || (port != port_) || (enabled != enabled_);
    host_    = host;
    port_    = port ? port : TCI_DEFAULT_PORT;
    enabled_ = enabled && host_.length() > 0;
    if (!changed) return;

    if (started_) { ws.disconnect(); started_ = false; connected_ = false; ready_ = false; }
    if (enabled_) {
        // Leeres Subprotokoll! arduinoWebSockets schickt sonst per Default
        // "Sec-WebSocket-Protocol: arduino" - deskHPSDR schliesst die
        // Verbindung daraufhin wortlos, ohne HTTP-Antwort. Gemessen: mit dem
        // Header kommt kein 101, ohne ihn schon. TCI kennt kein Subprotokoll.
        ws.begin(host_.c_str(), port_, "/", "");
        started_ = true;
    }
}

void TciClient::loop() {
    if (started_) ws.loop();
}

void TciClient::setFreq(uint32_t hz) {
    if (hz == freq_) return;
    freq_   = hz;
    freqAt_ = millis();
}

// Ein TCI-Kommando: "name:arg,arg,arg" oder nur "name".
void TciClient::handleCommand(char* cmd) {
    while (*cmd == ' ' || *cmd == '\n' || *cmd == '\r') cmd++;
    if (!*cmd) return;

    char* args = strchr(cmd, ':');
    if (args) { *args = '\0'; args++; }

    for (char* p = cmd; *p; p++) *p = (char)tolower(*p);

    if (!strcmp(cmd, "ready")) { ready_ = true; return; }

    if (!args) return;

    // Argumente aufteilen
    char* a[6] = {nullptr};
    uint8_t n = 0;
    a[n++] = args;
    for (char* p = args; *p && n < 6; p++) {
        if (*p == ',') { *p = '\0'; a[n++] = p + 1; }
    }

    if (!strcmp(cmd, "vfo") && n >= 3) {
        // vfo:<trx>,<channel>,<hz>   - absolute Empfangsfrequenz
        if (strtol(a[0], nullptr, 10) == 0 && strtol(a[1], nullptr, 10) == 0) {
            haveVfo_ = true;
            setFreq((uint32_t)strtoul(a[2], nullptr, 10));
        }
        return;
    }

    if (!strcmp(cmd, "dds") && n >= 2) {
        if (strtol(a[0], nullptr, 10) == 0) dds_ = (uint32_t)strtoul(a[1], nullptr, 10);
        return;
    }

    if (!strcmp(cmd, "if") && n >= 3) {
        // Fallback, solange kein vfo gesehen wurde: QRG = dds + if-Offset
        if (!haveVfo_ && strtol(a[0], nullptr, 10) == 0 && strtol(a[1], nullptr, 10) == 0) {
            long off = strtol(a[2], nullptr, 10);
            if (dds_) setFreq((uint32_t)((long)dds_ + off));
        }
        return;
    }

    if (!strcmp(cmd, "trx") && n >= 2) {
        // trx:<trx>,<true|false>[,<source>]
        if (strtol(a[0], nullptr, 10) == 0) {
            tx_ = (!strncmp(a[1], "true", 4));
        }
        return;
    }
}

void TciClient::onText(uint8_t* payload, size_t len) {
    msgs_++;
    // In-place zerlegen: mehrere Kommandos koennen in einem Frame stecken.
    static char work[512];
    if (len >= sizeof(work)) len = sizeof(work) - 1;
    memcpy(work, payload, len);
    work[len] = '\0';

    char* start = work;
    for (char* p = work; *p; p++) {
        if (*p == ';') { *p = '\0'; handleCommand(start); start = p + 1; }
    }
    if (*start) handleCommand(start);
}
