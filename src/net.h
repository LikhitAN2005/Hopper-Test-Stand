#pragma once
#include <Arduino.h>

// =====================================================================
//  WiFi bring-up. Tries the networks listed in WIFI_NETWORKS (main.cpp)
//  one by one and stays on the first that connects; if none do it starts
//  the fallback access point instead.
// =====================================================================

#define MAX_WIFI_NETWORKS 5

struct WifiNetwork {
    const char *ssid;
    const char *password;
    bool        useStaticIp;
};

extern const WifiNetwork WIFI_NETWORKS[];
extern const int         WIFI_NETWORK_COUNT;

void startWiFi();
void wifiTick();     // call from loop(): re-joins the network if the link drops

// IP the dashboard is currently reachable at (station IP, or the
// fallback AP's IP). Safe to call any time after startWiFi().
String currentIP();

bool wifiIsAP();     // true if we fell back to being the access point
int  wifiRSSI();     // link strength in dBm (station mode), 0 otherwise
