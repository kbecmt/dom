// =========================================
// Sterownik Solarny - Aplikacja Webowa
// =========================================

const DEFAULT_API_BASE = "http://192.168.1.139";

function getApiBase(locationObj, storageObj) {
    const loc = locationObj || (typeof window !== 'undefined' ? window.location : null);
    const storage = storageObj || (typeof window !== 'undefined' ? window.localStorage : null);

    if (loc) {
        const params = new URLSearchParams(loc.search || '');
        const apiFromQuery = params.get('api');
        if (apiFromQuery) {
            if (storage) storage.setItem('solarApiBase', apiFromQuery);
            return apiFromQuery.replace(/\/$/, '');
        }

        const isHostedUi = /(^|\.)github\.io$/i.test(loc.hostname || '') || (loc.hostname || '') === 'raw.githubusercontent.com';
        if (!isHostedUi && loc.origin && loc.origin !== 'null') return loc.origin;
    }

    if (storage) {
        const saved = storage.getItem('solarApiBase');
        if (saved) return saved.replace(/\/$/, '');
    }

    return DEFAULT_API_BASE;
}

const API_BASE = getApiBase();
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
    checkConnection();
    startPolling();
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
    state.pollingInterval = setInterval(fetchData, 15000); // Zmieniono na 15 sekund
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
}

// ============= POBIERANIE / SYMULACJA =============

async function fetchData() {
    if (state.simMode) { simulateData(); return; }
    try {
        const res = await fetch(`${API_BASE}/api/data`);
        if (!res.ok) throw Error();
        const data = await res.json();
        updateConnectionStatus(true);
        updateAll(data);
    } catch (e) {
        if (!state.simMode) updateConnectionStatus(false);
    }
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
    if (connected) { if (dot) dot.className = 'status-dot online'; if (text) text.textContent = 'Online'; }
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
    try {
        const payload = {}; payload[relay] = enable;
        const res = await fetch(`${API_BASE}/api/relay/control`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload)
        });
        if (!res.ok) throw Error();
        const r = await res.json();
        if (r.status === 'ok') { showAlert('✅', 'Sukces', `${names[relay] || relay} została ${action}.`); setTimeout(fetchData, 500); }
    } catch (e) { showAlert('❌', 'Błąd', 'Nie udało się sterować urządzeniem.'); }
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
    try {
        const payload = { system: system, enabled: enable };
        const res = await fetch(`${API_BASE}/api/automation/control`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload)
        });
        if (!res.ok) throw Error();
        const r = await res.json();
        if (r.status === 'ok') { showAlert('✅', 'Sukces', `${names[system] || system} została ${action}.`); setTimeout(fetchData, 500); }
    } catch (e) { showAlert('❌', 'Błąd', 'Nie udało się sterować automatyką.'); }
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
    try {
        const res = await fetch(`${API_BASE}/api/solar/settings`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ maxWaterTemp: maxWT, deltaOn: dOn, deltaOff: dOff }) });
        if (!res.ok) throw Error(); showAlert('✅', 'Zapisano', 'Ustawienia solarów zapisane.'); setTimeout(fetchData, 500);
    } catch (e) { showAlert('❌', 'Błąd', 'Nie można połączyć się z serwerem.'); }
}

async function saveCoSettings() {
    const maxMixer = val('coMaxMixerTemp'), target = val('coTargetTemp'), dOn = val('coDeltaOn'), dOff = val('coDeltaOff');
    if (!validateRange(maxMixer, 20, 80, 'Max temp. za mieszaczem') || !validateRange(target, 5, 35, 'Temp. zadana w domu') || !validateRange(dOn, 0.1, 20, 'Delta włączenia') || !validateRange(dOff, 0.1, 20, 'Delta wyłączenia') || !validateDelta(dOn, dOff, 'CO')) return;
    if (state.simMode) { sim.coMaxMixerTemp = maxMixer; sim.coTargetTemp = target; sim.coDeltaOn = dOn; sim.coDeltaOff = dOff; showAlert('✅', 'Zapis (sym)', 'Ustawienia CO zapisane.'); return; }
    try {
        const res = await fetch(`${API_BASE}/api/co/settings`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ maxMixerTemp: maxMixer, targetTemp: target, deltaOn: dOn, deltaOff: dOff }) });
        if (!res.ok) throw Error(); showAlert('✅', 'Zapisano', 'Ustawienia CO zapisane.'); setTimeout(fetchData, 500);
    } catch (e) { showAlert('❌', 'Błąd', 'Nie można połączyć się z serwerem.'); }
}

async function saveBufferSettings() {
    const maxBT = val('maxBufferTemp'), wbOn = val('wodaBuforDeltaOn'), wbOff = val('wodaBuforDeltaOff'), bwOn = val('buforWodaDeltaOn'), bwOff = val('buforWodaDeltaOff'), minWT = val('minWodaTemp');
    if (!validateRange(maxBT, 20, 99, 'Max temp. bufora') || !validateRange(wbOn, 1, 30, 'Delta wł. W→B') || !validateRange(wbOff, 0.5, 20, 'Delta wył. W→B') || !validateDelta(wbOn, wbOff, 'W→B') || !validateRange(bwOn, 1, 30, 'Delta wł. B→W') || !validateRange(bwOff, 0.5, 20, 'Delta wył. B→W') || !validateDelta(bwOn, bwOff, 'B→W') || !validateRange(minWT, 20, 80, 'Min temp. wody')) return;
    if (state.simMode) { sim.maxBufferTemp = maxBT; sim.wodaBuforDeltaOn = wbOn; sim.wodaBuforDeltaOff = wbOff; sim.buforWodaDeltaOn = bwOn; sim.buforWodaDeltaOff = bwOff; sim.minWodaTemp = minWT; showAlert('✅', 'Zapis (sym)', 'Ustawienia bufora zapisane.'); return; }
    try {
        const res = await fetch(`${API_BASE}/api/buffer/settings`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ maxBufferTemp: maxBT, wodaBuforDeltaOn: wbOn, wodaBuforDeltaOff: wbOff, buforWodaDeltaOn: bwOn, buforWodaDeltaOff: bwOff, minWodaTemp: minWT }) });
        if (!res.ok) throw Error(); showAlert('✅', 'Zapisano', 'Ustawienia bufora zapisane.'); setTimeout(fetchData, 500);
    } catch (e) { showAlert('❌', 'Błąd', 'Nie można połączyć się z serwerem.'); }
}

if (typeof module !== 'undefined') {
    module.exports = {
        DEFAULT_API_BASE,
        getApiBase,
        isDeltaValid,
        validateDelta
    };
}
