#pragma once
#include <Arduino.h>

// =====================================================================
//  Load-cell ambient temperature (optional DS18B20 on TEMP_PIN).
//  Disabled while TEMP_PIN < 0 in config.h - the dashboard then falls back
//  to manual temperature entry.
// =====================================================================

void   temperatureInit();
void   temperatureTick();     // call from loop(); non-blocking
bool   temperatureValid();    // true once a fresh reading exists
double temperatureC();        // last reading, degrees C (NAN if none)
