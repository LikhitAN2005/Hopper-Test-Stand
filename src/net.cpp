#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static bool apMode = false;

static void startFallbackAP()
{
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASS);
    WiFi.setSleep(false);
    Serial.println();
    Serial.printf("Hotspot not found - started fallback AP\n");
    Serial.printf("  SSID: %s   PASS: %s\n", FALLBACK_AP_SSID, FALLBACK_AP_PASS);
    Serial.print ("  Open:  http://");
    Serial.println(WiFi.softAPIP());
}

void startWiFi()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    int count = WIFI_NETWORK_COUNT;
    if (count > MAX_WIFI_NETWORKS) {
        Serial.printf("Only the first %d WiFi networks are used\n", MAX_WIFI_NETWORKS);
        count = MAX_WIFI_NETWORKS;
    }

    // Scan once so networks that aren't in range are skipped instantly
    // instead of each burning a full connection timeout.
    Serial.print("Scanning for WiFi... ");
    int found = WiFi.scanNetworks();
    Serial.printf("%d network(s) in range\n", found);

    bool connected = false;
    for (int i = 0; i < count && !connected; i++) {
        const WifiNetwork &n = WIFI_NETWORKS[i];

        bool visible = (found <= 0);          // scan failed -> try anyway
        for (int j = 0; j < found && !visible; j++)
            if (WiFi.SSID(j) == n.ssid) visible = true;
        if (!visible) {
            Serial.printf("[%d/%d] \"%s\" not in range - skipping\n", i + 1, count, n.ssid);
            continue;
        }

        WiFi.disconnect();
#if USE_STATIC_IP
        if (n.useStaticIp) {
            IPAddress ip(STATIC_IP), gw(GATEWAY_IP), mask(SUBNET_MASK), dns(DNS_IP);
            if (!WiFi.config(ip, gw, mask, dns))
                Serial.println("WiFi.config() failed - falling back to DHCP");
        } else
#endif
        {
            WiFi.config(IPAddress(), IPAddress(), IPAddress());   // back to DHCP
        }

        Serial.printf("[%d/%d] Joining \"%s\" ", i + 1, count, n.ssid);
        WiFi.begin(n.ssid, n.password);

        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_JOIN_TIMEOUT_MS) {
            delay(300);
            Serial.print('.');
        }
        Serial.println();
        connected = (WiFi.status() == WL_CONNECTED);
    }
    WiFi.scanDelete();

    if (!connected) {
        startFallbackAP();
    } else {
        Serial.print("Connected. Open:  http://");
        Serial.println(WiFi.localIP());
    }

    if (MDNS.begin(MDNS_NAME))
        Serial.printf("Also:  http://%s.local\n", MDNS_NAME);
}

void wifiTick()
{
    static unsigned long lastTry = 0;
    if (apMode || WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastTry < 5000) return;
    lastTry = now;

    Serial.println("WiFi link lost - reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
}

String currentIP()
{
    return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

bool wifiIsAP()
{
    return apMode;
}

// dBm in station mode; 0 when we're the access point (no meaningful RSSI).
int wifiRSSI()
{
    if (apMode || WiFi.status() != WL_CONNECTED) return 0;
    return WiFi.RSSI();
}
