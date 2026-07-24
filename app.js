// =========================================
// Sterownik Solarny - Aplikacja Webowa
// =========================================

const DEFAULT_GOOGLE_CSV_URL = "https://docs.google.com/spreadsheets/d/e/2PACX-1vTIo-0UREaUUsQabhvwHKmc9aE2vw-BZrLc5sER3FumTxucXr35FQ4Q-y-fnu6b8gBbCz2ieFgFKaHe/pub?output=csv";

const state = {
    connected: false,
    pollingInterval: null,
    simMode: false,
    initialized: false
};

// Cache dla elementów DOM, aby nie szukać ich przy każdej aktualizacji
const elCache = {};
function getEl(id) {
    if (!elCache[id]) elCache[id] = document.getElementById(id);
    return elCache[id];
}

const sim = {
    collector: 72, bufferTop: 35, bufferBottom: 28, waterTop: 60, waterBottom: 50,
    solarPump: false, bufferPump: false, valveOpen: false, direction: 'brak',
    maxWaterTemp: 85, solarDeltaOn: 8, solarDeltaOff: 4,
    maxBufferTemp: 85, wodaBuforDeltaOn: 8, wodaBuforDeltaOff: 4,
    buforWodaDeltaOn: 10, buforWodaDeltaOff: 3, minWodaTemp: 50,
    // CO
    houseTemp: 18.5, mixerTemp: 22, returnTemp: 18.0, outdoorTemp: 5.0, coPump: false,
    coPhase: 'idle', coMixerPos: 0, coMixerTarget: 0, coActionTicks: 0, coCheckTicks: 0,
    coMaxMixerTemp: 40, coTargetTemp: 20, coDeltaOn: 2, coDeltaOff: 1,
    targetCollector: 72, time: 0,
    autoCoEnabled: true, autoSolarEnabled: true, autoWbEnabled: true, autoBwEnabled: true
};

// Funkcja wywoływana z HTML po załadowaniu treści
function initApp() {
    if (state.initialized) return;
    state.initialized = true;

    setupTabs();
    setupEventListeners();
    setupSwipeNavigation();
    startPolling();
    checkConnection();
};

function setupSwipeNavigation() {
    const container = document.querySelector('.container');
    let touchstartX = 0;
    let touchendX = 0;

    container.addEventListener('touchstart', e => {
        touchstartX = e.changedTouches[0].screenX;
    }, { passive: true });

    container.addEventListener('touchend', e => {
        touchendX = e.changedTouches[0].screenX;
        handleSwipe();
    });

    function handleSwipe() {
        const swipeThreshold = 50; // Minimalna odległość dla gestu
        const deltaX = touchendX - touchstartX;

        if (Math.abs(deltaX) < swipeThreshold) return;

        const tabButtons = Array.from(document.querySelectorAll('.tab-btn'));
        const activeButton = document.querySelector('.tab-btn.active');
        const currentIndex = tabButtons.indexOf(activeButton);

        const newIndex = (deltaX > 0) ? currentIndex - 1 : currentIndex + 1;
        if (newIndex >= 0 && newIndex < tabButtons.length) tabButtons[newIndex].click();
    }
}function setupTabs() {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(btn.dataset.tab).classList.add('active');
        });
    });
}

function checkConnection() {
    fetchData();
}

function enableSimulation() {
    state.simMode = true;
    state.connected = true;
    if (getEl('connectionStatus')) getEl('connectionStatus').querySelector('.status-dot').className = 'status-dot online';
    document.getElementById('statusText').textContent = 'Symulacja';
    const btn = document.getElementById('btnSimToggle');
    if (btn) btn.classList.add('active');
    startPolling();
}

function disableSimulation() {
    state.simMode = false;
    state.connected = false;
    if (getEl('connectionStatus')) getEl('connectionStatus').querySelector('.status-dot').className = 'status-dot offline';
    document.getElementById('statusText').textContent = 'Offline';
    const btn = document.getElementById('btnSimToggle');
    if (btn) btn.classList.remove('active');
    if (state.pollingInterval) { clearInterval(state.pollingInterval); state.pollingInterval = null; }
    checkConnection();
}

function startPolling() {
    if (state.pollingInterval) clearInterval(state.pollingInterval);
    state.pollingInterval = setInterval(fetchData, 60000);
}

function setupEventListeners() {
    document.getElementById('btnSolarPumpOn').addEventListener('click', () => {
        showConfirm('Włączyć pompę solarną?', () => controlRelay('solar_pump', true));
    });
    document.getElementById('btnSolarPumpOff').addEventListener('click', () => {
        showConfirm('Wyłączyć pompę solarną?', () => controlRelay('solar_pump', false));
    });
    document.getElementById('btnBufferPumpOn').addEventListener('click', () => {
        showConfirm('Włączyć pompę obiegu?', () => controlRelay('buffer_pump', true));
    });
    document.getElementById('btnBufferPumpOff').addEventListener('click', () => {
        showConfirm('Wyłączyć pompę obiegu?', () => controlRelay('buffer_pump', false));
    });
    document.getElementById('btnValveOpen').addEventListener('click', () => {
        showConfirm('Otworzyć zawór kierunku?', () => controlRelay('valve', true));
    });
    document.getElementById('btnValveClose').addEventListener('click', () => {
        showConfirm('Zamknąć zawór kierunku?', () => controlRelay('valve', false));
    });
    document.getElementById('btnCoPumpOn').addEventListener('click', () => {
        showConfirm('Włączyć pompę CO?', () => controlRelay('co_pump', true));
    });
    document.getElementById('btnCoPumpOff').addEventListener('click', () => {
        showConfirm('Wyłączyć pompę CO?', () => controlRelay('co_pump', false));
    });
    document.getElementById('btnSaveWodaBuforSettings').addEventListener('click', () => {
        showConfirm('Zapisać ustawienia Woda → Bufor?', saveBufferSettings);
    });
    document.getElementById('btnSaveBuforWodaSettings').addEventListener('click', () => {
        showConfirm('Zapisać ustawienia Bufor → Woda?', saveBufferSettings);
    });
    document.getElementById('btnSaveSolarSettings').addEventListener('click', () => {
        showConfirm('Zapisać ustawienia solarów?', saveSolarSettings);
    });
    document.getElementById('btnSimToggle').addEventListener('click', () => {
        if (state.simMode) {
            disableSimulation();
        } else {
            enableSimulation();
        }
    });
    document.getElementById('btnSaveCoSettings').addEventListener('click', () => {
        showConfirm('Zapisać ustawienia CO?', saveCoSettings);
    });

    // Nowe event listenery dla sterowania automatyką
    document.getElementById('btnAutoCoOn').addEventListener('click', () => controlAutomation('co', true));
    document.getElementById('btnAutoCoOff').addEventListener('click', () => controlAutomation('co', false));

    document.getElementById('btnAutoSolarOn').addEventListener('click', () => controlAutomation('solar', true));
    document.getElementById('btnAutoSolarOff').addEventListener('click', () => controlAutomation('solar', false));

    document.getElementById('btnAutoWbOn').addEventListener('click', () => controlAutomation('wb', true));
    document.getElementById('btnAutoWbOff').addEventListener('click', () => controlAutomation('wb', false));

    document.getElementById('btnAutoBwOn').addEventListener('click', () => controlAutomation('bw', true));
    document.getElementById('btnAutoBwOff').addEventListener('click', () => controlAutomation('bw', false));

    document.getElementById('alertOkBtn').addEventListener('click', closeAlert);
    document.getElementById('confirmCancelBtn').addEventListener('click', closeConfirm);
    document.getElementById('confirmOkBtn').addEventListener('click', confirmAction);
    document.getElementById('alertModal').addEventListener('click', (e) => { if (e.target === e.currentTarget) closeAlert(); });
    document.getElementById('confirmModal').addEventListener('click', (e) => { if (e.target === e.currentTarget) closeConfirm(); });

    const googleRefreshBtn = document.getElementById('btnGoogleRefresh');
    if (googleRefreshBtn) googleRefreshBtn.addEventListener('click', fetchData);
}

// ============= POBIERANIE / SYMULACJA =============

async function fetchData() {
    if (state.simMode) { simulateData(); return; }
    if (getEl('googleLastStatus')) updateLed('googleLastStatus', false, 'Wczytano', 'Pobieram...');

    try {
        const lastRow = await loadGoogleFormLastEntry();
        const snapshot = snapshotFromGoogleRow(lastRow);
        const data = googleSnapshotToAppData(snapshot);
        updateConnectionStatus(true);
        updateAll(data);
        updateGoogleLastEntry(lastRow);
        updateLed('googleLastStatus', true, 'Wczytano', 'Błąd');
    } catch (e) {
        if (!state.simMode) {
            updateConnectionStatus(false);
            setText('googleLastSummary', `Błąd: ${e.message}`);
            updateLed('googleLastStatus', false, 'Wczytano', 'Błąd');
        }
    }
}

async function loadGoogleFormLastEntry() {
    const csvText = await fetchGoogleCsv(DEFAULT_GOOGLE_CSV_URL);
    const rows = parseCsv(csvText);
    const lastRow = getLastDataRow(rows);
    if (!lastRow) throw new Error('brak wpisów w arkuszu');
    return lastRow;
}

async function fetchGoogleCsv(sourceUrl) {
    try {
        const res = await fetch(sourceUrl, { cache: 'no-store' });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return await res.text();
    } catch (error) {
        const proxyUrl = buildGoogleCsvProxyUrl(sourceUrl);
        const res = await fetch(proxyUrl, { cache: 'no-store' });
        if (!res.ok) throw new Error(`proxy HTTP ${res.status}`);
        return await res.text();
    }
}

function buildGoogleCsvProxyUrl(sourceUrl) {
    return `https://api.allorigins.win/raw?url=${encodeURIComponent(sourceUrl)}`;
}

function getLastDataRow(rows) {
    if (!Array.isArray(rows) || rows.length < 2) return null;
    const dataRows = rows.slice(1).filter(row => row.some(value => String(value || '').trim()));
    const completeRows = dataRows.filter(row => hasAllGoogleTemps(snapshotFromGoogleRow(row)));
    return completeRows[completeRows.length - 1] || dataRows[dataRows.length - 1] || null;
}

function updateGoogleLastEntry(row) {
    const snapshot = snapshotFromGoogleRow(row);
    const temps = snapshot.temps || {};

    setText('googleLastTime', row[0] || '--');
    setText('googleLastBufferTop', formatGoogleTemp(temps.bufferTop));
    setText('googleLastBufferBottom', formatGoogleTemp(temps.bufferBottom));
    setText('googleLastCollector', formatGoogleTemp(temps.collector));
    setText('googleLastWaterTop', formatGoogleTemp(temps.waterTop));
    setText('googleLastWaterBottom', formatGoogleTemp(temps.waterBottom));
    setText('googleLastHouse', formatGoogleTemp(temps.house));
    setText('googleLastMixer', formatGoogleTemp(temps.mixer));
    setText('googleLastReturn', formatGoogleTemp(temps.return));
    setText('googleLastOutdoor', formatGoogleTemp(temps.outdoor));
    setText('googleLastSummary', row[2] || googleLegacySummary(row));
    setText('googleLastHealth', row[3] || googleLegacyHealth(row));
    setText('googleLastSnapshot', JSON.stringify(snapshot, null, 2));
    renderGoogleAllData(snapshot);
}

function hasAllGoogleTemps(snapshot) {
    const temps = snapshot && snapshot.temps ? snapshot.temps : {};
    return ['bufferTop', 'bufferBottom', 'collector', 'waterTop', 'waterBottom', 'house', 'mixer', 'return', 'outdoor']
        .every(key => temps[key] !== undefined && temps[key] !== null && temps[key] !== '');
}

function snapshotFromGoogleRow(row) {
    const raw = row[1] || '';
    try {
        const parsed = JSON.parse(raw);
        return parsed && typeof parsed === 'object' ? parsed : {};
    } catch (error) {
        return {
            temps: {
                collector: row[1],
                waterTop: row[2],
                waterBottom: row[3]
            },
            legacy: true
        };
    }
}

function googleSnapshotToAppData(snapshot) {
    const temps = snapshot.temps || {};
    const settings = snapshot.settings || {};
    const systemState = snapshot.state || {};
    const automation = snapshot.automation || {};
    const health = snapshot.health || {};

    return {
        solar: {
            collector: numOrNull(temps.collector),
            waterTop: numOrNull(temps.waterTop),
            waterBottom: numOrNull(temps.waterBottom),
            maxWaterTemp: numOrDefault(settings.maxWaterTemp, 85),
            deltaOn: numOrDefault(settings.solarDeltaOn, 8),
            deltaOff: numOrDefault(settings.solarDeltaOff, 4),
            pumpActive: boolOrDefault(systemState.solarPumpActive, false)
        },
        buffer: {
            bufferTop: numOrNull(temps.bufferTop),
            bufferBottom: numOrNull(temps.bufferBottom),
            maxBufferTemp: numOrDefault(settings.maxBufferTemp, 85),
            wodaBuforDeltaOn: numOrDefault(settings.wodaBuforDeltaOn, 8),
            wodaBuforDeltaOff: numOrDefault(settings.wodaBuforDeltaOff, 4),
            buforWodaDeltaOn: numOrDefault(settings.buforWodaDeltaOn, 10),
            buforWodaDeltaOff: numOrDefault(settings.buforWodaDeltaOff, 3),
            minWodaTemp: numOrDefault(settings.minWodaTemp, 50),
            pumpActive: boolOrDefault(systemState.bufferPumpActive, false),
            valveOpen: boolOrDefault(systemState.valveOpen, false),
            direction: systemState.direction || 'brak'
        },
        co: {
            houseTemp: numOrNull(temps.house),
            mixerTemp: numOrNull(temps.mixer),
            bufferTopTemp: numOrNull(temps.bufferTop),
            maxMixerTemp: numOrDefault(settings.coMaxMixerTemp, 40),
            targetTemp: numOrDefault(settings.coTargetTemp, 20),
            deltaOn: numOrDefault(settings.coDeltaOn, 2),
            deltaOff: numOrDefault(settings.coDeltaOff, 1),
            pumpActive: boolOrDefault(systemState.coPumpActive, false),
            mixerPercent: numOrDefault(systemState.mixerPercent, 0),
            phase: systemState.coPhase || 'idle',
            returnTemp: numOrNull(temps.return),
            autoCoEnabled: boolOrDefault(automation.autoCoEnabled, true),
            autoSolarEnabled: boolOrDefault(automation.autoSolarEnabled, true),
            autoWbEnabled: boolOrDefault(automation.autoWbEnabled, true),
            autoBwEnabled: boolOrDefault(automation.autoBwEnabled, true)
        },
        outdoorTemp: numOrNull(temps.outdoor),
        health
    };
}

function renderGoogleAllData(snapshot) {
    const container = getEl('googleAllData');
    if (!container) return;

    container.innerHTML = '';
    const rootValues = Object.fromEntries(Object.entries(snapshot || {}).filter(([, value]) => !value || typeof value !== 'object' || Array.isArray(value)));
    const sections = Object.entries(snapshot || {}).filter(([, value]) => value && typeof value === 'object' && !Array.isArray(value));
    if (Object.keys(rootValues).length) sections.unshift(['system', rootValues]);

    if (!sections.length) {
        const empty = document.createElement('div');
        empty.className = 'snapshot-empty';
        empty.textContent = 'Brak pełnego snapshotu w ostatnim wpisie.';
        container.appendChild(empty);
        return;
    }

    sections.forEach(([sectionName, sectionValue]) => {
        const section = document.createElement('div');
        section.className = 'snapshot-section';

        const title = document.createElement('div');
        title.className = 'snapshot-section-title';
        title.textContent = googleSectionLabel(sectionName);
        section.appendChild(title);

        Object.entries(sectionValue).forEach(([key, value]) => {
            const row = document.createElement('div');
            row.className = 'snapshot-row';

            const keyEl = document.createElement('div');
            keyEl.className = 'snapshot-key';
            keyEl.textContent = googleFieldLabel(key);

            const valueEl = document.createElement('div');
            valueEl.className = 'snapshot-value';
            valueEl.textContent = formatSnapshotValue(value);

            row.appendChild(keyEl);
            row.appendChild(valueEl);
            section.appendChild(row);
        });

        container.appendChild(section);
    });
}

function googleSectionLabel(sectionName) {
    const labels = {
        temps: 'Temperatury',
        runtime: 'Runtime',
        system: 'System',
        settings: 'Nastawy',
        state: 'Stany wyjść i pracy',
        automation: 'Automatyka',
        health: 'Zdrowie czujników',
        wifi: 'WiFi'
    };
    return labels[sectionName] || sectionName;
}

function googleFieldLabel(key) {
    const labels = {
        bufferTop: 'Bufor góra',
        bufferBottom: 'Bufor dół',
        collector: 'Kolektor',
        waterTop: 'Woda góra',
        waterBottom: 'Woda dół',
        house: 'Dom',
        mixer: 'Za mieszaczem',
        return: 'Powrót',
        outdoor: 'Zewnętrzna',
        maxWaterTemp: 'Max temp. wody',
        solarDeltaOn: 'Solary delta wł.',
        solarDeltaOff: 'Solary delta wył.',
        maxBufferTemp: 'Max temp. bufora',
        wodaBuforDeltaOn: 'Woda->bufor delta wł.',
        wodaBuforDeltaOff: 'Woda->bufor delta wył.',
        buforWodaDeltaOn: 'Bufor->woda delta wł.',
        buforWodaDeltaOff: 'Bufor->woda delta wył.',
        minWodaTemp: 'Min temp. wody',
        coMaxMixerTemp: 'CO max za mieszaczem',
        coTargetTemp: 'CO temp. zadana',
        coDeltaOn: 'CO delta wł.',
        coDeltaOff: 'CO delta wył.',
        solarPumpActive: 'Pompa solarna',
        bufferPumpActive: 'Pompa obiegu',
        valveOpen: 'Zawór',
        direction: 'Kierunek',
        coPumpActive: 'Pompa CO',
        mixerPercent: 'Mieszacz',
        mixerRunning: 'Mieszacz pracuje',
        mixerDirection: 'Kierunek mieszacza',
        coPhase: 'Faza CO',
        isMixerResetting: 'Reset mieszacza',
        autoCoEnabled: 'Auto CO',
        autoSolarEnabled: 'Auto solary',
        autoWbEnabled: 'Auto woda->bufor',
        autoBwEnabled: 'Auto bufor->woda',
        solarTempsOk: 'Czujniki solarów',
        bufferTempsOk: 'Czujniki bufora',
        coTempsOk: 'Czujniki CO',
        anyTempError: 'Błąd temperatur',
        ip: 'IP',
        rssi: 'RSSI',
        uptimeMs: 'Uptime',
        tempState: 'Stan odczytu temperatur',
        conversionStartTime: 'Start konwersji temperatur',
        googleFormLogIntervalMs: 'Interwał zapisu Google',
        lastGoogleFormLog: 'Ostatni zapis Google',
        googleFormLogPending: 'Zapis Google oczekuje',
        valveActionPending: 'Impuls zaworu trwa',
        valveActionEndTime: 'Koniec impulsu zaworu',
        mixerTimer: 'Timer mieszacza',
        mixerStepEnd: 'Koniec kroku mieszacza',
        mixerResetEndTime: 'Koniec resetu mieszacza',
        coCheckTimer: 'Timer kontroli CO'
    };
    return labels[key] || key;
}

function formatSnapshotValue(value) {
    if (value === null || value === undefined || value === '') return '--';
    if (typeof value === 'boolean') return value ? 'Tak' : 'Nie';
    if (typeof value === 'number') return Number.isInteger(value) ? String(value) : value.toFixed(1);
    if (typeof value === 'object') return JSON.stringify(value);
    return String(value);
}

function numOrNull(value) {
    if (value === null || value === undefined || value === '') return null;
    const number = Number.parseFloat(String(value).replace(',', '.'));
    return Number.isFinite(number) ? number : null;
}

function numOrDefault(value, fallback) {
    const number = numOrNull(value);
    return number === null ? fallback : number;
}

function boolOrDefault(value, fallback) {
    return typeof value === 'boolean' ? value : fallback;
}

function googleLegacySummary(row) {
    return `kolektor=${row[1] || '--'}, woda_gora=${row[2] || '--'}, woda_dol=${row[3] || '--'}`;
}

function googleLegacyHealth(row) {
    return row.some(value => String(value || '').trim()) ? 'stary format' : '';
}

function formatGoogleTemp(value) {
    if (value === undefined || value === null || value === '') return '--.-';
    const normalized = String(value).replace(',', '.');
    const number = Number.parseFloat(normalized);
    return Number.isFinite(number) ? number.toFixed(1) : String(value);
}

function parseCsv(text) {
    const rows = [];
    let row = [];
    let value = '';
    let quoted = false;

    for (let i = 0; i < text.length; i++) {
        const char = text[i];
        const next = text[i + 1];

        if (char === '"' && quoted && next === '"') {
            value += '"';
            i++;
        } else if (char === '"') {
            quoted = !quoted;
        } else if (char === ',' && !quoted) {
            row.push(value);
            value = '';
        } else if ((char === '\n' || char === '\r') && !quoted) {
            if (char === '\r' && next === '\n') i++;
            row.push(value);
            rows.push(row);
            row = [];
            value = '';
        } else {
            value += char;
        }
    }

    if (value || row.length) {
        row.push(value);
        rows.push(row);
    }

    return rows;
}

function simulateData() {
    sim.time += 2;
    if (Math.random() < 0.02) sim.targetCollector = 30 + Math.random() * 55;
    sim.collector += (sim.targetCollector - sim.collector) * 0.05;
    sim.collector += (Math.random() - 0.5) * 0.8;

    if (sim.solarPump) {
        const h = (sim.collector - sim.waterBottom) * 0.02;
        if (h > 0) { sim.waterBottom += h; sim.waterTop += h * 0.5; sim.collector -= h * 0.3; }
    }
    if (sim.bufferPump && sim.valveOpen) {
        if (sim.direction === 'woda->bufor') {
            const h = (sim.waterTop - sim.bufferBottom) * 0.03;
            if (h > 0) { sim.bufferBottom += h; sim.bufferTop += h * 0.6; sim.waterTop -= h * 0.4; }
        } else if (sim.direction === 'bufor->woda') {
            const h = (sim.bufferTop - sim.waterTop) * 0.025;
            if (h > 0) { sim.waterTop += h; sim.waterBottom += h * 0.4; sim.bufferTop -= h * 0.3; sim.bufferBottom -= h * 0.2; }
        }
    }
    sim.waterTop -= (sim.waterTop - 20) * 0.004;
    sim.waterBottom -= (sim.waterBottom - 18) * 0.003;
    sim.bufferTop -= (sim.bufferTop - sim.bufferBottom) * 0.008;
    sim.bufferBottom -= (sim.bufferBottom - 16) * 0.002;

    if (sim.autoSolarEnabled) {
        const dSolar = sim.collector - sim.waterBottom;
        if (dSolar > sim.solarDeltaOn && sim.waterTop < sim.maxWaterTemp) sim.solarPump = true;
        else if (dSolar < sim.solarDeltaOff) sim.solarPump = false;
    }

    if (sim.autoWbEnabled || sim.autoBwEnabled) {
        const dWB = sim.waterTop - sim.bufferBottom;
        const dBW = sim.bufferTop - sim.waterTop;
        const wb = sim.autoWbEnabled && (dWB > sim.wodaBuforDeltaOn && sim.waterTop < sim.maxBufferTemp && sim.waterTop > sim.minWodaTemp);
        const bw = sim.autoBwEnabled && (dBW > sim.buforWodaDeltaOn && sim.waterTop < sim.minWodaTemp);

        if (wb && dWB > dBW) {
            if (!sim.bufferPump || !sim.valveOpen || sim.direction !== 'woda->bufor') {
                sim.bufferPump = true; sim.valveOpen = true; sim.direction = 'woda->bufor';
            }
        } else if (bw) {
            if (!sim.bufferPump || !sim.valveOpen || sim.direction !== 'bufor->woda') {
                sim.bufferPump = true; sim.valveOpen = true; sim.direction = 'bufor->woda';
            }
        } else if ((sim.direction === 'woda->bufor' && dWB < sim.wodaBuforDeltaOff) ||
            (sim.direction === 'bufor->woda' && dBW < sim.buforWodaDeltaOff) || (!wb && !bw)) {
            if (sim.bufferPump || sim.valveOpen) { sim.bufferPump = false; sim.valveOpen = false; sim.direction = 'brak'; }
        }
    }

    // CO symulacja - nowa logika sterowania
    // 1 tick = 2s, 5s ≈ 3 ticki, 30s ≈ 15 ticków
    const deltaDomu = sim.coTargetTemp - sim.houseTemp;

    if (sim.coPhase === 'idle') {
        if (sim.autoCoEnabled && deltaDomu > sim.coDeltaOn) {
            // Warunek startu: otwórz mieszacz na 5s, potem włącz pompę
            sim.coPhase = 'starting';
            sim.coMixerTarget = 100;
            sim.coActionTicks = 3;
        }
    } else if (sim.coPhase === 'starting') {
        // Ruch mieszacza w kierunku otwartego (5s)
        sim.coMixerPos = Math.min(sim.coMixerTarget, sim.coMixerPos + 34);
        sim.coActionTicks--;
        if (sim.coActionTicks <= 0) {
            sim.coPump = true;
            sim.coPhase = 'running';
            sim.coCheckTicks = 15; // 30s do pierwszego sprawdzenia
        }
    } else if (sim.coPhase === 'running') {
        // Sprawdzaj co 30s temp za mieszaczem
        sim.coCheckTicks--;
        if (sim.coCheckTicks <= 0) {
            if (sim.mixerTemp < sim.coMaxMixerTemp) {
                sim.coMixerTarget = 100; // za niska → otwórz na 5s
            } else {
                sim.coMixerTarget = 0;   // za wysoka → zamknij na 5s
            }
            sim.coActionTicks = 3;
            sim.coCheckTicks = 15;
        }
        // Ruch mieszacza
        if (sim.coMixerPos < sim.coMixerTarget) {
            sim.coMixerPos = Math.min(sim.coMixerTarget, sim.coMixerPos + 34);
        } else if (sim.coMixerPos > sim.coMixerTarget) {
            sim.coMixerPos = Math.max(sim.coMixerTarget, sim.coMixerPos - 34);
        }
        // Warunek wyłączenia zgodny z firmware: dom osiągnął histerezę LUB bufor góra < temp zadana
        if ((sim.coTargetTemp - sim.houseTemp) <= sim.coDeltaOff || sim.bufferTop < sim.coTargetTemp) {
            sim.coPump = false;
            sim.coPhase = 'stopping';
            sim.coMixerTarget = 0;
            sim.coActionTicks = 3;
        }
    } else if (sim.coPhase === 'stopping') {
        // Zamykaj mieszacz (5s)
        sim.coMixerPos = Math.max(0, sim.coMixerPos - 34);
        sim.coActionTicks--;
        if (sim.coActionTicks <= 0) {
            sim.coMixerPos = 0;
            sim.coPhase = 'idle';
        }
    }

    // Symulacja temperatur CO na podstawie pozycji mieszacza
    const mixerFrac = sim.coMixerPos / 100;
    if (sim.coPump && mixerFrac > 0) {
        const targetMixerTemp = sim.houseTemp + mixerFrac * (sim.bufferTop - sim.houseTemp);
        sim.mixerTemp += (targetMixerTemp - sim.mixerTemp) * 0.3;
        sim.houseTemp += (sim.mixerTemp - sim.houseTemp) * 0.01;
        // Symulacja temp powrotu: rośnie gdy pompa działa
        sim.returnTemp += (sim.mixerTemp - sim.returnTemp) * 0.15;
    } else {
        sim.mixerTemp -= (sim.mixerTemp - sim.houseTemp) * 0.02;
        sim.houseTemp -= (sim.houseTemp - 15) * 0.005;
        // Symulacja temp powrotu: spada gdy pompa nie działa
        sim.returnTemp -= (sim.returnTemp - sim.houseTemp) * 0.01;
    }

    // Symulacja temp zewnętrznej: powolne zmiany wokół wartości bazowej
    sim.outdoorTemp += (Math.random() - 0.5) * 0.3;
    sim.outdoorTemp -= (sim.outdoorTemp - 5) * 0.001; // tendencja do 5°C

    const rd = (v) => Math.round(v * 10) / 10;
    updateAll({
        solar: {
            collector: rd(sim.collector), waterTop: rd(sim.waterTop), waterBottom: rd(sim.waterBottom),
            maxWaterTemp: sim.maxWaterTemp, deltaOn: sim.solarDeltaOn, deltaOff: sim.solarDeltaOff,
            pumpActive: sim.solarPump
        },
        buffer: {
            bufferTop: rd(sim.bufferTop), bufferBottom: rd(sim.bufferBottom),
            maxBufferTemp: sim.maxBufferTemp,
            wodaBuforDeltaOn: sim.wodaBuforDeltaOn, wodaBuforDeltaOff: sim.wodaBuforDeltaOff,
            buforWodaDeltaOn: sim.buforWodaDeltaOn, buforWodaDeltaOff: sim.buforWodaDeltaOff,
            minWodaTemp: sim.minWodaTemp,
            pumpActive: sim.bufferPump, valveOpen: sim.valveOpen, direction: sim.direction
        },
        co: {
            houseTemp: rd(sim.houseTemp), mixerTemp: rd(sim.mixerTemp),
            bufferTopTemp: rd(sim.bufferTop),
            maxMixerTemp: sim.coMaxMixerTemp, targetTemp: sim.coTargetTemp,
            deltaOn: sim.coDeltaOn, deltaOff: sim.coDeltaOff,
            pumpActive: sim.coPump, mixerPercent: Math.round(sim.coMixerPos),
            phase: sim.coPhase, returnTemp: rd(sim.returnTemp),
            autoCoEnabled: sim.autoCoEnabled, autoSolarEnabled: sim.autoSolarEnabled,
            autoWbEnabled: sim.autoWbEnabled, autoBwEnabled: sim.autoBwEnabled
        },
        outdoorTemp: rd(sim.outdoorTemp)
    });
}

// ============= AKTUALIZACJA UI =============

function updateAll(data) {
    const sol = data.solar;
    const buf = data.buffer;

    // Temperatury - wszystkie karty
    const diagTemps = {
        'collectorTemp': sol.collector, 'diagCollectorTemp': sol.collector,
        'ctrlCollectorTemp': sol.collector, 'solCollectorTemp': sol.collector,
        'bufferTopTemp': buf.bufferTop, 'diagBufferTopTemp': buf.bufferTop,
        'ctrlBufferTopTemp': buf.bufferTop,
        'bufferBottomTemp': buf.bufferBottom, 'diagBufferBottomTemp': buf.bufferBottom,
        'ctrlBufferBottomTemp': buf.bufferBottom,
        'waterTopTemp': sol.waterTop, 'diagWaterTopTemp': sol.waterTop,
        'ctrlWaterTopTemp': sol.waterTop, 'solWaterTopTemp': sol.waterTop,
        'waterBottomTemp': sol.waterBottom, 'diagWaterBottomTemp': sol.waterBottom,
        'ctrlWaterBottomTemp': sol.waterBottom, 'solWaterBottomTemp': sol.waterBottom
    };
    // CO temps
    if (data.co) {
        diagTemps['ctrlHouseTemp'] = data.co.houseTemp;
        diagTemps['ctrlMixerTemp'] = data.co.mixerTemp;
        diagTemps['diagHouseTemp'] = data.co.houseTemp;
        diagTemps['diagMixerTemp'] = data.co.mixerTemp;
        diagTemps['diagReturnTemp'] = data.co.returnTemp;
        const outT = data.outdoorTemp !== undefined ? data.outdoorTemp : (data.co ? data.co.outdoorTemp : undefined);
        diagTemps['overviewOutdoorTemp'] = outT;
        diagTemps['diagOutdoorTemp'] = outT;
        diagTemps['ctrlOutdoorTemp'] = outT;
    }
    Object.keys(diagTemps).forEach(id => setText(id, fmt(diagTemps[id])));

    // Delta solarna w zakładce Solary
    const solDelta = (sol.collector && sol.waterBottom)
        ? (sol.collector - sol.waterBottom).toFixed(1) + ' °C'
        : '--.- °C';
    setText('solDelta', solDelta);

    // Statusy - wszystkie karty
    const pumpStates = [
        [['solarPumpStatus', 'solarPumpStatus2', 'ctrlSolarPumpStatus'], 'diagSolarPump', sol.pumpActive, 'Włączona', 'Wyłączona'],
        [['bufferPumpStatus', 'bufferPumpStatus2', 'bufferPumpStatusWB', 'bufferPumpStatusBW', 'ctrlBufferPumpStatus'], 'diagBufferPump', buf.pumpActive, 'Włączona', 'Wyłączona'],
        [['valveStatus', 'valveStatus2', 'valveStatusWB', 'valveStatusBW', 'ctrlValveStatus'], 'diagValve', buf.valveOpen, 'Otwarty', 'Zamknięty']
    ];

    pumpStates.forEach(([ledIds, textId, active, on, off]) => {
        ledIds.forEach(id => updateLed(id, active, on, off));
        setText(textId, active ? on : off);
    });

    // Aktualizacja kierunku w nowych zakładkach
    const dirWB = buf.direction === 'woda->bufor';
    const dirBW = buf.direction === 'bufor->woda';
    updateLed('directionLedWB', dirWB, '⬆️ Woda → Bufor', 'Brak');
    updateLed('directionLedBW', dirBW, '⬇️ Bufor → Woda', 'Brak');
    
    // Temperatury i delty dla zakładek W-B i B-W
    setText('wbWaterTopTemp', fmt(sol.waterTop));
    setText('wbBufferBottomTemp', fmt(buf.bufferBottom));
    const wbDelta = (sol.waterTop && buf.bufferBottom) ? (sol.waterTop - buf.bufferBottom).toFixed(1) : '--.-';
    setText('wbDelta', wbDelta);

    setText('bwBufferTopTemp', fmt(buf.bufferTop));
    setText('bwWaterTopTemp', fmt(sol.waterTop));
    const bwDelta = (buf.bufferTop && sol.waterTop) ? (buf.bufferTop - sol.waterTop).toFixed(1) : '--.-';
    setText('bwDelta', bwDelta);


    // Kierunek
    const dirLed = getEl('directionLed');
    const dirText = getEl('directionText');
    if (buf.direction === 'woda->bufor') {
        if (dirLed) dirLed.className = 'led led-on';
        if (dirText) dirText.textContent = '⬆️ Woda → Bufor';
    } else if (buf.direction === 'bufor->woda') {
        if (dirLed) dirLed.className = 'led led-on';
        if (dirText) dirText.textContent = '⬇️ Bufor → Woda';
    } else {
        if (dirLed) dirLed.className = 'led led-off';
        if (dirText) dirText.textContent = 'Brak';
    }

    // CO - temperatury i status
    if (data.co) {
        setText('coHouseTemp', fmt(data.co.houseTemp));
        setText('coMixerTemp', fmt(data.co.mixerTemp));
        setText('overviewHouseTemp', fmt(data.co.houseTemp));
        setText('overviewMixerTemp', fmt(data.co.mixerTemp));
        setText('coReturnTemp', fmt(data.co.returnTemp));
        setText('overviewReturnTemp', fmt(data.co.returnTemp));
        setText('ctrlReturnTemp', fmt(data.co.returnTemp));
        setText('coBufferTopTemp', fmt(data.co.bufferTopTemp));

        // Obliczanie i wyświetlanie histerezy CO
        const turnOnTemp = data.co.targetTemp - data.co.deltaOn;
        const turnOffTemp = data.co.targetTemp - data.co.deltaOff;
        setText('coTurnOnTemp', fmt(turnOnTemp));
        setText('coTurnOffTemp', fmt(turnOffTemp));

        updateLed('coPumpStatus', data.co.pumpActive, 'Włączona', 'Wyłączona');
        updateLed('overviewCoPumpStatus', data.co.pumpActive, 'Włączona', 'Wyłączona');
        updateLed('ctrlCoPumpStatus', data.co.pumpActive, 'Wł', 'Wył');
        // Faza pracy
        const phaseNames = { 'idle': '⏸️ Oczekiwanie', 'starting': '🔄 Start...', 'running': '▶️ Pracuje', 'stopping': '⏹️ Zatrzymywanie...' };
        setText('coPhase', phaseNames[data.co.phase] || 'Nieznany');
        // Diagnostyka - CO
        setText('diagCoPump', data.co.pumpActive ? 'Włączona' : 'Wyłączona');
        const phaseNamesDiag = { 'idle': 'Oczekiwanie', 'starting': 'Start...', 'running': 'Pracuje', 'stopping': 'Zatrzymywanie...' };
        setText('diagCoPhase', phaseNamesDiag[data.co.phase] || 'Nieznany');
        setText('diagMixerPercent', (data.co.mixerPercent || 0) + '%');
        // Pasek mieszacza
        const pct = data.co.mixerPercent || 0;
        const bar = getEl('coMixerBar');
        const txt = getEl('coMixerPercentText');
        if (bar) bar.style.width = pct + '%';
        if (txt) txt.textContent = pct + '%';
        // Ustawienia CO (pierwsze ładowanie)
        const el = getEl('coMaxMixerTemp');
        if (el && el.dataset.loaded !== 'true') {
            setVal('coMaxMixerTemp', data.co.maxMixerTemp);
            setVal('coTargetTemp', data.co.targetTemp);
            setVal('coDeltaOn', data.co.deltaOn);
            setVal('coDeltaOff', data.co.deltaOff);
            el.dataset.loaded = 'true';
        }

        // Aktualizacja statusu automatyki
        updateLed('ctrlAutoCoStatus', data.co.autoCoEnabled, 'Wł', 'Wył');
        updateLed('ctrlAutoSolarStatus', data.co.autoSolarEnabled, 'Wł', 'Wył');
        updateLed('ctrlAutoWbStatus', data.co.autoWbEnabled, 'Wł', 'Wył');
        updateLed('ctrlAutoBwStatus', data.co.autoBwEnabled, 'Wł', 'Wył');
    }

    // Ustawienia (pierwsze ładowanie)
    const maxWTEl = getEl('maxWaterTemp');
    if (maxWTEl && maxWTEl.dataset.loaded !== 'true') {
        setVal('maxWaterTemp', sol.maxWaterTemp);
        setVal('solarDeltaOn', sol.deltaOn);
        setVal('solarDeltaOff', sol.deltaOff);
        setVal('maxBufferTemp', buf.maxBufferTemp);
        setVal('wodaBuforDeltaOn', buf.wodaBuforDeltaOn);
        setVal('wodaBuforDeltaOff', buf.wodaBuforDeltaOff);
        setVal('buforWodaDeltaOn', buf.buforWodaDeltaOn);
        setVal('buforWodaDeltaOff', buf.buforWodaDeltaOff);
        setVal('minWodaTemp', buf.minWodaTemp);
        maxWTEl.dataset.loaded = 'true';
    }

    // Diagnostyka - delty
    const dCW = (sol.collector && sol.waterBottom) ? (sol.collector - sol.waterBottom).toFixed(1) + ' °C' : '--.- °C';
    const dWB = (sol.waterTop && buf.bufferBottom) ? (sol.waterTop - buf.bufferBottom).toFixed(1) + ' °C' : '--.- °C';
    const dBW = (buf.bufferTop && sol.waterTop) ? (buf.bufferTop - sol.waterTop).toFixed(1) + ' °C' : '--.- °C';
    setText('deltaCollectorWater', dCW);
    setText('deltaWaterBuffer', dWB);
    setText('deltaBufferWater', dBW);
    setText('deltaBufferTopBottom', (buf.bufferTop && buf.bufferBottom) ? (buf.bufferTop - buf.bufferBottom).toFixed(1) + ' °C' : '--.- °C');
    setText('deltaWaterTopBottom', (sol.waterTop && sol.waterBottom) ? (sol.waterTop - sol.waterBottom).toFixed(1) + ' °C' : '--.- °C');

    const dirNames = { 'woda->bufor': '⬆️ Woda → Bufor', 'bufor->woda': '⬇️ Bufor → Woda', 'brak': '— Brak —' };
    setText('currentDirection', dirNames[buf.direction] || '— Brak —');
}

function setText(id, v) { const el = getEl(id); if (el) el.textContent = v; }
function setVal(id, v) { const el = getEl(id); if (el) el.value = v; }
function fmt(t) { if (t === null || t === undefined || isNaN(t)) return '--.-'; return t.toFixed(1); }

function updateLed(eid, active, onText, offText) {
    const c = getEl(eid);
    if (!c) return;
    const led = c.querySelector('.led');
    const txt = c.querySelector('.led-text');
    if (active) { if (led) led.className = 'led led-on'; if (txt) txt.textContent = onText; }
    else { if (led) led.className = 'led led-off'; if (txt) txt.textContent = offText; }
}

function updateConnectionStatus(connected) {
    const dot = getEl('connectionStatus') ? getEl('connectionStatus').querySelector('.status-dot') : null;
    const text = getEl('statusText');
    if (state.simMode) { if (dot) dot.className = 'status-dot online'; if (text) text.textContent = 'Symulacja'; return; }
    state.connected = connected;
    if (connected) { if (dot) dot.className = 'status-dot online'; if (text) text.textContent = 'Google Forms'; }
    else { if (dot) dot.className = 'status-dot offline'; if (text) text.textContent = 'Offline'; }
}

// ============= MODALE =============

function showAlert(icon, title, msg) {
    document.getElementById('alertIcon').textContent = icon;
    document.getElementById('alertTitle').textContent = title;
    document.getElementById('alertMessage').textContent = msg;
    document.getElementById('alertModal').classList.add('show');
}
function closeAlert() { document.getElementById('alertModal').classList.remove('show'); }

let pendingAction = null;
function showConfirm(msg, action) { pendingAction = action; document.getElementById('confirmMessage').textContent = msg; document.getElementById('confirmModal').classList.add('show'); }
function closeConfirm() { document.getElementById('confirmModal').classList.remove('show'); pendingAction = null; }
function confirmAction() { document.getElementById('confirmModal').classList.remove('show'); if (pendingAction) { const a = pendingAction; pendingAction = null; a(); } }

// ============= STEROWANIE =============

async function controlRelay(relay, enable) {
    const names = { 'solar_pump': 'Pompa solarna', 'buffer_pump': 'Pompa obiegu', 'valve': 'Zawór kierunku', 'co_pump': 'Pompa CO' };
    const action = enable ? 'włączona' : 'wyłączona';
    if (state.simMode) {
        if (relay === 'solar_pump') sim.solarPump = enable;
        if (relay === 'buffer_pump') sim.bufferPump = enable;
        if (relay === 'valve') sim.valveOpen = enable;
        if (relay === 'co_pump') sim.coPump = enable;
        showAlert('✅', 'Sukces (sym)', `${names[relay] || relay} została ${action}.`);
        return;
    }
    showAlert('ℹ️', 'Tryb odczytu', 'Index.html pobiera dane tylko z Google Forms. Sterowanie przez IP jest wyłączone.');
}

async function controlAutomation(system, enable) {
    const names = { 'co': 'Automatyka CO', 'solar': 'Automatyka solarów', 'wb': 'Automatyka W→B', 'bw': 'Automatyka B→W' };
    const action = enable ? 'włączona' : 'wyłączona';

    if (state.simMode) {
        if (system === 'co') sim.autoCoEnabled = enable;
        if (system === 'solar') sim.autoSolarEnabled = enable;
        if (system === 'wb') sim.autoWbEnabled = enable;
        if (system === 'bw') sim.autoBwEnabled = enable;
        showAlert('✅', 'Sukces (sym)', `${names[system] || system} została ${action}.`);
        // Odśwież UI w symulacji
        fetchData();
        return;
    }
    showAlert('ℹ️', 'Tryb odczytu', 'Index.html pobiera dane tylko z Google Forms. Sterowanie automatyką przez IP jest wyłączone.');
}

// ============= ZAPIS =============

function val(id) { return parseFloat(document.getElementById(id).value); }
function validateRange(v, min, max, label) {
    if (isNaN(v) || v < min || v > max) { showAlert('⚠️', 'Błąd', `${label} musi być między ${min} a ${max}`); return false; }
    return true;
}
function isDeltaValid(on, off) {
    return Number.isFinite(on) && Number.isFinite(off) && off < on;
}
function validateDelta(on, off, label) {
    if (!isDeltaValid(on, off)) { showAlert('⚠️', 'Błąd', `Delta wyłączenia ${label} musi być mniejsza niż delta włączenia`); return false; }
    return true;
}

async function saveSolarSettings() {
    const maxWT = val('maxWaterTemp'), dOn = val('solarDeltaOn'), dOff = val('solarDeltaOff');
    if (!validateRange(maxWT, 20, 99, 'Max temp. wody') || !validateRange(dOn, 1, 30, 'Delta włączenia') || !validateRange(dOff, 0.5, 20, 'Delta wyłączenia') || !validateDelta(dOn, dOff, 'solarów')) return;
    if (state.simMode) { sim.maxWaterTemp = maxWT; sim.solarDeltaOn = dOn; sim.solarDeltaOff = dOff; showAlert('✅', 'Zapis (sym)', 'Ustawienia solarów zapisane.'); return; }
    showAlert('ℹ️', 'Tryb odczytu', 'Index.html pobiera dane tylko z Google Forms. Zapis ustawień przez IP jest wyłączony.');
}

async function saveCoSettings() {
    const maxMixer = val('coMaxMixerTemp'), target = val('coTargetTemp'), dOn = val('coDeltaOn'), dOff = val('coDeltaOff');
    if (!validateRange(maxMixer, 20, 80, 'Max temp. za mieszaczem') || !validateRange(target, 5, 35, 'Temp. zadana w domu') || !validateRange(dOn, 0.1, 20, 'Delta włączenia') || !validateRange(dOff, 0.1, 20, 'Delta wyłączenia') || !validateDelta(dOn, dOff, 'CO')) return;
    if (state.simMode) { sim.coMaxMixerTemp = maxMixer; sim.coTargetTemp = target; sim.coDeltaOn = dOn; sim.coDeltaOff = dOff; showAlert('✅', 'Zapis (sym)', 'Ustawienia CO zapisane.'); return; }
    showAlert('ℹ️', 'Tryb odczytu', 'Index.html pobiera dane tylko z Google Forms. Zapis ustawień przez IP jest wyłączony.');
}

async function saveBufferSettings() {
    const maxBT = val('maxBufferTemp'), wbOn = val('wodaBuforDeltaOn'), wbOff = val('wodaBuforDeltaOff'), bwOn = val('buforWodaDeltaOn'), bwOff = val('buforWodaDeltaOff'), minWT = val('minWodaTemp');
    if (!validateRange(maxBT, 20, 99, 'Max temp. bufora') || !validateRange(wbOn, 1, 30, 'Delta wł. W→B') || !validateRange(wbOff, 0.5, 20, 'Delta wył. W→B') || !validateDelta(wbOn, wbOff, 'W→B') || !validateRange(bwOn, 1, 30, 'Delta wł. B→W') || !validateRange(bwOff, 0.5, 20, 'Delta wył. B→W') || !validateDelta(bwOn, bwOff, 'B→W') || !validateRange(minWT, 20, 80, 'Min temp. wody')) return;
    if (state.simMode) { sim.maxBufferTemp = maxBT; sim.wodaBuforDeltaOn = wbOn; sim.wodaBuforDeltaOff = wbOff; sim.buforWodaDeltaOn = bwOn; sim.buforWodaDeltaOff = bwOff; sim.minWodaTemp = minWT; showAlert('✅', 'Zapis (sym)', 'Ustawienia bufora zapisane.'); return; }
    showAlert('ℹ️', 'Tryb odczytu', 'Index.html pobiera dane tylko z Google Forms. Zapis ustawień przez IP jest wyłączony.');
}

if (typeof module !== 'undefined') {
    module.exports = {
        DEFAULT_GOOGLE_CSV_URL,
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
        isDeltaValid,
        validateDelta
    };
}
