#pragma once
#include <Arduino.h>

// =====================================================================
//  Relay control
//  Open-drain wiring:
//      LOW  = pull IN to GND  -> relay ON
//      HIGH = release IN (Hi-Z), external pull-up -> relay OFF
//
//  While relayEnabled is false the pin is held OFF and every command
//  (setRelay / pulseRelay) is refused.
// =====================================================================

extern bool  relayEnabled;
extern bool  relayState;
extern float relayCycleHz;          // > 0 while the valve is being cycled

void relayInit();                    // pinMode + held OFF, call from setup()
void applyRelay();                   // push relayEnabled/relayState to the pin
void setRelay(bool on);              // also cancels any pulse / cycle
void pulseRelay(unsigned long ms);   // one open pulse
void cycleRelay(float hz);           // toggle open/closed at hz until setRelay() stops it
void relayTick();                    // drives pulse + cycle timing, call from loop()
