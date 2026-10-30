#include "sensor.h"
#include "config.h"
#include "calibration.h"
#include "relay.h"
#include "HX711.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool   simulateSensor = SIMULATE_SENSOR_DEFAULT;

long   rawValue   = 0;
double weightRaw  = 0.0;
double weightFilt = 0.0;
double sps        = 0.0;
bool   haveSample = false;

static HX711 scale;
static unsigned long lastSampleMs = 0;
static unsigned long lastSimMs    = 0;

// If the real HX711 hasn't returned a reading in this long, treat it as
// disconnected.
static const unsigned long SENSOR_TIMEOUT_MS = 1500;

// ---- stage 1: median spike filter (raw counts) ----
static long medBuf[MEDIAN_WIN];
static int  medHead = 0;

static bool filtReady = false;
static int  outCount  = 0;   // consecutive samples currently outside the stable band

// Runs on its own FreeRTOS task (see sensorInit()) so HX711 sampling keeps
// its cadence regardless of what the web server / relay / serial code is
// doing - it no longer shares a single superloop with them.
static void sensorTaskFn(void *)
{
    for (;;) {
        updateSample();
        vTaskDelay(1);   // 1 tick (~1 ms); updateSample() self-paces off millis()
    }
}

void sensorInit()
{
#if HX711_RATE_PIN >= 0
    pinMode(HX711_RATE_PIN, OUTPUT);
    digitalWrite(HX711_RATE_PIN, HX711_FAST ? HIGH : LOW);
#endif
    scale.begin(HX711_DT, HX711_SCK);

    // Priority 2 (above the default Arduino loop task's priority 1) so it
    // always preempts relay/serial/web work; pinned to core 1, alongside
    // the Arduino loop task and away from the WiFi/LwIP/AsyncTCP tasks on
    // core 0.
    xTaskCreatePinnedToCore(sensorTaskFn, "sensor", 4096, nullptr, 2, nullptr, 1);
}

void sensorResetFilter()
{
    filtReady  = false;
    haveSample = false;
    outCount   = 0;
}

// Current stable reading expressed back in raw HX711 counts.
double sensorRawAverage()
{
    calLock();
    double cpu = (countsPerUnit != 0.0) ? countsPerUnit : 1.0;
    double zo  = tempAdjustedZero(zeroOffset, tempCoeff, tempRefC, liveTempC);
    calUnlock();
    if (!filtReady) return (double)rawValue;
    return weightFilt * cpu + zo;
}

// Instant tare - snap the zero to the average we already have, then let
// the filter re-settle from 0. No blocking read.
void sensorTare()
{
    // readAverageRaw() blocks for a burst of samples - never call it while
    // holding calLock(), or it'd stall the sensor task's calibration reads
    // for the whole burst.
    double nz = filtReady ? sensorRawAverage() : (double)readAverageRaw(10);
    calLock();
    // nz is the current raw reading (temp-compensated, if a live temp is
    // set). zeroOffset itself must stay temperature-independent - it's the
    // zero AT tempRefC - since updateSample() adds the temp term back on
    // top of it every sample. Storing nz directly here would bake today's
    // temp correction into zeroOffset and then apply it a second time on
    // the next sample, leaving a residual instead of a clean zero.
    zeroOffset = isnan(liveTempC) ? nz : nz - tempCoeff * (liveTempC - tempRefC);
    calUnlock();
    sensorResetFilter();
}

// Synthetic HX711 reading, in raw counts, built from the current
// calibration (grams) so a calibrated stand shows a clean signal.
// Models a cold gas thruster: ~0 at rest, a thrust plateau while the
// valve is open (~500 g-force ~= 4.9 N).
static long simulatedRaw()
{
    double t = millis() / 1000.0;
    double grams = 2.0 * sin(t * 0.3);                         // tiny zero drift
    if (relayEnabled && relayState)
        grams += 500.0 + 15.0 * sin(t * 8.0);                  // firing
    double noise = (random(-1000, 1001) / 1000.0) * 3.0;       // ~3 g
    return (long)(zeroOffset + (grams + noise) * countsPerUnit);
}

static long medianOf(const long *src, int n)
{
    long a[MEDIAN_WIN];
    for (int i = 0; i < n; i++) a[i] = src[i];
    for (int i = 1; i < n; i++) {           // insertion sort, n is tiny
        long k = a[i]; int j = i - 1;
        while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
        a[j + 1] = k;
    }
    return a[n / 2];
}

void updateSample()
{
    long r;
    if (simulateSensor) {
        if (millis() - lastSimMs < (unsigned long)(1000 / SIM_SPS)) return;
        lastSimMs = millis();
        r = simulatedRaw();
    } else {
        if (!scale.is_ready()) return;
        r = scale.read();
    }

    unsigned long now = millis();
    double dt = (lastSampleMs != 0) ? (now - lastSampleMs) / 1000.0 : (1.0 / SIM_SPS);
    if (dt <= 0) dt = 1e-3;

    // ---- stage 1: median spike filter on raw counts ----
    if (!filtReady) { for (int i = 0; i < MEDIAN_WIN; i++) medBuf[i] = r; medHead = 0; }
    else            { medBuf[medHead] = r; medHead = (medHead + 1) % MEDIAN_WIN; }
    long rMed = medianOf(medBuf, MEDIAN_WIN);
    rawValue = rMed;

    // ---- counts -> weight ----
    calLock();
    double cpu = (countsPerUnit != 0.0) ? countsPerUnit : 1.0;
    double zo  = tempAdjustedZero(zeroOffset, tempCoeff, tempRefC, liveTempC);
    calUnlock();
    double w0 = (rMed - zo) / cpu;
    weightRaw = w0;

    if (!filtReady) {
        weightFilt = w0;
        outCount   = 0;
        filtReady  = true;
        haveSample = true;
    } else {
        // ---- stage 2: weighing-scale stability gate ----
        // Hold the displayed value steady while readings stay inside the
        // noise band - no jittering digits. A real change has to stay
        // outside the band for STABLE_CONFIRM samples in a row before the
        // display jumps straight to it: instant, no averaging lag.
        if (fabs(w0 - weightFilt) > STABLE_BAND) {
            if (++outCount >= STABLE_CONFIRM) { weightFilt = w0; outCount = 0; }
        } else {
            outCount = 0;
        }
    }
    if (fabs(weightFilt) < STABLE_BAND) weightFilt = 0.0;   // clean 0.00 at rest

    // ---- rate estimate ----
    if (lastSampleMs != 0 && dt > 0) sps += 0.2 * (1.0 / dt - sps);
    lastSampleMs = now;
}

bool sensorOnline()
{
    if (simulateSensor) return true;
    return haveSample && (millis() - lastSampleMs) < SENSOR_TIMEOUT_MS;
}

long readAverageRaw(int times)
{
    if (times < 1) times = 1;
    const int N = (times > 64) ? 64 : times;
    long s[64];
    int got = 0;

    for (int i = 0; i < N; i++) {
        if (simulateSensor) {
            s[got++] = simulatedRaw();
            delay(1000 / SIM_SPS);
            continue;
        }
        unsigned long t0 = millis();
        while (!scale.is_ready() && millis() - t0 < 200) delay(1);
        if (!scale.is_ready()) break;                 // disconnected - bail out
        s[got++] = scale.read();
    }
    if (got == 0) return (long)zeroOffset;

    for (int i = 1; i < got; i++) {
        long k = s[i]; int j = i - 1;
        while (j >= 0 && s[j] > k) { s[j + 1] = s[j]; j--; }
        s[j + 1] = k;
    }
    // trimmed mean: drop the fastest/slowest 20% before averaging
    int lo = got / 5, hi = got - got / 5;
    if (hi <= lo) { lo = 0; hi = got; }
    double acc = 0;
    for (int i = lo; i < hi; i++) acc += s[i];
    return (long)llround(acc / (hi - lo));
}
