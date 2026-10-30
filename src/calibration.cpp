#include "calibration.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

double zeroOffset    = 0.0;
double countsPerUnit = 1.0;
double tempCoeff     = 0.0;     // counts per degree C; 0 = no temp compensation
double tempRefC      = 0.0;
double liveTempC     = NAN;     // session-only - never persisted, see calibration.h
char   unitLabel[8]  = "g";     // load cell is calibrated in grams; the
                                // dashboard shows N / kg / g from this

static Preferences prefs;

// Created at static-init time (before setup()/any task runs) so there's no
// first-use race between the sensor task and the main task.
static SemaphoreHandle_t calMutex = xSemaphoreCreateMutex();

void calLock()
{
    xSemaphoreTake(calMutex, portMAX_DELAY);
}

void calUnlock()
{
    xSemaphoreGive(calMutex);
}

void loadCalibration()
{
    prefs.begin("loadcell", true);
    double zero = prefs.getDouble("zero", 0.0);
    double cpu  = prefs.getDouble("cpu", 1.0);
    double tc   = prefs.getDouble("tempCoeff", 0.0);
    double tr   = prefs.getDouble("tempRef", 0.0);
    String u    = prefs.getString("unit", "g");
    prefs.end();
    if (cpu == 0.0) cpu = 1.0;

    calLock();
    zeroOffset    = zero;
    countsPerUnit = cpu;
    tempCoeff     = tc;
    tempRefC      = tr;
    u.toCharArray(unitLabel, sizeof(unitLabel));
    calUnlock();
}

void saveCalibration()
{
    calLock();
    double zero = zeroOffset, cpu = countsPerUnit;
    double tc = tempCoeff, tr = tempRefC;
    char   unit[sizeof(unitLabel)];
    memcpy(unit, unitLabel, sizeof(unit));
    calUnlock();

    prefs.begin("loadcell", false);
    prefs.putDouble("zero", zero);
    prefs.putDouble("cpu", cpu);
    prefs.putDouble("tempCoeff", tc);
    prefs.putDouble("tempRef", tr);
    prefs.putString("unit", unit);
    prefs.end();
}

void setUnitLabel(const String &u)
{
    if (u.length() && u.length() < sizeof(unitLabel)) {
        calLock();
        u.toCharArray(unitLabel, sizeof(unitLabel));
        calUnlock();
    }
}

bool computeCalibration(int n, const double *mass, const double *raw, String &err)
{
    if (n < 2) { err = "need at least 2 points (a zero reading + one weight)"; return false; }

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; i++) {
        sx  += mass[i];
        sy  += raw[i];
        sxx += mass[i] * mass[i];
        sxy += mass[i] * raw[i];
    }
    double denom = n * sxx - sx * sx;
    if (fabs(denom) < 1e-6) { err = "points are degenerate - use different masses"; return false; }

    double A = (n * sxy - sx * sy) / denom;
    double B = (sy * sxx - sx * sxy) / denom;
    if (fabs(A) < 1e-6) { err = "slope is ~0 - check load cell wiring / readings"; return false; }

    calLock();
    countsPerUnit = A;
    zeroOffset    = B;
    calUnlock();
    return true;
}
