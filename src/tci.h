#pragma once
#include <Arduino.h>

// TCI-Client (Transceiver Control Interface). Spricht jede SDR-Software, die
// TCI anbietet - ExpertSDR2/3, deskHPSDR und andere.
// Textprotokoll ueber WebSocket, Nachrichten mit ';' terminiert, z.B.
//   vfo:0,0,14074000;   trx:0,true;   dds:0,14000000;   if:0,0,74000;
// Liefert QRG und TX-Zustand; die Bandlogik sitzt in main.cpp.
class TciClient {
public:
    void begin();
    void loop();

    void configure(const String& host, uint16_t port, bool enabled);
    bool enabled()   const { return enabled_; }
    bool connected() const { return connected_; }
    bool ready()     const { return ready_; }
    String host()    const { return host_; }
    uint16_t port()  const { return port_; }

    uint32_t freqHz()    const { return freq_; }
    uint32_t freqSetAt() const { return freqAt_; }   // millis der letzten Aenderung
    bool     tx()        const { return tx_; }

    uint32_t rxMsgs() const { return msgs_; }
    uint32_t drops()  const { return drops_; }   // Verbindungsabbrueche

private:
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
};

extern TciClient tci;
