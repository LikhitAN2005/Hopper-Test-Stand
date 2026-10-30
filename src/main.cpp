#include <Arduino.h>

#include "config.h"
#include "calibration.h"
#include "sensor.h"
#include "temperature.h"
#include "relay.h"
#include "net.h"
#include "web_server.h"
#include "serial_cmd.h"

// ---------------------------------------------------------------------
//  WiFi networks to try, in order (up to MAX_WIFI_NETWORKS = 5). At boot
//  the board scans, then tries each network that is actually in range, top
//  to bottom, and stays on the first one that connects. If none connect it
//  starts its own fallback access point.
//
//  useStaticIp: true  -> pin the board to STATIC_IP from config.h (right for
//                        Windows "Mobile hotspot", subnet 192.168.137.x)
//               false -> take whatever address DHCP hands out (phone
//                        hotspots etc. - read it from the serial monitor
//                        or open http://hopper.local)
//  Turn the hotspot ON before powering the ESP32.
// ---------------------------------------------------------------------
const WifiNetwork WIFI_NETWORKS[] = {
    // ssid          password        useStaticIp
    { "c6ody",       "TaylorSwift",  true  },
    { "hopper",  "adithi1234",   true  },
    { "bipsan",  "60010611",   true  },
    // { "phone",     "password4",   false },
    // { "spare",     "password5",   true  },
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// =====================================================================
//  Hopper Test Stand - load cell web dashboard
//
//  This file just wires the modules together:
//    config.h       - pins, static IP, defaults (WiFi name/pass are below)
//    calibration.*  - the weight = (raw - zero) / cpu model + flash storage
//    sensor.*       - HX711 sampling (own FreeRTOS task), median + stability-gate filter, simulation mode
//    relay.*        - relay control (disabled until enabled from the UI)
//    net.*          - joins the laptop hotspot, else starts a fallback AP
//    web_server.*   - the HTTP endpoints
//    web_page.*     - the dashboard HTML/JS served at "/"
//    serial_cmd.*   - r / m / t / s serial shortcuts
// =====================================================================

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Hopper Test Stand ===");

    relayInit();

    sensorInit();
    Serial.println("HX711 initialized");

    temperatureInit();

    loadCalibration();
    Serial.printf("Calibration: countsPerUnit=%.6f  zeroOffset=%.3f  unit=%s\n",
                  countsPerUnit, zeroOffset, unitLabel);

    startWiFi();

    webServerInit();
    Serial.println("HTTP server started");
    Serial.println("Serial cmds:  r=toggle relay  m=toggle simulate  t=tare  s=status");
}

void loop()
{
    // Sensor sampling now runs on its own FreeRTOS task (see sensorInit())
    // so it keeps a steady cadence regardless of what this loop is doing.
    wifiTick();
    webServerTick();
    handleSerial();
    relayTick();
    temperatureTick();
}
