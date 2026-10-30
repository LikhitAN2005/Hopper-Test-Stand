#include "temperature.h"
#include "config.h"

#if TEMP_PIN >= 0
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire           oneWire(TEMP_PIN);
static DallasTemperature probe(&oneWire);

static double        lastC       = NAN;
static unsigned long lastReadMs  = 0;
static unsigned long convStartMs = 0;
static bool          converting  = false;

// A reading older than this is treated as "sensor gone".
static const unsigned long TEMP_STALE_MS = 5000;
// DS18B20 needs up to 750 ms at 12-bit resolution.
static const unsigned long TEMP_CONV_MS  = 800;

void temperatureInit()
{
    probe.begin();
    probe.setWaitForConversion(false);   // never block the main loop
}

void temperatureTick()
{
    unsigned long now = millis();
    if (!converting) {
        if (now - convStartMs >= TEMP_PERIOD_MS || convStartMs == 0) {
            probe.requestTemperatures();
            convStartMs = now;
            converting  = true;
        }
        return;
    }
    if (now - convStartMs < TEMP_CONV_MS) return;
    float c = probe.getTempCByIndex(0);
    converting = false;
    if (c != DEVICE_DISCONNECTED_C && c > -55.0f && c < 125.0f) {
        lastC      = c;
        lastReadMs = now;
    }
}

bool temperatureValid()
{
    return !isnan(lastC) && (millis() - lastReadMs) < TEMP_STALE_MS;
}

double temperatureC()
{
    return temperatureValid() ? lastC : NAN;
}

#else   // no sensor fitted

void   temperatureInit()  {}
void   temperatureTick()  {}
bool   temperatureValid() { return false; }
double temperatureC()     { return NAN; }

#endif
