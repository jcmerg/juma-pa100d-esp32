#pragma once
#include <stdint.h>

// The blue LED on the DevKit: the state of the controller at a glance, without
// a browser. One pattern per 2 s cycle, 20 steps of 100 ms.
//
// Driven from the main loop on purpose - if the loop hangs, the pattern
// freezes, and that is the heartbeat: a LED that has stopped blinking means
// the firmware has stopped running.
enum class LedPattern : uint8_t {
    NoWifi,       // short flash every 2 s: no Wi-Fi
    AccessPoint,  // double flash: fallback AP is up
    Waiting,      // slow blink: Wi-Fi fine, PA offline or TCI not connected
    Ready,        // on, dark for 100 ms every 2 s: PA online, TCI connected
    Transmit,     // fast blink: the PA reports TX
    Alarm,        // triple flash: the PA reports an alarm
};

void statusLedBegin();
void statusLedSet(LedPattern p);
void statusLedLoop();
