#include "pa_flash.h"
#include "pa_hex.h"
#include "juma.h"
#include "console.h"      // dbgOn()/dbg()
#include "config.h"
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include <stdarg.h>

PaFlash paFlash;

// Boot loader protocol, from bootloader/iBL.s (see tools/juma-flash.py):
//   autobaud  0x55 ...                -> 0x55
//   version   0x03                    -> major, minor, 0x55
//   read      0x01 a2 a1 a0           -> b23..16, b15..8, b7..0, 0x55
//   write     0x02 a2 a1 a0 N data chk-> 0x55 or 0xFF; N counts the check byte
//   run       0x0F                    -> jumps to the firmware
static const uint8_t ACK = 0x55, NACK = 0xFF;
static const uint8_t C_READ = 0x01, C_WRITE = 0x02, C_VERSION = 0x03, C_USER = 0x0F;
static const uint32_t DEVID_ADDR = 0xFF0000;

struct Device { uint16_t id; const char* name; uint32_t flashEnd; };
// tools/ingenia/ibl_dspiclist.xml of the firmware repository
static const Device DEVICES[] = {
    { 0x01C1, "dsPIC30F3011",  0x004000 },
    { 0x0101, "dsPIC30F4011",  0x008000 },
    { 0x0080, "dsPIC30F5011",  0x00B000 },
    { 0x0198, "dsPIC30F6014",  0x018000 },
    { 0x02C3, "dsPIC30F6014A", 0x018000 },
};

// RAII, as in juma.cpp.
namespace {
struct Lock {
    SemaphoreHandle_t m;
    explicit Lock(SemaphoreHandle_t h) : m(h) { if (m) xSemaphoreTake(m, portMAX_DELAY); }
    ~Lock() { if (m) xSemaphoreGive(m); }
};

// The image waits in the SPIFFS partition of the default partition table,
// which this firmware has no other use for. Raw, without a file system: the
// parser needs nothing but "write these bytes at this address".
struct PartStore : PaHex::Store {
    const esp_partition_t* p = nullptr;
    bool write(uint32_t off, const uint8_t* d, size_t n) override {
        return esp_partition_write(p, off, d, n) == ESP_OK;
    }
    bool read(uint32_t off, uint8_t* d, size_t n) override {
        return esp_partition_read(p, off, d, n) == ESP_OK;
    }
};
}

static PartStore store;
static PaHex     hex;

// --- Serial helpers ------------------------------------------------------
// Stream::readBytes() spins without yielding; at task priority 2 that would
// starve the main loop for as long as the timeout runs.

static void drain() {
    while (Serial2.available()) Serial2.read();
}

static size_t readExact(uint8_t* out, size_t n, uint32_t timeoutMs) {
    size_t got = 0;
    const uint32_t t0 = millis();
    while (got < n && millis() - t0 < timeoutMs) {
        if (Serial2.available()) out[got++] = (uint8_t)Serial2.read();
        else vTaskDelay(1);
    }
    return got;
}

static void addrBytes(uint8_t* o, uint32_t pc) {
    o[0] = (uint8_t)(pc >> 16);
    o[1] = (uint8_t)(pc >> 8);
    o[2] = (uint8_t)pc;
}

// --- State ---------------------------------------------------------------

void PaFlash::begin() {
    mtx_ = xSemaphoreCreateMutex();
    store.p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                       ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (store.p && store.p->size < PaHex::STORE_BYTES) store.p = nullptr;
    if (!store.p) log_e("PA flash: no SPIFFS partition of %u bytes - PA updates unavailable",
                        (unsigned)PaHex::STORE_BYTES);
}

PaFlash::Info PaFlash::info() const {
    Lock lk(mtx_);
    return info_;
}

bool PaFlash::busy() const {
    return running_ || hold_;
}

void PaFlash::setState(State s, uint16_t done, uint16_t total) {
    Lock lk(mtx_);
    info_.state = s;
    info_.done  = done;
    info_.total = total;
}

void PaFlash::setError(const char* code, const char* fmt, ...) {
    Lock lk(mtx_);
    strncpy(info_.err, code, sizeof(info_.err) - 1);
    info_.err[sizeof(info_.err) - 1] = '\0';
    info_.arg[0] = '\0';
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(info_.arg, sizeof(info_.arg), fmt, ap);
        va_end(ap);
    }
    info_.state = Failed;
    log_w("PA flash: %s %s", info_.err, info_.arg);
    if (dbgOn()) dbg("paflash: failed - %s %s", info_.err, info_.arg);
}

// --- Upload --------------------------------------------------------------

bool PaFlash::uploadBegin(const char* name) {
    if (busy()) return false;
    {
        Lock lk(mtx_);
        image_ = false;
        info_ = Info();
        // Printable ASCII only: the name goes out in every state frame, and
        // a multi-byte character cut in half is invalid UTF-8, on which the
        // browser drops the WebSocket. Quotes and backslashes would cost
        // escapes in a buffer of fixed size.
        size_t o = 0;
        for (const char* c = name; *c && o < sizeof(info_.file) - 1; c++)
            info_.file[o++] = (*c >= 32 && *c < 127 && *c != '"' && *c != '\\') ? *c : '_';
        info_.file[o] = '\0';
    }
    if (!store.p) { setError("nostore"); return false; }
    // Sector by sector, so the watchdog gets fed in between - this runs
    // inside handleClient().
    for (uint32_t off = 0; off < PaHex::STORE_BYTES; off += 4096) {
        if (esp_partition_erase_range(store.p, off, 4096) != ESP_OK) {
            setError("hexstore");
            return false;
        }
        esp_task_wdt_reset();
    }
    hex.begin(&store);
    return true;
}

bool PaFlash::uploadChunk(const uint8_t* d, size_t n) {
    return hex.feed(d, n);
}

bool PaFlash::uploadEnd() {
    if (!hex.finish()) {
        const PaHex::Err e = hex.error();
        if (e == PaHex::RANGE || e == PaHex::CONFLICT)
            setError(PaHex::errCode(e), "0x%06lX", (unsigned long)hex.errAddr());
        else if (hex.errLine())
            setError(PaHex::errCode(e), "%lu", (unsigned long)hex.errLine());
        else
            setError(PaHex::errCode(e));
        return false;
    }
    Lock lk(mtx_);
    image_ = true;
    info_.rows = hex.rowCount();
    info_.last = hex.lastAddr();
    info_.state = Loaded;
    log_i("PA flash: '%s' loaded, %u rows, 0x000000 - 0x%06lX%s", info_.file, info_.rows,
          (unsigned long)info_.last, hex.skippedConfig() ? ", configuration ignored" : "");
    return true;
}

void PaFlash::uploadAbort() {
    setError("upabort");
}

// --- Flashing ------------------------------------------------------------

bool PaFlash::start(bool verifyOnly) {
    bool held;
    {
        Lock lk(mtx_);
        if (!image_ || running_) return false;
        held = hold_;
        abort_ = false;
        hold_ = false;
        info_.hold = false;
        running_ = true;
        info_.verifyOnly = verifyOnly;
        info_.err[0] = info_.arg[0] = info_.boot[0] = '\0';
        info_.state = Waiting;
        info_.done = 0;
        info_.total = PA_FLASH_WAIT_S;
    }
    // Priority and core as the PA task: the boot loader waits only about a
    // second for its autobaud edges, so the 0x55 stream must not stall
    // behind the web server.
    if (xTaskCreatePinnedToCore(task, "paflash", 4096, this, 2, nullptr, 1) != pdPASS) {
        // The link stays paused as it was - abort() still releases it.
        hold_ = held;
        { Lock lk(mtx_); info_.hold = held; }
        running_ = false;
        setError("task");
        return false;
    }
    return true;
}

void PaFlash::abort() {
    abort_ = true;
    if (hold_ && !running_) {
        hold_ = false;
        { Lock lk(mtx_); info_.hold = false; }
        juma.pause(false);
        log_i("PA flash: PA link released");
    }
}

bool PaFlash::runFirmware() {
    if (running_ || info().state != Done) return false;
    juma.pause(true);
    Serial2.write(C_USER);
    Serial2.flush();
    juma.pause(false);
    setState(Started);
    log_i("PA flash: firmware started");
    return true;
}

void PaFlash::task(void* self) {
    PaFlash* f = (PaFlash*)self;
    // STANDBY first: the PA is about to be switched off, and that should
    // not happen in OPERATE.
    if (juma.online()) juma.sendNow("=S");
    juma.pause(true);
    f->run();
    if (!f->hold_) juma.pause(false);
    f->running_ = false;
    vTaskDelete(nullptr);
}

// Send 0x55 until the boot loader answers. A continuous stream is a square
// wave, so the autobaud measurement is right wherever the boot loader starts.
// It answers the detection with one ACK and every further 0x55 with NACK
// (unknown command). The running firmware may answer too, so only a valid
// version reply counts.
bool PaFlash::connect(uint8_t* major, uint8_t* minor) {
    // 32 bytes are 2.8 ms on the wire at 115200 baud, and the task sleeps
    // 2 ms per burst - so the FIFO never runs dry and the stream has no
    // gaps, yet the main loop gets its turn. Not Serial2.flush(): in this
    // core it spins on the UART until it is idle, without ever yielding,
    // and at priority 2 that starved the loop until the task watchdog
    // restarted the ESP32 - while waiting for somebody to press OPER + PWR.
    uint8_t burst[32];
    memset(burst, ACK, sizeof(burst));
    drain();
    bool answered = false;
    const uint32_t t0 = millis();
    uint16_t shown = 0xFFFF;
    while (millis() - t0 < PA_FLASH_WAIT_S * 1000UL) {
        if (abort_) { setError("aborted"); return false; }
        const uint16_t secs = (uint16_t)((millis() - t0) / 1000);
        if (secs != shown) { shown = secs; setState(Waiting, secs, PA_FLASH_WAIT_S); }
        // Without pauses: the boot loader only waits about 1 s for the edges
        // of its baud rate detection and then starts the firmware.
        Serial2.write(burst, sizeof(burst));
        vTaskDelay(pdMS_TO_TICKS(2));
        // A stray byte at power-on (0x00 on a Rowaves PA-100D) must not stop
        // the stream before the baud rate has been detected.
        bool seen = false;
        while (Serial2.available()) {
            const uint8_t b = (uint8_t)Serial2.read();
            if (b == ACK || b == NACK) seen = true;
        }
        if (!seen) continue;
        answered = true;
        vTaskDelay(pdMS_TO_TICKS(20));
        drain();
        Serial2.write(C_VERSION);
        uint8_t ver[3];
        if (readExact(ver, 3, 300) == 3 && ver[2] == ACK) {
            vTaskDelay(pdMS_TO_TICKS(50));
            drain();
            *major = ver[0];
            *minor = ver[1];
            return true;
        }
    }
    // Answers, but not as the boot loader: the firmware is still running.
    setError(answered ? "notboot" : "noboot");
    return false;
}

// Program words at consecutive addresses, the read commands in windows of 16
// so the boot loader's transmit FIFO cannot overflow.
bool PaFlash::readWords(uint32_t pc, uint8_t count, uint32_t* out) {
    for (uint8_t i = 0; i < count; i += 16) {
        const uint8_t n = (uint8_t)((count - i) < 16 ? (count - i) : 16);
        uint8_t cmd[16 * 4];
        for (uint8_t j = 0; j < n; j++) {
            cmd[4 * j] = C_READ;
            addrBytes(cmd + 4 * j + 1, pc + 2u * (i + j));
        }
        Serial2.write(cmd, 4 * n);
        uint8_t rx[16 * 4];
        if (readExact(rx, 4 * n, 1000) != 4u * n) {
            setError("rdto", "0x%06lX", (unsigned long)(pc + 2u * i));
            return false;
        }
        for (uint8_t j = 0; j < n; j++) {
            const uint8_t* b = rx + 4 * j;
            if (b[3] != ACK) {
                setError("rderr", "0x%06lX", (unsigned long)(pc + 2u * (i + j)));
                return false;
            }
            out[i + j] = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
        }
    }
    return true;
}

// One row. Only a NACK is retried: the boot loader sends it after the whole
// frame when the check byte was wrong, then waits for the next command.
// Without an answer a byte may have been lost and the boot loader is still
// waiting for the rest of the frame - sending again would be read partly as
// commands, so stop instead: the PA has to be restarted.
bool PaFlash::writeRow(uint32_t pc, const uint8_t* data) {
    uint8_t frame[1 + 3 + 1 + PaHex::ROW_BYTES + 1];
    frame[0] = C_WRITE;
    addrBytes(frame + 1, pc);
    frame[4] = (uint8_t)(PaHex::ROW_BYTES + 1);
    memcpy(frame + 5, data, PaHex::ROW_BYTES);
    uint8_t sum = 0;
    for (size_t i = 1; i < sizeof(frame) - 1; i++) sum += frame[i];
    frame[sizeof(frame) - 1] = (uint8_t)-sum;
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        Serial2.write(frame, sizeof(frame));
        uint8_t ans;
        if (readExact(&ans, 1, 1000) != 1 || (ans != ACK && ans != NACK)) {
            hold_ = true;
            { Lock lk(mtx_); info_.hold = true; }
            setError("wrnoans", "0x%06lX", (unsigned long)pc);
            return false;
        }
        if (ans == ACK) return true;
        vTaskDelay(pdMS_TO_TICKS(50));
        drain();
    }
    setError("wrnack", "0x%06lX", (unsigned long)pc);
    return false;
}

// Compare the words read back with a row, only where the HEX file has data.
// The gaps may hold 0x000000 (Ingenia GUI, this tool) or 0xFFFFFF (erased);
// they are never executed.
static bool same(const uint32_t* got, const uint8_t* row, uint32_t mask) {
    for (uint8_t k = 0; k < PaHex::ROW_BYTES / 4; k++)
        if ((mask & (1u << k)) && got[k] != paRowWord(row, k)) return false;
    return true;
}

void PaFlash::run() {
    const bool vo = info().verifyOnly;
    uint8_t major = 0, minor = 0;
    if (!connect(&major, &minor)) return;

    uint32_t devid;
    if (!readWords(DEVID_ADDR, 1, &devid)) return;
    devid &= 0xFFFF;
    const Device* dev = nullptr;
    for (const Device& d : DEVICES) if (d.id == devid) dev = &d;
    if (!dev) { setError("devid", "0x%04lX", (unsigned long)devid); return; }

    // Row 0 holds the reset vector and the interrupt vectors. The boot
    // loader runs because the reset vector points to it, so its target is
    // the boot loader address - in the last part of program memory and at
    // the start of a row, so no firmware row overlaps it.
    uint32_t row0[32];
    if (!readWords(0, 32, row0)) return;
    const int32_t boot = paGotoTarget(row0[0], row0[1]);
    if (boot < 0 || boot % PaHex::ROW_ADDR || (uint32_t)boot < dev->flashEnd - 0x1000 ||
        (uint32_t)boot >= dev->flashEnd) {
        setError("vector", "%06lX %06lX", (unsigned long)row0[0], (unsigned long)row0[1]);
        return;
    }
    {
        Lock lk(mtx_);
        snprintf(info_.boot, sizeof(info_.boot), "v%u.%u %s 0x%06lX", major, minor,
                 dev->name, (unsigned long)boot);
    }
    log_i("PA flash: boot loader %s", info_.boot);
    if (hex.lastAddr() >= (uint32_t)boot) {
        setError("bootarea", "0x%06lX", (unsigned long)boot);
        return;
    }

    // The firmware's reset vector is replaced by the device's own, so the
    // boot loader stays reachable. Row 0 is only rewritten if it differs:
    // between its erase and write a power failure would leave no reset
    // vector, and the boot loader could then only be restored with a
    // programmer.
    uint8_t first[PaHex::ROW_BYTES];
    if (!hex.rowData(0, first)) { setError("hexstore"); return; }
    for (uint8_t i = 0; i < 2; i++) {
        first[4 * i]     = (uint8_t)row0[i];
        first[4 * i + 1] = (uint8_t)(row0[i] >> 8);
        first[4 * i + 2] = (uint8_t)(row0[i] >> 16);
        first[4 * i + 3] = 0;
    }
    const uint32_t mask0 = hex.mask(0) | 3u;
    const uint16_t rows = hex.rowCount();
    uint8_t data[PaHex::ROW_BYTES];

    if (!vo) {
        const bool skip0 = same(row0, first, mask0);
        const uint16_t todo = (uint16_t)(rows - (skip0 ? 1 : 0));
        uint16_t n = 0;
        const uint32_t t0 = millis();
        setState(Writing, 0, todo);
        for (uint16_t r = 0; r < PaHex::ROWS; r++) {
            if (!hex.mask(r) || (r == 0 && skip0)) continue;
            if (abort_) { setError("aborted"); return; }
            const uint32_t pc = (uint32_t)r * PaHex::ROW_ADDR;
            if (r && !hex.rowData(r, data)) { setError("hexstore"); return; }
            if (!writeRow(pc, r ? data : first)) return;
            setState(Writing, ++n, todo);
        }
        log_i("PA flash: %u rows written in %lu s%s", n, (unsigned long)((millis() - t0) / 1000),
              skip0 ? ", row 0 unchanged" : "");
    }

    setState(Verifying, 0, rows);
    uint16_t n = 0, bad = 0;
    uint32_t firstBad = 0;
    for (uint16_t r = 0; r < PaHex::ROWS; r++) {
        if (!hex.mask(r)) continue;
        if (abort_) { setError("aborted"); return; }
        const uint32_t pc = (uint32_t)r * PaHex::ROW_ADDR;
        uint32_t got[32];
        if (!readWords(pc, 32, got)) return;
        if (r && !hex.rowData(r, data)) { setError("hexstore"); return; }
        if (!same(got, r ? data : first, r ? hex.mask(r) : mask0)) {
            if (!bad) firstBad = pc;
            bad++;
        }
        setState(Verifying, ++n, rows);
    }
    if (bad) {
        setError("verify", "%u, 0x%06lX", bad, (unsigned long)firstBad);
        return;
    }
    setState(Done, rows, rows);
    log_i("PA flash: verify OK");
}
