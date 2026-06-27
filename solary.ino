#include <dummy.h>
#include <WiFi.h>
// Zastąpienie WebServer przez ESPAsyncWebServer dla lepszej responsywności
#include <ESPAsyncWebServer.h>
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
  .then(response => {
      if (!response.ok) throw new Error('Network response was not ok');
      return response.text();
  })
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
//IPAddress local_IP(192, 168, 0, 139);
//IPAddress gateway(192, 168, 0, 1);
//IPAddress subnet(255, 255, 255, 0);

// ============= PINY GPIO =============
#define ONE_WIRE_BUS 32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/*

00:15:22.777 -> Adres: 28 DA 3A B2 A5 21 01 22 
rezerwa -> Adres: 28 D6 D7 D7 10 22 08 38 
out -> Adres: 28 4B E3 18 0D 00 00 33 
bg: 28 A7 C1 25 A6 21 01 A5 
mieszacz -> Adres: 28 77 C2 18 0D 00 00 96 


rezerwa = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF7, 0x95, 0x4F};

//DeviceAddress addrCollector = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF9, 0x9E, 0xB3 };
//DeviceAddress addrTankLow   = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0x01, 0xA7, 0x7E };
DeviceAddress addrTankHigh  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF7, 0x95, 0x4F };
DeviceAddress addrBuforLow  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0xDE, 0xEB, 0xDB };
DeviceAddress addrBuforHigh  = { 0x28, 0x61, 0x64, 0x0A, 0xF3, 0x81, 0x3F, 0x82 };
*/
DeviceAddress SENSOR_BUFFER_TOP = {0x28, 0xA7, 0xC1, 0x25, 0xA6, 0x21, 0x01, 0xA5};
DeviceAddress SENSOR_BUFFER_BOTTOM = {0x28, 0xDA, 0x3A, 0xB2, 0xA5, 0x21, 0x01, 0x22};
DeviceAddress SENSOR_COLLECTOR = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0xF9, 0x9E, 0xB3};
DeviceAddress SENSOR_WATER_TOP = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0x81, 0x3F, 0x82};
DeviceAddress SENSOR_WATER_BOTTOM = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0xDE, 0xEB, 0xDB};
DeviceAddress SENSOR_HOUSE = {0x28, 0x93, 0x78, 0xD7, 0x10, 0x22, 0x07, 0xBB};
DeviceAddress SENSOR_MIXER = {0x28, 0x77, 0xC2, 0x18, 0x0D, 0x00, 0x00, 0x96};
DeviceAddress SENSOR_RETURN = {0x28, 0x61, 0x64, 0x0A, 0xF3, 0x01, 0xA7, 0x7E};
DeviceAddress SENSOR_OUTDOOR = {0x28, 0x4B, 0xE3, 0x18, 0x0D, 0x00, 0x00, 0x33};

#define RELAY_SOLAR_PUMP 27  // Pompa solarna
#define RELAY_BUFFER_PUMP 14 // Pompa bufor<->woda
#define RELAY_VALVE_OPEN 15  // Zawór bufor<->woda
#define RELAY_VALVE_CLOSE 16 // Zawór bufor<->woda
#define RELAY_CO_PUMP 26     // Pompa CO
#define RELAY_MIXER_OPEN 33  // Mieszacz otwórz
#define RELAY_MIXER_CLOSE 25 // Mieszacz zamknij
#define LED 13 // Mieszacz zamknij
#define BUZZER 17 // Mieszacz zamknij

#define IMPULSE_MS 10000

// Optymalizacja odczytu DS18B20
#define CONVERSION_TIMEOUT 750 // Czas w ms na konwersję dla 12-bitowej rozdzielczości
enum TempState { TEMP_STATE_IDLE, TEMP_STATE_WAITING };
TempState tempState = TEMP_STATE_IDLE;
unsigned long conversionStartTime = 0;

// Mieszacz: 120s = 100% otwarcia, 130s = 100% zamknięcia
#define MIXER_FULL_OPEN_MS 120000  // czas pełnego otwarcia mieszacza
#define MIXER_FULL_CLOSE_MS 130000 //
#define MIXER_STEP_MS 5000         // czas pojedynczego kroku mieszacza (5s = ~4% otwarcia, ~3.8% zamknięcia)
#define CO_CHECK_INTERVAL_MS 30000 // Interwał sprawdzania temp. za mieszaczem (30s)

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
    unsigned long valveActionEndTime = 0; // Czas zakończenia impulsu zaworu
    bool valveActionPending = false; // Czy impuls zaworu jest w trakcie
    unsigned long mixerStepEnd = 0;
    String coPhase = "idle";        // idle, starting, running, stopping
    unsigned long coCheckTimer = 0; // timer 30s do sprawdzania temp za mieszaczem
};
struct AutomationState {
  bool autoCoEnabled = true;
  bool autoSolarEnabled = true;
  bool autoWbEnabled = true; // Woda -> Bufor
  bool autoBwEnabled = true; // Bufor -> Woda
};

// Forward declarations
void connectToWiFi();
void setupWebServer();
bool readTemps();
void updateControl();
void checkMixerTimer();
void handleSolarPump();
void handleBufferCircuit();
void handleCO();

SolarSystem solar;
AutomationState automationState;
AsyncWebServer server(80);

// ============= LittleFS =============

void loadSettings()
{
    Preferences prefs;
    prefs.begin("solar", true); // Tryb tylko do odczytu

    solar.maxWaterTemp      = prefs.getFloat("maxWaterTemp", 85.0);
    solar.solarDeltaOn      = prefs.getFloat("solarDeltaOn", 8.0);
    solar.solarDeltaOff     = prefs.getFloat("solarDeltaOff", 4.0);
    solar.maxBufferTemp     = prefs.getFloat("maxBufferTemp", 85.0);
    solar.wodaBuforDeltaOn  = prefs.getFloat("wodaBuforDeltaOn", 8.0);
    solar.wodaBuforDeltaOff = prefs.getFloat("wodaBuforDeltaOff", 4.0);
    solar.buforWodaDeltaOn  = prefs.getFloat("buforWodaDeltaOn", 10.0);
    solar.buforWodaDeltaOff = prefs.getFloat("buforWodaDeltaOff", 3.0);
    solar.coMaxMixerTemp    = prefs.getFloat("coMaxMixerTemp", 40.0); // Ujednolicona nazwa klucza
    solar.coTargetTemp      = prefs.getFloat("coTargetTemp", 20.0);
    solar.coDeltaOn         = prefs.getFloat("coDeltaOn", 2.0);
    solar.coDeltaOff        = prefs.getFloat("coDeltaOff", 1.0);
    solar.minWodaTemp       = prefs.getFloat("minWodaTemp", 50.0);

    // Wczytaj stan trybów
    automationState.autoCoEnabled = prefs.getBool("autoCo", true);
    automationState.autoSolarEnabled = prefs.getBool("autoSolar", true);
    automationState.autoWbEnabled = prefs.getBool("autoWb", true);
    automationState.autoBwEnabled = prefs.getBool("autoBw", true);

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
    prefs.putFloat("wodaBuforDeltaOn",     solar.wodaBuforDeltaOn);
    prefs.putFloat("wodaBuforDeltaOff",    solar.wodaBuforDeltaOff);
    prefs.putFloat("buforWodaDeltaOn",     solar.buforWodaDeltaOn);
    prefs.putFloat("buforWodaDeltaOff",    solar.buforWodaDeltaOff);
    prefs.putFloat("coMaxMixerTemp",  solar.coMaxMixerTemp); // Ujednolicona nazwa klucza
    prefs.putFloat("coTargetTemp",  solar.coTargetTemp);
    prefs.putFloat("coDeltaOn",     solar.coDeltaOn);
    prefs.putFloat("coDeltaOff",    solar.coDeltaOff);
    prefs.putFloat("minWodaTemp",   solar.minWodaTemp);

    // Zapisz stan trybów
    prefs.putBool("autoCo", automationState.autoCoEnabled);
    prefs.putBool("autoSolar", automationState.autoSolarEnabled);
    prefs.putBool("autoWb", automationState.autoWbEnabled);
    prefs.putBool("autoBw", automationState.autoBwEnabled);

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
    pinMode(LED, OUTPUT);
    pinMode(BUZZER, OUTPUT);

    digitalWrite(RELAY_SOLAR_PUMP, LOW);
    digitalWrite(RELAY_BUFFER_PUMP, LOW);
    digitalWrite(RELAY_VALVE_OPEN, LOW);
    digitalWrite(RELAY_VALVE_CLOSE, LOW);
    digitalWrite(RELAY_CO_PUMP, LOW);
    digitalWrite(RELAY_MIXER_OPEN, LOW);
    digitalWrite(RELAY_MIXER_CLOSE, LOW);
    digitalWrite(LED, LOW);
    digitalWrite(BUZZER, LOW);

    sensors.begin();
    // Ustaw tryb nieblokujący dla odczytu temperatur
    sensors.setWaitForConversion(false);

    loadSettings();
    connectToWiFi();
    setupWebServer();
    Serial.println("Setup OK");
}

void loop()
{
    // Nieblokujące sprawdzanie i ponawianie połączenia WiFi
    static unsigned long lastWifiCheck = 0;
    const unsigned long wifiCheckInterval = 15000; // Sprawdzaj co 15 sekund

    if (millis() - lastWifiCheck >= wifiCheckInterval) {
        lastWifiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Rozłączono z WiFi. Próbuję połączyć ponownie...");
            WiFi.reconnect();
        }
    }

    // Użyj nieblokującego timera do regularnych zadań
    static unsigned long lastRun = 0;
    const unsigned long interval = 2000; // Uruchamiaj logikę co 2 sekundy

    if (millis() - lastRun >= interval) {
        lastRun = millis();
        digitalWrite(LED, !digitalRead(LED)); // Miganie diodą jako "heartbeat"
        
        // Uruchom logikę sterowania tylko jeśli odczytano nowe temperatury
        if (readTemps()) {
            updateControl();
        }
    }

    checkMixerTimer();

    // Obsługa nieblokującego impulsu zaworu
    if (solar.valveActionPending && millis() >= solar.valveActionEndTime)
    {
        digitalWrite(RELAY_VALVE_OPEN, LOW);
        digitalWrite(RELAY_VALVE_CLOSE, LOW);
        solar.valveActionPending = false;
        Serial.println("Impuls zaworu zakończony.");
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
            // Dynamiczne obliczanie kroku
            int stepPercent = (MIXER_STEP_MS * 100) / MIXER_FULL_OPEN_MS;
            solar.mixerPercent = min(100, solar.mixerPercent + stepPercent);
        }
        else
        {
            // Dynamiczne obliczanie kroku
            int stepPercent = (MIXER_STEP_MS * 100) / MIXER_FULL_CLOSE_MS;
            solar.mixerPercent = max(0, solar.mixerPercent - stepPercent);
        }
        Serial.printf("Mieszacz: %d%%\n", solar.mixerPercent);
    }
}

// ============= WiFi =============

void connectToWiFi()
{
    Serial.print("Łączę z WiFi...");

    //WiFi.config(local_IP, gateway, subnet);
    WiFi.setAutoReconnect(true);
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
        WiFi.reconnect();
    }
}

// ============= CZUJNIKI =============

float getTemp(const DeviceAddress addr, const char *label)
{
    float t = sensors.getTempC(addr);
    if (t == DEVICE_DISCONNECTED_C)
    {
        Serial.printf("%s: BŁĄD ODCZYTU\n", label);
        return NAN;
    }
    Serial.printf("%s: %.1f°C\n", label, t);
    return t;
}

bool readTemps()
{
    if (tempState == TEMP_STATE_IDLE) {
        // Krok 1: Wyślij żądanie konwersji do wszystkich czujników
        sensors.requestTemperatures();
        conversionStartTime = millis();
        tempState = TEMP_STATE_WAITING;
        Serial.println("DS18B20: Rozpoczęto konwersję temperatur.");
        return false; // Dane nie są jeszcze gotowe
    } 
    
    if (tempState == TEMP_STATE_WAITING && millis() - conversionStartTime >= CONVERSION_TIMEOUT) {
        // Krok 2: Czas na konwersję minął, odczytaj dane
        Serial.println("DS18B20: Odczytuję temperatury po konwersji.");
        solar.bufferTopTemp = getTemp(SENSOR_BUFFER_TOP, "Bufor Góra");
        solar.bufferBottomTemp = getTemp(SENSOR_BUFFER_BOTTOM, "Bufor Dół");
        solar.collectorTemp = getTemp(SENSOR_COLLECTOR, "Kolektor");
        solar.waterTopTemp = getTemp(SENSOR_WATER_TOP, "Woda Góra");
        solar.waterBottomTemp = getTemp(SENSOR_WATER_BOTTOM, "Woda Dół");
        solar.houseTemp = getTemp(SENSOR_HOUSE, "Dom");
        solar.mixerTemp = getTemp(SENSOR_MIXER, "Mieszacz");
        solar.returnTemp = getTemp(SENSOR_RETURN, "Powrót");
        solar.outdoorTemp = getTemp(SENSOR_OUTDOOR, "Zewn");
        
        tempState = TEMP_STATE_IDLE; // Gotowy na następny cykl
        return true; // Dane są gotowe
    }

    return false; // Dane nie są jeszcze gotowe (oczekiwanie)
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
    if (!automationState.autoSolarEnabled) {
        if (solar.solarPumpActive) {
            solar.solarPumpActive = false;
            digitalWrite(RELAY_SOLAR_PUMP, LOW);
            Serial.println("=> Pompa solarna OFF (tryb wyłączony)");
        }
    } else {
        // Logika sterowania pompą solarną (jeśli tryb jest włączony)
        handleSolarPump();
    }

    // --- Wspólny obieg bufor<->woda ---
    if (!automationState.autoWbEnabled && !automationState.autoBwEnabled) {
        // Jeśli tryb jest wyłączony, upewnij się, że obieg jest zatrzymany
        if (!solar.valveActionPending && (solar.bufferPumpActive || solar.valveOpen)) {
            digitalWrite(RELAY_BUFFER_PUMP, LOW);
            digitalWrite(RELAY_VALVE_CLOSE, HIGH);
            solar.valveActionEndTime = millis() + IMPULSE_MS;
            solar.valveActionPending = true;

            solar.bufferPumpActive = false;
            solar.valveOpen = false;
            solar.direction = "brak";
            Serial.println("=> Obieg OFF (tryb wyłączony)");
        }
    } else {
        // Logika sterowania obiegiem (jeśli tryb jest włączony)
        handleBufferCircuit();
    }

    // --- CO z mieszaczem - nowa logika ---
    // 1. IDLE: temp zadana - temp domu > deltaOn → otwórz mieszacz 5s → STARTING
    // 2. STARTING: po 5s włącz pompę CO → RUNNING
    // 3. RUNNING: co 30s sprawdzaj temp za mieszaczem:
    //    - < maxMixerTemp → otwórz na 5s
    //    - > maxMixerTemp → zamknij na 5s
    //    - temp zadana - temp domu <= deltaOff → STOPPING
    // 4. STOPPING: zamknij mieszacz do 0% → IDLE
    if (!automationState.autoCoEnabled) {
        // Jeśli tryb CO jest wyłączony, przejdź do zatrzymywania
        if (solar.coPhase != "idle" && solar.coPhase != "stopping") {
            solar.coPumpActive = false;
            digitalWrite(RELAY_CO_PUMP, LOW);
            solar.coPhase = "stopping";
            Serial.println("=> CO: STOPPING (tryb wyłączony)");
            if (!solar.mixerRunning && solar.mixerPercent > 0) {
                mixerStep(false); // zacznij zamykać
            }
        }
    }
    // Logika CO (zawsze aktywna, aby móc zakończyć cykl)
    handleCO();
}

void handleBufferCircuit() {

    // --- Wspólny obieg bufor<->woda ---
    float dWB = solar.waterTopTemp - solar.bufferBottomTemp;
    float dBW = solar.bufferTopTemp - solar.waterTopTemp;

    bool shouldWodaBufor = automationState.autoWbEnabled && (dWB > solar.wodaBuforDeltaOn && solar.waterTopTemp < solar.maxBufferTemp && solar.waterTopTemp > solar.minWodaTemp);
    bool shouldBuforWoda = automationState.autoBwEnabled && (dBW > solar.buforWodaDeltaOn && solar.waterTopTemp < solar.minWodaTemp);

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
        if (!solar.valveActionPending && (!solar.bufferPumpActive || !solar.valveOpen || solar.direction != newDir))
        {
            // Start nieblokującego impulsu otwarcia
            digitalWrite(RELAY_VALVE_OPEN, HIGH);
            solar.valveActionEndTime = millis() + IMPULSE_MS;
            solar.valveActionPending = true;
            
            // Włączamy pompę od razu lub po zakończeniu impulsu (tutaj: od razu dla uproszczenia)
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

        if (shouldStop && !solar.valveActionPending && (solar.bufferPumpActive || solar.valveOpen))
        {
            // Najpierw wyłączamy pompę
            digitalWrite(RELAY_BUFFER_PUMP, LOW);
            
            // Start nieblokującego impulsu zamykania
            digitalWrite(RELAY_VALVE_CLOSE, HIGH);
            solar.valveActionEndTime = millis() + IMPULSE_MS;
            solar.valveActionPending = true;

            solar.bufferPumpActive = false;
            solar.valveOpen = false;
            solar.direction = "brak";
            Serial.println("=> Obieg OFF");
        }
    }
}

void handleSolarPump() {
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
}

void handleCO() {
    float deltaDomu = solar.coTargetTemp - solar.houseTemp;

    if (solar.coPhase == "idle") {
        if (automationState.autoCoEnabled && deltaDomu > solar.coDeltaOn && solar.bufferTopTemp > solar.coTargetTemp) {
            solar.coPhase = "starting";
            if (!solar.mixerRunning && solar.mixerPercent < 100) {
                mixerStep(true); // otwórz na 5s
            }
            Serial.printf("=> CO: STARTING (delta=%.1f > %.1f)\n", deltaDomu, solar.coDeltaOn);
        }
    } else if (solar.coPhase == "starting") {
        if (!solar.mixerRunning) {
            solar.coPumpActive = true;
            digitalWrite(RELAY_CO_PUMP, HIGH);
            solar.coPhase = "running";
            solar.coCheckTimer = millis();
            Serial.println("=> CO: RUNNING, pompa ON");
        }
    } else if (solar.coPhase == "running") { // --- Faza pracy ---
        // Sprawdzaj temperaturę za mieszaczem co zdefiniowany interwał
        if (!solar.mixerRunning && (millis() - solar.coCheckTimer >= CO_CHECK_INTERVAL_MS)) {
            solar.coCheckTimer = millis();
            if (solar.mixerTemp < solar.coMaxMixerTemp && solar.mixerPercent < 100) {
                mixerStep(true);
                Serial.printf("=> CO: Korekta - temp. za niska (%.1f°C < %.1f°C), otwieram mieszacz.\n", solar.mixerTemp, solar.coMaxMixerTemp);
            } else if (solar.mixerTemp >= solar.coMaxMixerTemp && solar.mixerPercent > 0) {
                mixerStep(false);
                Serial.printf("=> CO: Korekta - temp. za wysoka (%.1f°C >= %.1f°C), zamykam mieszacz.\n", solar.mixerTemp, solar.coMaxMixerTemp);
            }
        }
        // Warunek wyłączenia: temperatura w domu osiągnęła zadaną (z histerezą) LUB brakuje ciepła w buforze
        bool stopCondition = (solar.coTargetTemp - solar.houseTemp) <= solar.coDeltaOff;
        if (stopCondition || solar.bufferTopTemp < solar.coTargetTemp) {
            solar.coPumpActive = false;
            digitalWrite(RELAY_CO_PUMP, LOW);
            solar.coPhase = "stopping";
            Serial.printf("=> CO: STOPPING (powrót=%.1f >= zadana=%.1f)\n", solar.returnTemp, solar.coTargetTemp);
            if (!solar.mixerRunning && solar.mixerPercent > 0) mixerStep(false);
        }
    } else if (solar.coPhase == "stopping") {
        if (!solar.mixerRunning && solar.mixerPercent > 0) mixerStep(false);
        if (solar.mixerPercent <= 0 && !solar.mixerRunning) { solar.coPhase = "idle"; Serial.println("=> CO: IDLE"); }
    }
}
// ============= SERWER =============

// Funkcja do obsługi ścieżki głównej, teraz serwująca prostą wiadomość
void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_html);
}

void setupWebServer()
{
    // Obsługa ścieżki głównej
    server.on("/", HTTP_GET, handleRoot);

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

        // Dodaj nowe flagi do obiektu CO, aby pasowały do app.js
        doc["co"]["autoCoEnabled"] = automationState.autoCoEnabled;
        doc["co"]["autoSolarEnabled"] = automationState.autoSolarEnabled;
        doc["co"]["autoWbEnabled"] = automationState.autoWbEnabled;
        doc["co"]["autoBwEnabled"] = automationState.autoBwEnabled;

        String out; serializeJson(doc, out);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", out);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // Handler dla ustawień z ciałem JSON (AsyncWebServer)
    AsyncCallbackJsonWebHandler* solarSettingsHandler = new AsyncCallbackJsonWebHandler("/api/solar/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        if (doc.containsKey("maxWaterTemp")) solar.maxWaterTemp = doc["maxWaterTemp"];
        if (doc.containsKey("deltaOn")) solar.solarDeltaOn = doc["deltaOn"];
        if (doc.containsKey("deltaOff")) solar.solarDeltaOff = doc["deltaOff"];
        saveSettings();
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
    server.addHandler(solarSettingsHandler);

    AsyncCallbackJsonWebHandler* bufferSettingsHandler = new AsyncCallbackJsonWebHandler("/api/buffer/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        if (doc.containsKey("maxBufferTemp")) solar.maxBufferTemp = doc["maxBufferTemp"];
        if (doc.containsKey("wodaBuforDeltaOn")) solar.wodaBuforDeltaOn = doc["wodaBuforDeltaOn"];
        if (doc.containsKey("wodaBuforDeltaOff")) solar.wodaBuforDeltaOff = doc["wodaBuforDeltaOff"];
        if (doc.containsKey("buforWodaDeltaOn")) solar.buforWodaDeltaOn = doc["buforWodaDeltaOn"];
        if (doc.containsKey("buforWodaDeltaOff")) solar.buforWodaDeltaOff = doc["buforWodaDeltaOff"];
        if (doc.containsKey("minWodaTemp")) solar.minWodaTemp = doc["minWodaTemp"];
        saveSettings();
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
    server.addHandler(bufferSettingsHandler);

    AsyncCallbackJsonWebHandler* coSettingsHandler = new AsyncCallbackJsonWebHandler("/api/co/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        if (doc.containsKey("maxMixerTemp")) solar.coMaxMixerTemp = doc["maxMixerTemp"];
        if (doc.containsKey("targetTemp")) solar.coTargetTemp = doc["targetTemp"];
        if (doc.containsKey("deltaOn")) solar.coDeltaOn = doc["deltaOn"];
        if (doc.containsKey("deltaOff")) solar.coDeltaOff = doc["deltaOff"];
        saveSettings();
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
    server.addHandler(coSettingsHandler);

    AsyncCallbackJsonWebHandler* automationControlHandler = new AsyncCallbackJsonWebHandler("/api/automation/control", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        const char* system = doc["system"];
        bool enabled = doc["enabled"];
        if (strcmp(system, "co") == 0) {
            automationState.autoCoEnabled = enabled;
        } else if (strcmp(system, "solar") == 0) {
            automationState.autoSolarEnabled = enabled;
        } else if (strcmp(system, "wb") == 0) {
            automationState.autoWbEnabled = enabled;
        } else if (strcmp(system, "bw") == 0) {
            automationState.autoBwEnabled = enabled;
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown system\"}");
            return;
        }
        saveSettings();
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
    server.addHandler(automationControlHandler);

    AsyncCallbackJsonWebHandler* mixerControlHandler = new AsyncCallbackJsonWebHandler("/api/mixer/control", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        if (doc.containsKey("action")) {
            String action = doc["action"];
            if (action == "open") {
                if (solar.mixerPercent < 100) mixerStep(true);
                request->send(200, "application/json", "{\"status\":\"ok\", \"message\":\"Mieszacz otwierany\"}");
            } else if (action == "close") {
                if (solar.mixerPercent > 0) mixerStep(false);
                request->send(200, "application/json", "{\"status\":\"ok\", \"message\":\"Mieszacz zamykany\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Nieznana akcja\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Brak klucza 'action'\"}");
        }
    });
    server.addHandler(mixerControlHandler);

    AsyncCallbackJsonWebHandler* relayControlHandler = new AsyncCallbackJsonWebHandler("/api/relay/control", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
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
            if (solar.valveActionPending) {
                request->send(409, "application/json", "{\"status\":\"error\", \"message\":\"Impuls zaworu jest już w trakcie\"}");
                return;
            }
            if (ns && !solar.valveOpen) {
                digitalWrite(RELAY_VALVE_OPEN, HIGH);
                solar.valveActionEndTime = millis() + IMPULSE_MS; solar.valveActionPending = true;
            } else if (!ns && solar.valveOpen) {
                digitalWrite(RELAY_VALVE_CLOSE, HIGH);
                solar.valveActionEndTime = millis() + IMPULSE_MS; solar.valveActionPending = true;
            }
            solar.valveOpen = ns;
            if (!solar.valveOpen) solar.direction = "brak";
        }
        if (doc.containsKey("co_pump")) {
            solar.coPumpActive = doc["co_pump"];
            digitalWrite(RELAY_CO_PUMP, solar.coPumpActive ? HIGH : LOW);
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    server.addHandler(relayControlHandler);

    // Obsługa zapytań pre-flight CORS dla wszystkich ścieżek
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
    server.begin();
    Serial.println("Server started");
}