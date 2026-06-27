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
assert.match(firmware, /Access-Control-Allow-Methods/);
assert.match(firmware, /Access-Control-Allow-Headers/);
assert.match(firmware, /Nieprawidłowe nastawy solarów/);
assert.match(firmware, /Nieprawidłowe nastawy bufora/);
assert.match(firmware, /Nieprawidłowe nastawy CO/);
assert.match(firmware, /system == nullptr/);
assert.match(firmware, /changed\) saveSettings/);

console.log('firmware static tests ok');
