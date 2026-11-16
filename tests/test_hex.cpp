// Host tests for the HEX reader of the PA firmware update.
//   clang++ -std=c++17 -I src tests/test_hex.cpp src/pa_hex.cpp -o /tmp/t && /tmp/t
#include "pa_hex.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    fails++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Behaves like flash: starts erased, a write can only clear bits.
struct Mem : PaHex::Store {
    std::vector<uint8_t> m = std::vector<uint8_t>(PaHex::STORE_BYTES, 0xFF);
    bool write(uint32_t off, const uint8_t* d, size_t n) override {
        for (size_t i = 0; i < n; i++) m[off + i] &= d[i];
        return true;
    }
    bool read(uint32_t off, uint8_t* d, size_t n) override {
        memcpy(d, &m[off], n);
        return true;
    }
};

// One Intel HEX record with its checksum.
static std::string rec(uint16_t addr, uint8_t type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> r = { (uint8_t)data.size(), (uint8_t)(addr >> 8), (uint8_t)addr, type };
    r.insert(r.end(), data.begin(), data.end());
    uint8_t sum = 0;
    for (uint8_t b : r) sum += b;
    r.push_back((uint8_t)-sum);
    std::string s = ":";
    char h[3];
    for (uint8_t b : r) { snprintf(h, sizeof(h), "%02X", b); s += h; }
    return s + "\r\n";
}

// Instructions at a program address, as XC16 writes them: 4 bytes each,
// the phantom byte 0. Assumes the program address fits in 16 bits after * 2.
static std::string words(uint32_t pc, const std::vector<uint32_t>& w) {
    std::vector<uint8_t> d;
    for (uint32_t x : w) { d.push_back(x & 0xFF); d.push_back((x >> 8) & 0xFF); d.push_back(x >> 16); d.push_back(0); }
    return rec((uint16_t)(pc * 2), 0x00, d);
}
static std::string base(uint16_t upper) { return rec(0, 0x04, { (uint8_t)(upper >> 8), (uint8_t)upper }); }
static const std::string END = rec(0, 0x01, {});

// GOTO 0x000200: the reset vector and the copy at 0x100 the boot loader starts.
static const uint32_t GOTO0 = 0x040200, GOTO1 = 0x000000;

static Mem mem;
static PaHex hex;

static bool load(const std::string& s, size_t step = 0) {
    mem = Mem();
    hex.begin(&mem);
    const uint8_t* p = (const uint8_t*)s.data();
    if (!step) hex.feed(p, s.size());
    else for (size_t i = 0; i < s.size(); i += step) hex.feed(p + i, std::min(step, s.size() - i));
    return hex.finish();
}

static std::string firmware() {
    return base(0) + words(0x200, { 0x123456, 0xABCDEF, 0x000001 }) +
           words(0x100, { GOTO0, GOTO1 }) +
           words(0x000, { GOTO0, GOTO1 }) +          // vectors after the code, as XC16 does
           words(0x004, { 0x000300 }) + END;
}

static void test_valid() {
    printf("Valid firmware, vectors out of order\n");
    CHECK(load(firmware()), "should load, error %s line %u", PaHex::errCode(hex.error()), hex.errLine());
    CHECK(hex.rowCount() == 3, "3 rows, got %u", hex.rowCount());
    CHECK(hex.lastAddr() == 0x200 + 62, "last 0x23E, got 0x%X", hex.lastAddr());
    CHECK(hex.mask(0) == 0b111, "row 0 mask 0b111, got 0x%X", hex.mask(0));
    CHECK(hex.mask(4) == 0b11, "row 0x100 mask 0b11, got 0x%X", hex.mask(4));
    CHECK(hex.mask(8) == 0b111, "row 0x200 mask, got 0x%X", hex.mask(8));
    uint8_t row[PaHex::ROW_BYTES];
    CHECK(hex.rowData(8, row), "rowData");
    CHECK(paRowWord(row, 0) == 0x123456 && paRowWord(row, 2) == 0x000001, "data of row 0x200");
    bool gaps = true;
    for (size_t i = 12; i < sizeof(row); i++) gaps = gaps && row[i] == 0;
    CHECK(gaps, "gaps are NOP (0x000000), not erased flash");
    CHECK(!hex.skippedConfig() && !hex.skippedEeprom(), "nothing skipped");
}

static void test_split() {
    printf("Fed byte by byte and in odd chunks\n");
    CHECK(load(firmware(), 1), "byte by byte");
    CHECK(hex.rowCount() == 3, "3 rows, got %u", hex.rowCount());
    CHECK(load(firmware(), 7), "chunks of 7");
    CHECK(hex.mask(0) == 0b111, "row 0 mask");
    // Without the final line break, and with LF only.
    std::string s = firmware();
    s = s.substr(0, s.size() - 2);
    CHECK(load(s), "no trailing newline");
    std::string lf;
    for (char c : firmware()) if (c != '\r') lf += c;
    CHECK(load(lf), "LF only");
}

static void test_skipped() {
    printf("Configuration and EEPROM are ignored\n");
    // Configuration at program address 0xF80000 = byte address 0x1F00000.
    std::string cfg = base(0x01F0) + rec(0x0000, 0x00, { 0x00, 0xC3, 0x00, 0x00 });
    // Data EEPROM at 0x7FFC00 = byte address 0xFFF800.
    std::string ee  = base(0x00FF) + rec(0xF800, 0x00, { 0x12, 0x34, 0x00, 0x00 });
    std::string s = firmware();
    s.insert(s.size() - END.size(), cfg + ee);
    CHECK(load(s), "should load");
    CHECK(hex.skippedConfig(), "configuration reported");
    CHECK(hex.skippedEeprom(), "EEPROM reported");
    CHECK(hex.rowCount() == 3, "still 3 rows, got %u", hex.rowCount());
}

static void test_errors() {
    printf("Broken files are refused\n");
    std::string s = firmware();
    std::string line1 = rec(0, 0x04, { 0x00, 0x00 });
    line1[line1.size() - 3] = (line1[line1.size() - 3] == '0') ? '1' : '0';
    CHECK(!load(line1 + s.substr(line1.size())), "bad checksum refused");
    CHECK(hex.error() == PaHex::REC && hex.errLine() == 1, "hexrec on line 1, got %s line %u",
          PaHex::errCode(hex.error()), hex.errLine());

    CHECK(!load("garbage\r\n" + s), "not a record");
    CHECK(hex.error() == PaHex::REC, "hexrec");
    // Blank lines count: the number is the one an editor shows.
    CHECK(!load(s.substr(0, s.find('\n') + 1) + "\r\n\r\ngarbage\r\n" + s), "garbage after blank lines");
    CHECK(hex.errLine() == 4, "hexrec on line 4, got %u", hex.errLine());

    CHECK(!load(rec(0, 0x03, { 0, 0, 0, 0 }) + s), "type 03 refused");
    CHECK(hex.error() == PaHex::TYPE, "hextype, got %s", PaHex::errCode(hex.error()));

    // 0x018000 is past the end of program memory: byte address 0x30000.
    CHECK(!load(base(0x0003) + rec(0, 0x00, { 1, 2, 3, 0 }) + s), "beyond program memory");
    CHECK(hex.error() == PaHex::RANGE && hex.errAddr() == 0x18000,
          "hexrange at 0x18000, got %s 0x%X", PaHex::errCode(hex.error()), hex.errAddr());

    std::string dup = firmware();
    dup.insert(dup.size() - END.size(), words(0x200, { 0x123456 }));
    CHECK(load(dup), "the same data twice is fine");
    dup = firmware();
    dup.insert(dup.size() - END.size(), words(0x200, { 0x654321 }));
    CHECK(!load(dup), "conflicting data refused");
    CHECK(hex.error() == PaHex::CONFLICT && hex.errAddr() == 0x200,
          "hexdup at 0x200, got %s 0x%X", PaHex::errCode(hex.error()), hex.errAddr());

    CHECK(!load(words(0, { GOTO0, GOTO1 }) + words(0x200, { 1 }) + END), "no 0x100");
    CHECK(hex.error() == PaHex::VECTOR, "hexvec, got %s", PaHex::errCode(hex.error()));
    // Missing records are no fault of the last line - no line number.
    CHECK(hex.errLine() == 0, "hexvec without a line number, got %u", hex.errLine());

    CHECK(!load(words(0, { GOTO0, GOTO1 }) + words(0x100, { 0x040300, 0 }) + END), "0x100 differs");
    CHECK(hex.error() == PaHex::NOGOTO, "hexgoto, got %s", PaHex::errCode(hex.error()));
    CHECK(hex.errLine() == 0, "hexgoto without a line number, got %u", hex.errLine());

    // Cut short after the vectors: everything checks out but the end record.
    std::string cut = firmware();
    cut = cut.substr(0, cut.size() - END.size());
    CHECK(!load(cut), "no end record");
    CHECK(hex.error() == PaHex::NOEOF, "hexeof, got %s", PaHex::errCode(hex.error()));

    CHECK(!load(words(0, { 0x123456, 0 }) + words(0x100, { 0x123456, 0 }) + END), "no GOTO");
    CHECK(hex.error() == PaHex::NOGOTO, "hexgoto");

    CHECK(!load(""), "empty file");
    CHECK(hex.error() == PaHex::NOEOF, "hexeof for an empty file");
}

static void test_goto() {
    printf("GOTO decoding\n");
    CHECK(paGotoTarget(0x047D00, 0x000001) == 0x017D00, "GOTO 0x17D00");
    CHECK(paGotoTarget(0x040200, 0x000000) == 0x000200, "GOTO 0x200");
    CHECK(paGotoTarget(0x057D00, 0x000001) < 0, "not a GOTO");
    CHECK(paGotoTarget(0x047D00, 0x000080) < 0, "second word out of range");
}

int main() {
    test_valid();
    test_split();
    test_skipped();
    test_errors();
    test_goto();
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
