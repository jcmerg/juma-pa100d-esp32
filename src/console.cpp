#include "console.h"
#include "config.h"
#include "web.h"

// defined in main.cpp
const char* resetReasonName();
uint32_t bootNumber();
uint32_t lastRunSecs();
uint32_t wifiDropCount();
uint32_t wifiRoamCount();
int32_t  wifiRssiAvg();
#include "juma.h"
#include "tci.h"
#include "web.h"

// in main.cpp
uint32_t wifiDropCount();
uint32_t wifiRoamCount();
int32_t  wifiRssiAvg();
#include "bands.h"
#include <Arduino.h>
#include <stdarg.h>
#include <esp_task_wdt.h>
#include <WiFi.h>

static WiFiServer telnet(TELNET_PORT);
static WiFiClient tc;

// The NVT of RFC 854 wants CR LF on the wire, and once the client runs in
// character mode its terminal stops turning a bare LF into a carriage return
// of its own - every line of output would then walk further to the right.
// The printf formats here all end in "\n", so the translation happens at the
// last possible point instead of in fifty format strings.
class TelnetPrint : public Print {
public:
    // WiFiClient::write() selects with a 1 s timeout and retries ten times,
    // so a single write to a client that stopped reading costs up to 10 s -
    // the socket timeout does not apply, it sends with MSG_DONTWAIT. A 'show'
    // is twenty writes, which would be well past the 20 s task watchdog. So
    // feed the watchdog per write, and give up on a client that has gone.
    size_t write(uint8_t c) override {
        if (c == '\n' && !lastCR) tc.write('\r');
        lastCR = (c == '\r');
        size_t n = tc.write(c);
        esp_task_wdt_reset();
        return n;
    }
    // Print::printf() lands here - send whole runs, not byte by byte.
    size_t write(const uint8_t* b, size_t n) override {
        size_t start = 0;
        for (size_t i = 0; i < n; i++) {
            bool crBefore = i ? (b[i - 1] == '\r') : lastCR;
            if (b[i] == '\n' && !crBefore) {
                if (i > start) tc.write(b + start, i - start);
                tc.write((const uint8_t*)"\r\n", 2);
                start = i + 1;
            }
        }
        if (start < n) tc.write(b + start, n - start);
        if (n) lastCR = (b[n - 1] == '\r');
        esp_task_wdt_reset();
        return n;
    }
    void reset() { lastCR = false; }
private:
    bool lastCR = false;
};
static TelnetPrint tout;

// Where console output goes - set to the source for each command.
static Print* io = &Serial;

static char line[128];
static uint8_t len = 0;

// Set by 'quit': the session is gone, do not write a new prompt into it.
static bool closed = false;

// 'debug 1'. Deliberately not persisted: tracing belongs to a session
// somebody is watching, not to the next twelve months of operation.
static bool dbgFlag = false;

static void help() {
    io->println(F(
        "\nCommands:\n"
        "  show                current configuration and state\n"
        "  scan                scan for Wi-Fi networks\n"
        "  hostname <name>     network name for Wi-Fi, mDNS and OTA\n"
        "  ssid <name>         set the Wi-Fi SSID\n"
        "  pass <secret>       set the Wi-Fi password\n"
        "  tci <host> [port]   TCI host of the SDR software (port default 50002)\n"
        "  tcien <0|1>         TCI client off/on\n"
        "  autoband <0|1>      band selection via TCI off/on\n"
        "  otastby <0|1>       send =S to the PA before a firmware update\n"
        "  tcilosta <0|1>      on TCI loss send =A (the PA selects again)\n"
        "  tempwarn <deg>      pre-warning from this temperature\n"
        "  temphigh <deg>      red in the gauge from here\n"
        "  tempalarm <0|1>     temperature pre-warning off/on\n"
        "  swrwarn <value>     SWR pre-warning from this value\n"
        "  swrhigh <value>     SWR red in the gauge from here\n"
        "  swralarm <0|1>      SWR pre-warning off/on\n"
        "  sel <a|m>           PA band select to automatic / manual\n"
        "  save                save and restart\n"
        "  reboot              restart only\n"
        "  pa <cmd>            raw command to the PA, e.g.  pa =R\n"
        "  raw                 last status line from the PA\n"
        "  quit                close the telnet session\n"
        "  debug <0|1>         trace timings (web, loop, Wi-Fi) - not stored\n"
        "  help"));
}

static void show() {
    const JumaStatus s = juma.status();
    io->println();
    io->printf("Name      %s  ->  http://%s.local/\n",
                  cfg.hostname.c_str(), cfg.hostname.c_str());
    io->printf("Wi-Fi     SSID '%s'  password %s\n",
                  cfg.ssid.c_str(), cfg.pass.length() ? "set" : "-");
    if (WiFi.status() == WL_CONNECTED)
        // Which AP, not just how strong: on one SSID with several APs the
        // BSSID is the only way to tell "weak link" from "stuck on the far
        // one" - compare it against 'scan'.
        io->printf("          connected, IP %s, RSSI %d dBm (avg %d), AP %s ch %d\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI(), (int)wifiRssiAvg(),
                      WiFi.BSSIDstr().c_str(), WiFi.channel());
    else if (WiFi.getMode() & WIFI_AP)
        io->printf("          AP '%s', IP %s\n", AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    else
        io->println(F("          not connected"));

    io->printf("TCI       %s  %s:%u  %s   messages %lu, drops %lu\n",
                  cfg.tciEn ? "on" : "off",
                  cfg.tciHost.length() ? cfg.tciHost.c_str() : "-",
                  cfg.tciPort,
                  tci.connected() ? "connected" : "disconnected",
                  (unsigned long)tci.rxMsgs(), (unsigned long)tci.drops());
    if (tci.freqHz())
        io->printf("          freq %.4f MHz (%s), %s\n", tci.freqHz() / 1e6,
                      bandNameFromHz(tci.freqHz()), tci.tx() ? "TX" : "RX");

    io->printf("Temp      warn from %u, red from %u deg, pre-warning %s\n",
                  cfg.tempWarn, cfg.tempHigh, cfg.tempAlarm ? "on" : "off");
    io->printf("SWR       warn from %.1f, red from %.1f, pre-warning %s\n",
                  cfg.swrWarnX10 / 10.0f, cfg.swrHighX10 / 10.0f,
                  cfg.swrAlarm ? "on" : "off");
    io->printf("Band      via TCI %s   STANDBY before update: %s   =A on TCI loss: %s\n",
                  cfg.autoband ? "on" : "off", cfg.otaStandby ? "yes" : "no",
                  cfg.tciLostAuto ? "yes" : "no");
    io->printf("UART2     RX=%d TX=%d @ %ld%s\n", JUMA_RX_PIN, JUMA_TX_PIN,
                  JUMA_BAUD, JUMA_INVERT ? " inverted" : "");
    io->printf("PA        %s   bytes %lu   lines ok %lu / dropped %lu   last sent '%s'\n",
                  juma.online() ? "online" : "OFFLINE",
                  (unsigned long)juma.rxBytes(),
                  (unsigned long)juma.rxLines(), (unsigned long)juma.badLines(),
                  juma.lastSent());
    char banner[48];
    juma.bannerCopy(banner, sizeof(banner));
    if (banner[0]) io->printf("          says: '%s'\n", banner);
    if (juma.rxBytes()) {
        char hex[96];
        juma.hexTail(hex, sizeof(hex));
        io->printf("          last raw bytes: %s\n", hex);
    } else {
        io->println(F("          nothing received - PA off? remote mode? tip/ring swapped?"));
    }
    if (s.valid)
        io->printf("          %s  Band %s  Gain %u  %.1f W  SWR %.1f  %.2f V  %.1f A  %d%c  Alarms 0x%02X\n",
                      s.operate ? "OPERATE" : "STANDBY", bandName(s.band), s.gain,
                      s.watts, s.swr, s.volts, s.amps, s.temp,
                      s.celsius ? 'C' : 'F', s.alarms);
    io->printf("Browser   %u WebSocket clients connected\n", webClients());
    io->printf("Version   %s\n", FW_VERSION);
    // Free heap alone is misleading: what matters is the largest contiguous
    // block. If that falls while "free" holds steady, it is fragmentation
    // rather than a leak.
    io->printf("Heap      %u free, largest block %u, minimum since boot %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                  (unsigned)ESP.getMinFreeHeap());
    io->printf("Uptime    %lu s   Wi-Fi: %lu drops, %lu AP switches\n",
                  (unsigned long)(millis() / 1000),
                  (unsigned long)wifiDropCount(), (unsigned long)wifiRoamCount());
    // A restart is invisible from the outside - the device is simply back.
    io->printf("Boot      #%lu, last reset: %s",
                  (unsigned long)bootNumber(), resetReasonName());
    if (lastRunSecs()) io->printf(", previous run %lu s", (unsigned long)lastRunSecs());
    io->println();
    io->println();
}

static void scan() {
    io->println(F("\nScanning..."));
    int n = WiFi.scanNetworks();
    if (n <= 0) { io->println(F("nothing found\n")); return; }
    for (int i = 0; i < n; i++) {
        io->printf("  %-24s %-17s %4d dBm  Ch %2d  %s\n",
                      WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(),
                      WiFi.RSSI(i), WiFi.channel(i),
                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "encrypted");
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
    if (!strcmp(cmd, "reboot")) { io->println(F("Restarting...")); delay(100); ESP.restart(); }

    if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "bye")) {
        if (io == &tout) {
            io->println(F("bye"));
            tc.flush();
            tc.stop();
            io = &Serial;
            closed = true;               // feed() then skips the prompt
        } else {
            io->println(F("'quit' only ends a telnet session"));
        }
        return;
    }

    if (!strcmp(cmd, "debug")) {
        dbgFlag = (arg && atoi(arg) != 0);
        io->printf("debug tracing %s (off again after a restart)\n",
                      dbgFlag ? "on" : "off");
        return;
    }

    if (!strcmp(cmd, "raw")) {
        io->printf("'%s'\n", juma.status().raw);
        return;
    }

    if (!strcmp(cmd, "pa")) {
        if (!arg) { io->println(F("example: pa =R")); return; }
        juma.send(arg);
        io->printf("sent: '%s'\n", arg);
        return;
    }

    if (!strcmp(cmd, "hostname")) {
        if (!arg) { io->println(F("example: hostname juma-pa")); return; }
        cfg.hostname = sanitizeHostname(arg);
        io->printf("hostname = '%s'   ('save' to store, effective after restart)\n",
                      cfg.hostname.c_str());
        return;
    }

    if (!strcmp(cmd, "ssid")) {
        if (!arg) { io->println(F("example: ssid MyNetwork")); return; }
        cfg.ssid = arg;
        io->printf("SSID = '%s'   ('save' to store)\n", cfg.ssid.c_str());
        return;
    }

    if (!strcmp(cmd, "pass")) {
        if (!arg) { io->println(F("example: pass secret123")); return; }
        cfg.pass = arg;
        io->printf("password set (%u characters)   ('save' to store)\n",
                      cfg.pass.length());
        return;
    }

    if (!strcmp(cmd, "tci")) {
        if (!arg) { io->println(F("example: tci 192.168.1.20 50002")); return; }
        char* p = strchr(arg, ' ');
        if (p) { *p = '\0'; cfg.tciPort = (uint16_t)atoi(p + 1); }
        cfg.tciHost = arg;
        if (!cfg.tciPort) cfg.tciPort = TCI_DEFAULT_PORT;
        cfg.tciEn = true;
        // apply at once so testing does not require a reboot
        tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
        io->printf("TCI = %s:%u, enabled   ('save' to persist)\n",
                      cfg.tciHost.c_str(), cfg.tciPort);
        return;
    }

    if (!strcmp(cmd, "tcien")) {
        cfg.tciEn = (arg && atoi(arg) != 0);
        tci.configure(cfg.tciHost, cfg.tciPort, cfg.tciEn);
        io->printf("TCI %s   ('save' to persist)\n", cfg.tciEn ? "on" : "off");
        return;
    }

    if (!strcmp(cmd, "autoband")) {
        cfg.autoband = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("band selection via TCI %s (saved)\n", cfg.autoband ? "on" : "off");
        return;
    }

    if (!strcmp(cmd, "sel")) {
        if (!arg) { io->println(F("sel a  or  sel m")); return; }
        if (cfg.autoband) {
            io->println(F("refused: TCI band selection is active - use 'autoband 0' first"));
            return;
        }
        if (*arg == 'a' || *arg == 'A') {
            juma.setAutoSelect();
            io->println(F("=A sent (automatic)"));
        } else {
            uint8_t b = juma.status().band;
            if (b >= 1 && b <= 9) {
                juma.setBand(b);
                io->printf("=B%u sent (manual, band unchanged)\n", b);
            } else {
                io->printf("PA reports band %u - no =Bn possible\n", b);
            }
        }
        return;
    }

    if (!strcmp(cmd, "tempwarn") || !strcmp(cmd, "temphigh")) {
        if (!arg) { io->println(F("example: tempwarn 50")); return; }
        int v = atoi(arg);
        if (v < 20 || v > 120) { io->println(F("only 20..120 degrees")); return; }
        if (cmd[4] == 'w') cfg.tempWarn = (uint8_t)v; else cfg.tempHigh = (uint8_t)v;
        if (cfg.tempHigh < cfg.tempWarn) cfg.tempHigh = cfg.tempWarn;
        settingsSave();
        io->printf("temperature: warn from %u, red from %u degrees (saved)\n",
                      cfg.tempWarn, cfg.tempHigh);
        return;
    }

    if (!strcmp(cmd, "swrwarn") || !strcmp(cmd, "swrhigh")) {
        if (!arg) { io->println(F("example: swrwarn 2.0")); return; }
        float f = atof(arg);
        if (f < 1.0f || f > 10.0f) { io->println(F("only 1.0..10.0")); return; }
        uint8_t x = (uint8_t)(f * 10.0f + 0.5f);
        if (cmd[3] == 'w') cfg.swrWarnX10 = x; else cfg.swrHighX10 = x;
        if (cfg.swrHighX10 < cfg.swrWarnX10) cfg.swrHighX10 = cfg.swrWarnX10;
        settingsSave();
        io->printf("SWR: warn from %.1f, red from %.1f (saved)\n",
                      cfg.swrWarnX10 / 10.0f, cfg.swrHighX10 / 10.0f);
        return;
    }

    if (!strcmp(cmd, "swralarm")) {
        cfg.swrAlarm = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("SWR pre-warning: %s (saved)\n", cfg.swrAlarm ? "on" : "off");
        return;
    }

    if (!strcmp(cmd, "tempalarm")) {
        cfg.tempAlarm = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("temperature pre-warning: %s (saved)\n", cfg.tempAlarm ? "on" : "off");
        return;
    }

    if (!strcmp(cmd, "tcilosta")) {
        cfg.tciLostAuto = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("=A on TCI loss: %s (saved)\n",
                      cfg.tciLostAuto ? "yes" : "no");
        return;
    }

    if (!strcmp(cmd, "otastby")) {
        cfg.otaStandby = (arg && atoi(arg) != 0);
        settingsSave();
        io->printf("STANDBY before firmware update: %s (saved)\n",
                      cfg.otaStandby ? "yes" : "no");
        return;
    }

    if (!strcmp(cmd, "save")) {
        settingsSave();
        io->println(F("saved - restarting..."));
        delay(300);
        ESP.restart();
    }

    io->printf("unknown: '%s' - 'help' lists the commands\n", cmd);
}

bool dbgOn() { return dbgFlag; }

// Trace lines are queued, not written by whoever produced them. The PA task
// calls dbg() while holding its mutex, and a write to a stalled telnet client
// takes up to 10 s - that would stop the poll and cost the operating mode,
// which is exactly the failure the tracing is there to investigate. Dropping
// a line when the queue is full is the right trade: diagnostics must not
// change what they measure.
static const uint8_t  DBG_QN = 16;
static const uint8_t  DBG_QL = 144;
static QueueHandle_t  dbgQ = nullptr;

// Called from the loop task only.
static void dbgDrain() {
    if (!dbgQ) return;
    char buf[DBG_QL];
    while (xQueueReceive(dbgQ, buf, 0) == pdTRUE) {
        Print* out = (tc && tc.connected()) ? (Print*)&tout : (Print*)&Serial;
        for (uint8_t i = 0; i < len; i++) out->print(F("\b \b"));
        out->println(buf);
        out->print(F("> "));
        if (len) { line[len] = '\0'; out->print(line); }
    }
}

// Trace output arrives asynchronously, in the middle of whatever is half
// typed at the prompt. So take the input line off the screen, print the
// trace, and put the prompt and the typed text back.
void dbg(const char* fmt, ...) {
    if (!dbgFlag || !dbgQ) return;

    char buf[DBG_QL];
    int n = snprintf(buf, sizeof(buf), "[%8.3f] ", millis() / 1000.0f);
    if (n < 0 || n >= (int)sizeof(buf)) return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);

    xQueueSend(dbgQ, buf, 0);            // full queue: drop it, never wait
}

void consoleBegin() {
    dbgQ = xQueueCreate(DBG_QN, DBG_QL);
    io = &Serial;
    help();
    io->print(F("> "));
    telnet.begin();
    telnet.setNoDelay(true);
}

// One character from a source into the line buffer. Serial and telnet share
// the buffer, so do not type on both at the same time.
// Take the typed text off the screen again, buffer and display in step.
static void eraseLine() {
    while (len) { len--; io->print(F("\b \b")); }
}

static void feed(uint8_t c) {
    // Telnet sends CR LF or CR NUL for Enter - the second byte must not
    // trigger a second prompt.
    static bool afterCR = false;
    static uint8_t esc = 0;                      // 0 none, 1 after ESC, 2 in CSI
    static char history[sizeof(line)] = "";
    bool wasCR = afterCR;
    afterCR = (c == '\r');
    if (wasCR && (c == '\n' || c == 0)) return;

    // Arrow keys arrive as ESC [ A and the like. Without this the final letter
    // would land in the line as "[A".
    if (esc) {
        if (esc == 1) { esc = (c == '[' || c == 'O') ? 2 : 0; return; }
        if (c >= 0x30 && c <= 0x3F) return;      // parameter bytes
        esc = 0;
        if (c == 'A') {                          // up: the last command again
            eraseLine();
            len = (uint8_t)strlen(history);
            memcpy(line, history, len);
            line[len] = '\0';
            io->print(line);
        } else if (c == 'B') {                   // down: empty line
            eraseLine();
        }
        return;
    }
    if (c == 27) { esc = 1; return; }

    if (c == '\n' || c == '\r') {
        io->println();                           // also for an empty line
        if (len) {
            line[len] = '\0';
            strcpy(history, line);
            dispatch(line);                      // dispatch cuts up the buffer
            len = 0;
        }
        if (closed) { closed = false; return; }
        io->print(F("> "));
    } else if (c == 8 || c == 127) {             // Backspace
        if (len) { len--; io->print(F("\b \b")); }
    } else if (c == 3) {                         // Ctrl-C: discard the line
        len = 0;
        io->println(F("^C"));
        io->print(F("> "));
    } else if (c == 21) {                        // Ctrl-U: erase the line
        eraseLine();
    } else if (c == 4) {                         // Ctrl-D on an empty line: quit
        if (len) return;
        char q[] = "quit";
        io->println();
        dispatch(q);
        if (closed) { closed = false; return; }
        io->print(F("> "));
    } else if (c < 32) {                         // remaining control bytes
        return;                                  // (8-bit stays: SSID, password)
    } else if (len < sizeof(line) - 1) {
        line[len++] = (char)c;
        io->print((char)c);                      // Echo
    }
}

// Telnet control sequences, RFC 854. IAC introduces a command: option
// negotiation (WILL/WONT/DO/DONT) is three bytes, a subnegotiation runs until
// IAC SE. Without swallowing them here their bytes land in the line buffer and
// appear as garbage right after the login - which is exactly what a telnet
// client sends first thing.
enum : uint8_t {
    T_SE = 240, T_SB = 250, T_WILL = 251, T_WONT = 252,
    T_DO = 253, T_DONT = 254, T_IAC = 255
};
enum TnState : uint8_t { TN_DATA, TN_IAC, TN_OPT, TN_SUB, TN_SUB_IAC };
static TnState tnState = TN_DATA;

static void feedTelnet(uint8_t c) {
    switch (tnState) {
    case TN_DATA:
        if (c == T_IAC) { tnState = TN_IAC; return; }
        feed(c);
        return;
    case TN_IAC:
        if (c == T_IAC) { tnState = TN_DATA; feed(c); return; }   // escaped 0xFF
        if (c >= T_WILL && c <= T_DONT) { tnState = TN_OPT; return; }
        if (c == T_SB) { tnState = TN_SUB; return; }
        tnState = TN_DATA;                                        // two-byte command
        return;
    case TN_OPT:                                                  // option byte
        tnState = TN_DATA;
        return;
    case TN_SUB:
        if (c == T_IAC) tnState = TN_SUB_IAC;
        return;
    case TN_SUB_IAC:
        tnState = (c == T_SE) ? TN_DATA : TN_SUB;
        return;
    }
}

void consoleLoop() {
    dbgDrain();

    // Telnet: exactly one client, a new one displaces the old
    if (telnet.hasClient()) {
        if (tc && tc.connected()) tc.stop();
        tc = telnet.available();
        tc.setNoDelay(true);
        io = &tout;
        tout.reset();
        len = 0;
        tnState = TN_DATA;
        closed = false;
        // We echo ourselves, so ask the client for character mode and to keep
        // its own echo off: IAC WILL ECHO, IAC WILL SUPPRESS-GO-AHEAD.
        static const uint8_t hello[] = { T_IAC, T_WILL, 1, T_IAC, T_WILL, 3 };
        tc.write(hello, sizeof(hello));
        io->printf("\nJUMA PA controller %s - 'help' lists the commands\n> ", FW_VERSION);
    }

    uint16_t budget = RX_MAX_PER_LOOP;
    while (Serial.available() && budget--) {
        io = &Serial;
        feed((uint8_t)Serial.read());
    }

    if (tc && tc.connected()) {
        budget = RX_MAX_PER_LOOP;
        while (tc.available() && budget--) {
            io = &tout;
            feedTelnet((uint8_t)tc.read());
        }
    }

    io = &Serial;
}
