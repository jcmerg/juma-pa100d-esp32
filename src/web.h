#pragma once
#include <Arduino.h>

struct Settings {
    String   hostname;   // Netzname fuer WLAN, mDNS und OTA
    String   ssid;
    String   pass;
    String   tciHost;
    uint16_t tciPort  = 40001;
    bool     tciEn    = false;
    bool     autoband = false;   // nach Reset aus: fail-safe
    bool     otaStandby = true;  // vor einem Firmware-Update "=S" an die PA
    // Temperaturwarnung. Das Alarmbit der PA kommt erst beim Abschalten - dann
    // ist es zu spaet. Diese Schwellen sind Werte des Dashboards; die echte
    // Abschaltgrenze der PA steht nicht in der Statusmeldung.
    uint8_t  tempWarn  = 50;     // Grad C: ab hier Warnung (Ton und Banner)
    uint8_t  tempHigh  = 60;     // Grad C: ab hier rot in der Anzeige
    bool     tempAlarm = true;   // Warnung ueberhaupt ausloesen

    bool     tciLostAuto = false;// bei TCI-Verlust "=A": PA waehlt wieder selbst
};

extern Settings cfg;

String sanitizeHostname(const String& in);

void settingsLoad();
void settingsSave();

void webBegin();
uint8_t webClients();   // verbundene Dashboard-Browser
void webLoop();

// Hinweis fuer das Dashboard. Uebertragen wird nur ein Code plus optionales
// Argument - die Uebersetzung passiert im Browser, damit die Sprachumschaltung
// nicht an in der Firmware festgetackerten Texten scheitert.
//   ""            kein Hinweis
//   "unsupported" Band wird von der PA nicht abgedeckt   (arg = Bandname)
//   "bandset"     Band umgeschaltet                      (arg = Bandname)
//   "abon"/"aboff" TCI-Bandwahl ein/aus
//   "tciauto"     TCI weg, PA auf eigene Bandwahl zurueckgestellt
void webSetNote(const char* code, const char* arg = "");
