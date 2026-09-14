#pragma once
#include <Arduino.h>

// TCI client (Transceiver Control Interface). Talks to any SDR software that
// offers TCI - ExpertSDR2/3, deskHPSDR and others.
// Text protocol over WebSocket, messages terminated with ';', e.g.
//   vfo:0,0,14074000;   trx:0,true;   dds:0,14000000;   if:0,0,74000;
// Provides frequency and TX state; the band logic lives in main.cpp.
// Runs in a task of its own, for the same reason juma does: reconnecting to
// an SDR that is not there blocks in WiFiClient::connect() for seconds, and
// the arduinoWebSockets client retries every 5 s. In the main loop that made
// a switched-off SDR look like a firmware hang.
class TciClient {
public:
    void begin();                        // also starts the task

    // Takes effect in the task, not here: applying it disconnects and
    // reconnects, and no caller should wait through that.
    void configure(const String& host, uint16_t port, bool enabled);
    bool enabled()   const { return enabled_; }
    bool connected() const { return connected_; }
    bool ready()     const { return ready_; }
    String host()    const { return host_; }
    uint16_t port()  const { return port_; }

    uint32_t freqHz()    const { return freq_; }
    uint32_t freqSetAt() const { return freqAt_; }   // millis of the last change
    bool     tx()        const { return tx_; }

    uint32_t rxMsgs() const { return msgs_; }
    uint32_t drops()  const { return drops_; }   // connection drops

private:
    static void task(void* self);
    void service();                      // one pass through the client
    void applyPending();
    void onText(uint8_t* payload, size_t len);
    void handleCommand(char* cmd);
    void setFreq(uint32_t hz);

    String   host_;
    uint16_t port_     = 40001;
    bool     enabled_  = false;
    bool     connected_ = false;
    bool     ready_    = false;
    bool     started_  = false;

    uint32_t freq_   = 0;
    uint32_t freqAt_ = 0;
    uint32_t dds_    = 0;
    bool     haveVfo_ = false;
    bool     tx_     = false;
    uint32_t msgs_   = 0;
    uint32_t drops_  = 0;

    // Handover from whoever calls configure() to the task.
    SemaphoreHandle_t mtx_ = nullptr;
    String   pendHost_;
    uint16_t pendPort_ = 0;
    bool     pendEn_   = false;
    bool     pending_  = false;
};

extern TciClient tci;
