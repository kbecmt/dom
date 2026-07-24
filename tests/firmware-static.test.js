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

assert.doesNotMatch(firmware, /WIFI_USE_STATIC_IP/);
assert.doesNotMatch(firmware, /WiFi\.config/);
assert.doesNotMatch(firmware, /WiFi\.mode/);
assert.doesNotMatch(firmware, /WiFi\.mode\s*\([^)]*WIFI_STA/);
assert.doesNotMatch(firmware, /local_IP\(192,\s*168,\s*1,\s*139\)/);
assert.match(firmware, /WIFI_RETRY_INTERVAL_MS 30000/);
assert.match(firmware, /WIFI_SCAN_INTERVAL_MS 120000/);
assert.match(firmware, /WIFI_SCAN_RETRY_COUNT 3/);
assert.match(firmware, /WIFI_SCAN_RETRY_DELAY_MS 1000/);
assert.match(firmware, /WIFI_VERBOSE_SCAN_LOGS 0/);
assert.match(firmware, /wifiStatusName\(wl_status_t status\)/);
assert.match(firmware, /wifiAuthModeName\(wifi_auth_mode_t authMode\)/);
assert.match(firmware, /formatEspEfuseMac\(\)/);
assert.match(firmware, /logWiFiScanResults\(bool forceScan\)/);
assert.match(firmware, /WL_DISCONNECTED/);
assert.match(firmware, /WiFi\.disconnect\(false, false\)/);
assert.match(firmware, /WiFi\.setSleep\(false\)/);
assert.match(firmware, /WiFi\.macAddress\(\)/);
assert.match(firmware, /ESP32 eFuse MAC/);
assert.match(firmware, /ESP\.getEfuseMac\(\)/);
assert.match(firmware, /logWiFiScanResults\(true\)/);
assert.match(firmware, /WiFi: retry #%u/);
assert.match(firmware, /ESP\.getFreeHeap\(\)/);
assert.match(firmware, /WiFi\.scanNetworks\(false, true\)/);
assert.match(firmware, /scanAttempt <= WIFI_SCAN_RETRY_COUNT/);
assert.match(firmware, /delay\(WIFI_SCAN_RETRY_DELAY_MS\)/);
assert.match(firmware, /po %d próbach/);
assert.doesNotMatch(firmware, /pełny skan za %lus/);
assert.match(firmware, /ESP32 widzi tylko sieci 2\.4 GHz/);
assert.match(firmware, /auth=%s, BSSID=%s/);
assert.match(firmware, /<ukryta>/);
assert.match(firmware, /cel WIDOCZNY/);
assert.match(firmware, /cel NIEWIDOCZNY/);
assert.match(firmware, /WiFi OK: IP=%s, RSSI=%d dBm/);
assert.match(firmware, /#include <HTTPClient\.h>/);
assert.match(firmware, /#include <WiFiClientSecure\.h>/);
assert.match(firmware, /#include <ArduinoJson\.h>/);
assert.match(firmware, /GOOGLE_WEB_APP_URL "https:\/\/script\.google\.com\/macros\/s\/[^"]+\/exec"/);
assert.match(firmware, /GOOGLE_DATA_URL GOOGLE_WEB_APP_URL/);
assert.match(firmware, /GOOGLE_FORM_LOG_INTERVAL_MS 30000/);
assert.match(firmware, /GOOGLE_SETTINGS_URL GOOGLE_WEB_APP_URL "\?type=settings"/);
assert.match(firmware, /GOOGLE_SETTINGS_FETCH_INTERVAL_MS 60000/);
assert.match(firmware, /handleGoogleFormLogging\(\);/);
assert.match(firmware, /handleGoogleSettings\(\);/);
assert.match(firmware, /buildGoogleFormSnapshotJson\(\)/);
for (const snapshotMarker of [
  '"temps"',
  '"runtime"',
  '"settings"',
  '"state"',
  '"automation"',
  '"health"',
  '"wifi"',
  'solar.bufferTopTemp',
  'solar.collectorTemp',
  'solar.coMaxMixerTemp',
  'solar.mixerPercent',
  'solar.valveActionPending',
  'solar.valveActionEndTime',
  'solar.coCheckTimer',
  'googleFormLogPending',
  'GOOGLE_FORM_LOG_INTERVAL_MS',
  'automationState.autoCoEnabled',
  'WiFi.localIP().toString()'
]) {
  assert.ok(firmware.includes(snapshotMarker), `missing snapshot marker: ${snapshotMarker}`);
}

for (const tempMarker of [
  'addJsonFloat(json, "bufferTop", solar.bufferTopTemp',
  'addJsonFloat(json, "bufferBottom", solar.bufferBottomTemp',
  'addJsonFloat(json, "collector", solar.collectorTemp',
  'addJsonFloat(json, "waterTop", solar.waterTopTemp',
  'addJsonFloat(json, "waterBottom", solar.waterBottomTemp',
  'addJsonFloat(json, "house", solar.houseTemp',
  'addJsonFloat(json, "mixer", solar.mixerTemp',
  'addJsonFloat(json, "return", solar.returnTemp',
  'addJsonFloat(json, "outdoor", solar.outdoorTemp'
]) {
  assert.ok(firmware.includes(tempMarker), `missing Google data temp field: ${tempMarker}`);
}

for (const appTempKey of [
  'bufferTop',
  'bufferBottom',
  'collector',
  'waterTop',
  'waterBottom',
  'house',
  'mixer',
  'return',
  'outdoor'
]) {
  assert.match(firmware, new RegExp(`"${appTempKey}"`));
}
assert.match(firmware, /urlEncode\(snapshot\)/);
assert.match(firmware, /type=data&snapshot=/);
assert.match(firmware, /http\.POST\(body\)/);
assert.match(firmware, /http\.setFollowRedirects\(HTTPC_STRICT_FOLLOW_REDIRECTS\)/);
assert.match(firmware, /Google Data: POST %s/);
assert.match(firmware, /snapshot=%uB, summary=%uB, health=%uB/);
assert.match(firmware, /Google Data: WiFi RSSI=%d dBm, IP=%s/);
assert.match(firmware, /wysyłam body=%uB/);
assert.match(firmware, /odpowiedź błędu/);
assert.match(firmware, /client\.setInsecure\(\)/);
for (const settingsMarker of [
  'fetchSettingsFromGoogle()',
  'applyGoogleSettingsJson',
  'deserializeJson(doc, payload)',
  'JsonObject settings = doc["settings"]',
  'readJsonFloat(settings, "maxWaterTemp"',
  'readJsonFloat(settings, "coTargetTemp"',
  'automationState.autoCoEnabled = settings["autoCoEnabled"]',
  'saveSettings();',
  'lastGoogleSettingsUpdatedAt'
]) {
  assert.ok(firmware.includes(settingsMarker), `missing Google settings marker: ${settingsMarker}`);
}
assert.doesNotMatch(firmware, /#include <WebServer\.h>/);
assert.doesNotMatch(firmware, /WebServer server/);
assert.doesNotMatch(firmware, /server\.handleClient/);
assert.doesNotMatch(firmware, /server\.on/);
assert.doesNotMatch(firmware, /\/api\//);

console.log('firmware static tests ok');
