#pragma once
#include <Arduino.h>

// Firmware update of the PA itself, through the Ingenia dsPIC boot loader on
// the same RS-232 line the controller talks to the PA on. A port of
// tools/juma-flash.py from the firmware repository: the same protocol, the
// same checks, the same rules - only program memory is written, never the
// configuration registers or the data EEPROM (calibration), never the boot
// loader area, and the reset vector always keeps pointing at the boot loader.
//
// The HEX file is uploaded first and checked; the image waits in the unused
// SPIFFS partition. Flashing runs in a task of its own, so the dashboard
// keeps showing the progress. While it runs the PA link is paused.
class PaFlash {
public:
    enum State : uint8_t {
        Empty,      // no image
        Loaded,     // image checked, ready
        Waiting,    // sending 0x55, waiting for the boot loader
        Writing,
        Verifying,
        Done,       // written and verified, the PA sits in its boot loader
        Failed,
        Started     // firmware started from the boot loader
    };

    // What the dashboard shows. A copy: the task writes it while the web
    // server reads it.
    struct Info {
        State    state = Empty;
        bool     verifyOnly = false;
        uint16_t done = 0, total = 0;   // rows; seconds left while Waiting
        uint16_t rows = 0;              // rows in the image
        uint32_t last = 0;              // last program address of the image
        char     file[33] = "";
        char     err[12] = "";          // code, translated in the browser
        char     arg[24] = "";
        char     boot[40] = "";         // "v1.1 dsPIC30F6014A 0x017D00"
        bool     hold = false;          // PA link held, see hold_
    };

    void begin();
    Info info() const;
    bool busy() const;                  // the task runs: hands off the PA link

    // HEX upload, from the HTTP upload handler.
    bool uploadBegin(const char* name);
    bool uploadChunk(const uint8_t* d, size_t n);
    bool uploadEnd();
    void uploadAbort();

    bool start(bool verifyOnly);        // false: no image, or busy
    void abort();                       // also releases a held PA link
    bool runFirmware();                 // only in Done: 0x0F to the boot loader

private:
    static void task(void* self);
    void run();
    void setState(State s, uint16_t done = 0, uint16_t total = 0);
    void setError(const char* code, const char* fmt = nullptr, ...);

    // Protocol steps. Each one reports its own error and returns false.
    bool connect(uint8_t* major, uint8_t* minor);
    bool readWords(uint32_t pc, uint8_t count, uint32_t* out);
    bool writeRow(uint32_t pc, const uint8_t* data);

    mutable SemaphoreHandle_t mtx_ = nullptr;
    Info info_;
    bool image_ = false;
    volatile bool abort_ = false;
    // Set by start(), cleared only once the task has handed the line back to
    // the PA link - the final state is published before that.
    volatile bool running_ = false;
    // A write went unanswered: the boot loader may still be waiting for the
    // rest of that frame, and would take the PA link's polls as row data -
    // with one chance in 256 of a matching check byte and a garbage row
    // written. So the link stays paused until the next flash or abort().
    volatile bool hold_ = false;
};

extern PaFlash paFlash;
