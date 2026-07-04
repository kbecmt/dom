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

for (const route of [
  '/api/data',
  '/api/solar/settings',
  '/api/buffer/settings',
  '/api/co/settings',
  '/api/automation/control',
  '/api/mixer/control',
  '/api/relay/control'
]) {
  assert.ok(firmware.includes(route), `missing route: ${route}`);
}

assert.match(firmware, /HTTP_OPTIONS/);
assert.match(firmware, /WIFI_USE_STATIC_IP true/);
assert.match(firmware, /local_IP\(192,\s*168,\s*1,\s*139\)/);
assert.match(firmware, /raw\.githubusercontent\.com\/kbecmt\/dom/);
assert.match(firmware, /kbecmt\.github\.io\/dom\/style\.css/);
assert.match(firmware, /kbecmt\.github\.io\/dom\/app\.js/);
assert.match(firmware, /Access-Control-Allow-Methods/);
assert.match(firmware, /Access-Control-Allow-Headers/);
assert.match(firmware, /Nieprawidłowe nastawy solarów/);
assert.match(firmware, /Nieprawidłowe nastawy bufora/);
assert.match(firmware, /Nieprawidłowe nastawy CO/);
assert.match(firmware, /system == nullptr/);
assert.match(firmware, /changed\) saveSettings/);

console.log('firmware static tests ok');
