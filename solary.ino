#include <dummy.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // Wymagane dla trwałego przechowywania ustawień
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef WIFI_SSID
#define WIFI_SSID "ITway.dev"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "polpolpol1"
#endif

//
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
// IPAddress local_IP(192, 168, 0, 139);
// IPAddress gateway(192, 168, 0, 1);
// IPAddress subnet(255, 255, 255, 0);

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
#define LED 13               // Mieszacz zamknij
#define BUZZER 17            // Mieszacz zamknij

#define IMPULSE_MS 10000

// Optymalizacja odczytu DS18B20
#define CONVERSION_TIMEOUT 750 // Czas w ms na konwersję dla 12-bitowej rozdzielczości
enum TempState
{
    TEMP_STATE_IDLE,
    TEMP_STATE_WAITING
};
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
    int mixerPercent = 0;                 // 0-100% otwarcia mieszacza
    unsigned long mixerTimer = 0;         // licznik ms dla mieszacza
    bool mixerRunning = false;            // true = trwa ruch mieszacza
    bool mixerDirection = false;          // true = otwieranie, false = zamykanie
    unsigned long valveActionEndTime = 0; // Czas zakończenia impulsu zaworu
    bool valveActionPending = false;      // Czy impuls zaworu jest w trakcie
    unsigned long mixerStepEnd = 0;
    String coPhase = "idle";        // idle, starting, running, stopping
    unsigned long coCheckTimer = 0; // timer 30s do sprawdzania temp za mieszaczem

    bool solarTempsOk = false;
    bool bufferTempsOk = false;
    bool coTempsOk = false;
    bool anyTempError = true;
};

bool isMixerResetting = false;
unsigned long mixerResetEndTime = 0;
struct AutomationState
{
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
void mixerStep(bool open);
void updateTempHealth();
void stopSolarPump(const char *reason);
void stopBufferCircuit(const char *reason);
void stopCO(const char *reason);
void addCorsHeaders();
void sendJson(int code, const char *payload);
bool validateSettings(float on, float off, const char *label);

SolarSystem solar;
AutomationState automationState;
WebServer server(80);

// ============= LittleFS =============

void loadSettings()
{
    Preferences prefs;
    prefs.begin("solar", true); // Tryb tylko do odczytu

    solar.maxWaterTemp = prefs.getFloat("maxWaterTemp", 85.0);
    solar.solarDeltaOn = prefs.getFloat("solarDeltaOn", 8.0);
    solar.solarDeltaOff = prefs.getFloat("solarDeltaOff", 4.0);
    solar.maxBufferTemp = prefs.getFloat("maxBufferTemp", 85.0);
    solar.wodaBuforDeltaOn = prefs.getFloat("wodaBuforDeltaOn", 8.0);
    solar.wodaBuforDeltaOff = prefs.getFloat("wodaBuforDeltaOff", 4.0);
    solar.buforWodaDeltaOn = prefs.getFloat("buforWodaDeltaOn", 10.0);
    solar.buforWodaDeltaOff = prefs.getFloat("buforWodaDeltaOff", 3.0);
    solar.coMaxMixerTemp = prefs.getFloat("coMaxMixerTemp", 40.0);
    solar.coTargetTemp = prefs.getFloat("coTargetTemp", 20.0);
    solar.coDeltaOn = prefs.getFloat("coDeltaOn", 2.0);
    solar.coDeltaOff = prefs.getFloat("coDeltaOff", 1.0);
    solar.minWodaTemp = prefs.getFloat("minWodaTemp", 50.0);

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

    prefs.putFloat("maxWaterTemp", solar.maxWaterTemp);
    prefs.putFloat("solarDeltaOn", solar.solarDeltaOn);
    prefs.putFloat("solarDeltaOff", solar.solarDeltaOff);
    prefs.putFloat("maxBufferTemp", solar.maxBufferTemp);
    prefs.putFloat("wodaBuforDeltaOn", solar.wodaBuforDeltaOn);
    prefs.putFloat("wodaBuforDeltaOff", solar.wodaBuforDeltaOff);
    prefs.putFloat("buforWodaDeltaOn", solar.buforWodaDeltaOn);
    prefs.putFloat("buforWodaDeltaOff", solar.buforWodaDeltaOff);
    prefs.putFloat("coMaxMixerTemp", solar.coMaxMixerTemp);
    prefs.putFloat("coTargetTemp", solar.coTargetTemp);
    prefs.putFloat("coDeltaOn", solar.coDeltaOn);
    prefs.putFloat("coDeltaOff", solar.coDeltaOff);
    prefs.putFloat("minWodaTemp", solar.minWodaTemp);

    // Zapisz stan trybów
    prefs.putBool("autoCo", automationState.autoCoEnabled);
    prefs.putBool("autoSolar", automationState.autoSolarEnabled);
    prefs.putBool("autoWb", automationState.autoWbEnabled);
    prefs.putBool("autoBw", automationState.autoBwEnabled);

    prefs.end();
    Serial.println("Ustawienia zapisane do Preferences.");
}

bool isValidTemp(float t)
{
    return !isnan(t) && t > -55.0 && t < 125.0;
}

bool inRange(float value, float minValue, float maxValue)
{
    return !isnan(value) && value >= minValue && value <= maxValue;
}

bool validateSettings(float on, float off, const char *label)
{
    if (off >= on)
    {
        Serial.printf("Błędne nastawy %s: deltaOff >= deltaOn\n", label);
        return false;
    }
    return true;
}

void updateTempHealth()
{
    solar.solarTempsOk = isValidTemp(solar.collectorTemp) && isValidTemp(solar.waterBottomTemp) && isValidTemp(solar.waterTopTemp);
    solar.bufferTempsOk = isValidTemp(solar.waterTopTemp) && isValidTemp(solar.bufferBottomTemp) && isValidTemp(solar.bufferTopTemp);
    solar.coTempsOk = isValidTemp(solar.houseTemp) && isValidTemp(solar.mixerTemp) && isValidTemp(solar.bufferTopTemp);
    solar.anyTempError = !solar.solarTempsOk || !solar.bufferTempsOk || !solar.coTempsOk;

    if (!solar.solarTempsOk)
        Serial.println("ALARM: Niepoprawne temperatury solarów.");
    if (!solar.bufferTempsOk)
        Serial.println("ALARM: Niepoprawne temperatury obiegu bufor-woda.");
    if (!solar.coTempsOk)
        Serial.println("ALARM: Niepoprawne temperatury CO.");
}

void stopSolarPump(const char *reason)
{
    if (solar.solarPumpActive)
    {
        digitalWrite(RELAY_SOLAR_PUMP, LOW);
        solar.solarPumpActive = false;
        Serial.printf("=> Pompa solarna OFF (%s)\n", reason);
    }
}

void stopBufferCircuit(const char *reason)
{
    if (solar.bufferPumpActive)
    {
        digitalWrite(RELAY_BUFFER_PUMP, LOW);
        solar.bufferPumpActive = false;
    }

    if (solar.valveActionPending)
    {
        digitalWrite(RELAY_VALVE_OPEN, LOW);
        digitalWrite(RELAY_VALVE_CLOSE, LOW);
        solar.valveActionPending = false;
    }

    if (!solar.valveActionPending && solar.valveOpen)
    {
        digitalWrite(RELAY_VALVE_CLOSE, HIGH);
        solar.valveActionEndTime = millis() + IMPULSE_MS;
        solar.valveActionPending = true;
    }

    if (solar.valveOpen || solar.direction != "brak")
        Serial.printf("=> Obieg OFF (%s)\n", reason);

    solar.valveOpen = false;
    solar.direction = "brak";
}

void stopCO(const char *reason)
{
    if (solar.coPumpActive)
    {
        digitalWrite(RELAY_CO_PUMP, LOW);
        solar.coPumpActive = false;
        Serial.printf("=> Pompa CO OFF (%s)\n", reason);
    }

    if (solar.coPhase != "idle")
    {
        solar.coPhase = "stopping";
        Serial.printf("=> CO: STOPPING (%s)\n", reason);
    }

    if (!solar.mixerRunning && solar.mixerPercent > 0)
        mixerStep(false);
}

void addCorsHeaders()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJson(int code, const char *payload)
{
    addCorsHeaders();
    server.send(code, "application/json", payload);
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

    // Rozpocznij resetowanie mieszacza do pozycji 0
    Serial.println("Rozpoczynam resetowanie pozycji mieszacza CO...");
    digitalWrite(RELAY_MIXER_CLOSE, HIGH);
    isMixerResetting = true;
    // Użyj czasu dłuższego niż pełny cykl, aby mieć pewność
    mixerResetEndTime = millis() + MIXER_FULL_CLOSE_MS;
    solar.mixerPercent = 0; // Zakładamy, że po tym będzie na 0

    // Upewnij się, że zawór obiegu jest zamknięty przy starcie
    Serial.println("Zamykam zawór obiegu bufor-woda...");
    digitalWrite(RELAY_VALVE_CLOSE, HIGH);
    solar.valveActionPending = true;
    solar.valveActionEndTime = millis() + IMPULSE_MS;
    solar.valveOpen = false;
    solar.direction = "brak";

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
    // Obsługa resetowania mieszacza przy starcie
    if (isMixerResetting)
    {
        digitalWrite(LED, (millis() / 200) % 2); // Szybkie miganie diody
        if (millis() >= mixerResetEndTime)
        {
            digitalWrite(RELAY_MIXER_CLOSE, LOW);
            isMixerResetting = false;
            digitalWrite(LED, LOW);
            Serial.println("Resetowanie mieszacza zakończone. System gotowy.");
        }
        // Nie wykonuj reszty pętli podczas resetowania
        // Ale obsłuż klienta, żeby UI się załadowało
        server.handleClient();
        return;
    }

    // Nieblokujące sprawdzanie i ponawianie połączenia WiFi
    static unsigned long lastWifiCheck = 0;
    const unsigned long wifiCheckInterval = 30000; // Sprawdzaj co 30 sekund

    if (millis() - lastWifiCheck >= wifiCheckInterval)
    {
        lastWifiCheck = millis();
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Rozłączono z WiFi. Próbuję połączyć ponownie...");
            // Wymuś rozłączenie i ponowne połączenie od zera dla większej niezawodności
            WiFi.disconnect();
            delay(500);
            connectToWiFi();
        }
    }
    // Zapewnij maksymalną responsywność serwera WWW
    server.handleClient();

    // Użyj nieblokującego timera do regularnych zadań
    static unsigned long lastUpdate = 0;
    const unsigned long updateInterval = 20000; // Uruchamiaj logikę co 20 sekund

    // Inicjuj odczyt co `updateInterval`
    if (millis() - lastUpdate >= updateInterval)
    {
        lastUpdate = millis();
        if (readTemps())
        {                    // Sprawdź, czy poprzedni odczyt się zakończył i rozpocznij nowy
            updateControl(); // Jeśli tak, zaktualizuj logikę
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
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println(" Rozpoczęto próbę połączenia.");
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
    if (tempState == TEMP_STATE_IDLE)
    {
        // Krok 1: Wyślij żądanie konwersji do wszystkich czujników
        sensors.requestTemperatures();
        conversionStartTime = millis();
        tempState = TEMP_STATE_WAITING;
        Serial.println("DS18B20: Rozpoczęto konwersję temperatur.");
        return false; // Dane nie są jeszcze gotowe
    }

    if (tempState == TEMP_STATE_WAITING && millis() - conversionStartTime >= CONVERSION_TIMEOUT)
    {
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
        updateTempHealth();

        tempState = TEMP_STATE_IDLE; // Gotowy na następny cykl
        return true;                 // Dane są gotowe
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
    if (!solar.solarTempsOk)
    {
        stopSolarPump("błąd czujnika");
    }
    else if (!automationState.autoSolarEnabled)
    {
        stopSolarPump("tryb wyłączony");
    }
    else
    {
        // Logika sterowania pompą solarną (jeśli tryb jest włączony)
        handleSolarPump();
    }

    // --- Wspólny obieg bufor<->woda ---
    if (!solar.bufferTempsOk)
    {
        stopBufferCircuit("błąd czujnika");
    }
    else if (!automationState.autoWbEnabled && !automationState.autoBwEnabled)
    {
        // Jeśli tryb jest wyłączony, upewnij się, że obieg jest zatrzymany
        stopBufferCircuit("tryb wyłączony");
    }
    else
    {
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
    if (!solar.coTempsOk)
    {
        stopCO("błąd czujnika");
    }
    else if (!automationState.autoCoEnabled)
    {
        // Jeśli tryb CO jest wyłączony, przejdź do zatrzymywania
        if (solar.coPhase != "idle" && solar.coPhase != "stopping")
        {
            stopCO("tryb wyłączony");
        }
    }
    // Logika CO (zawsze aktywna, aby móc zakończyć cykl)
    handleCO();
}

void handleBufferCircuit()
{
    if (!solar.bufferTempsOk)
    {
        stopBufferCircuit("błąd czujnika");
        return;
    }

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

void handleSolarPump()
{
    if (!solar.solarTempsOk)
    {
        stopSolarPump("błąd czujnika");
        return;
    }

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

void handleCO()
{
    if (!solar.coTempsOk)
    {
        stopCO("błąd czujnika");
        return;
    }

    float deltaDomu = solar.coTargetTemp - solar.houseTemp;

    if (solar.coPhase == "idle")
    {
        // Jeśli system jest w stanie spoczynku, a mieszacz nie jest na pozycji 0, zamknij go.
        if (solar.mixerPercent > 0 && !solar.mixerRunning)
        {
            mixerStep(false); // Zainicjuj krok zamykający
            Serial.println("=> CO: IDLE, resetuję pozycję mieszacza do zera.");
        }
        else if (automationState.autoCoEnabled && deltaDomu > solar.coDeltaOn && solar.bufferTopTemp > solar.coTargetTemp)
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
        if (!solar.mixerRunning)
        {
            // Ponownie sprawdź warunki PRZED włączeniem pompy
            if (automationState.autoCoEnabled && deltaDomu > solar.coDeltaOn && solar.bufferTopTemp > solar.coTargetTemp)
            {
                solar.coPumpActive = true;
                digitalWrite(RELAY_CO_PUMP, HIGH);
                solar.coPhase = "running";
                solar.coCheckTimer = millis();
                Serial.println("=> CO: Warunki OK, włączam pompę. Faza: RUNNING.");
            }
            else
            {
                // Warunki przestały być spełnione, anuluj start
                solar.coPhase = "stopping";
                Serial.println("=> CO: Anulowano start, warunki niespełnione. Zamykam mieszacz.");
                if (solar.mixerPercent > 0 && !solar.mixerRunning)
                    mixerStep(false);
            }
        }
    }
    else if (solar.coPhase == "running")
    {
        // Sprawdzaj temperaturę za mieszaczem co zdefiniowany interwał, ale tylko jeśli warunki pracy są nadal spełnione
        if (!solar.mixerRunning && (millis() - solar.coCheckTimer >= CO_CHECK_INTERVAL_MS))
        {
            solar.coCheckTimer = millis();
            // Otwieraj mieszacz tylko, gdy jest potrzeba grzania (delta > deltaOff)
            if (solar.mixerTemp < solar.coMaxMixerTemp && solar.mixerPercent < 100 && deltaDomu > solar.coDeltaOff)
            {
                mixerStep(true);
                Serial.printf("=> CO: Korekta - temp. za niska (%.1f°C < %.1f°C), otwieram mieszacz.\n", solar.mixerTemp, solar.coMaxMixerTemp);
            }
            else if (solar.mixerTemp >= solar.coMaxMixerTemp && solar.mixerPercent > 0)
            {
                mixerStep(false);
                Serial.printf("=> CO: Korekta - temp. za wysoka (%.1f°C >= %.1f°C), zamykam mieszacz.\n", solar.mixerTemp, solar.coMaxMixerTemp);
            }
        }
        // Warunek wyłączenia: temperatura w domu osiągnęła zadaną (z histerezą) LUB brakuje ciepła w buforze
        bool stopCondition = (solar.coTargetTemp - solar.houseTemp) <= solar.coDeltaOff;
        if (stopCondition || solar.bufferTopTemp < solar.coTargetTemp)
        {
            solar.coPumpActive = false;
            digitalWrite(RELAY_CO_PUMP, LOW);
            solar.coPhase = "stopping";
            Serial.printf("=> CO: STOPPING (warunek: %s, bufor: %.1f°C)\n", stopCondition ? "osiągnięto temp" : "niski bufor", solar.bufferTopTemp);
            if (!solar.mixerRunning && solar.mixerPercent > 0)
                mixerStep(false);
        }
    }
    else if (solar.coPhase == "stopping")
    {
        if (!solar.mixerRunning && solar.mixerPercent > 0)
            mixerStep(false);
        if (solar.mixerPercent <= 0 && !solar.mixerRunning)
        {
            solar.coPhase = "idle";
            Serial.println("=> CO: IDLE");
        }
    }
}
// ============= SERWER =============

// Funkcja do obsługi ścieżki głównej, teraz serwująca prostą wiadomość
void handleRoot()
{
    server.send(200, "text/html", index_html);
}

void setupWebServer()
{

    server.on("/", handleRoot);
    const char *apiPaths[] = {
        "/api/data",
        "/api/solar/settings",
        "/api/buffer/settings",
        "/api/co/settings",
        "/api/automation/control",
        "/api/mixer/control",
        "/api/relay/control"};

    for (const char *path : apiPaths)
    {
        server.on(path, HTTP_OPTIONS, []()
                  {
            addCorsHeaders();
            server.send(204, "text/plain", ""); });
    }

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
        doc["health"]["solarTempsOk"] = solar.solarTempsOk;
        doc["health"]["bufferTempsOk"] = solar.bufferTempsOk;
        doc["health"]["coTempsOk"] = solar.coTempsOk;
        doc["health"]["anyTempError"] = solar.anyTempError;

        // Dodaj nowe flagi do obiektu CO, aby pasowały do app.js
        doc["co"]["autoCoEnabled"] = automationState.autoCoEnabled;
        doc["co"]["autoSolarEnabled"] = automationState.autoSolarEnabled;
        doc["co"]["autoWbEnabled"] = automationState.autoWbEnabled;
        doc["co"]["autoBwEnabled"] = automationState.autoBwEnabled;

        addCorsHeaders();
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out); });

    server.on("/api/solar/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        float maxWaterTemp = doc["maxWaterTemp"] | solar.maxWaterTemp;
        float deltaOn = doc["deltaOn"] | solar.solarDeltaOn;
        float deltaOff = doc["deltaOff"] | solar.solarDeltaOff;
        if (!inRange(maxWaterTemp, 20, 99) || !inRange(deltaOn, 1, 30) || !inRange(deltaOff, 0.5, 20) || !validateSettings(deltaOn, deltaOff, "solarów")) {
            sendJson(422, "{\"status\":\"error\", \"message\":\"Nieprawidłowe nastawy solarów\"}");
            return;
        }

        solar.maxWaterTemp = maxWaterTemp;
        solar.solarDeltaOn = deltaOn;
        solar.solarDeltaOff = deltaOff;
        saveSettings();
        sendJson(200, "{\"status\":\"ok\"}"); });

    server.on("/api/buffer/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        float maxBufferTemp = doc["maxBufferTemp"] | solar.maxBufferTemp;
        float wodaBuforDeltaOn = doc["wodaBuforDeltaOn"] | solar.wodaBuforDeltaOn;
        float wodaBuforDeltaOff = doc["wodaBuforDeltaOff"] | solar.wodaBuforDeltaOff;
        float buforWodaDeltaOn = doc["buforWodaDeltaOn"] | solar.buforWodaDeltaOn;
        float buforWodaDeltaOff = doc["buforWodaDeltaOff"] | solar.buforWodaDeltaOff;
        float minWodaTemp = doc["minWodaTemp"] | solar.minWodaTemp;
        if (!inRange(maxBufferTemp, 20, 99) || !inRange(wodaBuforDeltaOn, 1, 30) || !inRange(wodaBuforDeltaOff, 0.5, 20) ||
            !inRange(buforWodaDeltaOn, 1, 30) || !inRange(buforWodaDeltaOff, 0.5, 20) || !inRange(minWodaTemp, 20, 80) ||
            !validateSettings(wodaBuforDeltaOn, wodaBuforDeltaOff, "W-B") || !validateSettings(buforWodaDeltaOn, buforWodaDeltaOff, "B-W")) {
            sendJson(422, "{\"status\":\"error\", \"message\":\"Nieprawidłowe nastawy bufora\"}");
            return;
        }

        solar.maxBufferTemp = maxBufferTemp;
        solar.wodaBuforDeltaOn = wodaBuforDeltaOn;
        solar.wodaBuforDeltaOff = wodaBuforDeltaOff;
        solar.buforWodaDeltaOn = buforWodaDeltaOn;
        solar.buforWodaDeltaOff = buforWodaDeltaOff;
        solar.minWodaTemp = minWodaTemp;
        saveSettings();
        sendJson(200, "{\"status\":\"ok\"}"); });

    server.on("/api/co/settings", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        float maxMixerTemp = doc["maxMixerTemp"] | solar.coMaxMixerTemp;
        float targetTemp = doc["targetTemp"] | solar.coTargetTemp;
        float deltaOn = doc["deltaOn"] | solar.coDeltaOn;
        float deltaOff = doc["deltaOff"] | solar.coDeltaOff;
        if (!inRange(maxMixerTemp, 20, 80) || !inRange(targetTemp, 5, 35) || !inRange(deltaOn, 0.1, 20) ||
            !inRange(deltaOff, 0.1, 20) || !validateSettings(deltaOn, deltaOff, "CO")) {
            sendJson(422, "{\"status\":\"error\", \"message\":\"Nieprawidłowe nastawy CO\"}");
            return;
        }

        solar.coMaxMixerTemp = maxMixerTemp;
        solar.coTargetTemp = targetTemp;
        solar.coDeltaOn = deltaOn;
        solar.coDeltaOff = deltaOff;
        saveSettings();
        sendJson(200, "{\"status\":\"ok\"}"); });

    // Nowy endpoint do sterowania automatyką
    server.on("/api/automation/control", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        const char* system = doc["system"];
        if (system == nullptr || !doc["enabled"].is<bool>()) {
            sendJson(400, "{\"status\":\"error\",\"message\":\"Brak lub błędny system/enabled\"}");
            return;
        }

        bool enabled = doc["enabled"];
        bool changed = false;

        if (strcmp(system, "co") == 0) {
            changed = automationState.autoCoEnabled != enabled;
            automationState.autoCoEnabled = enabled;
        } else if (strcmp(system, "solar") == 0) {
            changed = automationState.autoSolarEnabled != enabled;
            automationState.autoSolarEnabled = enabled;
        } else if (strcmp(system, "wb") == 0) {
            changed = automationState.autoWbEnabled != enabled;
            automationState.autoWbEnabled = enabled;
        } else if (strcmp(system, "bw") == 0) {
            changed = automationState.autoBwEnabled != enabled;
            automationState.autoBwEnabled = enabled;
        } else {
            sendJson(400, "{\"status\":\"error\",\"message\":\"Unknown system\"}");
            return;
        }

        if (changed) saveSettings();
        sendJson(200, "{\"status\":\"ok\"}"); });

    server.on("/api/mixer/control", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        if (doc.containsKey("action")) {
            String action = doc["action"];
            if (action == "open") {
                if (solar.mixerPercent < 100) mixerStep(true);
                sendJson(200, "{\"status\":\"ok\", \"message\":\"Mieszacz otwierany\"}");
            } else if (action == "close") {
                if (solar.mixerPercent > 0) mixerStep(false);
                sendJson(200, "{\"status\":\"ok\", \"message\":\"Mieszacz zamykany\"}");
            } else {
                sendJson(400, "{\"status\":\"error\", \"message\":\"Nieznana akcja\"}");
            }
        } else {
            sendJson(400, "{\"status\":\"error\", \"message\":\"Brak klucza 'action'\"}");
        } });

    server.on("/api/relay/control", HTTP_POST, []()
              {
        if (!server.hasArg("plain")) { sendJson(400, "{\"status\":\"error\", \"message\":\"Brak danych\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) { sendJson(400, "{\"status\":\"error\", \"message\":\"Błąd deserializacji JSON\"}"); return; }

        if (doc.containsKey("solar_pump")) {
            solar.solarPumpActive = doc["solar_pump"];
            if (solar.solarPumpActive && !solar.solarTempsOk) {
                solar.solarPumpActive = false;
                digitalWrite(RELAY_SOLAR_PUMP, LOW);
                sendJson(409, "{\"status\":\"error\", \"message\":\"Błąd czujników solarów\"}");
                return;
            }
            digitalWrite(RELAY_SOLAR_PUMP, solar.solarPumpActive ? HIGH : LOW);
        }
        if (doc.containsKey("buffer_pump")) {
            solar.bufferPumpActive = doc["buffer_pump"];
            if (solar.bufferPumpActive && !solar.bufferTempsOk) {
                solar.bufferPumpActive = false;
                digitalWrite(RELAY_BUFFER_PUMP, LOW);
                sendJson(409, "{\"status\":\"error\", \"message\":\"Błąd czujników obiegu bufor-woda\"}");
                return;
            }
            digitalWrite(RELAY_BUFFER_PUMP, solar.bufferPumpActive ? HIGH : LOW);
        }
        if (doc.containsKey("valve")) {
            bool ns = doc["valve"];
            // Upewnij się, że nie ma już aktywnego impulsu
            if (solar.valveActionPending) {
                sendJson(409, "{\"status\":\"error\", \"message\":\"Impuls zaworu jest już w trakcie\"}");
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
            if (solar.coPumpActive && !solar.coTempsOk) {
                solar.coPumpActive = false;
                digitalWrite(RELAY_CO_PUMP, LOW);
                sendJson(409, "{\"status\":\"error\", \"message\":\"Błąd czujników CO\"}");
                return;
            }
            digitalWrite(RELAY_CO_PUMP, solar.coPumpActive ? HIGH : LOW);
        }
        sendJson(200, "{\"status\":\"ok\"}"); });

    server.begin();
    Serial.println("Server started");
}
