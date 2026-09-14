#pragma once
#include <Arduino.h>

struct Settings {
    String   hostname;   // network name for Wi-Fi, mDNS and OTA
    String   ssid;
    String   pass;
    String   tciHost;
    uint16_t tciPort  = 40001;
    bool     tciEn    = false;
    bool     autoband = false;   // off after reset: fail-safe
    bool     otaStandby = true;  // send "=S" before a firmware update
    // Temperature warning. The PA raises its alarm bit only when it shuts
    // down - by then it is too late. These thresholds belong to the dashboard;
    // the PA's real cut-out limit is not part of the status message.
    uint8_t  tempWarn  = 50;     // deg C: warn from here (tone and banner)
    uint8_t  tempHigh  = 60;     // deg C: red in the gauge from here
    bool     tempAlarm = true;   // raise the warning at all

    // The same for SWR. The PA's trip limit (factory default 3.0, adjustable
    // 1.0-10.0) is likewise absent from the status message.
    uint8_t  swrWarnX10  = 20;   // 2.0 - warn from here
    uint8_t  swrHighX10  = 25;   // 2.5 - red in the gauge from here
    bool     swrAlarm    = true;

    bool     tciLostAuto = false;// on TCI loss send "=A": the PA selects again

    // Fixed address instead of DHCP. Kept as text: that is what goes in and
    // comes out of the console and the web form, and an empty string is an
    // unambiguous "not set" - an IPAddress of 0.0.0.0 is not.
    bool     staticIp = false;
    String   ip;                 // e.g. "192.168.15.190"
    String   gw;                 // gateway; DNS follows it unless dns is set
    String   mask = "255.255.255.0";
    String   dns;
};

extern Settings cfg;

String sanitizeHostname(const String& in);

void settingsLoad();
void settingsSave();

void webBegin();
uint8_t webClients();   // connected dashboard browsers
void webLoop();

// Hint for the dashboard. Only a code plus an optional argument travels over
// the wire - the translation happens in the browser, so switching the UI
// language does not trip over text hard-coded in the firmware.
//   ""             no hint
//   "unsupported"  band not covered by the PA            (arg = band name)
//   "bandset"      band switched                         (arg = band name)
//   "abon"/"aboff" TCI band select on/off
//   "tciauto"      TCI gone, PA put back on its own band select
void webSetNote(const char* code, const char* arg = "");
