#pragma once
#include <stdint.h>

// JUMA-Bandindex, wie ihn '=Bn' und Statusfeld 5 verwenden.
// 1 = 160 m ... 9 = 10 m, 10 = "Unknown Band" (nur als Statusantwort).
enum : uint8_t { BAND_NONE = 0, BAND_UNKNOWN = 10 };

// Frequenz -> JUMA-Bandindex. 0, wenn die PA das Band nicht abdeckt
// (6 m, 4 m, 2 m, 60 m, LF/MF) oder die QRG in keinem Band liegt.
uint8_t  bandFromHz(uint32_t hz);

// Klartextname fuer Anzeige/Log. bandFromHz()==0 -> Name des erkannten,
// aber nicht abgedeckten Bandes, sonst "-".
const char* bandName(uint8_t jumaIndex);
const char* bandNameFromHz(uint32_t hz);
