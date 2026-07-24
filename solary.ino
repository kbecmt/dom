#include <dummy.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h> // Wymagane dla trwałego przechowywania ustawień
#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef WIFI_SSID
#define WIFI_SSID "ITway.dev"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "polpolpol1"
#endif

#define WIFI_STATUS_LOG_INTERVAL_MS 30000
#define WIFI_CONNECT_GRACE_MS 120000

#define WIFI_USE_STATIC_IP true
IPAddress local_IP(192, 168, 1, 139);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(1, 1, 1, 1);

#define GOOGLE_FORM_URL "https://docs.google.com/forms/u/0/d/e/1FAIpQLSeMyHb_K9o5BwSu5TI9O8MQ973W9DqwT4RfNv4NN-t1LpUDQg/formResponse"
#define GOOGLE_FORM_ENTRY_SNAPSHOT_JSON "entry.1561554265"
#define GOOGLE_FORM_ENTRY_SUMMARY "entry.2118651019"
#define GOOGLE_FORM_ENTRY_HEALTH "entry.607098449"
#define GOOGLE_FORM_LOG_INTERVAL_MS 600000

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
#define MIXER_START_RESET_MS 120000 // reset mieszacza przy starcie
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
void handleWiFi();
bool readTemps();
void updateControl();
void checkMixerTimer();
void handleGoogleFormLogging();
bool sendTempsToGoogleForm();
String buildGoogleFormSnapshotJson();
String buildGoogleFormSummary();
String buildGoogleFormHealth();
String urlEncode(const String &value);
void handleSolarPump();
void handleBufferCircuit();
void handleCO();
void mixerStep(bool open);
void updateTempHealth();
void stopSolarPump(const char *reason);
void stopBufferCircuit(const char *reason);
void stopCO(const char *reason);
bool validateSettings(float on, float off, const char *label);

SolarSystem solar;
AutomationState automationState;
unsigned long lastGoogleFormLog = 0;
bool googleFormLogPending = true;

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
    digitalWrite(RELAY_MIXER_OPEN, LOW);
    digitalWrite(RELAY_MIXER_CLOSE, HIGH);
    isMixerResetting = true;
    solar.mixerRunning = true;
    solar.mixerDirection = false;
    mixerResetEndTime = millis() + MIXER_START_RESET_MS;
    solar.mixerStepEnd = mixerResetEndTime;

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
    Serial.println("Setup OK");
}

void loop()
{
    // Obsługa resetowania mieszacza przy starcie
    if (isMixerResetting)
    {
        digitalWrite(LED, (millis() / 200) % 2); // Szybkie miganie diody
        digitalWrite(RELAY_MIXER_OPEN, LOW);
        digitalWrite(RELAY_MIXER_CLOSE, HIGH);
        if (millis() >= mixerResetEndTime)
        {
            digitalWrite(RELAY_MIXER_OPEN, LOW);
            digitalWrite(RELAY_MIXER_CLOSE, LOW);
            isMixerResetting = false;
            solar.mixerRunning = false;
            solar.mixerDirection = false;
            solar.mixerPercent = 0;
            solar.coPhase = "idle";
            digitalWrite(LED, LOW);
            Serial.println("Resetowanie mieszacza zakończone. System gotowy.");
        }
        // Nie wykonuj reszty pętli podczas resetowania.
        handleWiFi();
        return;
    }

    handleWiFi();
    handleGoogleFormLogging();

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
            googleFormLogPending = true;
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

void handleWiFi()
{
    static unsigned long lastWifiCheck = 0;
    static bool wasConnected = false;

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        if (!wasConnected)
        {
            wasConnected = true;
            Serial.print("WiFi połączone. IP: ");
            Serial.println(WiFi.localIP());
        }
        return;
    }

    wasConnected = false;

    if (millis() - lastWifiCheck >= WIFI_STATUS_LOG_INTERVAL_MS)
    {
        lastWifiCheck = millis();
        Serial.printf("WiFi status=%d, czekam na połączenie...\n", status);
        connectToWiFi();
    }
}

void connectToWiFi()
{
    static bool wifiConfigured = false;
    static bool credentialsStarted = false;
    static unsigned long lastConnectAttempt = 0;

    if (!wifiConfigured)
    {
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        wifiConfigured = true;
    }

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
        return;

    if (!credentialsStarted)
    {
        credentialsStarted = true;
        lastConnectAttempt = millis();
        Serial.print("Łączę z WiFi...");
        if (WIFI_USE_STATIC_IP && !WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
        {
            Serial.println(" Nie udało się ustawić stałego IP.");
        }
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.println(" Rozpoczęto próbę połączenia.");
        return;
    }

    if (millis() - lastConnectAttempt < WIFI_CONNECT_GRACE_MS)
        return;

    lastConnectAttempt = millis();
    Serial.print("WiFi: próba trwa zbyt długo, restartuję połączenie...");
    WiFi.disconnect(false, false);
    delay(1000);
    if (WIFI_USE_STATIC_IP && !WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
        Serial.print(" nie udało się ustawić stałego IP.");
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println(" nowa próba rozpoczęta.");
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
    if (isMixerResetting)
    {
        Serial.println("Mieszacz: ignoruję krok, trwa reset do pozycji zamkniętej.");
        return;
    }

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
// ============= GOOGLE FORMS =============

void addJsonComma(String &json, bool &first)
{
    if (!first)
        json += ",";
    first = false;
}

void addJsonFloat(String &json, const char *key, float value, bool &first)
{
    addJsonComma(json, first);
    json += "\"";
    json += key;
    json += "\":";
    if (!isValidTemp(value))
    {
        json += "null";
        return;
    }

    json += String(value, 1);
}

void addJsonNumber(String &json, const char *key, long value, bool &first)
{
    addJsonComma(json, first);
    json += "\"";
    json += key;
    json += "\":";
    json += String(value);
}

void addJsonBool(String &json, const char *key, bool value, bool &first)
{
    addJsonComma(json, first);
    json += "\"";
    json += key;
    json += "\":";
    json += value ? "true" : "false";
}

void addJsonString(String &json, const char *key, const String &value, bool &first)
{
    addJsonComma(json, first);
    json += "\"";
    json += key;
    json += "\":\"";
    for (unsigned int i = 0; i < value.length(); i++)
    {
        char c = value[i];
        if (c == '"' || c == '\\')
            json += "\\";
        json += c;
    }
    json += "\"";
}

void addJsonObjectStart(String &json, const char *key, bool &first)
{
    addJsonComma(json, first);
    json += "\"";
    json += key;
    json += "\":{";
}

String buildGoogleFormSnapshotJson()
{
    String json;
    json.reserve(1500);
    bool rootFirst = true;

    json += "{";
    addJsonNumber(json, "uptimeMs", (long)millis(), rootFirst);

    addJsonObjectStart(json, "temps", rootFirst);
    bool tempsFirst = true;
    addJsonFloat(json, "bufferTop", solar.bufferTopTemp, tempsFirst);
    addJsonFloat(json, "bufferBottom", solar.bufferBottomTemp, tempsFirst);
    addJsonFloat(json, "collector", solar.collectorTemp, tempsFirst);
    addJsonFloat(json, "waterTop", solar.waterTopTemp, tempsFirst);
    addJsonFloat(json, "waterBottom", solar.waterBottomTemp, tempsFirst);
    addJsonFloat(json, "house", solar.houseTemp, tempsFirst);
    addJsonFloat(json, "mixer", solar.mixerTemp, tempsFirst);
    addJsonFloat(json, "return", solar.returnTemp, tempsFirst);
    addJsonFloat(json, "outdoor", solar.outdoorTemp, tempsFirst);
    json += "}";

    addJsonObjectStart(json, "settings", rootFirst);
    bool settingsFirst = true;
    addJsonFloat(json, "maxWaterTemp", solar.maxWaterTemp, settingsFirst);
    addJsonFloat(json, "solarDeltaOn", solar.solarDeltaOn, settingsFirst);
    addJsonFloat(json, "solarDeltaOff", solar.solarDeltaOff, settingsFirst);
    addJsonFloat(json, "maxBufferTemp", solar.maxBufferTemp, settingsFirst);
    addJsonFloat(json, "wodaBuforDeltaOn", solar.wodaBuforDeltaOn, settingsFirst);
    addJsonFloat(json, "wodaBuforDeltaOff", solar.wodaBuforDeltaOff, settingsFirst);
    addJsonFloat(json, "buforWodaDeltaOn", solar.buforWodaDeltaOn, settingsFirst);
    addJsonFloat(json, "buforWodaDeltaOff", solar.buforWodaDeltaOff, settingsFirst);
    addJsonFloat(json, "minWodaTemp", solar.minWodaTemp, settingsFirst);
    addJsonFloat(json, "coMaxMixerTemp", solar.coMaxMixerTemp, settingsFirst);
    addJsonFloat(json, "coTargetTemp", solar.coTargetTemp, settingsFirst);
    addJsonFloat(json, "coDeltaOn", solar.coDeltaOn, settingsFirst);
    addJsonFloat(json, "coDeltaOff", solar.coDeltaOff, settingsFirst);
    json += "}";

    addJsonObjectStart(json, "state", rootFirst);
    bool stateFirst = true;
    addJsonBool(json, "solarPumpActive", solar.solarPumpActive, stateFirst);
    addJsonBool(json, "bufferPumpActive", solar.bufferPumpActive, stateFirst);
    addJsonBool(json, "valveOpen", solar.valveOpen, stateFirst);
    addJsonString(json, "direction", solar.direction, stateFirst);
    addJsonBool(json, "coPumpActive", solar.coPumpActive, stateFirst);
    addJsonNumber(json, "mixerPercent", solar.mixerPercent, stateFirst);
    addJsonBool(json, "mixerRunning", solar.mixerRunning, stateFirst);
    addJsonString(json, "mixerDirection", solar.mixerDirection ? "open" : "close", stateFirst);
    addJsonString(json, "coPhase", solar.coPhase, stateFirst);
    addJsonBool(json, "isMixerResetting", isMixerResetting, stateFirst);
    json += "}";

    addJsonObjectStart(json, "automation", rootFirst);
    bool automationFirst = true;
    addJsonBool(json, "autoCoEnabled", automationState.autoCoEnabled, automationFirst);
    addJsonBool(json, "autoSolarEnabled", automationState.autoSolarEnabled, automationFirst);
    addJsonBool(json, "autoWbEnabled", automationState.autoWbEnabled, automationFirst);
    addJsonBool(json, "autoBwEnabled", automationState.autoBwEnabled, automationFirst);
    json += "}";

    addJsonObjectStart(json, "health", rootFirst);
    bool healthFirst = true;
    addJsonBool(json, "solarTempsOk", solar.solarTempsOk, healthFirst);
    addJsonBool(json, "bufferTempsOk", solar.bufferTempsOk, healthFirst);
    addJsonBool(json, "coTempsOk", solar.coTempsOk, healthFirst);
    addJsonBool(json, "anyTempError", solar.anyTempError, healthFirst);
    json += "}";

    addJsonObjectStart(json, "wifi", rootFirst);
    bool wifiFirst = true;
    addJsonString(json, "ip", WiFi.localIP().toString(), wifiFirst);
    addJsonNumber(json, "rssi", WiFi.RSSI(), wifiFirst);
    json += "}";

    json += "}";
    return json;
}

String buildGoogleFormSummary()
{
    String summary;
    summary.reserve(220);
    summary += "kolektor=" + String(solar.collectorTemp, 1);
    summary += ", woda_gora=" + String(solar.waterTopTemp, 1);
    summary += ", woda_dol=" + String(solar.waterBottomTemp, 1);
    summary += ", bufor_gora=" + String(solar.bufferTopTemp, 1);
    summary += ", dom=" + String(solar.houseTemp, 1);
    summary += ", solar_pompa=" + String(solar.solarPumpActive ? "ON" : "OFF");
    summary += ", co_pompa=" + String(solar.coPumpActive ? "ON" : "OFF");
    return summary;
}

String buildGoogleFormHealth()
{
    if (!solar.anyTempError)
        return "OK";

    String health;
    health.reserve(80);
    health += solar.solarTempsOk ? "solar_ok" : "solar_error";
    health += ",";
    health += solar.bufferTempsOk ? "buffer_ok" : "buffer_error";
    health += ",";
    health += solar.coTempsOk ? "co_ok" : "co_error";
    return health;
}

String urlEncode(const String &value)
{
    const char *hex = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);

    for (unsigned int i = 0; i < value.length(); i++)
    {
        unsigned char c = (unsigned char)value[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += (char)c;
        }
        else if (c == ' ')
        {
            encoded += '+';
        }
        else
        {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 0x0F];
        }
    }

    return encoded;
}

void handleGoogleFormLogging()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    unsigned long now = millis();
    if (!googleFormLogPending && now - lastGoogleFormLog < GOOGLE_FORM_LOG_INTERVAL_MS)
        return;

    if (!solar.solarTempsOk)
    {
        Serial.println("Google Forms: pomijam zapis, błędne temperatury solarów.");
        googleFormLogPending = false;
        lastGoogleFormLog = now;
        return;
    }

    if (sendTempsToGoogleForm())
    {
        googleFormLogPending = false;
        lastGoogleFormLog = now;
    }
}

bool sendTempsToGoogleForm()
{
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();

    if (!http.begin(client, GOOGLE_FORM_URL))
    {
        Serial.println("Google Forms: nie można rozpocząć połączenia.");
        return false;
    }

    String snapshot = buildGoogleFormSnapshotJson();
    String summary = buildGoogleFormSummary();
    String health = buildGoogleFormHealth();

    String body;
    body.reserve(snapshot.length() * 3 + summary.length() * 3 + 96);
    body += GOOGLE_FORM_ENTRY_SNAPSHOT_JSON;
    body += "=";
    body += urlEncode(snapshot);
    body += "&";
    body += GOOGLE_FORM_ENTRY_SUMMARY;
    body += "=";
    body += urlEncode(summary);
    body += "&";
    body += GOOGLE_FORM_ENTRY_HEALTH;
    body += "=";
    body += urlEncode(health);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(body);
    http.end();

    if (code > 0 && code < 400)
    {
        Serial.printf("Google Forms: zapis OK (HTTP %d)\n", code);
        return true;
    }

    Serial.printf("Google Forms: błąd zapisu (HTTP %d)\n", code);
    return false;
}
