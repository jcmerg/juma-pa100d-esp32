#pragma once

// Configuration and diagnostics over the USB serial monitor (UART0).
// Meant for commissioning: enter Wi-Fi credentials without the detour via the
// fallback AP, and test the RS-232 link to the PA directly.
void consoleBegin();
void consoleLoop();
