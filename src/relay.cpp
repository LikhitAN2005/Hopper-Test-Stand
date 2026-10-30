#include "relay.h"
#include "config.h"

bool  relayEnabled  = RELAY_ENABLED_DEFAULT;
bool  relayState    = false;
float relayCycleHz  = 0.0f;

static unsigned long relayPulseUntil = 0;
static unsigned long relayCycleNext  = 0;

void relayInit()
{
    pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(RELAY_PIN, HIGH);
    applyRelay();
}

void applyRelay()
{
    if (relayEnabled && relayState)
        digitalWrite(RELAY_PIN, LOW);
    else
        digitalWrite(RELAY_PIN, HIGH);
}

void setRelay(bool on)
{
    relayCycleHz = 0.0f;          // any explicit command cancels cycling
    if (!relayEnabled) {
        relayState = false;
        relayPulseUntil = 0;
        applyRelay();
        return;
    }
    relayState = on;
    if (!on) relayPulseUntil = 0;
    applyRelay();
}

void pulseRelay(unsigned long ms)
{
    if (!relayEnabled) return;
    setRelay(true);
    relayPulseUntil = millis() + ms;
}

void cycleRelay(float hz)
{
    if (!relayEnabled) return;
    if (hz < 0.2f)  hz = 0.2f;
    if (hz > 50.0f) hz = 50.0f;
    relayPulseUntil = 0;
    relayCycleHz    = hz;
    relayCycleNext  = millis();   // toggle on the next tick
}

void relayTick()
{
    if (relayCycleHz > 0.0f) {
        if (!relayEnabled) { setRelay(false); return; }
        unsigned long now = millis();
        if (now >= relayCycleNext) {
            relayState = !relayState;
            applyRelay();
            unsigned long half = (unsigned long)(500.0f / relayCycleHz);
            relayCycleNext = now + (half ? half : 1);
        }
        return;
    }

    if (relayPulseUntil && millis() >= relayPulseUntil) {
        relayPulseUntil = 0;
        setRelay(false);
    }
}
