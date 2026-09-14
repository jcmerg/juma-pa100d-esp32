#pragma once
#include <Arduino.h>
#include "juma_status.h"

// The link to the PA runs in a task of its own. The PA leaves remote mode
// 5 s after the last command and then sits in STANDBY, so the 500 ms poll
// must not depend on the main loop: a single HTTP write to a browser on a
// weak link blocks that loop for seconds (measured: 10 s), which cost the
// operating mode every time.
class Juma {
public:
    void begin();                        // also starts the task

    // A copy, not a reference: the task writes the status while others read
    // it. The struct is small enough that copying it beats holding a lock
    // across whatever the caller does with it.
    JumaStatus status() const;
    bool online() const;                 // status reply recent enough?

    bool send(const char* cmd);          // into the queue
    void sendNow(const char* cmd);       // at once, bypassing the queue (OTA)
    bool setBand(uint8_t n);             // 1..9
    bool setGain(uint8_t n);             // 1..4
    bool setOperate(bool on);            // =O / =S
    bool setAutoSelect();                // =A
    bool clearAlarm();                   // =C
    bool powerOff(bool saveState);       // =P1 / =P0

    uint32_t rxLines() const { return rxLines_; }
    uint32_t badLines() const { return badLines_; }
    uint32_t rxBytes() const { return rxBytes_; }
    // Last received raw bytes as hex - separates "nothing arrives" from
    // "bytes arrive but baud rate or framing is wrong".
    void     hexTail(char* out, size_t cap) const;
    const char* lastSent() const { return lastSent_; }

private:
    static const uint8_t QN = 8;         // command queue
    static const uint8_t QL = 10;

    static void task(void* self);
    void service();                      // one pass: send, receive, poll
    void parseLine(char* line);
    bool enqueue(const char* cmd);
    void pumpQueue();

    JumaStatus st_;
    char     buf_[128];
    uint8_t  len_ = 0;
    uint32_t lastPoll_ = 0;
    uint32_t lastTx_   = 0;
    char     q_[QN][QL];
    uint8_t  qHead_ = 0, qTail_ = 0;
    uint32_t rxLines_ = 0, badLines_ = 0, rxBytes_ = 0;
    static const uint8_t TAIL = 24;
    uint8_t  tail_[TAIL] = {0};
    uint8_t  tailN_ = 0, tailPos_ = 0;
    char     lastSent_[QL] = {0};
    SemaphoreHandle_t mtx_ = nullptr;
};

extern Juma juma;
