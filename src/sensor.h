#pragma once
#include <Arduino.h>

// =====================================================================
//  Load cell sampling, the denoising pipeline, and the simulation mode
// =====================================================================

extern bool   simulateSensor;   // true -> feed the UI synthetic data

extern long   rawValue;         // last median-filtered HX711 counts
extern double weightRaw;        // stage-1 weight (median only) - keeps true dynamics
extern double weightFilt;       // stage-2 stabilized weight (see STABLE_BAND) - what
                                 // the dashboard's big number / "denoised" trace show
extern double sps;              // measured samples per second
extern bool   haveSample;       // a sample has been taken since boot/reset

void sensorInit();              // scale.begin() + RATE pin, call from setup()
void updateSample();            // one sample per call when ready, call from loop()
void sensorResetFilter();       // re-init the filter + auto-zero (after calibrate)

// Instant tare: zero the reading using the stable value the filter already
// holds - no blocking burst read.
void   sensorTare();
double sensorRawAverage();      // current stable reading, back in raw HX711 counts

// false if the real HX711 has gone quiet (unplugged / broken wire).
// Always true in simulation mode.
bool sensorOnline();

// Trimmed-mean burst read in raw counts - used for tare / calibration capture.
long readAverageRaw(int times);
