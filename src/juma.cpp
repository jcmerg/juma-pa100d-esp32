#include "juma.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

Juma juma;

// Commands and status replies are terminated with 0x0A 0x0D - LF before CR,
// not the usual order. Manual: "=[Command][Parameter]\n\r".
static const char* TERM = "\n\r";

void Juma::begin() {
    Serial2.begin(JUMA_BAUD, SERIAL_8N1, JUMA_RX_PIN, JUMA_TX_PIN, JUMA_INVERT);
    len_ = 0;
}

bool Juma::online() const {
    return st_.valid && (millis() - st_.lastRxMs) < STALE_AFTER_MS;
}

// --- Sending --------------------------------------------------------------

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
    Serial2.print(q_[qHead_]);
    Serial2.print(TERM);
    strncpy(lastSent_, q_[qHead_], QL - 1);
    lastSent_[QL - 1] = '\0';
    qHead_ = (uint8_t)((qHead_ + 1) % QN);
    lastTx_ = millis();
}

bool Juma::send(const char* cmd)        { return enqueue(cmd); }

// Bypasses the queue and CMD_GAP. For cases where loop() will not run
// afterwards - such as just before an OTA update.
void Juma::sendNow(const char* cmd) {
    Serial2.print(cmd);
    Serial2.print(TERM);
    Serial2.flush();
    lastTx_ = millis();
}
bool Juma::setOperate(bool on)          { return enqueue(on ? "=O" : "=S"); }
bool Juma::setAutoSelect()              { return enqueue("=A"); }
bool Juma::clearAlarm()                 { return enqueue("=C"); }
bool Juma::powerOff(bool saveState)     { return enqueue(saveState ? "=P1" : "=P0"); }

bool Juma::setBand(uint8_t n) {
    if (n < 1 || n > 9) return false;
    char c[4] = { '=', 'B', (char)('0' + n), '\0' };
    return enqueue(c);
}

bool Juma::setGain(uint8_t n) {
    if (n < 1 || n > 4) return false;
    char c[4] = { '=', 'G', (char)('0' + n), '\0' };
    return enqueue(c);
}

// --- Receiving ------------------------------------------------------------

void Juma::loop() {
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
    if (!jumaParseStatus(line, st_)) { badLines_++; return; }
    st_.lastRxMs = millis();
    rxLines_++;
}

void Juma::hexTail(char* out, size_t cap) const {
    size_t o = 0;
    uint8_t start = (uint8_t)((tailPos_ + TAIL - tailN_) % TAIL);
    for (uint8_t i = 0; i < tailN_ && o + 4 < cap; i++) {
        uint8_t b = tail_[(start + i) % TAIL];
        o += (size_t)snprintf(out + o, cap - o, "%02X ", b);
    }
    if (o && o < cap) out[o - 1] = '\0';
    else if (o < cap) out[o] = '\0';
}
