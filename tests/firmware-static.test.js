const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const firmware = fs.readFileSync(path.join(__dirname, '..', 'solary.ino'), 'utf8');

for (const marker of [
  'solarTempsOk',
  'bufferTempsOk',
  'coTempsOk',
  'updateTempHealth();',
  'stopSolarPump("błąd czujnika")',
  'stopBufferCircuit("błąd czujnika")',
  'stopCO("błąd czujnika")'
]) {
  assert.ok(firmware.includes(marker), `missing firmware fail-safe marker: ${marker}`);
}

assert.match(firmware, /WIFI_USE_STATIC_IP true/);
assert.match(firmware, /local_IP\(192,\s*168,\s*1,\s*139\)/);
assert.match(firmware, /#include <HTTPClient\.h>/);
assert.match(firmware, /#include <WiFiClientSecure\.h>/);
assert.match(firmware, /GOOGLE_FORM_URL "https:\/\/docs\.google\.com\/forms\/u\/0\/d\/e\/1FAIpQLSeMyHb_K9o5BwSu5TI9O8MQ973W9DqwT4RfNv4NN-t1LpUDQg\/formResponse"/);
assert.match(firmware, /GOOGLE_FORM_ENTRY_SNAPSHOT_JSON "entry\.1561554265"/);
assert.match(firmware, /GOOGLE_FORM_ENTRY_SUMMARY "entry\.2118651019"/);
assert.match(firmware, /GOOGLE_FORM_ENTRY_HEALTH "entry\.607098449"/);
assert.match(firmware, /GOOGLE_FORM_LOG_INTERVAL_MS 600000/);
assert.match(firmware, /handleGoogleFormLogging\(\);/);
assert.match(firmware, /buildGoogleFormSnapshotJson\(\)/);
for (const snapshotMarker of [
  '"temps"',
  '"settings"',
  '"state"',
  '"automation"',
  '"health"',
  '"wifi"',
  'solar.bufferTopTemp',
  'solar.collectorTemp',
  'solar.coMaxMixerTemp',
  'solar.mixerPercent',
  'automationState.autoCoEnabled',
  'WiFi.localIP().toString()'
]) {
  assert.ok(firmware.includes(snapshotMarker), `missing snapshot marker: ${snapshotMarker}`);
}
assert.match(firmware, /urlEncode\(snapshot\)/);
assert.match(firmware, /http\.POST\(body\)/);
assert.match(firmware, /client\.setInsecure\(\)/);
assert.doesNotMatch(firmware, /#include <WebServer\.h>/);
assert.doesNotMatch(firmware, /WebServer server/);
assert.doesNotMatch(firmware, /server\.handleClient/);
assert.doesNotMatch(firmware, /server\.on/);
assert.doesNotMatch(firmware, /\/api\//);

console.log('firmware static tests ok');
