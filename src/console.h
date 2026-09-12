#pragma once

// Konfiguration und Diagnose ueber den USB-Serialmonitor (UART0).
// Gedacht fuer die Inbetriebnahme: WLAN eintragen, ohne erst den AP-Umweg zu
// gehen, und die RS-232-Strecke zur PA direkt testen.
void consoleBegin();
void consoleLoop();
