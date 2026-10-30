#pragma once
#include <Arduino.h>

// =====================================================================
//  Calibration model
//      zero(T) = zeroOffset + tempCoeff * (liveTempC - tempRefC)
//      weight  = (raw - zero(T)) / countsPerUnit
//  countsPerUnit is HX711 counts per one unit of mass (grams by default).
//  tempCoeff/tempRefC come from the dashboard's temperature-aware fit
//  (counts per degree C, and the reference temperature that fit's
//  zeroOffset corresponds to); tempCoeff==0 or liveTempC unset (NAN)
//  disables the correction entirely, so calibrations captured before this
//  feature existed - or a board nobody has told the current temperature -
//  behave exactly as before.
// =====================================================================

extern double zeroOffset;
extern double countsPerUnit;
extern double tempCoeff;    // counts per degree C; 0 = no temp compensation
extern double tempRefC;     // reference temperature zeroOffset was fit at
extern double liveTempC;    // current ambient temp from the dashboard; NAN = unknown
extern char   unitLabel[8];

// zeroOffset/countsPerUnit/tempCoeff/tempRefC/liveTempC/unitLabel are read
// on every sensor sample (from the dedicated sensor task) and written
// occasionally from a web request handler (a different FreeRTOS task, since
// the web server is async) - hold this only around the actual read/write,
// never across I/O or a blocking call.
void calLock();
void calUnlock();

// Load / store calibration in flash (Preferences namespace "loadcell").
// liveTempC is intentionally session-only and never persisted - a stale
// temperature should never silently keep correcting readings after a reboot.
void loadCalibration();
void saveCalibration();

// Set the unit label safely (truncates / ignores empty).
void setUnitLabel(const String &u);

// Least squares fit of  raw = A * mass + B   ->  A = countsPerUnit,
// B = zeroOffset. Returns false + fills err on bad input. On success it
// writes the results straight into countsPerUnit / zeroOffset.
bool computeCalibration(int n, const double *mass, const double *raw, String &err);

// Pure arithmetic, no locking - caller reads zero/coeff/refC/liveC under
// calLock() and passes them in. liveC==NAN (temperature never set this
// session) returns zero unchanged.
static inline double tempAdjustedZero(double zero, double coeff, double refC, double liveC)
{
    return isnan(liveC) ? zero : zero + coeff * (liveC - refC);
}
