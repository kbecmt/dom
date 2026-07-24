const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname, '..', 'google-form-create.gs'), 'utf8');

for (const marker of [
  'function doPost(e)',
  'function doGet(e)',
  'SOLARY_SETTINGS_SHEET_NAME',
  'Ustawienia',
  'SOLARY_SPREADSHEET_ID',
  'SOLARY_CURRENT_SETTINGS',
  'validateSettings_',
  'maxWaterTemp',
  'solarDeltaOn',
  'solarDeltaOff',
  'autoCoEnabled',
  'ContentService.MimeType.JSON'
]) {
  assert.ok(source.includes(marker), `missing Google Apps Script marker: ${marker}`);
}

assert.match(source, /solarDeltaOff[\s\S]+solarDeltaOn/);
assert.match(source, /wodaBuforDeltaOff[\s\S]+wodaBuforDeltaOn/);
assert.match(source, /buforWodaDeltaOff[\s\S]+buforWodaDeltaOn/);
assert.match(source, /coDeltaOff[\s\S]+coDeltaOn/);

console.log('google form create static tests ok');
