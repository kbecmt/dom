const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');

assert.match(html, /<link rel="stylesheet" href="style\.css">/);
assert.match(html, /<script src="app\.js\?v=20260724-google-webapp"><\/script>/);
assert.match(html, /typeof initApp === 'function'\) initApp\(\)/);
for (const id of [
  'lastEntryAgeTop',
  'googleLastStatus',
  'googleLastTime',
  'googleLastBufferTop',
  'googleLastBufferBottom',
  'googleLastCollector',
  'googleLastWaterTop',
  'googleLastWaterBottom',
  'googleLastHouse',
  'googleLastMixer',
  'googleLastReturn',
  'googleLastOutdoor',
  'googleLastSummary',
  'googleLastHealth',
  'googleAllData',
  'googleLastSnapshot',
  'btnGoogleRefresh'
]) {
  assert.match(html, new RegExp(`id="${id}"`));
}

console.log('index static tests ok');
