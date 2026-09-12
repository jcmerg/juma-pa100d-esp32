#pragma once
#include <Arduino.h>
#include "juma_status.h"

class Juma {
public:
    void begin();
    void loop();

    const JumaStatus& status() const { return st_; }
    bool online() const;                 // Statusantwort frisch genug?

    bool send(const char* cmd);          // in die Queue
    void sendNow(const char* cmd);       // sofort, ohne Queue (OTA/Notfall)          // "=R" o.ae.; "\n\r" haengt der Treiber an
    bool setBand(uint8_t n);             // 1..9
    bool setGain(uint8_t n);             // 1..4
    bool setOperate(bool on);            // =O / =S
    bool setAutoSelect();                // =A
    bool clearAlarm();                   // =C
    bool powerOff(bool saveState);       // =P1 / =P0

    uint32_t rxLines() const { return rxLines_; }
    uint32_t badLines() const { return badLines_; }
    uint32_t rxBytes() const { return rxBytes_; }
    // Letzte empfangene Rohbytes als Hex - trennt "nichts kommt an" von
    // "Bytes kommen an, aber Baudrate/Framing passt nicht".
    void     hexTail(char* out, size_t cap) const;
    const char* lastSent() const { return lastSent_; }

private:
    static const uint8_t QN = 8;         // Kommando-Queue
    static const uint8_t QL = 10;

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
};

extern Juma juma;
