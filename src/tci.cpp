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
                log_i("TCI connected");
                break;
            case WStype_DISCONNECTED:
                if (connected_) { drops_++; log_w("TCI disconnected"); }
                connected_ = false; ready_ = false;
                // Do NOT keep the last state: a frozen tx_=true would block
                // band changes for good after the reconnect, and a stale
                // frequency could select the wrong band. The server sends the
                // complete state again on connect anyway, vfo included.
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

    mtx_ = xSemaphoreCreateMutex();
    // Same priority as the PA task and off the Wi-Fi core. The stack has to
    // carry the WebSocket client and the JSON-free text parsing below.
    xTaskCreatePinnedToCore(task, "tci", 4096, this, 2, nullptr, 1);
}

void TciClient::task(void* self) {
    TciClient* t = (TciClient*)self;
    for (;;) {
        t->service();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void TciClient::service() {
    applyPending();
    if (started_) ws.loop();     // may block for seconds - but only this task
}

void TciClient::configure(const String& host, uint16_t port, bool enabled) {
    if (mtx_) xSemaphoreTake(mtx_, portMAX_DELAY);
    pendHost_ = host;
    pendPort_ = port;
    pendEn_   = enabled;
    pending_  = true;
    if (mtx_) xSemaphoreGive(mtx_);
}

// In the task: pick up what configure() left behind.
void TciClient::applyPending() {
    if (!pending_) return;
    String host;
    uint16_t port;
    bool enabled;
    if (mtx_) xSemaphoreTake(mtx_, portMAX_DELAY);
    host = pendHost_; port = pendPort_; enabled = pendEn_;
    pending_ = false;
    if (mtx_) xSemaphoreGive(mtx_);

    bool changed = (host != host_) || ((port ? port : TCI_DEFAULT_PORT) != port_)
                   || ((enabled && host.length() > 0) != enabled_);
    host_    = host;
    port_    = port ? port : TCI_DEFAULT_PORT;
    enabled_ = enabled && host_.length() > 0;
    if (!changed) return;

    if (started_) { ws.disconnect(); started_ = false; connected_ = false; ready_ = false; }
    if (enabled_) {
        // Empty subprotocol! Otherwise arduinoWebSockets sends
        // "Sec-WebSocket-Protocol: arduino" by default - deskHPSDR then closes
        // the connection silently, without an HTTP reply. Measured: with the
        // header no 101 arrives, without it one does. TCI has no subprotocol.
        ws.begin(host_.c_str(), port_, "/", "");
        started_ = true;
    }
}

void TciClient::setFreq(uint32_t hz) {
    if (hz == freq_) return;
    freq_   = hz;
    freqAt_ = millis();
}

// A single TCI command: "name:arg,arg,arg" or just "name".
void TciClient::handleCommand(char* cmd) {
    while (*cmd == ' ' || *cmd == '\n' || *cmd == '\r') cmd++;
    if (!*cmd) return;

    char* args = strchr(cmd, ':');
    if (args) { *args = '\0'; args++; }

    for (char* p = cmd; *p; p++) *p = (char)tolower(*p);

    if (!strcmp(cmd, "ready")) { ready_ = true; return; }

    if (!args) return;

    // split the arguments
    char* a[6] = {nullptr};
    uint8_t n = 0;
    a[n++] = args;
    for (char* p = args; *p && n < 6; p++) {
        if (*p == ',') { *p = '\0'; a[n++] = p + 1; }
    }

    if (!strcmp(cmd, "vfo") && n >= 3) {
        // vfo:<trx>,<channel>,<hz>   - absolute receive frequency
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
        // Fallback while no vfo has been seen: frequency = dds + if offset
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
    // Split in place: one frame may carry several commands.
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
