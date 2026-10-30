#include "serial_cmd.h"
#include "calibration.h"
#include "sensor.h"
#include "relay.h"
#include <Arduino.h>

void handleSerial()
{
    if (!Serial.available()) return;
    char c = Serial.read();
    if (c == 'r') {
        relayEnabled = !relayEnabled;
        if (!relayEnabled) setRelay(false);
        Serial.printf("relayEnabled = %s\n", relayEnabled ? "true" : "false");
    } else if (c == 'm') {
        simulateSensor = !simulateSensor;
        sensorResetFilter();
        Serial.printf("simulateSensor = %s\n", simulateSensor ? "true" : "false");
    } else if (c == 't') {
        sensorTare();
        Serial.printf("tared, zeroOffset = %.3f\n", zeroOffset);
    } else if (c == 's') {
        Serial.printf("raw=%ld  weight=%.3f %s  filt=%.3f  sps=%.1f  relayEn=%d relaySt=%d\n",
                      rawValue, weightRaw, unitLabel, weightFilt, sps,
                      relayEnabled, relayState);
    }
}
