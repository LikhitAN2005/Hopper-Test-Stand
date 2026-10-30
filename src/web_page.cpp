#include <Arduino.h>
#include "web_page.h"

// =====================================================================
//  The heavy dashboard now lives in /dashboard.html - a plain file you
//  open in a browser, which streams from this board's REST API. That
//  keeps the big HTML/JS off the MCU flash and out of its RAM.
//
//  This page, served at "/", is just a tiny status readout so hitting
//  the board's IP directly still shows something useful.
// =====================================================================
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hopper Thruster Stand</title>
<style>
  body{background:#0a0a0a;color:#ededed;font:14px/1.5 ui-sans-serif,system-ui,Segoe UI,Roboto,sans-serif;
       margin:0;padding:26px;max-width:620px}
  h1{letter-spacing:3px;font-size:22px;margin:0 0 2px}
  .mut{color:#7c7c7c} a{color:#7aa2f7}
  code{background:#161616;border:1px solid #262626;border-radius:4px;padding:1px 5px}
  pre{background:#101010;border:1px solid #262626;border-radius:7px;padding:14px;white-space:pre-wrap;
      font:12px/1.6 ui-monospace,Menlo,Consolas,monospace}
</style></head><body>
<h1>HOPPER</h1>
<div class="mut">cold-gas thruster stand &mdash; REST API online</div>
<p>Open <code>dashboard.html</code> (in the project folder) in a browser for the
full UI, and point it at <code id="url">http://this-board-ip</code>.</p>
<p class="mut">Endpoints:
  <a href="/data">/data</a> ·
  <a href="/tare">/tare</a> ·
  /capture · /calibrate · /setcal · /savecal ·
  /relay · /sim
</p>
<pre id="live">loading…</pre>
<script>
document.getElementById('url').textContent='http://'+location.host;
async function tick(){
  try{
    const d=await (await fetch('/data',{cache:'no-store'})).json();
    const N=g=>(g/1000*9.80665).toFixed(3);
    document.getElementById('live').textContent=
`thrust    ${N(d.filt)} N   (${(d.filt/1000).toFixed(3)} kg / ${(+d.filt).toFixed(1)} g)${d.online?'':'   NO SIGNAL'}
raw/fast  ${N(d.weight)} N
counts    ${d.raw}
rate      ${(+d.sps).toFixed(0)} /s
valve     ${d.relayState?'OPEN':'CLOSED'}      system ${d.relayEnabled?'ARMED':'SAFE'}
link      ${d.ap?'AP mode':d.rssi+' dBm'}      uptime ${d.uptime}s
ip        ${d.ip}`;
  }catch(e){ document.getElementById('live').textContent='(no response)'; }
}
setInterval(tick,500); tick();
</script>
</body></html>
)HTML";
