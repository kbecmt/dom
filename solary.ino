#include <dummy.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // Wymagane dla trwałego przechowywania ustawień
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// ===== HTML (RENAMED - safe ASCII) =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Podole</title>
<link rel="stylesheet" href="https://kbecmt.github.io/dom/style.css">
</head>
<body>

<div id="tresc"></div>
<script>
    fetch("https://raw.githubusercontent.com/kbecmt/dom/refs/heads/main/index.html")
  .then(response => response.text())
  .then(html => {
    tresc.innerHTML = html;
    if (typeof initApp === 'function') initApp();
  });
</script>
<script src="https://kbecmt.github.io/dom/app.js"></script>
</body>
</html>
)rawliteral";


// ============= KONFIGURACJA WIFI =============
IPAddress local_IP(192, 168, 0, 139);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// ============= PINY GPIO =============
#define ONE_WIRE_BUS 32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/*
//DeviceAddress addrCollector = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF9, 0x9E, 0xB3 };
//DeviceAddress addrTankLow   = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0x01, 0xA7, 0x7E };
DeviceAddress addrTankHigh  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF7, 0x95, 0x4F };
DeviceAddress addrBuforLow  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xDE, 0xEB, 0xDB };
DeviceAddress addrBuforHigh  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0x81, 0x3F, 0x82 };
*/
DeviceAddress SENSOR_BUFFER_TOP = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
DeviceAddress SENSOR_BUFFER_BOTTOM = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
DeviceAddress SENSOR_COLLECTOR = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF9, 0x9E, 0xB3};
DeviceAddress SENSOR_WATER_TOP = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08};
DeviceAddress SENSOR_WATER_BOTTOM = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09};
DeviceAddress SENSOR_HOUSE = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A};
DeviceAddress SENSOR_MIXER = {0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B};
DeviceAddress SENSOR_RETURN = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0x01, 0xA7, 0x7E};
DeviceAddress SENSOR_OUTDOOR = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF7, 0x95, 0x4F};

#define RELAY_SOLAR_PUMP 27  // Pompa solarna
#define RELAY_BUFFER_PUMP 14 // Pompa bufor<->woda
#define RELAY_VALVE_OPEN 15  // Zawór bufor<->woda
#define RELAY_VALVE_CLOSE 16 // Zawór bufor<->woda
#define RELAY_CO_PUMP 26     // Pompa CO
#define RELAY_MIXER_OPEN 33  // Mieszacz otwórz
#define RELAY_MIXER_CLOSE 25 // Mieszacz zamknij

#define IMPULSE_MS 10000

// Mieszacz: 120s = 100% otwarcia, 130s = 100% zamknięcia
#define MIXER_FULL_OPEN_MS 120000  // czas pełnego otwarcia mieszacza
#define MIXER_FULL_CLOSE_MS 130000 //
#define MIXER_STEP_MS 5000         // czas pojedynczego kroku mieszacza (5s = ~4% otwarcia, ~3.8% zamknięcia)

// ============= STRUKTURA =============

struct SolarSystem
{
    float bufferTopTemp = 0, bufferBottomTemp = 0;
    float collectorTemp = 0, waterTopTemp = 0, waterBottomTemp = 0;
    float houseTemp = 0, mixerTemp = 0, returnTemp = 0, outdoorTemp = 0;

    float maxWaterTemp = 85.0;
    float solarDeltaOn = 8.0, solarDeltaOff = 4.0;

    float maxBufferTemp = 85.0;
    float wodaBuforDeltaOn = 8.0, wodaBuforDeltaOff = 4.0;
    float buforWodaDeltaOn = 10.0, buforWodaDeltaOff = 3.0;
    float minWodaTemp = 50.0; // min temp wody góra do woda→bufor / max bufor→woda

    bool solarPumpActive = false;
    bool bufferPumpActive = false;
    bool valveOpen = false;
    String direction = "brak";

    // CO
    float coMaxMixerTemp = 40.0;
    float coTargetTemp = 20.0;
    float coDeltaOn = 2.0;
    float coDeltaOff = 1.0;
    bool coPumpActive = false;
    int mixerPercent = 0;         // 0-100% otwarcia mieszacza
    unsigned long mixerTimer = 0; // licznik ms dla mieszacza
    bool mixerRunning = false;    // true = trwa ruch mieszacza
    bool mixerDirection = false;  // true = otwieranie, false = zamykanie
    unsigned long mixerStepEnd = 0;
    String coPhase = "idle";        // idle, starting, running, stopping
    unsigned long coCheckTimer = 0; // timer 30s do sprawdzania temp za mieszaczem

    // Nieblokujący zawór
    bool valveMoving = false;
    unsigned long valveEndMillis = 0;
    int activeValveRelay = -1;
};

// Forward declarations
void connectToWiFi();
void setupWebServer();
void readTemps();
void updateControl();
void checkMixerTimer();
void checkValveTimer();

SolarSystem solar;
WebServer server(80);

// ============= LittleFS =============

void loadSettings()
{
    Preferences prefs;
    prefs.begin("solar", true); // Tryb tylko do odczytu

    solar.maxWaterTemp      = prefs.getFloat("maxWaterTemp", 85.0);
    solar.solarDeltaOn      = prefs.getFloat("solarDeltaOn", 8.0);
    solar.solarDeltaOff     = prefs.getFloat("solarDeltaOff", 4.0);
    solar.maxBufferTemp     = prefs.getFloat("maxBufferTemp", 85.0);
    solar.wodaBuforDeltaOn  = prefs.getFloat("wodaBufOn", 8.0);
    solar.wodaBuforDeltaOff = prefs.getFloat("wodaBufOff", 4.0);
    solar.buforWodaDeltaOn  = prefs.getFloat("bufWodaOn", 10.0);
    solar.buforWodaDeltaOff = prefs.getFloat("bufWodaOff", 3.0);
    solar.coMaxMixerTemp    = prefs.getFloat("coMaxMixTemp", 40.0);
    solar.coTargetTemp      = prefs.getFloat("coTargetTemp", 20.0);
    solar.coDeltaOn         = prefs.getFloat("coDeltaOn", 2.0);
    solar.coDeltaOff        = prefs.getFloat("coDeltaOff", 1.0);
    solar.minWodaTemp       = prefs.getFloat("minWodaTemp", 50.0);

    prefs.end();
    Serial.println("Ustawienia wczytane z Preferences.");
}

void saveSettings()
{
    Preferences prefs;
    prefs.begin("solar", false); // Tryb zapisu

    prefs.putFloat("maxWaterTemp",  solar.maxWaterTemp);
    prefs.putFloat("solarDeltaOn",  solar.solarDeltaOn);
    prefs.putFloat("solarDeltaOff", solar.solarDeltaOff);
    prefs.putFloat("maxBufferTemp", solar.maxBufferTemp);
    prefs.putFloat("wodaBufOn",     solar.wodaBuforDeltaOn);
    prefs.putFloat("wodaBufOff",    solar.wodaBuforDeltaOff);
    prefs.putFloat("bufWodaOn",     solar.buforWodaDeltaOn);
    prefs.putFloat("bufWodaOff",    solar.buforWodaDeltaOff);
    prefs.putFloat("coMaxMixTemp",  solar.coMaxMixerTemp);
    prefs.putFloat("coTargetTemp",  solar.coTargetTemp);
    prefs.putFloat("coDeltaOn",     solar.coDeltaOn);
    prefs.putFloat("coDeltaOff",    solar.coDeltaOff);
    prefs.putFloat("minWodaTemp",   solar.minWodaTemp);

    prefs.end();
    Serial.println("Ustawienia zapisane do Preferences.");
}

// ============= SETUP / LOOP =============

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 Solar Controller ===");

    pinMode(RELAY_SOLAR_PUMP, OUTPUT);
    pinMode(RELAY_BUFFER_PUMP, OUTPUT);
    pinMode(RELAY_VALVE_OPEN, OUTPUT);
    pinMode(RELAY_VALVE_CLOSE, OUTPUT);
    pinMode(RELAY_CO_PUMP, OUTPUT);
    pinMode(RELAY_MIXER_OPEN, OUTPUT);
    pinMode(RELAY_MIXER_CLOSE, OUTPUT);

    digitalWrite(RELAY_SOLAR_PUMP, LOW);
    digitalWrite(RELAY_BUFFER_PUMP, LOW);
    digitalWrite(RELAY_VALVE_OPEN, LOW);
    digitalWrite(RELAY_VALVE_CLOSE, LOW);
    digitalWrite(RELAY_CO_PUMP, LOW);
    digitalWrite(RELAY_MIXER_OPEN, LOW);
    digitalWrite(RELAY_MIXER_CLOSE, LOW);

    sensors.begin();
    loadSettings();
    connectToWiFi();
    setupWebServer();
    Serial.println("Setup OK");
}

void loop()
{
    server.handleClient();
    static unsigned long t = 0;
    if (millis() - t > 20000)
    {
        t = millis();
        readTemps();
        updateControl();
    }
    checkMixerTimer();
    checkValveTimer();
}

void startValveAction(int relay) {
    digitalWrite(RELAY_VALVE_OPEN, LOW);
    digitalWrite(RELAY_VALVE_CLOSE, LOW);
    digitalWrite(relay, HIGH);
    solar.valveMoving = true;
    solar.valveEndMillis = millis() + IMPULSE_MS;
    solar.activeValveRelay = relay;
}

void checkValveTimer() {
    if (solar.valveMoving && millis() >= solar.valveEndMillis) {
        if (solar.activeValveRelay != -1) digitalWrite(solar.activeValveRelay, LOW);
        solar.valveMoving = false;
        solar.activeValveRelay = -1;
    }
}

void checkMixerTimer()
{
    if (!solar.mixerRunning)
        return;
    if (millis() >= solar.mixerStepEnd)
    {
        // Zakończ krok mieszacza
        digitalWrite(RELAY_MIXER_OPEN, LOW);
        digitalWrite(RELAY_MIXER_CLOSE, LOW);
        solar.mixerRunning = false;

        if (solar.mixerDirection)
        {
            // Otwieranie: +5% (120s = 100%, 5s = ~4.17%)
            solar.mixerPercent = min(100, solar.mixerPercent + 4);
        }
        else
        {
            // Zamykanie: -5% (130s = 100%, 5s = ~3.85%)
            solar.mixerPercent = max(0, solar.mixerPercent - 4);
        }
        Serial.printf("Mieszacz: %d%%\n", solar.mixerPercent);
    }
}

// ============= WiFi =============

void connectToWiFi()
{
    Serial.print("Łączę z WiFi...");

    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    WiFi.begin("ITway.dev", "polpolpol1");
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nIP: " + WiFi.localIP().toString());
    }
    else
    {
        Serial.println("\n FAIL");
    }
}

// ============= CZUJNIKI =============

float readTemp(const DeviceAddress addr, const char *label)
{
    float t = sensors.getTempC(addr);
    if (t == DEVICE_DISCONNECTED_C)
    {
        return NAN;
    }
    Serial.printf("%s: %.1f°C\n", label, t);
    return t;
}

void readTemps()
{
    sensors.requestTemperatures();
    solar.bufferTopTemp = readTemp(SENSOR_BUFFER_TOP, "Bufor Góra");
    solar.bufferBottomTemp = readTemp(SENSOR_BUFFER_BOTTOM, "Bufor Dół");
    solar.collectorTemp = readTemp(SENSOR_COLLECTOR, "Kolektor");
    solar.waterTopTemp = readTemp(SENSOR_WATER_TOP, "Woda Góra");
    solar.waterBottomTemp = readTemp(SENSOR_WATER_BOTTOM, "Woda Dół");
    solar.houseTemp = readTemp(SENSOR_HOUSE, "Dom");
    solar.mixerTemp = readTemp(SENSOR_MIXER, "Mieszacz");
    solar.returnTemp = readTemp(SENSOR_RETURN, "Powrót");
    solar.outdoorTemp = readTemp(SENSOR_OUTDOOR, "Zewn");
}

void mixerStep(bool open)
{
    // Zatrzymaj poprzedni ruch
    digitalWrite(RELAY_MIXER_OPEN, LOW);
    digitalWrite(RELAY_MIXER_CLOSE, LOW);
    solar.mixerRunning = false;

    // Ustaw nowy krok
    solar.mixerDirection = open;
    digitalWrite(open ? RELAY_MIXER_OPEN : RELAY_MIXER_CLOSE, HIGH);
    solar.mixerRunning = true;
    solar.mixerStepEnd = millis() + MIXER_STEP_MS;
    Serial.printf("Mieszacz %s na %dms\n", open ? "OTWIERAM" : "ZAMYKAM", MIXER_STEP_MS);
}

// ============= LOGIKA STEROWANIA =============

void updateControl()
{
    // --- Solary ---
    float dSolar = solar.collectorTemp - solar.waterBottomTemp;
    if (dSolar > solar.solarDeltaOn && solar.waterTopTemp < solar.maxWaterTemp)
    {
        if (!solar.solarPumpActive)
        {
            solar.solarPumpActive = true;
            digitalWrite(RELAY_SOLAR_PUMP, HIGH);
            Serial.println("=> Pompa solarna ON");
        }
    }
    else if (dSolar < solar.solarDeltaOff)
    {
        if (solar.solarPumpActive)
        {
            solar.solarPumpActive = false;
            digitalWrite(RELAY_SOLAR_PUMP, LOW);
            Serial.println("=> Pompa solarna OFF");
        }
    }

    // --- CO z mieszaczem - nowa logika ---
    // 1. IDLE: temp zadana - temp domu > deltaOn → otwórz mieszacz 5s → STARTING
    // 2. STARTING: po 5s włącz pompę CO → RUNNING
    // 3. RUNNING: co 30s sprawdzaj temp za mieszaczem:
    //    - < maxMixerTemp → otwórz na 5s
    //    - > maxMixerTemp → zamknij na 5s
    //    - temp zadana - temp domu <= deltaOff → STOPPING
    // 4. STOPPING: zamknij mieszacz do 0% → IDLE

    float deltaDomu = solar.coTargetTemp - solar.houseTemp;

    if (solar.coPhase == "idle")
    {
        if (deltaDomu > solar.coDeltaOn && solar.bufferTopTemp > solar.coTargetTemp)
        {
            solar.coPhase = "starting";
            if (!solar.mixerRunning && solar.mixerPercent < 100)
            {
                mixerStep(true); // otwórz na 5s
            }
            Serial.printf("=> CO: STARTING (delta=%.1f > %.1f)\n", deltaDomu, solar.coDeltaOn);
        }
    }
    else if (solar.coPhase == "starting")
    {
        // Po 5s (krok mieszacza zakończony) → włącz pompę
        if (!solar.mixerRunning)
        {
            solar.coPumpActive = true;
            digitalWrite(RELAY_CO_PUMP, HIGH);
            solar.coPhase = "running";
            solar.coCheckTimer = millis();
            Serial.println("=> CO: RUNNING, pompa ON");
        }
    }
    else if (solar.coPhase == "running")
    {
        // Co 30s sprawdzaj temp za mieszaczem
        if (!solar.mixerRunning && (millis() - solar.coCheckTimer >= 30000))
        {
            solar.coCheckTimer = millis();
            if (solar.mixerTemp < solar.coMaxMixerTemp && solar.mixerPercent < 100)
            {
                mixerStep(true); // za niska → otwórz na 5s
                Serial.printf("=> CO: mixerTemp %.1f < %.1f → otwórz\n", solar.mixerTemp, solar.coMaxMixerTemp);
            }
            else if (solar.mixerTemp >= solar.coMaxMixerTemp && solar.mixerPercent > 0)
            {
                mixerStep(false); // za wysoka → zamknij na 5s
                Serial.printf("=> CO: mixerTemp %.1f >= %.1f → zamknij\n", solar.mixerTemp, solar.coMaxMixerTemp);
            }
        }
        // Warunek wyłączenia: (temp powrotu - temp zadana) > delta wyłączenia LUB bufor góra < temp zadana
        if ((solar.returnTemp - solar.coTargetTemp) > solar.coDeltaOff || solar.bufferTopTemp < solar.coTargetTemp)
        {
            solar.coPumpActive = false;
            digitalWrite(RELAY_CO_PUMP, LOW);
            solar.coPhase = "stopping";
            Serial.printf("=> CO: STOPPING (powrót-zadana=%.1f > deltaOff=%.1f)\n", solar.returnTemp - solar.coTargetTemp, solar.coDeltaOff);
            if (!solar.mixerRunning && solar.mixerPercent > 0)
            {
                mixerStep(false); // zacznij zamykać
            }
        }
    }
    else if (solar.coPhase == "stopping")
    {
        // Zamykaj mieszacz aż do 0%
        if (!solar.mixerRunning && solar.mixerPercent > 0)
        {
            mixerStep(false);
        }
        if (solar.mixerPercent <= 0 && !solar.mixerRunning)
        {
            solar.coPhase = "idle";
            Serial.println("=> CO: IDLE");
        }
    }

    // --- Wspólny obieg bufor<->woda ---
    float dWB = solar.waterTopTemp - solar.bufferBottomTemp;
    float dBW = solar.bufferTopTemp - solar.waterTopTemp;

    bool shouldWodaBufor = (dWB > solar.wodaBuforDeltaOn && solar.waterTopTemp < solar.maxBufferTemp && solar.waterTopTemp > solar.minWodaTemp);
    bool shouldBuforWoda = (dBW > solar.buforWodaDeltaOn && solar.waterTopTemp < solar.minWodaTemp);

    String newDir = solar.direction;
    bool shouldActivate = false;

    if (shouldWodaBufor && dWB > dBW)
    {
        newDir = "woda->bufor";
        shouldActivate = true;
    }
    else if (shouldBuforWoda)
    {
        newDir = "bufor->woda";
        shouldActivate = true;
    }

    if (shouldActivate)
    {
        if (!solar.bufferPumpActive || !solar.valveOpen || solar.direction != newDir)
        {
            startValveAction(RELAY_VALVE_OPEN);
            digitalWrite(RELAY_BUFFER_PUMP, HIGH);
            solar.bufferPumpActive = true;
            solar.valveOpen = true;
            solar.direction = newDir;
            Serial.printf("=> Obieg %s ON\n", newDir.c_str());
        }
    }
    else
    {
        bool shouldStop = false;
        if (solar.direction == "woda->bufor" && dWB < solar.wodaBuforDeltaOff)
            shouldStop = true;
        if (solar.direction == "bufor->woda" && dBW < solar.buforWodaDeltaOff)
            shouldStop = true;
        if (!shouldWodaBufor && !shouldBuforWoda)
            shouldStop = true;

        if (shouldStop && (solar.bufferPumpActive || solar.valveOpen))
        {
            digitalWrite(RELAY_BUFFER_PUMP, LOW);
            startValveAction(RELAY_VALVE_CLOSE);
            solar.bufferPumpActive = false;
            solar.valveOpen = false;
            solar.direction = "brak";
            Serial.println("=> Obieg OFF");
        }
    }
}

// ============= SERWER =============

void sendJsonResponse(JsonDocument& doc) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String out; 
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// Funkcja do obsługi ścieżki głównej, teraz serwująca prostą wiadomość
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void setupWebServer()
{

    server.on("/", handleRoot);

    server.on("/api/data", HTTP_GET, []()
              {
        JsonDocument doc;
        doc["solar"]["collector"] = solar.collectorTemp;
        doc["solar"]["waterTop"] = solar.waterTopTemp;
        doc["solar"]["waterBottom"] = solar.waterBottomTemp;
        doc["solar"]["maxWaterTemp"] = solar.maxWaterTemp;
        doc["solar"]["deltaOn"] = solar.solarDeltaOn;
        doc["solar"]["deltaOff"] = solar.solarDeltaOff;
        doc["solar"]["pumpActive"] = solar.solarPumpActive;

        doc["buffer"]["bufferTop"] = solar.bufferTopTemp;
        doc["buffer"]["bufferBottom"] = solar.bufferBottomTemp;
        doc["buffer"]["maxBufferTemp"] = solar.maxBufferTemp;
        doc["buffer"]["wodaBuforDeltaOn"] = solar.wodaBuforDeltaOn;
        doc["buffer"]["wodaBuforDeltaOff"] = solar.wodaBuforDeltaOff;
        doc["buffer"]["buforWodaDeltaOn"] = solar.buforWodaDeltaOn;
        doc["buffer"]["buforWodaDeltaOff"] = solar.buforWodaDeltaOff;
        doc["buffer"]["minWodaTemp"] = solar.minWodaTemp;
        doc["buffer"]["pumpActive"] = solar.bufferPumpActive;
        doc["buffer"]["valveOpen"] = solar.valveOpen;
        doc["buffer"]["direction"] = solar.direction;

        doc["co"]["houseTemp"] = solar.houseTemp;
        doc["co"]["mixerTemp"] = solar.mixerTemp;
        doc["co"]["bufferTopTemp"] = solar.bufferTopTemp;
        doc["co"]["maxMixerTemp"] = solar.coMaxMixerTemp;
        doc["co"]["targetTemp"] = solar.coTargetTemp;
        doc["co"]["deltaOn"] = solar.coDeltaOn;
        doc["co"]["deltaOff"] = solar.coDeltaOff;
        doc["co"]["pumpActive"] = solar.coPumpActive;
        doc["co"]["mixerPercent"] = solar.mixerPercent;
        doc["co"]["phase"] = solar.coPhase;
        doc["co"]["returnTemp"] = solar.returnTemp;
        doc["outdoorTemp"] = solar.outdoorTemp;

        sendJsonResponse(doc); });

    server.on("/api/solar/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) return;
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }
        if (doc.containsKey("maxWaterTemp")) solar.maxWaterTemp = doc["maxWaterTemp"];
        if (doc.containsKey("deltaOn")) solar.solarDeltaOn = doc["deltaOn"];
        if (doc.containsKey("deltaOff")) solar.solarDeltaOff = doc["deltaOff"];
        saveSettings();
        JsonDocument res; res["status"] = "ok";
        sendJsonResponse(res);
    });

    server.on("/api/buffer/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) return;
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }
        if (doc.containsKey("maxBufferTemp")) solar.maxBufferTemp = doc["maxBufferTemp"];
        if (doc.containsKey("wodaBuforDeltaOn")) solar.wodaBuforDeltaOn = doc["wodaBuforDeltaOn"];
        if (doc.containsKey("wodaBuforDeltaOff")) solar.wodaBuforDeltaOff = doc["wodaBuforDeltaOff"];
        if (doc.containsKey("buforWodaDeltaOn")) solar.buforWodaDeltaOn = doc["buforWodaDeltaOn"];
        if (doc.containsKey("buforWodaDeltaOff")) solar.buforWodaDeltaOff = doc["buforWodaDeltaOff"];
        if (doc.containsKey("minWodaTemp")) solar.minWodaTemp = doc["minWodaTemp"];
        saveSettings();
        JsonDocument res; res["status"] = "ok";
        sendJsonResponse(res);
    });

    server.on("/api/co/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) return;
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }
        if (doc.containsKey("maxMixerTemp")) solar.coMaxMixerTemp = doc["maxMixerTemp"];
        if (doc.containsKey("targetTemp")) solar.coTargetTemp = doc["targetTemp"];
        if (doc.containsKey("deltaOn")) solar.coDeltaOn = doc["deltaOn"];
        if (doc.containsKey("deltaOff")) solar.coDeltaOff = doc["deltaOff"];
        saveSettings();
        JsonDocument res; res["status"] = "ok";
        sendJsonResponse(res);
    });

    server.on("/api/relay/control", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) return;
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }

        if (doc.containsKey("solar_pump")) {
            solar.solarPumpActive = doc["solar_pump"];
            digitalWrite(RELAY_SOLAR_PUMP, solar.solarPumpActive ? HIGH : LOW);
        }
        if (doc.containsKey("buffer_pump")) {
            solar.bufferPumpActive = doc["buffer_pump"];
            digitalWrite(RELAY_BUFFER_PUMP, solar.bufferPumpActive ? HIGH : LOW);
        }
        if (doc.containsKey("valve")) {
            bool ns = doc["valve"];
            if (ns && !solar.valveOpen) { startValveAction(RELAY_VALVE_OPEN); }
            else if (!ns && solar.valveOpen) { startValveAction(RELAY_VALVE_CLOSE); }
            solar.valveOpen = ns;
            if (!solar.valveOpen) solar.direction = "brak";
        }
        if (doc.containsKey("co_pump")) {
            solar.coPumpActive = doc["co_pump"];
            digitalWrite(RELAY_CO_PUMP, solar.coPumpActive ? HIGH : LOW);
        }
        JsonDocument res; res["status"] = "ok";
        sendJsonResponse(res); });

    server.begin();
    Serial.println("Server started");
}