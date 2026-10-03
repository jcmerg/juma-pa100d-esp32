// JUMA PA controller on an ESP32.
//
//   - RS-232 to the PA via MAX3232 on UART2 (115200 8N1, commands "=X\n\r")
//   - web dashboard over Wi-Fi (HTTP :80, WebSocket :81)
//   - band selection via a TCI client (ExpertSDR, deskHPSDR, Thetis)
//
// OPERATE/STANDBY is taken strictly from the device status, and we send an
// explicit =O or =S instead of a toggle - so the state never has to be
// guessed.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>          // esp_wifi_get_ps() for the debug trace
#include <esp_netif.h>         // to check that the AP really serves DHCP
#include <esp_system.h>        // esp_reset_reason()
#include <esp_mac.h>           // esp_read_mac() for the AP name
#include <DNSServer.h>
#include "config.h"
#include "juma.h"
#include "tci.h"
#include "bands.h"
#include "web.h"
#include "console.h"
#include "status_led.h"

static uint32_t lastBandCmd  = 0;

// ---------------------------------------------------------------------------
// Wi-Fi supervision
// ---------------------------------------------------------------------------
static uint32_t wifiLastOk   = 0;
static uint32_t wifiRetryAt  = 0;
static uint32_t wifiCheckAt  = 0;
static uint32_t wifiDrops_   = 0;
static uint32_t wifiRoams_   = 0;
static uint32_t wifiWeakAt   = 0;     // since when has the signal been poor?
static uint32_t wifiRoamAt   = 0;
static bool     wifiWasUp    = false;
static float    rssiAvg      = 0;     // moving average

// Fallback AP. The station side keeps running next to it (AP+STA), so the
// device finds its way back into the network by itself - a router that boots
// slower than the ESP32 after a power cut is the everyday case.
static bool      apOn       = false;
static char      apName[24];
static DNSServer apDns;               // every name -> AP_IP: the captive portal
static uint32_t  apStaUpAt  = 0;      // station side back while the AP is open
static uint32_t  apClientAt = 0;      // a client was last seen on the AP

// Why the station side failed, from the Wi-Fi event task. Without it a wrong
// password and an out-of-range router look the same: "not connected".
static volatile uint8_t  staReason = 0;
static volatile uint32_t staFails  = 0;
static uint32_t          staFailsSeen = 0;

const char* apSsid()   { return apName; }
bool        apActive() { return apOn; }
uint8_t     wifiLastReason() { return staReason; }

const char* wifiReasonText(uint8_t r) {
    switch (r) {
    case 0:                                   return "-";
    case WIFI_REASON_NO_AP_FOUND:             return "SSID not found - wrong name, out of range or 5 GHz only";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:      return "authentication failed - wrong password?";
    case WIFI_REASON_AKMP_INVALID:
    case WIFI_REASON_GROUP_CIPHER_INVALID:
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
    case WIFI_REASON_BAD_CIPHER_OR_AKM:       return "encryption not supported (WPA3 only?)";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:           return "the access point refused the association";
    case WIFI_REASON_BEACON_TIMEOUT:          return "signal lost";
    default:                                  return "disconnected";
    }
}

static void onStaDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
    const uint8_t r = info.wifi_sta_disconnected.reason;
    if (r == WIFI_REASON_ASSOC_LEAVE) return;     // our own disconnect()
    staReason = r;
    staFails++;
}

uint32_t wifiDropCount() { return wifiDrops_; }
uint32_t wifiRoamCount() { return wifiRoams_; }
int32_t  wifiRssiAvg()   { return (int32_t)rssiAvg; }
uint32_t wifiDownSecs()  {
    return (WiFi.status() == WL_CONNECTED) ? 0 : (millis() - wifiLastOk) / 1000;
}

// A fixed address has to be set before begin(), and applies to this
// connection attempt and every reconnect after it - but not across the
// WIFI_OFF that raising the AP goes through, hence its own function.
static void staStaticIp() {
    if (!cfg.staticIp || !cfg.ip.length()) return;
    IPAddress ip, gw, mask, dns;
    bool ok = ip.fromString(cfg.ip) && mask.fromString(cfg.mask);
    if (ok && cfg.gw.length()) ok = gw.fromString(cfg.gw);
    if (ok && cfg.dns.length()) dns.fromString(cfg.dns);
    else dns = gw;                     // no DNS given: ask the gateway
    if (ok && WiFi.config(ip, gw, mask, dns))
        log_i("static address %s, gateway %s", cfg.ip.c_str(), cfg.gw.c_str());
    else
        log_e("static address %s rejected - falling back to DHCP", cfg.ip.c_str());
}

// Settings that only hold once associated - set earlier, WiFi.begin()
// discards them again.
static void staUp() {
    // With modem sleep every round trip waits for the next beacon
    // (~100 ms); that made the 23 kB page take 6-20 s.
    WiFi.setSleep(false);
    // Full transmit power. The default depends on the calibration in
    // efuse and on the country setting, and at -74 dBm every dB counts.
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
}

static void apStop() {
    apDns.stop();
    WiFi.softAPdisconnect(true);       // back to station only
    apOn = false;
    log_i("Wi-Fi back - fallback AP '%s' closed", apName);
    if (dbgOn()) dbg("wifi: fallback AP closed");
}

static void wifiSupervise() {
    if (millis() - wifiCheckAt < WIFI_CHECK_MS) return;
    wifiCheckAt = millis();

    // Without a configured SSID, AP mode is the intended state.
    if (!cfg.ssid.length()) return;

    const uint8_t apClients = apOn ? WiFi.softAPgetStationNum() : 0;
    if (apClients) apClientAt = millis();

    // Every failed attempt once, not every 5 s the same line.
    if (staFails != staFailsSeen) {
        staFailsSeen = staFails;
        log_w("Wi-Fi '%s': reason %u, %s", cfg.ssid.c_str(), staReason,
              wifiReasonText(staReason));
        if (dbgOn()) dbg("wifi: '%s' failed, reason %u - %s", cfg.ssid.c_str(),
                         staReason, wifiReasonText(staReason));
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasUp) {
            log_i("Wi-Fi back: %s", WiFi.localIP().toString().c_str());
            staUp();
            apStaUpAt = millis();
        }
        wifiWasUp  = true;
        wifiLastOk = millis();

        if (apOn && !apClients && millis() - apStaUpAt >= AP_LINGER_MS) apStop();

        // Moving average instead of the instantaneous value: RSSI swings by
        // 10 dB and more. With the instantaneous value every single good
        // sample resets the timer, so the condition "poor for 60 s straight"
        // is never met and the AP switch never fires.
        const int32_t r = WiFi.RSSI();
        rssiAvg = rssiAvg ? (rssiAvg * 0.8f + (float)r * 0.2f) : (float)r;

        if (rssiAvg > (float)WIFI_ROAM_RSSI) { wifiWeakAt = 0; return; }
        if (!wifiWeakAt) { wifiWeakAt = millis(); return; }
        if (millis() - wifiWeakAt < WIFI_ROAM_HOLD_MS) return;
        // wifiRoamAt == 0 means "never roamed", not "roamed at boot" - without
        // that distinction the first five minutes after a restart are exactly
        // when the device may not leave a bad AP, which is when it is most
        // likely to be stuck on one.
        if (wifiRoamAt && millis() - wifiRoamAt < WIFI_ROAM_MIN_GAP) return;

        // Reconnect. The scan configured above picks the strongest AP - that
        // may well be the same one, in which case it was merely an attempt.
        log_w("RSSI averaging %d dBm for %lu s - looking for a stronger AP",
              (int)rssiAvg, (unsigned long)((millis() - wifiWeakAt) / 1000));
        wifiRoams_++;
        if (dbgOn()) dbg("wifi: RSSI %d dBm for %lu s - reconnecting to the "
                         "strongest AP", (int)rssiAvg,
                         (unsigned long)((millis() - wifiWeakAt) / 1000));
        wifiRoamAt = millis();
        wifiWeakAt = 0;
        rssiAvg    = 0;
        WiFi.disconnect();
        WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
        return;
    }

    if (wifiWasUp) {
        wifiDrops_++;
        wifiWasUp = false;
        log_w("Wi-Fi lost");
        if (dbgOn()) dbg("wifi: association lost after %lu s, RSSI last %d dBm",
                         (unsigned long)(millis() / 1000), (int)rssiAvg);
    }

    // With the AP up, an attempt would knock its client off the air - and
    // that client is most likely somebody entering the right password.
    const uint32_t retryGap = apOn ? AP_STA_RETRY_MS : WIFI_RETRY_MS;
    if ((WiFi.getMode() & WIFI_STA) && !apClients &&
        millis() - wifiRetryAt > retryGap) {
        wifiRetryAt = millis();
        // begin() rather than reconnect(): reconnect() takes the AP last
        // used, begin() scans and picks the strongest one.
        WiFi.disconnect();
        if (apOn) staStaticIp();
        WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    }

    // If it does not come back for a long time, only a reboot helps - an
    // unreachable device is no use to anyone, and a reboot starts over cleanly.
    // Not while somebody is on the AP, though: that would throw them out in
    // the middle of fixing the very configuration that keeps it from working.
    if (millis() - wifiLastOk > WIFI_REBOOT_AFTER_MS &&
        millis() - apClientAt > WIFI_REBOOT_AFTER_MS) {
        log_e("Wi-Fi gone for %lu s - rebooting", (unsigned long)wifiDownSecs());
        Serial.flush();
        delay(100);
        ESP.restart();
    }
}
static uint32_t tciLostAt    = 0;
static uint8_t  autoSelTries = 0;       // attempts to put the PA back on A
static uint32_t lastAutoSel  = 0;
static bool     paWasOnline  = false;
static uint32_t lastForceM   = 0;       // last attempt to force the PA to M
static uint8_t  forceMTries  = 0;

// ---------------------------------------------------------------------------
// Automatic band selection: TCI frequency -> "=Bn"
// ---------------------------------------------------------------------------
static void bandControl() {
    // The counterpart to the PA poll gap: if this task ever stops running,
    // band changes stop with it, and that must not go unnoticed.
    static uint32_t lastRun = 0;
    if (dbgOn() && lastRun && millis() - lastRun > 1000)
        dbg("band: %lu ms since the last pass", (unsigned long)(millis() - lastRun));
    lastRun = millis();

    const JumaStatus s = juma.status();

    // The hint in the dashboard always shows the CURRENT reason, never a
    // one-off event - otherwise a stale message lingers after a change, and
    // right after start-up there is none at all.
    if (!cfg.autoband)     { webSetNote("aboff");     return; }
    if (!tci.enabled())    { webSetNote("tcioff");    return; }
    // When the PA comes back up its band select may well be on M again. The
    // fallback must then be allowed to act once more - otherwise switching the
    // PA off and on would disable it for good.
    if (juma.online() != paWasOnline) {
        paWasOnline = juma.online();
        if (paWasOnline) autoSelTries = 0;
    }

    if (!tci.connected()) {
        // Per the manual '=Bn' is a MANUAL band selection - it moves the PA
        // from A to M, so after losing TCI it stays on the last commanded
        // band. '=A' gives it automatic band selection back; which method it
        // then uses (F-Sense, FT-817, CAT, ...) is set in its own config.
        if (!tciLostAt) tciLostAt = millis();
        const bool grace = (millis() - tciLostAt >= TCI_LOST_GRACE_MS);

        // Driven by state, not by an event: as long as the PA sits on M
        // while TCI is missing we keep trying - bounded, so a deliberate
        // choice made at the unit is not overridden forever.
        if (cfg.tciLostAuto && grace && juma.online() && !s.tx && !s.autoSel &&
            autoSelTries < 3 && millis() - lastAutoSel >= 5000) {
            juma.setAutoSelect();
            lastAutoSel = millis();
            autoSelTries++;
        }

        if (s.autoSel)                       webSetNote("tciauto");
        else if (cfg.tciLostAuto && grace && autoSelTries >= 3)
                                             webSetNote("selstuck");
        else                                 webSetNote("tcidis");
        return;
    }
    tciLostAt    = 0;
    autoSelTries = 0;

    uint32_t hz = tci.freqHz();
    if (!hz)               { webSetNote("tcinofreq"); return; }

    uint8_t target = bandFromHz(hz);
    if (target == BAND_NONE) { webSetNote("unsupported", bandNameFromHz(hz)); return; }

    // Settle time: only send once the frequency has stopped moving.
    // Otherwise tuning across a band edge fires a band command every time.
    if (millis() - tci.freqSetAt() < BAND_SETTLE_MS) return;

    // Never switch during TX - neither per TCI nor per the PA. Without a
    // hint: that you do not change bands while transmitting is self-evident,
    // and the message was only in the way.
    if (tci.tx() || s.tx)  { return; }
    if (!juma.online())    { webSetNote("paoff");     return; }

    if (s.band == target) {
        // While the ESP32 determines the band, the PA must not select one
        // as well - otherwise F-Sense will eventually pull it elsewhere. A
        // '=Bn' on the band already in use moves it from A to M without
        // changing the band. Bounded retries: if the PA will not take it, we
        // report instead of firing forever.
        if (s.autoSel) {
            if (forceMTries < 3 && millis() - lastForceM >= 5000) {
                juma.setBand(target);
                lastForceM = millis();
                forceMTries++;
            }
            webSetNote(forceMTries >= 3 ? "selstuck" : "bandok", bandName(target));
        } else {
            forceMTries = 0;
            webSetNote("bandok", bandName(target));
        }
        return;
    }
    if (millis() - lastBandCmd < 1000) return;      // do not hammer it

    juma.setBand(target);
    lastBandCmd = millis();
    webSetNote("bandset", bandName(target));
}

// ---------------------------------------------------------------------------
// Is the DHCP server of the fallback AP actually running? A client that
// associates and then gets no address is the symptom; this is the answer.
bool apDhcpRunning() {
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) return false;
    esp_netif_dhcp_status_t st = ESP_NETIF_DHCP_INIT;
    if (esp_netif_dhcps_get_status(ap, &st) != ESP_OK) return false;
    return st == ESP_NETIF_DHCP_STARTED;
}

static void apFallback() {
    // Raising the AP straight out of station mode leaves its DHCP server
    // behind: association works, the address never arrives. Take the station
    // side down first, then configure the AP network *before* raising it -
    // softAPConfig() is what stops the DHCP server, writes the address range
    // and starts it again.
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    // With an SSID configured the station side stays up next to the AP and
    // wifiSupervise() keeps trying it.
    WiFi.mode(cfg.ssid.length() ? WIFI_AP_STA : WIFI_AP);
    if (!WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK))
        log_e("softAPConfig failed - the AP will not hand out addresses");
    if (!WiFi.softAP(apName, cfg.apPass.c_str()))
        log_e("softAP failed");
    delay(100);
    // Every name resolves to us, so a phone's connectivity check lands on the
    // dashboard and the OS opens it by itself. The redirect for paths we do
    // not serve is in web.cpp.
    apDns.start(53, "*", AP_IP);
    apOn       = true;
    wifiRetryAt = millis();          // the boot attempt just failed
    log_w("No Wi-Fi - AP '%s' on %s, DHCP %s", apName,
          WiFi.softAPIP().toString().c_str(),
          apDhcpRunning() ? "running" : "NOT RUNNING");
}

static void wifiBegin() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(apName, sizeof(apName), "%s-%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
    WiFi.onEvent(onStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(cfg.hostname.c_str());
    staStaticIp();

    if (cfg.ssid.length()) {
        // The default is WIFI_FAST_SCAN, which makes the ESP32 take the
        // FIRST access point it finds for the SSID, not the strongest. With
        // several APs on one SSID it easily ends up on the weakest - with
        // packet loss that then looks like a firmware fault.
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        // This is where commissioning over USB stalls - say why.
        if (cfg.ssid.length())
            Serial.printf("Wi-Fi '%s' failed: reason %u, %s\n", cfg.ssid.c_str(),
                          staReason, wifiReasonText(staReason));
        staFailsSeen = staFails;
        // Without Wi-Fi, raise an AP of our own so the web UI stays
        // reachable for configuration.
        apFallback();
    } else {
        staUp();
        wifiWasUp = true;
        log_i("Wi-Fi connected: %s, sleep off, tx power %.1f dBm",
              WiFi.localIP().toString().c_str(), WiFi.getTxPower() * 0.25f);
    }
}

// Reboots are the kind of thing nobody sees happen: the device is simply back,
// with its uptime at zero. These few words live in RTC RAM, which survives a
// watchdog reset, a panic and a software restart - only a power cut or a
// brownout clears them, and esp_reset_reason() names that case anyway.
// RAM, not NVS: the running uptime is written on every loop, which would wear
// out flash within weeks.
#define BOOT_MAGIC 0x4A554D41            // 'JUMA'
RTC_NOINIT_ATTR static uint32_t bootMagic;
RTC_NOINIT_ATTR static uint32_t bootCount;
RTC_NOINIT_ATTR static uint32_t lastRunS;     // uptime reached before the reset
RTC_NOINIT_ATTR static uint32_t runS;         // uptime of the current run
static esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;

const char* resetReasonName() {
    switch (resetReason) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_EXT:      return "reset pin";
    case ESP_RST_SW:       return "software restart";
    case ESP_RST_PANIC:    return "crash (panic)";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog - loop blocked";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_BROWNOUT: return "brownout - supply dipped";
    case ESP_RST_DEEPSLEEP:return "deep sleep";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "unknown";
    }
}
uint32_t bootNumber()   { return bootCount; }
uint32_t lastRunSecs()  { return lastRunS; }

static void bootCensus() {
    resetReason = esp_reset_reason();
    // A brownout or a power cut leaves RTC RAM as garbage - the magic says
    // whether the counters below mean anything.
    const bool carriedOver = (bootMagic == BOOT_MAGIC);
    if (!carriedOver) {
        bootMagic = BOOT_MAGIC;
        bootCount = 0;
        runS      = 0;          // uninitialised RTC RAM - not a previous run
    }
    bootCount++;
    lastRunS = carriedOver ? runS : 0;
    runS     = 0;
    Serial.printf("Boot %lu, reset: %s", (unsigned long)bootCount, resetReasonName());
    if (lastRunS) Serial.printf(", previous run %lu s", (unsigned long)lastRunS);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("JUMA PA Controller " FW_VERSION);

    bootCensus();
    statusLedBegin();
    settingsLoad();
    juma.begin();
    wifiBegin();
    // OTA: flashing over Wi-Fi so no USB cable has to hang off the amplifier.
    // Put the PA into STANDBY before the update - loop() does not run while
    // flashing, and although the PA would fall back by itself after 5 s, this
    // way the state is defined rather than merely expired.
    ArduinoOTA.setHostname(cfg.hostname.c_str());
    // ArduinoOTA takes its password once, before begin() - so a password set
    // with 'otapass' reaches espota only after a restart.
    ArduinoOTA.setPassword(cfg.otaPass.c_str());
    ArduinoOTA.onStart([]() {
        if (cfg.otaStandby) {
            juma.sendNow("=S");
            Serial.println("OTA update starting, PA to STANDBY");
        }
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
    // Locked: no listener at all rather than one guarded by the placeholder.
    if (otaLocked()) Serial.println("Updates over the network locked - console: otapass <new>");
    else             ArduinoOTA.begin();

    if (MDNS.begin(cfg.hostname.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        MDNS.addService("telnet", "tcp", TELNET_PORT);
    }

    tci.begin();
    tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
    webBegin();

    // Arm the watchdog only here - the Wi-Fi connection attempt above is
    // allowed to take up to 15 s.
    esp_task_wdt_init(WDT_TIMEOUT_S, true);   // true = reboot on expiry
    esp_task_wdt_add(NULL);                   // watch loopTask
    Serial.printf("Watchdog active (%lu s)\n", (unsigned long)WDT_TIMEOUT_S);

    wifiLastOk = millis();
    consoleBegin();

    // The band decision belongs with the PA and TCI tasks, not behind the web
    // server: a blocked loop would delay a band change by as long as it is
    // blocked, and a band change that arrives late means the PA is on the
    // wrong band when the next transmission starts. It only reads state and
    // drops commands into juma's queue, both of which are already locked.
    xTaskCreatePinnedToCore([](void*) {
        for (;;) {
            bandControl();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }, "band", 3072, nullptr, 2, nullptr, 1);
}

// Which pattern the status LED shows. Most urgent first: an alarm outranks
// TX, and both only mean anything while the PA answers.
static LedPattern ledPattern() {
    if (juma.online()) {
        const JumaStatus s = juma.status();
        if (s.alarms) return LedPattern::Alarm;
        if (s.tx)     return LedPattern::Transmit;
    }
    if (apOn)                           return LedPattern::AccessPoint;
    if (WiFi.status() != WL_CONNECTED)  return LedPattern::NoWifi;
    // TCI switched off is a choice, not a fault - then the PA alone decides.
    if (!juma.online() || (tci.enabled() && !tci.connected()))
        return LedPattern::Waiting;
    return LedPattern::Ready;
}

// Loop timing, evaluated only while 'debug 1' is on. Everything - web server,
// console, PA link - runs from this one loop, so a single blocking call shows
// up as the worst case here. That is the number worth watching.
static void debugTick(uint32_t startedUs) {
    static uint32_t worstUs = 0, sumUs = 0, loops = 0, lastAt = 0;
    if (!dbgOn()) { worstUs = sumUs = loops = 0; lastAt = millis(); return; }

    uint32_t us = micros() - startedUs;
    if (us > worstUs) worstUs = us;
    sumUs += us;
    loops++;
    if (millis() - lastAt < 5000) return;
    lastAt = millis();

    wifi_ps_type_t ps = WIFI_PS_NONE;
    esp_wifi_get_ps(&ps);
    dbg("loop: %lu runs, avg %lu us, worst %lu us | RSSI %d dBm, ps %d | heap %u",
        (unsigned long)loops, (unsigned long)(sumUs / (loops ? loops : 1)),
        (unsigned long)worstUs, (int)WiFi.RSSI(), (int)ps,
        (unsigned)ESP.getFreeHeap());
    worstUs = sumUs = loops = 0;
}

// Everything runs from this one loop, and the PA leaves remote mode 5 s after
// the last command - so a single blocking call costs the operating mode. Which
// call it was is the question the aggregate above cannot answer, so with
// tracing on each step is timed separately. Two millis() per step, nothing
// when it is off.
static const uint32_t PHASE_WARN_MS = 250;
static uint32_t phaseAt = 0;
// Cleared when tracing is off, so switching it on mid-loop cannot make the
// step it was switched on in look like it took the time since the last run.
static inline void phaseStart() { phaseAt = dbgOn() ? millis() : 0; }
static inline void phaseEnd(const char* what) {
    // phaseAt == 0 means tracing was switched on inside this very step - the
    // step has no start time, and millis() alone would report the uptime as
    // its duration.
    if (!dbgOn() || !phaseAt) return;
    uint32_t ms = millis() - phaseAt;
    phaseAt = 0;
    if (ms > PHASE_WARN_MS) dbg("slow: %s blocked the loop for %lu ms", what,
                                (unsigned long)ms);
}
#define PHASE(name, call) do { phaseStart(); call; phaseEnd(name); } while (0)

void loop() {
    uint32_t startedUs = micros();
    esp_task_wdt_reset();
    PHASE("ArduinoOTA.handle", ArduinoOTA.handle());
    if (apOn) apDns.processNextRequest();
    // juma and tci run in tasks of their own - that is the point: whatever
    // blocks there, the PA keeps getting its poll and stays in remote mode.
    PHASE("webLoop",           webLoop());
    PHASE("consoleLoop",       consoleLoop());
    PHASE("wifiSupervise",     wifiSupervise());
    // The LED steps in 100 ms; no need to take the PA's lock more often.
    static uint32_t ledAt = 0;
    if (millis() - ledAt >= 100) { ledAt = millis(); statusLedSet(ledPattern()); }
    statusLedLoop();
    runS = millis() / 1000;          // RTC RAM: readable again after a reset
    debugTick(startedUs);
}
