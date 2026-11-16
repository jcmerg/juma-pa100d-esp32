#pragma once
#include <stddef.h>
#include <stdint.h>

// Intel HEX of a JUMA firmware (dsPIC30F, Ingenia boot loader), read as it
// streams in. The rules are those of tools/juma-flash.py in the firmware
// repository: only program memory counts, the configuration registers and the
// data EEPROM are reported and ignored, and the file must carry a reset
// vector plus the GOTO __reset at 0x100 that the boot loader starts.
//
// The image does not fit in RAM next to Wi-Fi - up to 192 kB - so the bytes
// go to a Store at their byte address (program address * 2). A store starts
// out erased (0xFF) and a write may only clear bits, as on flash. Records do
// not arrive sorted - XC16 puts the vectors after the code - so the parser
// does not assume an order. Which instructions the file holds is kept here,
// one bit per instruction: gaps within a row are written as NOP, and the
// verify only compares what the file holds.
class PaHex {
public:
    static const uint32_t FLASH_MAX = 0x018000;   // end of program memory, dsPIC30F6014A
    static const uint32_t ROW_ADDR  = 64;         // program addresses per row (32 instructions)
    static const uint32_t ROW_BYTES = 128;        // bytes per row in the file and the store
    static const uint16_t ROWS      = FLASH_MAX / ROW_ADDR;
    static const uint32_t STORE_BYTES = FLASH_MAX * 2;

    struct Store {
        virtual bool write(uint32_t off, const uint8_t* d, size_t n) = 0;
        virtual bool read(uint32_t off, uint8_t* d, size_t n) = 0;
    };

    // Codes rather than text: the dashboard translates them.
    enum Err : uint8_t { OK, REC, TYPE, RANGE, CONFLICT, VECTOR, NOGOTO, STORE, NOEOF };
    static const char* errCode(Err e);

    void begin(Store* s);                         // store must be erased
    bool feed(const uint8_t* d, size_t n);        // any split of the file
    bool finish();

    Err      error() const      { return err_; }
    uint32_t errLine() const    { return errLine_; }
    uint32_t errAddr() const    { return errAddr_; }
    uint16_t rowCount() const;
    uint32_t lastAddr() const;                    // last program address in the image
    bool     skippedConfig() const { return skipCfg_; }
    bool     skippedEeprom() const { return skipEe_; }

    uint32_t mask(uint16_t row) const { return row < ROWS ? mask_[row] : 0; }
    // One row as it goes to the device: gaps as NOP (0x000000).
    bool     rowData(uint16_t row, uint8_t out[ROW_BYTES]) const;

private:
    bool line();
    bool fail(Err e, uint32_t addr = 0);
    bool put(uint32_t byteAddr, const uint8_t* d, size_t n);

    Store*   st_ = nullptr;
    uint32_t mask_[ROWS];
    char     buf_[528];                           // ':' + 2 * (5 + 255) + slack
    uint16_t len_ = 0;
    bool     over_ = false;                       // line longer than buf_
    bool     eof_ = false;                        // type 01 seen
    uint32_t base_ = 0;
    uint32_t lineNo_ = 0;                         // line breaks so far
    bool     whole_ = false;                      // past the last line
    Err      err_ = OK;
    uint32_t errLine_ = 0, errAddr_ = 0;
    bool     skipCfg_ = false, skipEe_ = false;
};

// Words of a row as the device reads them back: 24 bits per instruction, the
// fourth (phantom) byte is not part of it.
uint32_t paRowWord(const uint8_t* row, uint8_t k);
// Target of a GOTO (two words), or -1.
int32_t  paGotoTarget(uint32_t w0, uint32_t w1);
