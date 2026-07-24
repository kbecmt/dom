const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {
  DEFAULT_GOOGLE_CSV_URL,
  GOOGLE_DATA_POLL_INTERVAL_MS,
  buildGoogleCsvProxyUrl,
  getLastDataRow,
  snapshotFromGoogleRow,
  hasAllGoogleTemps,
  googleSnapshotToAppData,
  googleSectionLabel,
  googleFieldLabel,
  formatSnapshotValue,
  formatGoogleTemp,
  parseCsv,
  parseGoogleTimestamp,
  formatElapsedSince,
  buildSettingsPayload,
  isDeltaValid
} = require('../app.js');

const appSource = fs.readFileSync(path.join(__dirname, '..', 'app.js'), 'utf8');

assert.equal(isDeltaValid(8, 4), true);
assert.equal(isDeltaValid(4, 4), false);
assert.equal(isDeltaValid(3, 4), false);
assert.equal(isDeltaValid(Number.NaN, 1), false);

assert.match(DEFAULT_GOOGLE_CSV_URL, /2PACX-1vTIo-0UREaUUsQabhvwHKmc9aE2vw-BZrLc5sER3FumTxucXr35FQ4Q-y-fnu6b8gBbCz2ieFgFKaHe\/pub\?output=csv/);
assert.equal(GOOGLE_DATA_POLL_INTERVAL_MS, 30000);
assert.equal(
  buildGoogleCsvProxyUrl('https://example.com/a?b=1'),
  'https://api.allorigins.win/raw?url=https%3A%2F%2Fexample.com%2Fa%3Fb%3D1'
);

const rows = parseCsv([
  'czas,json,podsumowanie,zdrowie',
  '',
  '2026-07-24,"{""temps"":{""bufferTop"":42,""bufferBottom"":31.5,""collector"":73.1,""waterTop"":59,""waterBottom"":44.5,""house"":21,""mixer"":33,""return"":29,""outdoor"":7}}",summary,OK',
  '2026-07-24,"{""temps"":{""collector"":74,""waterTop"":60,""waterBottom"":45}}",short,SHORT'
].join('\n'));
const currentDataRows = parseCsv([
  'Current data JSON',
  '"{""ok"":true,""updatedAt"":""2026-07-24T14:51:17.000Z"",""summary"":""current"",""health"":""OK"",""snapshot"":{""temps"":{""bufferTop"":42,""bufferBottom"":31.5,""collector"":73.1,""waterTop"":59,""waterBottom"":44.5,""house"":21,""mixer"":33,""return"":29,""outdoor"":7}}}"'
].join('\n'));
assert.deepEqual(getLastDataRow(rows), rows[2]);
assert.deepEqual(getLastDataRow(currentDataRows), currentDataRows[1]);
assert.equal(snapshotFromGoogleRow(rows[2]).temps.collector, 73.1);
assert.equal(snapshotFromGoogleRow(currentDataRows[1]).temps.collector, 73.1);
assert.equal(hasAllGoogleTemps(snapshotFromGoogleRow(rows[2])), true);
assert.equal(hasAllGoogleTemps(snapshotFromGoogleRow(currentDataRows[1])), true);
assert.equal(hasAllGoogleTemps(snapshotFromGoogleRow(rows[3])), false);
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
assert.equal(googleSectionLabel('system'), 'System');
assert.equal(googleSectionLabel('runtime'), 'Runtime');
assert.equal(googleFieldLabel('coTargetTemp'), 'CO temp. zadana');
assert.equal(googleFieldLabel('googleFormLogPending'), 'Zapis Google oczekuje');
assert.equal(formatSnapshotValue(true), 'Tak');
assert.equal(formatSnapshotValue(21.55), '21.6');
assert.equal(parseGoogleTimestamp('2026-07-24 14:51:17').getFullYear(), 2026);
assert.equal(formatElapsedSince(new Date('2026-07-24T14:50:17'), new Date('2026-07-24T14:51:17')), '1min temu');
assert.equal(formatElapsedSince(new Date('2026-07-24T12:20:17'), new Date('2026-07-24T14:51:17')), '2h 31min temu');
assert.deepEqual(buildSettingsPayload({
  maxWaterTemp: '70',
  solarDeltaOn: '8',
  solarDeltaOff: '4',
  maxBufferTemp: '85',
  wodaBuforDeltaOn: '6',
  wodaBuforDeltaOff: '3',
  buforWodaDeltaOn: '7',
  buforWodaDeltaOff: '3',
  minWodaTemp: '35',
  coMaxMixerTemp: '45',
  coTargetTemp: '22',
  coDeltaOn: '0.5',
  coDeltaOff: '0.2',
  autoCoEnabled: true,
  autoSolarEnabled: false,
  autoWbEnabled: true,
  autoBwEnabled: true
}, { autoCoEnabled: false }), {
  maxWaterTemp: 70,
  solarDeltaOn: 8,
  solarDeltaOff: 4,
  maxBufferTemp: 85,
  wodaBuforDeltaOn: 6,
  wodaBuforDeltaOff: 3,
  buforWodaDeltaOn: 7,
  buforWodaDeltaOff: 3,
  minWodaTemp: 35,
  coMaxMixerTemp: 45,
  coTargetTemp: 22,
  coDeltaOn: 0.5,
  coDeltaOff: 0.2,
  autoCoEnabled: false,
  autoSolarEnabled: false,
  autoWbEnabled: true,
  autoBwEnabled: true
});

assert.doesNotMatch(appSource, /fetch\(`\$\{API_BASE\}\/api\//);
assert.doesNotMatch(appSource, /\/api\/data/);
assert.match(appSource, /function renderGoogleAllData\(snapshot\)/);
assert.match(appSource, /GOOGLE_SETTINGS_WEB_APP_URL/);
assert.match(appSource, /function sendSettingsToGoogle\(scope, settings\)/);

console.log('app tests ok');
