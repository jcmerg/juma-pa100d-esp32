#pragma once
#include <stdint.h>

// Arduino-frei, damit der Parser auch auf dem Host getestet werden kann
// (siehe tests/test_parse.cpp).

// Alarm-Bits aus Statusfeld 13. ACHTUNG: das Feld ist HEXADEZIMAL kodiert
// (Manual: "HH Hexadecimal bit-mapped alarm status"). Wer es dezimal liest,
// dekodiert ab 0x0A falsch: "10" ist Bit 4, nicht Bit 1 und 3.
enum JumaAlarm : uint16_t {
    ALARM_HIGH_SWR       = 1 << 0,
    ALARM_OVERCURRENT    = 1 << 1,
    ALARM_HIGH_TEMP      = 1 << 2,
    ALARM_HIGH_VOLT      = 1 << 3,
    ALARM_LOW_VOLT_PRE   = 1 << 4,
    ALARM_LOW_VOLT_FINAL = 1 << 5,
};

struct JumaStatus {
    bool     valid    = false;   // mindestens eine Statusantwort verstanden
    uint32_t lastRxMs = 0;       // setzt der Treiber, nicht der Parser
    bool     operate  = false;   // Feld 1: O / S
    bool     autoSel  = false;   // Feld 2: A / M  (Bandwahl der PA selbst)
    bool     tx       = false;   // Feld 3: T / R
    bool     celsius  = true;    // Feld 4: C / F
    uint8_t  band     = 0;       // Feld 5: 1..9, 10 = unknown
    uint8_t  gain     = 0;       // Feld 6: 1..4  (real ein Abschwaecher)
    float    swr      = 0;       // Feld 7
    float    volts    = 0;       // Feld 8
    float    amps     = 0;       // Feld 9
    float    watts    = 0;       // Feld 10
    int      temp     = 0;       // Feld 11
    uint8_t  fan      = 0;       // Feld 12: 0=Off 1=Slow 2=Medium 3=Fast
    uint16_t alarms   = 0;       // Feld 13, hex
    char     raw[100] = {0};
};

// Zerlegt eine Statusantwort der PA, z.B.
//   O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0
// Die Zeile wird dabei in place veraendert. Rueckgabe false, wenn es keine
// verwertbare Statusantwort ist (zu wenig Felder, Feld 1 nicht O/S).
bool jumaParseStatus(char* line, JumaStatus& st);
