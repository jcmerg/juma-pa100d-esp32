#include "pa_hex.h"
#include <string.h>

const char* PaHex::errCode(Err e) {
    switch (e) {
    case OK:       return "";
    case REC:      return "hexrec";      // not a record, or bad checksum
    case TYPE:     return "hextype";     // record type other than 00/01/02/04
    case RANGE:    return "hexrange";    // data beyond program memory
    case CONFLICT: return "hexdup";      // one address, two different values
    case VECTOR:   return "hexvec";      // no reset vector or no 0x100
    case NOGOTO:   return "hexgoto";     // no GOTO __reset at 0x100
    case STORE:    return "hexstore";    // writing the scratch area failed
    case NOEOF:    return "hexeof";      // no end record: file cut short
    }
    return "hexrec";
}

uint32_t paRowWord(const uint8_t* row, uint8_t k) {
    const uint8_t* p = row + 4 * k;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

int32_t paGotoTarget(uint32_t w0, uint32_t w1) {
    if ((w0 >> 16) != 0x04 || (w1 & 0xFFFF80)) return -1;
    return (int32_t)((w0 & 0xFFFE) | ((w1 & 0x7F) << 16));
}

void PaHex::begin(Store* s) {
    st_ = s;
    memset(mask_, 0, sizeof(mask_));
    len_ = 0;
    over_ = eof_ = false;
    base_ = 0;
    lineNo_ = 0;
    whole_ = false;
    err_ = OK;
    errLine_ = errAddr_ = 0;
    skipCfg_ = skipEe_ = false;
}

bool PaHex::fail(Err e, uint32_t addr) {
    if (err_ == OK) { err_ = e; errLine_ = whole_ ? 0 : lineNo_ + 1; errAddr_ = addr; }
    return false;
}

bool PaHex::feed(const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (err_ != OK) return false;
        const char c = (char)d[i];
        if (c == '\n' || c == '\r') {
            if ((len_ || over_) && !line()) return false;
            len_ = 0;
            over_ = false;
            if (c == '\n') lineNo_++;
            continue;
        }
        if (len_ < sizeof(buf_) - 1) buf_[len_++] = c;
        else over_ = true;
    }
    return err_ == OK;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool PaHex::line() {
    // Blank lines (a trailing CR LF, spaces) are allowed, as in the Python tool.
    uint16_t s = 0, e = len_;
    while (s < e && (buf_[s] == ' ' || buf_[s] == '\t')) s++;
    while (e > s && (buf_[e - 1] == ' ' || buf_[e - 1] == '\t')) e--;
    if (s == e && !over_) return true;
    if (eof_) return true;                    // the Python tool stops at 01 too
    if (over_ || buf_[s] != ':' || ((e - s - 1) & 1)) return fail(REC);

    uint8_t rec[264];
    const uint16_t n = (uint16_t)((e - s - 1) / 2);
    if (n < 5 || n > sizeof(rec)) return fail(REC);
    uint8_t sum = 0;
    for (uint16_t i = 0; i < n; i++) {
        int hi = hexNibble(buf_[s + 1 + 2 * i]), lo = hexNibble(buf_[s + 2 + 2 * i]);
        if (hi < 0 || lo < 0) return fail(REC);
        rec[i] = (uint8_t)(hi << 4 | lo);
        sum += rec[i];
    }
    if (n != rec[0] + 5u || sum) return fail(REC);

    const uint8_t  count = rec[0];
    const uint32_t addr  = ((uint32_t)rec[1] << 8) | rec[2];
    const uint8_t* data  = rec + 4;
    switch (rec[3]) {
    case 0x00: break;
    case 0x01: eof_ = true; return true;
    case 0x04:
        if (count < 2) return fail(REC);
        base_ = (((uint32_t)data[0] << 8) | data[1]) << 16;
        return true;
    case 0x02:
        if (count < 2) return fail(REC);
        base_ = (((uint32_t)data[0] << 8) | data[1]) << 4;
        return true;
    default:
        return fail(TYPE);
    }

    // Byte by byte into program memory, configuration or EEPROM - one record
    // could in principle straddle a boundary, so no shortcut per record.
    uint8_t i = 0;
    while (i < count) {
        const uint32_t byteAddr = base_ + addr + i;
        const uint32_t pc = byteAddr / 2;
        if (pc >= 0x800000) { skipCfg_ = true; i++; continue; }   // configuration 0xF80000
        if (pc >= 0x7FF000) { skipEe_ = true; i++; continue; }    // data EEPROM
        if (pc >= FLASH_MAX) return fail(RANGE, pc);
        // A run up to the end of this row, so one store write per record.
        const uint32_t rowEnd = (byteAddr / ROW_BYTES + 1) * ROW_BYTES;
        uint8_t run = (uint8_t)(count - i);
        if (byteAddr + run > rowEnd) run = (uint8_t)(rowEnd - byteAddr);
        if (!put(byteAddr, data + i, run)) return false;
        i += run;
    }
    return true;
}

bool PaHex::put(uint32_t byteAddr, const uint8_t* d, size_t n) {
    // A second record for the same address: harmless if it says the same,
    // an error otherwise. The store can only clear bits, so silently taking
    // the later value as the Python tool does is not an option.
    uint8_t old[ROW_BYTES];
    if (!st_->read(byteAddr, old, n)) return fail(STORE, byteAddr / 2);
    for (size_t k = 0; k < n; k++)
        if (old[k] != 0xFF && old[k] != d[k]) return fail(CONFLICT, (byteAddr + k) / 2);
    if (!st_->write(byteAddr, d, n)) return fail(STORE, byteAddr / 2);
    for (size_t k = 0; k < n; k++) {
        const uint32_t pc = (byteAddr + k) / 2;
        mask_[pc / ROW_ADDR] |= 1u << ((pc % ROW_ADDR) / 2);
    }
    return true;
}

bool PaHex::finish() {
    if (err_ == OK && (len_ || over_)) line();
    len_ = 0;
    over_ = false;
    if (err_ != OK) return false;

    // Errors from here on concern the file as a whole, not a line of it.
    whole_ = true;
    // Without the end record a file cut short would pass whenever its
    // vectors happened to come first - and flash half a firmware.
    if (!eof_) return fail(NOEOF);
    const uint16_t startRow = 0x100 / ROW_ADDR;
    if (!mask_[0] || !mask_[startRow]) return fail(VECTOR);
    uint8_t r0[ROW_BYTES], r1[ROW_BYTES];
    if (!rowData(0, r0) || !rowData(startRow, r1)) return fail(STORE);
    // The boot loader starts the firmware through the GOTO at 0x100 (see
    // juma-trx2.gld), and that has to be the same jump as the reset vector.
    const uint32_t a = paRowWord(r0, 0), b = paRowWord(r0, 1);
    if ((a >> 16) != 0x04 || paRowWord(r1, 0) != a || paRowWord(r1, 1) != b)
        return fail(NOGOTO);
    return true;
}

bool PaHex::rowData(uint16_t row, uint8_t out[ROW_BYTES]) const {
    if (row >= ROWS || !st_->read((uint32_t)row * ROW_BYTES, out, ROW_BYTES)) return false;
    for (uint8_t k = 0; k < ROW_BYTES / 4; k++)
        if (!(mask_[row] & (1u << k))) memset(out + 4 * k, 0, 4);
    return true;
}

uint16_t PaHex::rowCount() const {
    uint16_t n = 0;
    for (uint16_t r = 0; r < ROWS; r++) if (mask_[r]) n++;
    return n;
}

uint32_t PaHex::lastAddr() const {
    for (int r = ROWS - 1; r >= 0; r--)
        if (mask_[r]) return (uint32_t)r * ROW_ADDR + ROW_ADDR - 2;
    return 0;
}
