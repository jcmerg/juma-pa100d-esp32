// Hosttests fuer den Statusparser und die Bandzuordnung.
//   clang++ -std=c++17 -I src tests/test_parse.cpp src/juma_status.cpp src/bands.cpp -o /tmp/t && /tmp/t
#include "juma_status.h"
#include "bands.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    fails++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool feq(float a, float b) { return std::fabs(a - b) < 0.005f; }

// Der Parser arbeitet in place - also jedes Mal auf einer Kopie.
static bool parse(const char* s, JumaStatus& st) {
    char buf[160];
    std::strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return jumaParseStatus(buf, st);
}

static void test_manual_example() {
    printf("Beispiel aus dem Manual (Annex D)\n");
    JumaStatus st;
    // O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st), "sollte parsen");
    CHECK(st.valid,            "valid");
    CHECK(st.operate,          "Feld 1 O -> operate");
    CHECK(st.autoSel,          "Feld 2 A -> autoSel");
    CHECK(st.tx,               "Feld 3 T -> tx");
    CHECK(st.celsius,          "Feld 4 C -> celsius");
    CHECK(st.band == 5,        "Feld 5 Band = 5, ist %u", st.band);
    CHECK(st.gain == 1,        "Feld 6 Gain = 1, ist %u", st.gain);
    CHECK(feq(st.swr, 1.0f),   "Feld 7 SWR = 1.0, ist %.2f", st.swr);
    CHECK(feq(st.volts, 14.09f), "Feld 8 Volt = 14.09, ist %.2f", st.volts);
    CHECK(feq(st.amps, 8.1f),  "Feld 9 Ampere = 8.1, ist %.2f", st.amps);
    CHECK(feq(st.watts, 27.2f),"Feld 10 Watt = 27.2, ist %.2f", st.watts);
    CHECK(st.temp == 26,       "Feld 11 Temp = 26, ist %d", st.temp);
    CHECK(st.fan == 0,         "Feld 12 Fan = 0, ist %u", st.fan);
    CHECK(st.alarms == 0,      "Feld 13 Alarm = 0, ist 0x%02X", st.alarms);
}

static void test_standby_variant() {
    printf("STANDBY / Manual / RX / Fahrenheit\n");
    JumaStatus st;
    CHECK(parse("S:M:R:F: 3:4:1.4:13.80: 0.4:  0.0: 79:2: 0", st), "sollte parsen");
    CHECK(!st.operate, "S -> standby");
    CHECK(!st.autoSel, "M -> manuell");
    CHECK(!st.tx,      "R -> rx");
    CHECK(!st.celsius, "F -> Fahrenheit");
    CHECK(st.band == 3, "Band 3 (40m)");
    CHECK(st.gain == 4, "Gain 4");
    CHECK(feq(st.watts, 0.0f), "0 W");
    CHECK(st.temp == 79, "Temp 79");
    CHECK(st.fan == 2, "Fan medium");
}

static void test_alarms_are_hex() {
    printf("Alarmfeld ist hexadezimal, nicht dezimal\n");
    JumaStatus st;

    // "10" hex = Bit 4 = Low Voltage Pre-Limit.
    // Dezimal gelesen waere es 10 = 0b1010 = Over-Current + High Voltage.
    CHECK(parse("O:A:R:C: 5:1:1.0:11.20: 0.2:  0.0: 30:0: 10", st), "parst");
    CHECK(st.alarms == ALARM_LOW_VOLT_PRE, "0x10 -> nur LOW_VOLT_PRE, ist 0x%02X", st.alarms);
    CHECK(!(st.alarms & ALARM_OVERCURRENT), "kein Fehlalarm Over-Current");
    CHECK(!(st.alarms & ALARM_HIGH_VOLT),   "kein Fehlalarm High Voltage");

    // "20" hex = Bit 5. Dezimal waere 32 = Bit 5 - zufaellig gleich, aber
    // "30" trennt die Faelle: hex = Bit 4|5, dezimal 30 = 0b11110.
    CHECK(parse("O:A:R:C: 5:1:1.0:10.50: 0.2:  0.0: 30:0: 30", st), "parst");
    CHECK(st.alarms == (ALARM_LOW_VOLT_PRE | ALARM_LOW_VOLT_FINAL),
          "0x30 -> beide Low-Voltage-Bits, ist 0x%02X", st.alarms);
    CHECK(!(st.alarms & ALARM_HIGH_TEMP), "kein Fehlalarm High Temp");

    CHECK(parse("O:A:T:C: 9:2:3.1:12.00:12.0:110.0: 75:3: 3F", st), "parst");
    CHECK(st.alarms == 0x3F, "0x3F -> alle sechs Bits, ist 0x%02X", st.alarms);

    CHECK(parse("O:A:T:C: 9:2:3.1:12.00:12.0:110.0: 75:3: 1", st), "parst");
    CHECK(st.alarms == ALARM_HIGH_SWR, "0x01 -> High SWR");

    CHECK(parse("O:A:T:C: 9:2:1.1:13.80: 9.0: 99.0: 60:3: c", st), "parst");
    CHECK(st.alarms == 0x0C, "kleingeschriebenes Hex 'c' -> 0x0C, ist 0x%02X", st.alarms);
}

static void test_edge_cases() {
    printf("Randfaelle\n");
    JumaStatus st;

    CHECK(parse("O:A:R:C:10:1:1.0:13.80: 0.3:  0.0: 25:0: 0", st), "parst");
    CHECK(st.band == BAND_UNKNOWN, "Band 10 = Unknown Band, ist %u", st.band);

    CHECK(!parse("O:A:T:C: 5:1:1.0", st), "zu wenig Felder -> abgewiesen");
    CHECK(!parse("X:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st),
          "Feld 1 weder O noch S -> abgewiesen");
    CHECK(!parse("", st), "Leerzeile -> abgewiesen");
    CHECK(!parse("Juma PA-100D V4.00a", st), "Bootmeldung -> abgewiesen");

    // Eine Firmware, die weitere Felder anhaengt, soll nicht alles blockieren.
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0:XX", st),
          "14 Felder -> parst trotzdem");
    CHECK(st.band == 5, "Bandfeld bleibt korrekt");

    // raw muss die Zeile wieder hergeben (der Parser zerschneidet sie in place)
    CHECK(parse("O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0", st), "parst");
    CHECK(!std::strcmp(st.raw, "O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0"),
          "raw rekonstruiert, ist '%s'", st.raw);

    // Dreistellige Temperatur und 100+ W
    CHECK(parse("O:A:T:C: 9:4:1.2:13.60:18.5:105.4:103:3: 4", st), "parst");
    CHECK(st.temp == 103, "Temp 103, ist %d", st.temp);
    CHECK(feq(st.watts, 105.4f), "105.4 W, ist %.1f", st.watts);
    CHECK(st.alarms == ALARM_HIGH_TEMP, "High Temperature");
}

struct BandCase { uint32_t hz; uint8_t juma; const char* name; };

static void test_bands() {
    printf("Frequenz -> Bandindex\n");
    static const BandCase cases[] = {
        { 1840000, 1, "160m"}, { 1999000, 1, "160m"},
        { 3573000, 2, "80m" }, { 3800000, 2, "80m" },
        { 7074000, 3, "40m" }, { 7200000, 3, "40m" },
        {10136000, 4, "30m" },
        {14074000, 5, "20m" }, {14350000, 5, "20m" },
        {18100000, 6, "17m" },
        {21074000, 7, "15m" },
        {24915000, 8, "12m" },
        {28074000, 9, "10m" }, {29600000, 9, "10m" },
        // erkannt, aber von der PA-100D nicht abgedeckt -> 0, kein =Bn
        {50313000, 0, "6m"  }, { 5357000, 0, "60m" },
        {70200000, 0, "4m"  }, {144300000,0, "2m"  },
        {  474200, 0, "630m"}, {  137500, 0, "2200m"},
        // gar kein Band
        { 9000000, 0, "-"   }, {15000000, 0, "-"   }, {       0, 0, "-" },
    };
    for (const auto& c : cases) {
        uint8_t got = bandFromHz(c.hz);
        CHECK(got == c.juma, "%u Hz -> erwartet %u (%s), ist %u",
              c.hz, c.juma, c.name, got);
        CHECK(!std::strcmp(bandNameFromHz(c.hz), c.name),
              "%u Hz -> Name '%s', ist '%s'", c.hz, c.name, bandNameFromHz(c.hz));
    }
    // bandName() fuer die Statusanzeige
    CHECK(!std::strcmp(bandName(1), "160m"), "bandName(1)");
    CHECK(!std::strcmp(bandName(9), "10m"),  "bandName(9)");
    CHECK(!std::strcmp(bandName(10), "?"),   "bandName(10) = Unknown");
    CHECK(!std::strcmp(bandName(0), "-"),    "bandName(0)");
}

int main() {
    test_manual_example();
    test_standby_variant();
    test_alarms_are_hex();
    test_edge_cases();
    test_bands();
    printf("\n%d Checks, %d Fehler\n", checks, fails);
    return fails ? 1 : 0;
}
