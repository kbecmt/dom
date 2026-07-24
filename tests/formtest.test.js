const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const html = fs.readFileSync(path.join(__dirname, '..', 'formtest.html'), 'utf8');

assert.match(html, /Publiczny CSV z arkusza odpowiedzi Google/);
assert.match(html, /solarGoogleCsvUrl/);
assert.match(html, /2PACX-1vTIo-0UREaUUsQabhvwHKmc9aE2vw-BZrLc5sER3FumTxucXr35FQ4Q-y-fnu6b8gBbCz2ieFgFKaHe\/pub\?output=csv/);
assert.match(html, /fetch\(url, \{ cache: 'no-store' \}\)/);
assert.match(html, /loadCsvByProxy\(url\)/);
assert.match(html, /function loadCsvByProxy\(sourceUrl\)/);
assert.match(html, /api\.allorigins\.win\/raw\?url=/);
assert.match(html, /loadSheetByJsonp\(url\)/);
assert.match(html, /function buildGvizUrl\(sourceUrl\)/);
assert.match(html, /\/gviz\/tq/);
assert.match(html, /tqx=responseHandler:/);
assert.match(html, /function gvizResponseToRows\(response\)/);
assert.match(html, /function parseCsv\(text\)/);
assert.match(html, /function snapshotFromRow\(row\)/);
assert.match(html, /JSON\.parse\(raw\)/);
assert.match(html, /JSON\.stringify\(snapshot, null, 2\)/);
assert.match(html, /Dane wczytane z Google Sheets/);
assert.doesNotMatch(html, /formResponse/);
assert.doesNotMatch(html, /method:\s*['"]POST['"]/);

console.log('formtest static tests ok');
