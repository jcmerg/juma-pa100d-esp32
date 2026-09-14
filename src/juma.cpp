#include "juma.h"
#include "console.h"      // dbgOn()/dbg() for the command trace
#include "config.h"
#include <stdlib.h>
#include <string.h>

Juma juma;

// Commands and status replies are terminated with 0x0A 0x0D - LF before CR,
// not the usual order. Manual: "=[Command][Parameter]\n\r".
static const char* TERM = "\n\r";

// RAII, so no early return can leave the mutex taken.
namespace {
struct Lock {
    SemaphoreHandle_t m;
    explicit Lock(SemaphoreHandle_t h) : m(h) { if (m) xSemaphoreTake(m, portMAX_DELAY); }
    ~Lock() { if (m) xSemaphoreGive(m); }
};
}

void Juma::begin() {
    Serial2.begin(JUMA_BAUD, SERIAL_8N1, JUMA_RX_PIN, JUMA_TX_PIN, JUMA_INVERT);
    len_ = 0;
    mtx_ = xSemaphoreCreateMutex();
    // Priority above the Arduino loop task (1) so a busy loop cannot push the
    // poll aside, and on the same core - core 0 belongs to the Wi-Fi stack.
    xTaskCreatePinnedToCore(task, "juma", 3072, this, 2, nullptr, 1);
}

void Juma::task(void* self) {
    Juma* j = (Juma*)self;
    for (;;) {
        j->service();
        // Well below CMD_GAP_MS and the 500 ms poll, and it keeps the UART
        // buffer from running over at 115200 baud.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool Juma::online() const {
    // Two aligned 32-bit reads - no lock needed, and a torn answer here would
    // only ever be one poll interval stale.
    return st_.valid && (millis() - st_.lastRxMs) < STALE_AFTER_MS;
}

JumaStatus Juma::status() const {
    Lock lk(mtx_);
    return st_;
}

// --- Sending --------------------------------------------------------------

// Private: always called with the lock held.
bool Juma::enqueue(const char* cmd) {
    uint8_t next = (uint8_t)((qTail_ + 1) % QN);
    if (next == qHead_) return false;             // queue full
    strncpy(q_[qTail_], cmd, QL - 1);
    q_[qTail_][QL - 1] = '\0';
    qTail_ = next;
    return true;
}

void Juma::pumpQueue() {
    if (qHead_ == qTail_) return;
    if (millis() - lastTx_ < CMD_GAP_MS) return;
    // The PA leaves remote mode 5 s after the last command, and then sits in
    // STANDBY. Everything runs from one loop, so anything that blocks it -
    // a WebSocket write to a browser that stopped reading, for instance -
    // eats into that budget. Report the gap before it gets dangerous.
    if (dbgOn()) {
        uint32_t gap = millis() - lastTx_;
        if (lastTx_ && gap > 2000)
            dbg("pa: %lu ms since the last command - remote drops out at 5000",
                (unsigned long)gap);
        if (strcmp(q_[qHead_], "=R")) dbg("pa: -> '%s'", q_[qHead_]);
    }
    Serial2.print(q_[qHead_]);
    Serial2.print(TERM);
    strncpy(lastSent_, q_[qHead_], QL - 1);
    lastSent_[QL - 1] = '\0';
    qHead_ = (uint8_t)((qHead_ + 1) % QN);
    lastTx_ = millis();
}

bool Juma::send(const char* cmd)        { Lock lk(mtx_); return enqueue(cmd); }

// Bypasses the queue and CMD_GAP. For cases where loop() will not run
// afterwards - such as just before an OTA update.
void Juma::sendNow(const char* cmd) {
    Lock lk(mtx_);
    Serial2.print(cmd);
    Serial2.print(TERM);
    Serial2.flush();
    lastTx_ = millis();
}
bool Juma::setOperate(bool on)          { Lock lk(mtx_); return enqueue(on ? "=O" : "=S"); }
bool Juma::setAutoSelect()              { Lock lk(mtx_); return enqueue("=A"); }
bool Juma::clearAlarm()                 { Lock lk(mtx_); return enqueue("=C"); }
bool Juma::powerOff(bool saveState)     { Lock lk(mtx_); return enqueue(saveState ? "=P1" : "=P0"); }

bool Juma::setBand(uint8_t n) {
    if (n < 1 || n > 9) return false;
    char c[4] = { '=', 'B', (char)('0' + n), '\0' };
    Lock lk(mtx_);
    return enqueue(c);
}

bool Juma::setGain(uint8_t n) {
    if (n < 1 || n > 4) return false;
    char c[4] = { '=', 'G', (char)('0' + n), '\0' };
    Lock lk(mtx_);
    return enqueue(c);
}

// --- Receiving ------------------------------------------------------------

void Juma::service() {
    Lock lk(mtx_);
    pumpQueue();

    uint16_t budget = RX_MAX_PER_LOOP;
    while (Serial2.available() && budget--) {
        char c = (char)Serial2.read();
        rxBytes_++;
        tail_[tailPos_] = (uint8_t)c;
        tailPos_ = (uint8_t)((tailPos_ + 1) % TAIL);
        if (tailN_ < TAIL) tailN_++;
        if (c == '\n' || c == '\r') {
            if (len_ > 0) {
                buf_[len_] = '\0';
                parseLine(buf_);
                len_ = 0;
            }
            continue;                              // the second terminator byte
        }
        if (len_ < sizeof(buf_) - 1) buf_[len_++] = c;
        else len_ = 0;                              // overflow: drop the line
    }

    if (millis() - lastPoll_ >= POLL_INTERVAL_MS) {
        lastPoll_ = millis();
        enqueue("=R");
    }
}

void Juma::parseLine(char* line) {
    const bool hadState = st_.valid;
    const bool wasOp    = st_.operate;
    const bool wasAuto  = st_.autoSel;
    const uint8_t wasBand = st_.band;

    if (!jumaParseStatus(line, st_)) {
        badLines_++;
        // Keep it if it reads as text. Power-up banners look like
        // "Juma PA-100D V4.00a"; a wrong baud rate produces bytes that do
        // not, and those must not be mistaken for one.
        uint8_t printable = 0, total = 0;
        for (const char* c = line; *c && total < sizeof(banner_); c++, total++)
            if (*c >= 32 && *c < 127) printable++;
        if (total >= 4 && printable == total) {
            strncpy(banner_, line, sizeof(banner_) - 1);
            banner_[sizeof(banner_) - 1] = '\0';
        }
        return;
    }
    st_.lastRxMs = millis();
    rxLines_++;

    // What the PA does on its own is the other half of the picture: the
    // trace above shows what was sent, this shows how it answered. A
    // STANDBY right behind a '=Bn' is the PA dropping out on a band change,
    // not somebody at the front panel.
    if (dbgOn() && hadState) {
        if (wasOp != st_.operate)
            dbg("pa: %s -> %s (band %u, sel %c, last sent '%s')",
                wasOp ? "OPERATE" : "STANDBY", st_.operate ? "OPERATE" : "STANDBY",
                st_.band, st_.autoSel ? 'A' : 'M', lastSent_);
        if (wasBand != st_.band)
            dbg("pa: band %u -> %u (sel %c)", wasBand, st_.band,
                st_.autoSel ? 'A' : 'M');
        if (wasAuto != st_.autoSel)
            dbg("pa: band select %c -> %c", wasAuto ? 'A' : 'M',
                st_.autoSel ? 'A' : 'M');
    }
}

void Juma::bannerCopy(char* out, size_t cap) const {
    Lock lk(mtx_);
    strncpy(out, banner_, cap - 1);
    out[cap - 1] = '\0';
}

void Juma::hexTail(char* out, size_t cap) const {
    Lock lk(mtx_);
    size_t o = 0;
    uint8_t start = (uint8_t)((tailPos_ + TAIL - tailN_) % TAIL);
    for (uint8_t i = 0; i < tailN_ && o + 4 < cap; i++) {
        uint8_t b = tail_[(start + i) % TAIL];
        o += (size_t)snprintf(out + o, cap - o, "%02X ", b);
    }
    if (o && o < cap) out[o - 1] = '\0';
    else if (o < cap) out[o] = '\0';
}
