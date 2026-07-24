const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {
  DEFAULT_GOOGLE_CSV_URL,
  buildGoogleCsvProxyUrl,
  getLastDataRow,
  snapshotFromGoogleRow,
  googleSnapshotToAppData,
  googleSectionLabel,
  googleFieldLabel,
  formatSnapshotValue,
  formatGoogleTemp,
  parseCsv,
  isDeltaValid
} = require('../app.js');

const appSource = fs.readFileSync(path.join(__dirname, '..', 'app.js'), 'utf8');

assert.equal(isDeltaValid(8, 4), true);
assert.equal(isDeltaValid(4, 4), false);
assert.equal(isDeltaValid(3, 4), false);
assert.equal(isDeltaValid(Number.NaN, 1), false);

assert.match(DEFAULT_GOOGLE_CSV_URL, /2PACX-1vTIo-0UREaUUsQabhvwHKmc9aE2vw-BZrLc5sER3FumTxucXr35FQ4Q-y-fnu6b8gBbCz2ieFgFKaHe\/pub\?output=csv/);
assert.equal(
  buildGoogleCsvProxyUrl('https://example.com/a?b=1'),
  'https://api.allorigins.win/raw?url=https%3A%2F%2Fexample.com%2Fa%3Fb%3D1'
);

const rows = parseCsv([
  'czas,json,podsumowanie,zdrowie',
  '',
  '2026-07-24,"{""temps"":{""collector"":73.1,""waterTop"":59,""waterBottom"":44.5}}",summary,OK'
].join('\n'));
assert.deepEqual(getLastDataRow(rows), rows[2]);
assert.equal(snapshotFromGoogleRow(rows[2]).temps.collector, 73.1);
assert.equal(formatGoogleTemp('73,125'), '73.1');

const appData = googleSnapshotToAppData({
  temps: { collector: 73.1, waterTop: 59, waterBottom: 44.5, bufferTop: 42, house: 21, mixer: 33, return: 29, outdoor: 7 },
  settings: { solarDeltaOn: 9, coTargetTemp: 21.5 },
  state: { solarPumpActive: true, coPumpActive: false, mixerPercent: 12, direction: 'woda->bufor' },
  automation: { autoCoEnabled: false }
});
assert.equal(appData.solar.collector, 73.1);
assert.equal(appData.solar.deltaOn, 9);
assert.equal(appData.buffer.direction, 'woda->bufor');
assert.equal(appData.co.targetTemp, 21.5);
assert.equal(appData.co.autoCoEnabled, false);
assert.equal(appData.outdoorTemp, 7);
assert.equal(googleSectionLabel('settings'), 'Nastawy');
assert.equal(googleFieldLabel('coTargetTemp'), 'CO temp. zadana');
assert.equal(formatSnapshotValue(true), 'Tak');
assert.equal(formatSnapshotValue(21.55), '21.6');

assert.doesNotMatch(appSource, /fetch\(`\$\{API_BASE\}\/api\//);
assert.doesNotMatch(appSource, /\/api\/data/);
assert.match(appSource, /function renderGoogleAllData\(snapshot\)/);

console.log('app tests ok');
