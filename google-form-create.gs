/**
 * Uruchom w Google Apps Script:
 * 1. Wejdz na https://script.google.com/
 * 2. Nowy projekt
 * 3. Wklej ten plik
 * 4. Uruchom createSolaryGoogleForm()
 *
 * Skrypt obsluguje dwa tryby pracy:
 * - type=data: nadpisuje aktualny snapshot w arkuszu "Aktualny stan", komorka A2
 * - type=settings: nadpisuje aktualne ustawienia w arkuszu "Ustawienia", komorka A2
 *
 * Google Form moze zostac tylko do starej kompatybilnosci. Nowy zapis ESP nie dopisuje odpowiedzi formularza.
 */
var SOLARY_DATA_SHEET_NAME = 'Aktualny stan';
var SOLARY_SETTINGS_SHEET_NAME = 'Ustawienia';

function createSolaryGoogleForm() {
  var form = FormApp.create('Solary ESP32 - zapis danych');
  form.setDescription(
    'Formularz do automatycznego zapisu danych z ESP32. ' +
    'Pole Snapshot JSON zawiera wszystkie temperatury, nastawy, stany, automatyke, diagnostyke i WiFi.'
  );
  form.setCollectEmail(false);
  form.setAllowResponseEdits(false);
  form.setLimitOneResponsePerUser(false);

  var snapshotItem = form.addParagraphTextItem()
    .setTitle('Snapshot JSON')
    .setHelpText([
      'Pelny JSON z ESP32.',
      'Wymagane sekcje:',
      'runtime: uptimeMs, tempState, conversionStartTime, googleFormLogIntervalMs, lastGoogleFormLog, googleFormLogPending, valveActionPending, valveActionEndTime, mixerTimer, mixerStepEnd, mixerResetEndTime, coCheckTimer',
      'temps: bufferTop, bufferBottom, collector, waterTop, waterBottom, house, mixer, return, outdoor',
      'settings: maxWaterTemp, solarDeltaOn, solarDeltaOff, maxBufferTemp, wodaBuforDeltaOn, wodaBuforDeltaOff, buforWodaDeltaOn, buforWodaDeltaOff, minWodaTemp, coMaxMixerTemp, coTargetTemp, coDeltaOn, coDeltaOff',
      'state: solarPumpActive, bufferPumpActive, valveOpen, direction, coPumpActive, mixerPercent, mixerRunning, mixerDirection, coPhase, isMixerResetting',
      'automation: autoCoEnabled, autoSolarEnabled, autoWbEnabled, autoBwEnabled',
      'health: solarTempsOk, bufferTempsOk, coTempsOk, anyTempError',
      'wifi: ip, rssi'
    ].join('\n'))
    .setRequired(true);

  var summaryItem = form.addParagraphTextItem()
    .setTitle('Podsumowanie')
    .setHelpText('Krotki tekst do szybkiego podgladu w arkuszu.')
    .setRequired(false);

  var healthItem = form.addTextItem()
    .setTitle('Zdrowie')
    .setHelpText('Np. solar_ok,buffer_ok,co_ok albo opis bledu.')
    .setRequired(false);

  var spreadsheet = SpreadsheetApp.create('Solary ESP32 - odpowiedzi');
  form.setDestination(FormApp.DestinationType.SPREADSHEET, spreadsheet.getId());
  PropertiesService.getScriptProperties().setProperty('SOLARY_SPREADSHEET_ID', spreadsheet.getId());
  ensureCurrentDataSheet_(spreadsheet);
  ensureSettingsSheet_(spreadsheet);

  var entryIds = getEntryIdsFromPrefill_(form, [
    { item: snapshotItem, marker: 'SNAPSHOT_JSON_MARKER' },
    { item: summaryItem, marker: 'SUMMARY_MARKER' },
    { item: healthItem, marker: 'HEALTH_MARKER' }
  ]);

  Logger.log('FORM_EDIT_URL: ' + form.getEditUrl());
  Logger.log('FORM_PUBLIC_URL: ' + form.getPublishedUrl());
  Logger.log('FORM_RESPONSE_URL: ' + form.getPublishedUrl().replace('/viewform', '/formResponse'));
  Logger.log('SPREADSHEET_ID: ' + spreadsheet.getId());
  Logger.log('GOOGLE_FORM_ENTRY_SNAPSHOT_JSON: ' + entryIds.SNAPSHOT_JSON_MARKER);
  Logger.log('GOOGLE_FORM_ENTRY_SUMMARY: ' + entryIds.SUMMARY_MARKER);
  Logger.log('GOOGLE_FORM_ENTRY_HEALTH: ' + entryIds.HEALTH_MARKER);
  Logger.log('Wklej powyzszy FORM_RESPONSE_URL i entry.* do solary.ino.');

  return {
    editUrl: form.getEditUrl(),
    publicUrl: form.getPublishedUrl(),
    responseUrl: form.getPublishedUrl().replace('/viewform', '/formResponse'),
    entrySnapshotJson: entryIds.SNAPSHOT_JSON_MARKER,
    entrySummary: entryIds.SUMMARY_MARKER,
    entryHealth: entryIds.HEALTH_MARKER,
    spreadsheetId: spreadsheet.getId()
  };
}

function doPost(e) {
  var type = e && e.parameter && e.parameter.type ? e.parameter.type : '';
  if (type === 'data') return saveCurrentData_(e);

  var payload = parseSettingsPayload_(e);
  var settings = validateSettings_(payload.settings);
  var spreadsheet = getSolarySpreadsheet_();
  var sheet = ensureSettingsSheet_(spreadsheet);
  var now = new Date();
  var current = {
    ok: true,
    updatedAt: now.toISOString(),
    scope: payload.scope || 'all',
    source: payload.source || 'index.html',
    settings: settings
  };

  sheet.getRange('A2').setValue(JSON.stringify(current));
  PropertiesService.getScriptProperties().setProperty('SOLARY_CURRENT_SETTINGS', JSON.stringify(current));

  return jsonOutput_({
    ok: true,
    updatedAt: current.updatedAt,
    settings: settings
  });
}

function doGet(e) {
  var type = e && e.parameter && e.parameter.type ? e.parameter.type : 'settings';
  if (type === 'data') return jsonOrJsonpOutput_(getCurrentData_(), e);

  var current = getCurrentSettings_();
  return jsonOrJsonpOutput_(current, e);
}

function saveSolaryTestCurrentData() {
  var snapshot = {
    uptimeMs: 987654,
    runtime: {
      uptimeMs: 987654,
      tempState: 0,
      conversionStartTime: 0,
      googleFormLogIntervalMs: 600000,
      lastGoogleFormLog: 987654,
      googleFormLogPending: false,
      valveActionPending: false,
      valveActionEndTime: 0,
      mixerTimer: 0,
      mixerStepEnd: 0,
      mixerResetEndTime: 0,
      coCheckTimer: 0
    },
    temps: {
      bufferTop: 48.7,
      bufferBottom: 37.4,
      collector: 76.2,
      waterTop: 58.9,
      waterBottom: 45.6,
      house: 21.8,
      mixer: 34.2,
      return: 29.7,
      outdoor: 12.4
    },
    settings: {
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
      coDeltaOff: 0.2
    },
    state: {
      solarPumpActive: true,
      bufferPumpActive: false,
      valveOpen: true,
      direction: 'bufor_woda',
      coPumpActive: false,
      mixerPercent: 42,
      mixerRunning: false,
      mixerDirection: 'close',
      coPhase: 'idle',
      isMixerResetting: false
    },
    automation: {
      autoCoEnabled: true,
      autoSolarEnabled: true,
      autoWbEnabled: true,
      autoBwEnabled: true
    },
    health: {
      solarTempsOk: true,
      bufferTempsOk: true,
      coTempsOk: true,
      anyTempError: false
    },
    wifi: {
      ip: '192.168.1.139',
      rssi: -55
    }
  };

  saveCurrentData_({
    parameter: {
      snapshot: JSON.stringify(snapshot),
      summary: 'TEST pelny wpis: 9 temperatur, nastawy, stany, automatyka, zdrowie, wifi',
      health: 'TEST_OK_FULL'
    }
  });
}

function getEntryIdsFromPrefill_(form, fields) {
  var response = form.createResponse();
  fields.forEach(function(field) {
    response.withItemResponse(field.item.createResponse(field.marker));
  });

  var prefilledUrl = response.toPrefilledUrl();
  var result = {};
  fields.forEach(function(field) {
    var escapedMarker = encodeURIComponent(field.marker);
    var pattern = new RegExp('(entry\\.[0-9]+)=' + escapedMarker);
    var match = prefilledUrl.match(pattern);
    result[field.marker] = match ? match[1] : 'NIE_ZNALEZIONO';
  });
  return result;
}

function parseSettingsPayload_(e) {
  if (!e) throw new Error('Brak danych POST.');

  var scope = e.parameter && e.parameter.scope ? e.parameter.scope : 'all';
  var source = e.parameter && e.parameter.source ? e.parameter.source : 'index.html';
  var settingsText = e.parameter && e.parameter.settings ? e.parameter.settings : '';

  if (!settingsText && e.postData && e.postData.contents) {
    var body = e.postData.contents;
    try {
      var parsed = JSON.parse(body);
      settingsText = parsed.settings ? JSON.stringify(parsed.settings) : body;
      scope = parsed.scope || scope;
      source = parsed.source || source;
    } catch (err) {
      settingsText = body;
    }
  }

  if (!settingsText) throw new Error('Brak pola settings.');
  return {
    scope: scope,
    source: source,
    settings: JSON.parse(settingsText)
  };
}

function saveCurrentData_(e) {
  var snapshotText = e.parameter && e.parameter.snapshot ? e.parameter.snapshot : '';
  var summary = e.parameter && e.parameter.summary ? e.parameter.summary : '';
  var health = e.parameter && e.parameter.health ? e.parameter.health : '';

  if (!snapshotText && e.postData && e.postData.contents) {
    var body = e.postData.contents;
    try {
      var parsedBody = JSON.parse(body);
      snapshotText = parsedBody.snapshot ? JSON.stringify(parsedBody.snapshot) : body;
      summary = parsedBody.summary || summary;
      health = parsedBody.health || health;
    } catch (err) {
      snapshotText = body;
    }
  }

  if (!snapshotText) throw new Error('Brak pola snapshot.');
  var snapshot = typeof snapshotText === 'string' ? JSON.parse(snapshotText) : snapshotText;
  var now = new Date();
  var current = {
    ok: true,
    updatedAt: now.toISOString(),
    summary: summary,
    health: health,
    snapshot: snapshot
  };

  var spreadsheet = getSolarySpreadsheet_();
  var sheet = ensureCurrentDataSheet_(spreadsheet);
  sheet.getRange('A2').setValue(JSON.stringify(current));
  PropertiesService.getScriptProperties().setProperty('SOLARY_CURRENT_DATA', JSON.stringify(current));

  return jsonOutput_({
    ok: true,
    updatedAt: current.updatedAt
  });
}

function validateSettings_(settings) {
  var result = {
    maxWaterTemp: readNumber_(settings, 'maxWaterTemp', 20, 99),
    solarDeltaOn: readNumber_(settings, 'solarDeltaOn', 1, 30),
    solarDeltaOff: readNumber_(settings, 'solarDeltaOff', 0.5, 20),
    maxBufferTemp: readNumber_(settings, 'maxBufferTemp', 20, 99),
    wodaBuforDeltaOn: readNumber_(settings, 'wodaBuforDeltaOn', 1, 30),
    wodaBuforDeltaOff: readNumber_(settings, 'wodaBuforDeltaOff', 0.5, 20),
    buforWodaDeltaOn: readNumber_(settings, 'buforWodaDeltaOn', 1, 30),
    buforWodaDeltaOff: readNumber_(settings, 'buforWodaDeltaOff', 0.5, 20),
    minWodaTemp: readNumber_(settings, 'minWodaTemp', 20, 80),
    coMaxMixerTemp: readNumber_(settings, 'coMaxMixerTemp', 20, 80),
    coTargetTemp: readNumber_(settings, 'coTargetTemp', 5, 35),
    coDeltaOn: readNumber_(settings, 'coDeltaOn', 0.1, 20),
    coDeltaOff: readNumber_(settings, 'coDeltaOff', 0.1, 20),
    autoCoEnabled: settings.autoCoEnabled !== false,
    autoSolarEnabled: settings.autoSolarEnabled !== false,
    autoWbEnabled: settings.autoWbEnabled !== false,
    autoBwEnabled: settings.autoBwEnabled !== false
  };

  if (result.solarDeltaOff >= result.solarDeltaOn) throw new Error('solarDeltaOff musi byc mniejsze niz solarDeltaOn.');
  if (result.wodaBuforDeltaOff >= result.wodaBuforDeltaOn) throw new Error('wodaBuforDeltaOff musi byc mniejsze niz wodaBuforDeltaOn.');
  if (result.buforWodaDeltaOff >= result.buforWodaDeltaOn) throw new Error('buforWodaDeltaOff musi byc mniejsze niz buforWodaDeltaOn.');
  if (result.coDeltaOff >= result.coDeltaOn) throw new Error('coDeltaOff musi byc mniejsze niz coDeltaOn.');
  return result;
}

function readNumber_(settings, key, min, max) {
  var value = Number(settings[key]);
  if (!isFinite(value) || value < min || value > max) {
    throw new Error(key + ' musi byc liczba od ' + min + ' do ' + max + '.');
  }
  return value;
}

function getCurrentSettings_() {
  var fromProps = PropertiesService.getScriptProperties().getProperty('SOLARY_CURRENT_SETTINGS');
  if (fromProps) return JSON.parse(fromProps);

  var sheet = ensureSettingsSheet_(getSolarySpreadsheet_());
  var value = sheet.getRange('A2').getValue();
  if (!value) return { ok: false, message: 'Brak zapisanych ustawien.' };

  return JSON.parse(value);
}

function getCurrentData_() {
  var fromProps = PropertiesService.getScriptProperties().getProperty('SOLARY_CURRENT_DATA');
  if (fromProps) return JSON.parse(fromProps);

  var sheet = ensureCurrentDataSheet_(getSolarySpreadsheet_());
  var value = sheet.getRange('A2').getValue();
  if (!value) return { ok: false, message: 'Brak zapisanego stanu.' };

  return JSON.parse(value);
}

function getSolarySpreadsheet_() {
  var id = PropertiesService.getScriptProperties().getProperty('SOLARY_SPREADSHEET_ID');
  if (!id) {
    var spreadsheet = SpreadsheetApp.create('Solary ESP32 - odpowiedzi');
    PropertiesService.getScriptProperties().setProperty('SOLARY_SPREADSHEET_ID', spreadsheet.getId());
    return spreadsheet;
  }
  return SpreadsheetApp.openById(id);
}

function ensureSettingsSheet_(spreadsheet) {
  var sheet = spreadsheet.getSheetByName(SOLARY_SETTINGS_SHEET_NAME);
  if (!sheet) sheet = spreadsheet.insertSheet(SOLARY_SETTINGS_SHEET_NAME);
  if (sheet.getLastRow() === 0) {
    sheet.getRange('A1').setValue('Current settings JSON');
  }
  return sheet;
}

function ensureCurrentDataSheet_(spreadsheet) {
  var sheet = spreadsheet.getSheetByName(SOLARY_DATA_SHEET_NAME);
  if (!sheet) sheet = spreadsheet.insertSheet(SOLARY_DATA_SHEET_NAME);
  spreadsheet.setActiveSheet(sheet);
  spreadsheet.moveActiveSheet(1);
  if (sheet.getLastRow() === 0) {
    sheet.getRange('A1').setValue('Current data JSON');
  }
  return sheet;
}

function jsonOutput_(value) {
  return ContentService
    .createTextOutput(JSON.stringify(value))
    .setMimeType(ContentService.MimeType.JSON);
}

function jsonOrJsonpOutput_(value, e) {
  var callback = e && e.parameter ? e.parameter.callback : '';
  if (callback) {
    return ContentService
      .createTextOutput(callback + '(' + JSON.stringify(value) + ');')
      .setMimeType(ContentService.MimeType.JAVASCRIPT);
  }
  return jsonOutput_(value);
}
