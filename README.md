# Hopper Test Stand

ESP32 + HX711 dashboard for a cold-gas thruster test stand. Dark, minimal
web UI: live thrust chart, load-cell calibration, and an armed valve control.

## Dashboard

The dashboard is a **standalone file — [`dashboard.html`](dashboard.html)** — that
you open in a browser (double-click it, or host it anywhere). It connects to the
board over a WebSocket (`/ws`) that pushes a reading straight from the ESP32 -
no polling - plus a small REST API for everything else; the ESP32 only serves
JSON and none of the UI. Edit and reload the dashboard freely; no reflashing.

On first open, set the **API** box at the top to the board's address
(`http://192.168.137.50` by default) — it's remembered per browser. Hitting the
board's IP directly (`http://192.168.137.50/`) still shows a tiny text status
readout.

- **Status box (top right):** link strength (dBm + bars), uptime, valve
  state (OPEN / CLOSED), system state (ARMED / SAFE).
- **Thrust:** big live reading in **N** with **kg** and **g** beside it;
  scrolling chart in newtons (`denoised` / `raw-fast`) or `raw counts`,
  5 / 20 / 60 s window; `tare`, `clear`, `pause`, `sim`.
  `tare` is instant — it snaps the zero to the stable reading the filter
  already holds, no blocking burst read.
- **Valve:** one `ARM` / `DISARM` toggle, then `OPEN` / `CLOSE` (hold),
  `FIRE` (single pulse of *N* ms), and `CYCLE` (repeatedly open/close at
  *N* Hz until stopped, timed on the MCU). All disabled until armed. Button
  colours: amber = arm, red = open/fire/cycle, green = close.
- **Load cell calibration (in grams):** put known masses on the stand, enter
  each in **grams**, capture, `compute & apply` a least-squares fit
  (counts/g), `save` to flash. Live plot of the points against the fitted
  line shows calibration quality. The dashboard converts grams-force → N for
  display (`N = g / 1000 × 9.80665`).

## File layout (`src/`)

| File | What's in it |
|------|--------------|
| `main.cpp` | `setup()` / `loop()` only - wires the modules together |
| `config.h` | **Edit this.** Pins, WiFi hotspot name/password, static IP, defaults |
| `calibration.{h,cpp}` | `weight = (raw - zeroOffset) / countsPerUnit`, least-squares fit, flash storage |
| `sensor.{h,cpp}` | HX711 sampling on its own FreeRTOS task, the denoising pipeline, simulation mode |
| `relay.{h,cpp}` | Relay control (held OFF until enabled from the UI) |
| `net.{h,cpp}` | Joins the laptop hotspot; starts a fallback AP if it can't |
| `web_server.{h,cpp}` | Async HTTP + WebSocket server - pushes readings over `/ws`, plus REST endpoints (`/data`, `/tare`, `/capture`, `/calibrate`, `/relay`, `/sim`), CORS enabled |
| `web_page.{h,cpp}` | Tiny text status page served at `/` (the real UI is `dashboard.html`) |
| `serial_cmd.{h,cpp}` | `r` / `m` / `t` / `s` serial shortcuts |

`dashboard.html` (project root) is the full UI — a plain file, not on the MCU.

## WiFi - connecting to your laptop's hotspot

The ESP32 now joins your **laptop's Mobile hotspot** as a client, so you stay
on the internet and never have to switch networks.

1. Turn on Windows **Mobile hotspot** (Settings > Network & internet > Mobile hotspot).
2. At the top of [`src/main.cpp`](src/main.cpp) fill in the `WIFI_NETWORKS` list (up to 5):
   ```c
   const WifiNetwork WIFI_NETWORKS[] = {
       { "laptop-1", "password1", true  },   // true = use the fixed STATIC_IP
       { "laptop-2", "password2", true  },
       { "phone",    "password3", false },   // false = DHCP
   };
   ```
   At boot the board scans, tries each network in range from top to bottom, and
   stays on the first one that connects. If none do, it starts its own access point.
   (Names and passwords are on each Mobile hotspot settings page.)
3. Flash the board with the hotspot already on.

### Which IP?

Windows Mobile hotspot always uses the `192.168.137.x` subnet, and the firmware
pins the ESP32 to a fixed address on it:

> **http://192.168.137.50**

That URL never changes. `http://hopper.local` also works.

If you'd rather use a **phone** hotspot (different subnet), set
`#define USE_STATIC_IP 0` in `config.h` and read the IP the board prints on the
serial monitor at boot.

If the hotspot isn't found within 20 s, the board starts its own network
`HopperTestStand` / `hopper12345` at `http://192.168.4.1` as a fallback.

## Streaming architecture

- **Sensor sampling runs on its own FreeRTOS task** (`sensorInit()` in
  `sensor.cpp`), pinned to the same core as the Arduino loop but at a higher
  priority, so HX711 timing stays steady no matter what the web server,
  relay, or serial handling are doing.
- **The web server is fully async** (`ESPAsyncWebServer` + `AsyncTCP`,
  replacing the old blocking `WebServer`), running on its own task too -
  handling a request never stalls sampling or relay timing.
- **The dashboard streams over `/ws`** instead of polling `/data` on a
  timer: the board pushes one JSON reading to every connected dashboard
  every `WS_BROADCAST_MS` (`config.h`, 50 ms by default). One persistent
  connection, no repeated HTTP connect/handshake per sample - this is what
  fixed the streaming lag that showed up after long runs.
- Because sampling, the web server, and `loop()` (relay/serial) now run as
  genuinely separate tasks, `zeroOffset` / `countsPerUnit` / `unitLabel`
  (`calibration.cpp`) are guarded by a small FreeRTOS mutex (`calLock()` /
  `calUnlock()`) - they're read on every sample and written occasionally
  from a web request handler on a different task. The live weight readout
  itself isn't locked: it has a single writer (the sensor task) and is
  read-only telemetry, so a rare torn read is a one-frame cosmetic glitch,
  not a correctness issue.

## Load cell noise - how it's handled, and how to get it lower

### Sample rate

The HX711 chip has **only two rates**, set by its `RATE` pin: **10 SPS** or
**80 SPS**. There is no 50 SPS mode. The firmware runs it at **80 SPS**
(`HX711_FAST 1` in `config.h`) and filters down; every raw sample is streamed to
the graph so fast transients during a burn are never lost. If your board breaks
`RATE` out to a GPIO, set `HX711_RATE_PIN`; otherwise strap it in hardware and
match `HX711_FAST`.

Note: 80 SPS is noisier *per sample* than 10 SPS (the chip does less internal
averaging and loses its sharp 50/60 Hz notch). You're trading steady-state
quiet for transient response - the filter below buys most of the quiet back.

### The denoising pipeline (`sensor.cpp`)

Deliberately just two stages, no continuous averaging/smoothing - a smoothed
signal always trades away latency, and for a thrust burn that latency is the
thing you actually care about. Every knob is in `config.h`.

1. **Median-of-7** on raw counts (`MEDIAN_WIN`) - removes single-sample HX711
   glitches before they reach anything downstream. This is the **"raw / fast"**
   plot - full true dynamics, ~1 sample of lag, use it for thrust curves.
2. **Stability gate** (`STABLE_BAND`, `STABLE_CONFIRM`) - the weighing-scale
   behavior: while the stage-1 reading stays within `STABLE_BAND` of the
   currently displayed value, the display doesn't move at all (no jittering
   digits). A genuine change has to sit outside that band for `STABLE_CONFIRM`
   consecutive samples before the display **snaps straight to it** - instant,
   not a crawl to the new level like an EMA/moving-average would do. A settled
   reading inside the band is also forced to exactly `0` so an empty stand
   reads a clean `0.00`. This is the **"denoised"** plot and the big live
   number.

Raise `STABLE_BAND` if the idle reading still visibly jitters; raise
`STABLE_CONFIRM` if brief noise spikes cause false jumps. Both trade a little
responsiveness for stability, but neither ever blends/lags a real change the
way a moving average does.

### Hardware is the bigger lever

Filtering can't recover information the ADC never captured cleanly. In rough
order of impact:

- **Clean power to the HX711.** Its analog supply is also its ADC reference and
  the bridge excitation, so supply noise goes straight into the reading. Feed it
  from a quiet LDO (not the ESP32's USB-derived 3V3 rail), with a 100 µF bulk +
  100 nF ceramic right at the chip.
- **Shielded 4-wire cable** to the cell, shield grounded at the HX711 end only.
  Keep it away from the relay wiring, any motor/servo leads, and the ESP32
  antenna.
- **Rigid, low-stress mount.** The cell faithfully reports vibration, cable tug,
  air currents from AC/fans, and thermal expansion of the fixture - all of that
  looks like "noise" but isn't electrical.
- **Let it warm up** ~15-30 min; strain-gauge output drifts until temperatures
  settle.
- **Use channel A, gain 128** (the HX711 default) - it's the quietest input.
- If you can tolerate the latency, drop to **10 SPS** (`HX711_FAST 0`) for the
  lowest steady-state noise and built-in mains rejection.

### Calibration capture

`Capture` and `Tare` now take a **trimmed-mean burst** (drops the outlier 20% at
each end before averaging), so a stray spike during capture won't bias the fit.

### References

- [RobTillaart/HX711](https://github.com/RobTillaart/HX711) - `read_median`
  and noise notes (only 16-20 of 24 bits usable)
- Arduino forum: [load cell + HX711 80 Hz noise](https://forum.arduino.cc/t/loadcell-hx711-80hz-mode-and-noisy-readings-to-uno/299344)
  - vibration is usually the dominant noise source; add a 10 µF + ceramic at the
  HX711 supply
