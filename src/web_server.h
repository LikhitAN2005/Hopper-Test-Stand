#pragma once

// =====================================================================
//  Async HTTP + WebSocket server. The dashboard connects to "/ws" and gets
//  pushed a JSON reading every WS_BROADCAST_MS (config.h) - no polling.
//  Small JSON endpoints for everything else: /data, /capture, /tare,
//  /calibrate, /setcal, /savecal, /relay, /sim.
// =====================================================================

void webServerInit();     // register routes + server.begin(), call from setup()
void webServerTick();     // WS housekeeping + broadcast, call from loop()
