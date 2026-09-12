#pragma once
#include <Arduino.h>

struct Settings {
    String   ssid;
    String   pass;
    String   tciHost;
    uint16_t tciPort  = 40001;
    bool     tciEn    = false;
    bool     autoband = false;   // nach Reset aus: fail-safe
    bool     otaStandby = true;  // vor einem Firmware-Update "=S" an die PA
    bool     tciLostAuto = false;// bei TCI-Verlust "=A": PA waehlt wieder selbst
};

extern Settings cfg;

void settingsLoad();
void settingsSave();

void webBegin();
uint8_t webClients();   // verbundene Dashboard-Browser
void webLoop();

// Hinweis fuer das Dashboard. Uebertragen wird nur ein Code plus optionales
// Argument - die Uebersetzung passiert im Browser, damit die Sprachumschaltung
// nicht an in der Firmware festgetackerten Texten scheitert.
//   ""            kein Hinweis
//   "txwait"      Bandwechsel wartet, TX aktiv
//   "unsupported" Band wird von der PA nicht abgedeckt   (arg = Bandname)
//   "bandset"     Band umgeschaltet                      (arg = Bandname)
//   "abon"/"aboff" TCI-Bandwahl ein/aus
//   "tciauto"     TCI weg, PA auf eigene Bandwahl zurueckgestellt
void webSetNote(const char* code, const char* arg = "");
