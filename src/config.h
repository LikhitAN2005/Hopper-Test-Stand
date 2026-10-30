#pragma once

// =====================================================================
//  config.h - everything you might want to edit lives here
// =====================================================================

// ---------------------------------------------------------------------
//  Pin definitions
// ---------------------------------------------------------------------
// NOTE: do NOT use GPIO 0/2/5/12/15 for HX711_DT - they are ESP32 strapping
// pins. The HX711 DOUT line idles HIGH and, on GPIO12, forces the chip into
// 1.8V flash mode at boot -> "spi_flash: Probe failed" boot loop.
// Also avoid GPIO34-39: input-only, no internal pull-up/down, so a marginal
// connection floats and reads garbage instead of holding a defined level.
#define HX711_DT   34
#define HX711_SCK  14
#define RELAY_PIN  27

// Optional DS18B20 temperature probe taped next to the load cell, used by the
// multi-run calibration. Data pin + 4.7k pull-up to 3V3. Leave at -1 if no
// probe is fitted - the dashboard then lets you type the temperature in.
#define TEMP_PIN        -1
#define TEMP_PERIOD_MS  1000   // how often to take a new reading

// ---------------------------------------------------------------------
//  WiFi - join the laptop's hotspot (station mode)
// ---------------------------------------------------------------------
// The hotspot names + passwords (up to 5) live in the WIFI_NETWORKS list at the
// top of main.cpp. Turn the hotspot ON before powering the
// ESP32.
//
// Windows "Mobile hotspot" always hands out addresses on the
// 192.168.137.x subnet, with the laptop itself at 192.168.137.1.
// We pin the ESP32 to a fixed address on that subnet so the dashboard
// is ALWAYS at the same URL:  http://192.168.137.50
//
// If you use a phone hotspot instead (different subnet), set
// USE_STATIC_IP to 0 and read the printed IP from the serial monitor,
// or just open http://hopper.local
#define USE_STATIC_IP  1
#define STATIC_IP      192, 168, 137, 50
#define GATEWAY_IP     192, 168, 137, 1
#define SUBNET_MASK    255, 255, 255, 0
#define DNS_IP         192, 168, 137, 1

// How long to wait for EACH listed network before moving on to the next one.
// When none connect the board starts its own fallback access point (below).
#define WIFI_JOIN_TIMEOUT_MS  10000

// Fallback AP - only used if the hotspot can't be joined. Connect the
// laptop to this and open http://192.168.4.1
#define FALLBACK_AP_SSID  "HopperTestStand"
#define FALLBACK_AP_PASS  "hopper12345"     // must be >= 8 chars

// mDNS name -> http://hopper.local  (works in either mode)
#define MDNS_NAME  "hopper"

// ---------------------------------------------------------------------
//  Defaults for the two master switches
// ---------------------------------------------------------------------
// Relay hardware has been flaky, so it is OFF by default and every
// relay command is refused until you enable it from the web UI / serial.
#define RELAY_ENABLED_DEFAULT   false

// true -> ignore the HX711 and feed the UI synthetic data so you can
// demo the whole dashboard with no load cell wired.
#define SIMULATE_SENSOR_DEFAULT false

// ---------------------------------------------------------------------
//  Sample rate
// ---------------------------------------------------------------------
// The HX711 chip only has TWO hardware rates, chosen by its RATE pin:
//     RATE low  -> 10 samples/s  (quietest, best mains rejection)
//     RATE high -> 80 samples/s  (noisier per sample, catches transients)
// There is no 50 SPS mode in the silicon. For a hopper thrust/mass trace
// we run it at 80 and filter down; every sample is streamed to the graph.
//
//   * If RATE is broken out to a GPIO on your board, set HX711_RATE_PIN
//     to that pin and it will be driven for you.
//   * If RATE is hardwired on the board, leave it at -1 and set
//     HX711_FAST to match how the board is strapped.
#define HX711_RATE_PIN   -1
#define HX711_FAST        1        // 1 -> 80 SPS, 0 -> 10 SPS

#define SIM_SPS           50       // synthetic feed rate in simulation mode

// ---------------------------------------------------------------------
//  Streaming
// ---------------------------------------------------------------------
// The dashboard connects over a WebSocket instead of polling /data with a
// fresh HTTP request - one persistent connection, no repeat connect/
// handshake overhead per sample. This is how often (ms) the board pushes
// a reading to every connected dashboard.
#define WS_BROADCAST_MS   50       // 20 Hz

// ---------------------------------------------------------------------
//  Denoising pipeline - kept deliberately simple
//
//    raw counts
//      1  median-of-N       drop single-sample HX711 spikes
//      2  stability gate    weighing-scale behavior: hold the displayed
//                           value rock-steady while readings sit inside
//                           the noise band (no jittering digits), then
//                           jump straight to a real change the instant
//                           it's confirmed - no averaging, no lag.
//
//  The "raw / fast" plot is stage 1 only, so a burn's true dynamics are
//  never hidden - stage 2 only affects the live number / "denoised" trace.
// ---------------------------------------------------------------------
#define MEDIAN_WIN      7      // odd, 3..15   (stage 1)
#define STABLE_BAND     0.5    // stage 2: noise band, in calibrated units
                                // (grams by default) - readings inside this
                                // window of the current display don't move it
#define STABLE_CONFIRM  3      // consecutive out-of-band samples required
                                // before accepting a real change
