#pragma once

// Configuration and diagnostics over the USB serial monitor (UART0).
// Meant for commissioning: enter Wi-Fi credentials without the detour via the
// fallback AP, and test the RS-232 link to the PA directly.
void consoleBegin();
void consoleLoop();

// Diagnostic tracing, off by default and switched on at runtime with the
// console command 'debug 1'. Not stored, so a restart always turns it off
// again - tracing costs time in the very loop it measures.
//
// Guard every call site with dbgOn() so an inactive trace costs one bool
// test, not a printf and its arguments:
//     if (dbgOn()) dbg("web: / %u B in %lu ms", n, ms);
bool dbgOn();
void dbg(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
