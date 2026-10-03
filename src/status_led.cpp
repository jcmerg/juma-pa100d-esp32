#include <Arduino.h>
#include "config.h"
#include "status_led.h"

static const uint32_t STEP_MS = 100;
static const uint32_t STEPS   = 20;

static LedPattern pattern   = LedPattern::NoWifi;
static int8_t     lastLevel = -1;

// Bit n = LED on in step n.
static uint32_t mask(LedPattern p) {
    switch (p) {
    case LedPattern::NoWifi:      return 0b1;
    case LedPattern::AccessPoint: return 0b101;
    case LedPattern::Waiting:     return 0x1F | (0x1F << 10);
    // Not steady on: steady on is also what a LED does when the loop has
    // stopped at the wrong moment. The dark step is the proof of life.
    case LedPattern::Ready:       return 0xFFFFE;
    case LedPattern::Transmit:    return 0x55555;
    case LedPattern::Alarm:       return 0b10101;
    }
    return 0;
}

void statusLedBegin() {
    if (LED_PIN < 0) return;
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, !LED_ON);
}

void statusLedSet(LedPattern p) { pattern = p; }

void statusLedLoop() {
    if (LED_PIN < 0) return;
    const uint32_t step  = (millis() / STEP_MS) % STEPS;
    const int8_t   level = (mask(pattern) >> step) & 1;
    if (level == lastLevel) return;
    lastLevel = level;
    digitalWrite(LED_PIN, level ? LED_ON : !LED_ON);
}
