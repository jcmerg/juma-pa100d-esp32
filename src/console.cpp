#include "console.h"
#include "config.h"
#include "web.h"
#include "juma.h"
#include "tci.h"
#include "web.h"
#include "bands.h"
#include <Arduino.h>
#include <WiFi.h>

static WiFiServer telnet(TELNET_PORT);
static WiFiClient tc;

// Ziel der Konsolenausgabe - je Kommando auf die Quelle gesetzt.
static Print* io = &Serial;

static char line[128];
static uint8_t len = 0;

static void help() {
    io->println(F(
        "\nKommandos:\n"
        "  show                aktuelle Konfiguration und Zustand\n"
        "  scan                WLAN-Scan\n"
        "  ssid <name>         WLAN-SSID setzen\n"
        "  pass <secret>       WLAN-Passwort setzen\n"
        "  tci <host> [port]   TCI-Host der SDR-Software (Port default 50002)\n"
        "  tcien <0|1>         TCI-Client aus/ein\n"
        "  autoband <0|1>      Bandwahl per TCI aus/ein\n"
        "  otastby <0|1>       vor dem Firmware-Update =S an die PA\n"
        "  tcilosta <0|1>      bei TCI-Verlust =A senden (PA waehlt wieder selbst)\n"
        "  sel <a|m>           Bandwahl der PA auf Automatik / Manuell\n"
        "  save                speichern und neu starten\n"
        "  reboot              nur neu starten\n"
        "  pa <kdo>            Rohkommando an die PA, z.B.  pa =R\n"
        "  raw                 letzte Statuszeile der PA\n"
        "  help"));
}

static void show() {
    const JumaStatus& s = juma.status();
    io->println();
    io->printf("WLAN      SSID '%s'  Passwort %s\n",
                  cfg.ssid.c_str(), cfg.pass.length() ? "gesetzt" : "-");
    if (WiFi.status() == WL_CONNECTED)
        io->printf("          verbunden, IP %s, RSSI %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else if (WiFi.getMode() & WIFI_AP)
        io->printf("          AP '%s', IP %s\n", AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    else
        io->println(F("          nicht verbunden"));

    io->printf("TCI       %s  %s:%u  %s   Nachrichten %lu, Abbrueche %lu\n",
                  cfg.tciEn ? "ein" : "aus",
                  cfg.tciHost.length() ? cfg.tciHost.c_str() : "-",
                  cfg.tciPort,
                  tci.connected() ? "verbunden" : "getrennt",
                  (unsigned long)tci.rxMsgs(), (unsigned long)tci.drops());
    if (tci.freqHz())
        io->printf("          QRG %.4f MHz (%s), %s\n", tci.freqHz() / 1e6,
                      bandNameFromHz(tci.freqHz()), tci.tx() ? "TX" : "RX");

    io->printf("Bandwahl  per TCI %s   STANDBY vor Update: %s   =A bei TCI-Verlust: %s\n",
                  cfg.autoband ? "ein" : "aus", cfg.otaStandby ? "ja" : "nein",
                  cfg.tciLostAuto ? "ja" : "nein");
    io->printf("UART2     RX=%d TX=%d @ %ld%s\n", JUMA_RX_PIN, JUMA_TX_PIN,
                  JUMA_BAUD, JUMA_INVERT ? " invertiert" : "");
    io->printf("PA        %s   Bytes %lu   Zeilen ok %lu / verworfen %lu   zuletzt gesendet '%s'\n",
                  juma.online() ? "online" : "OFFLINE",
                  (unsigned long)juma.rxBytes(),
                  (unsigned long)juma.rxLines(), (unsigned long)juma.badLines(),
                  juma.lastSent());
    if (juma.rxBytes()) {
        char hex[96];
        juma.hexTail(hex, sizeof(hex));
        io->printf("          letzte Rohbytes: %s\n", hex);
    } else {
        io->println(F("          nichts empfangen - PA aus? Remote-Mode? Tip/Ring vertauscht?"));
    }
    if (s.valid)
        io->printf("          %s  Band %s  Gain %u  %.1f W  SWR %.1f  %.2f V  %.1f A  %d%c  Alarme 0x%02X\n",
                      s.operate ? "OPERATE" : "STANDBY", bandName(s.band), s.gain,
                      s.watts, s.swr, s.volts, s.amps, s.temp,
                      s.celsius ? 'C' : 'F', s.alarms);
    io->printf("Browser   %u WebSocket-Clients verbunden\n", webClients());
    io->printf("Version   %s\n", FW_VERSION);
    io->printf("Heap      %u Bytes frei, Uptime %lu s\n\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned long)(millis() / 1000));
}

static void scan() {
    io->println(F("\nScan..."));
    int n = WiFi.scanNetworks();
    if (n <= 0) { io->println(F("nichts gefunden\n")); return; }
    for (int i = 0; i < n; i++) {
        io->printf("  %-32s %4d dBm  Ch %2d  %s\n",
                      WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "offen" : "verschluesselt");
    }
    io->println();
    WiFi.scanDelete();
}

static void dispatch(char* s) {
    while (*s == ' ') s++;
    if (!*s) return;

    char* arg = strchr(s, ' ');
    if (arg) { *arg = '\0'; arg++; while (*arg == ' ') arg++; }
    const char* cmd = s;

    if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) { help(); return; }
    if (!strcmp(cmd, "show")) { show(); return; }
    if (!strcmp(cmd, "scan")) { scan(); return; }
    if (!strcmp(cmd, "reboot")) { io->println(F("Neustart...")); delay(100); ESP.restart(); }

    if (!strcmp(cmd, "raw")) {
        io->printf("'%s'\n", juma.status().raw);
        return;
    }

    if (!strcmp(cmd, "pa")) {
        if (!arg) { io->println(F("Beispiel: pa =R")); return; }
        juma.send(arg);
        io->printf("gesendet: '%s'\n", arg);
        return;
    }

    if (!strcmp(cmd, "ssid")) {
        if (!arg) { io->println(F("Beispiel: ssid MeinNetz")); return; }
        cfg.ssid = arg;
        io->printf("SSID = '%s'   (mit 'save' speichern)\n", cfg.ssid.c_str());
        return;
    }

    if (!strcmp(cmd, "pass")) {
        if (!arg) { io->println(F("Beispiel: pass geheim123")); return; }
        cfg.pass = arg;
        io->printf("Passwort gesetzt (%u Zeichen)   (mit 'save' speichern)\n",
                      cfg.pass.length());
        return;
    }

    if (!strcmp(cmd, "tci")) {
        if (!arg) { io->println(F("Beispiel: tci 192.168.1.20 50002")); return; }
        char* p = strchr(arg, ' ');
        if (p) { *p = '\0'; cfg.tciPort = (uint16_t)atoi(p + 1); }
        cfg.tciHost = arg;
        if (!cfg.tciPort) cfg.tciPort = TCI_DEFAULT_PORT;
        cfg.tciEn = true;
        // sofort anwenden, damit sich zum Testen kein Neustart braucht
        tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
        io->printf("TCI = %s:%u, aktiv   (mit 'save' dauerhaft)\n",
                      cfg.tciHost.c_str(), cfg.tciPort);
        return;
    }

    if (!strcmp(cmd, "tcien")) {
        cfg.tciEn = (arg && atoi(arg) != 0);
        tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
        io->printf("TCI %s   (mit 'save' dauerhaft)\n", cfg.tciEn ? "ein" : "aus");
        return;
    }

    if (!strcmp(cmd, "autoband")) {
        cfg.autoband = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("Bandwahl per TCI %s (gespeichert)\n", cfg.autoband ? "ein" : "aus");
        return;
    }

    if (!strcmp(cmd, "sel")) {
        if (!arg) { io->println(F("sel a  oder  sel m")); return; }
        if (cfg.autoband) {
            io->println(F("abgelehnt: TCI-Bandwahl ist aktiv - erst 'autoband 0'"));
            return;
        }
        if (*arg == 'a' || *arg == 'A') {
            juma.setAutoSelect();
            io->println(F("=A gesendet (Automatik)"));
        } else {
            uint8_t b = juma.status().band;
            if (b >= 1 && b <= 9) {
                juma.setBand(b);
                io->printf("=B%u gesendet (manuell, Band unveraendert)\n", b);
            } else {
                io->printf("PA meldet Band %u - kein =Bn moeglich\n", b);
            }
        }
        return;
    }

    if (!strcmp(cmd, "tcilosta")) {
        cfg.tciLostAuto = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("=A bei TCI-Verlust: %s (gespeichert)\n",
                      cfg.tciLostAuto ? "ja" : "nein");
        return;
    }

    if (!strcmp(cmd, "otastby")) {
        cfg.otaStandby = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("STANDBY vor Firmware-Update: %s (gespeichert)\n",
                      cfg.otaStandby ? "ja" : "nein");
        return;
    }

    if (!strcmp(cmd, "save")) {
        settingsSave();
        io->println(F("gespeichert - Neustart..."));
        delay(300);
        ESP.restart();
    }

    io->printf("unbekannt: '%s' - 'help' zeigt die Kommandos\n", cmd);
}

void consoleBegin() {
    io = &Serial;
    help();
    io->print(F("> "));
    telnet.begin();
    telnet.setNoDelay(true);
}

// Ein Zeichen aus einer Quelle in den Zeilenpuffer. Serial und Telnet teilen
// sich den Puffer - gleichzeitig tippen sollte man also nicht auf beiden.
static void feed(char c) {
    if (c == '\n' || c == '\r') {
        if (len) {
            line[len] = '\0';
            io->println();
            dispatch(line);
            len = 0;
        }
        io->print(F("> "));
    } else if (c == 8 || c == 127) {            // Backspace
        if (len) { len--; io->print(F("\b \b")); }
    } else if (c == 0 || c == 0xFF) {           // Telnet-IAC und Fuellbytes
        return;
    } else if (len < sizeof(line) - 1) {
        line[len++] = c;
        io->print(c);                            // Echo
    }
}

void consoleLoop() {
    // Telnet: genau ein Client, ein neuer verdraengt den alten
    if (telnet.hasClient()) {
        if (tc && tc.connected()) tc.stop();
        tc = telnet.available();
        tc.setNoDelay(true);
        io = &tc;
        len = 0;
        io->printf("\nJUMA PA-100D Controller %s - 'help' zeigt die Kommandos\n> ", FW_VERSION);
    }

    while (Serial.available()) {
        io = &Serial;
        feed((char)Serial.read());
    }

    if (tc && tc.connected()) {
        while (tc.available()) {
            io = &tc;
            feed((char)tc.read());
        }
    }

    io = &Serial;
}
