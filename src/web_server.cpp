#include "web_server.h"
#include "web_page.h"
#include "config.h"
#include "calibration.h"
#include "sensor.h"
#include "relay.h"
#include "temperature.h"
#include "net.h"
#include <ESPAsyncWebServer.h>

// ---------------------------------------------------------------------
//  Async server: the dashboard gets pushed a JSON reading over a
//  persistent WebSocket ("/ws") instead of polling /data with a fresh
//  HTTP request every tick - one connection, no repeated TCP/HTTP
//  handshake overhead per sample. /data is kept as a plain REST endpoint
//  too (the tiny status page at "/" uses it, and it's handy for curl).
// ---------------------------------------------------------------------
#define CAL_MAX_POINTS 128

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static void buildDataJson(char *buf, size_t len)
{
    calLock();
    double cpu = countsPerUnit, zo = zeroOffset;
    double tc = tempCoeff, tr = tempRefC, lt = liveTempC;
    char unit[sizeof(unitLabel)];
    memcpy(unit, unitLabel, sizeof(unit));
    calUnlock();

    char tbuf[16];
    if (temperatureValid()) snprintf(tbuf, sizeof(tbuf), "%.2f", temperatureC());
    else                    strcpy(tbuf, "null");

    char ltbuf[16];
    if (!isnan(lt)) snprintf(ltbuf, sizeof(ltbuf), "%.2f", lt);
    else            strcpy(ltbuf, "null");

    snprintf(buf, len,
        "{\"raw\":%ld,\"weight\":%.4f,\"filt\":%.4f,\"unit\":\"%s\","
        "\"sps\":%.1f,\"countsPerUnit\":%.6f,\"zeroOffset\":%.3f,"
        "\"tempCoeff\":%.4f,\"tempRefC\":%.2f,\"liveTempC\":%s,"
        "\"relayEnabled\":%s,\"relayState\":%s,\"relayCycle\":%.1f,"
        "\"simulate\":%s,\"online\":%s,"
        "\"ip\":\"%s\",\"ap\":%s,\"rssi\":%d,"
        "\"uptime\":%lu,\"tnow\":%lu,\"tempC\":%s}",
        rawValue, weightRaw, weightFilt, unit,
        sps, cpu, zo,
        tc, tr, ltbuf,
        relayEnabled ? "true" : "false",
        relayState ? "true" : "false",
        relayCycleHz,
        simulateSensor ? "true" : "false",
        sensorOnline() ? "true" : "false",
        currentIP().c_str(),
        wifiIsAP() ? "true" : "false", wifiRSSI(),
        millis() / 1000UL, millis(), tbuf);
}

// ---------------------------------------------------------------------
static void handleRoot(AsyncWebServerRequest *request)
{
    request->send(200, "text/html", INDEX_HTML);
}

static void handleData(AsyncWebServerRequest *request)
{
    char buf[480];
    buildDataJson(buf, sizeof(buf));
    request->send(200, "application/json", buf);
}

static void handleCapture(AsyncWebServerRequest *request)
{
    // instant: read straight off the stable reading (capture while settled)
    long r = (long)llround(sensorRawAverage());
    char buf[80];
    if (temperatureValid())
        snprintf(buf, sizeof(buf), "{\"raw\":%ld,\"tempC\":%.2f}", r, temperatureC());
    else
        snprintf(buf, sizeof(buf), "{\"raw\":%ld,\"tempC\":null}", r);
    request->send(200, "application/json", buf);
}

static void handleTare(AsyncWebServerRequest *request)
{
    sensorTare();                         // instant - no blocking burst read
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"zeroOffset\":%.3f}", zeroOffset);
    request->send(200, "application/json", buf);
}

static void handleCalibrate(AsyncWebServerRequest *request)
{
    int n = request->hasArg("n") ? request->arg("n").toInt() : 0;
    if (n < 2 || n > CAL_MAX_POINTS) {
        request->send(200, "application/json", "{\"ok\":false,\"error\":\"need 2..128 points\"}");
        return;
    }

    static double mass[CAL_MAX_POINTS], raw[CAL_MAX_POINTS];
    for (int i = 0; i < n; i++) {
        mass[i] = request->arg(String("mass") + i).toFloat();
        raw[i]  = request->arg(String("raw") + i).toFloat();
    }

    setUnitLabel(request->arg("unit"));

    String err;
    bool ok = computeCalibration(n, mass, raw, err);

    char buf[200];
    if (ok) {
        sensorResetFilter();  // let the filter re-settle on the new scale
        snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"countsPerUnit\":%.6f,\"zeroOffset\":%.3f}",
            countsPerUnit, zeroOffset);
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", err.c_str());
    }
    request->send(200, "application/json", buf);
}

static void handleSetCal(AsyncWebServerRequest *request)
{
    calLock();
    if (request->hasArg("factor")) {
        double f = request->arg("factor").toFloat();
        if (f != 0.0) countsPerUnit = f;
    }
    if (request->hasArg("offset")) zeroOffset = request->arg("offset").toFloat();
    // Optional - only sent by a temperature-aware fit. Left alone (not
    // reset to 0) when omitted, so applying a plain mass-only fit doesn't
    // silently wipe out a temp coefficient saved earlier.
    if (request->hasArg("tempCoeff")) tempCoeff = request->arg("tempCoeff").toFloat();
    if (request->hasArg("tempRef"))   tempRefC  = request->arg("tempRef").toFloat();
    calUnlock();
    if (request->hasArg("unit")) setUnitLabel(request->arg("unit"));
    sensorResetFilter();
    request->send(200, "application/json", "{\"ok\":true}");
}

static void handleSaveCal(AsyncWebServerRequest *request)
{
    saveCalibration();
    request->send(200, "application/json", "{\"ok\":true}");
}

// Dashboard-reported ambient temperature (no onboard probe - TEMP_PIN < 0).
// Session-only: never persisted, so a stale value can't keep correcting
// readings after a reboot - see calibration.h.
static void handleSetTemp(AsyncWebServerRequest *request)
{
    calLock();
    if (request->hasArg("c")) liveTempC = request->arg("c").toFloat();
    double lt = liveTempC;
    calUnlock();
    if (request->hasArg("c")) sensorResetFilter();  // re-settle at the new effective zero

    char buf[48];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"liveTempC\":%.2f}", lt);
    request->send(200, "application/json", buf);
}

static void handleSim(AsyncWebServerRequest *request)
{
    if (request->hasArg("on")) {
        simulateSensor = request->arg("on").toInt() != 0;
        sensorResetFilter();
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "{\"simulate\":%s}", simulateSensor ? "true" : "false");
    request->send(200, "application/json", buf);
}

static void handleRelay(AsyncWebServerRequest *request)
{
    if (request->hasArg("enable")) {
        relayEnabled = request->arg("enable").toInt() != 0;
        if (!relayEnabled) setRelay(false);
        else applyRelay();
    }
    if (request->hasArg("state")) {
        String s = request->arg("state");
        if (s == "on")         setRelay(true);
        else if (s == "off")   setRelay(false);
        else if (s == "pulse") pulseRelay(request->hasArg("ms") ? request->arg("ms").toInt() : 500);
        else if (s == "cycle") cycleRelay(request->hasArg("hz") ? request->arg("hz").toFloat() : 5.0f);
    }
    char buf[110];
    snprintf(buf, sizeof(buf),
             "{\"relayEnabled\":%s,\"relayState\":%s,\"relayCycle\":%.1f}",
             relayEnabled ? "true" : "false", relayState ? "true" : "false",
             relayCycleHz);
    request->send(200, "application/json", buf);
}

// Push-only stream - the dashboard doesn't send anything over the socket,
// so there's nothing to react to here beyond letting the library track
// connect/disconnect (ws.count() below then reflects who's listening).
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    (void)server; (void)client; (void)type; (void)arg; (void)data; (void)len;
}

// ---------------------------------------------------------------------
void webServerInit()
{
    // allow a dashboard.html opened from file:// or another origin to
    // fetch the JSON endpoints / open the socket
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/",          HTTP_GET,  handleRoot);
    server.on("/data",      HTTP_GET,  handleData);
    server.on("/capture",   HTTP_GET,  handleCapture);
    server.on("/tare",      HTTP_GET,  handleTare);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/setcal",    HTTP_GET,  handleSetCal);
    server.on("/savecal",   HTTP_GET,  handleSaveCal);
    server.on("/settemp",   HTTP_GET,  handleSetTemp);
    server.on("/relay",     HTTP_GET,  handleRelay);
    server.on("/sim",       HTTP_GET,  handleSim);
    server.begin();
}

void webServerTick()
{
    ws.cleanupClients();

    static unsigned long lastBroadcast = 0;
    unsigned long now = millis();
    if (now - lastBroadcast < WS_BROADCAST_MS) return;
    lastBroadcast = now;

    if (ws.count() == 0) return;   // nobody listening - nothing to push
    // If a client can't keep up (weak link), skip this frame instead of
    // piling messages into its queue - an overfull queue makes the library
    // drop the connection, which looks like the stream "disconnecting".
    if (!ws.availableForWriteAll()) return;
    char buf[480];
    buildDataJson(buf, sizeof(buf));
    ws.textAll(buf);
}
