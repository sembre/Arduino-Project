#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <update.h>

// ==================== KONFIGURASI PIN ====================
#define SENSOR_IN_PIN 4
#define SENSOR_OUT_PIN 5
#define RELAY_PIN 18       // Aktif HIGH - Pin 18
#define RESET_BUTTON_PIN 0 // GPIO 0 = Boot button (bawaan ESP32)

// ==================== KONFIGURASI WIFI & MQTT ====================
Preferences prefs;
WebServer server(80);

String ssid = "";
String password = "";
const char *apSSID = "ESP32-WaterMeter";
const char *apPASS = "12345678";
const char *deviceHostname = "tandon-air";

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_client = "TandonAirAgus";

WiFiClient espClient;
PubSubClient mqtt(espClient);
unsigned long mqttConnectTime = 0;
const long MQTT_SETTLING_TIME = 10000;

// ==================== FLOW SENSOR VARIABLES ====================
volatile unsigned long pulseCount_IN = 0;
volatile unsigned long pulseCount_OUT = 0;
volatile unsigned long lastPulseTime_IN = 0;
volatile unsigned long lastPulseTime_OUT = 0;

const float PULSE_FACTOR = 2.67;
const float MAX_FLOW_RATE = 30.0;

float flowRate_IN = 0.0;
float flowRate_OUT = 0.0;
float volume_IN = 0.0;
float volume_OUT = 0.0;

unsigned long lastFlowTime = 0;
const unsigned long flowInterval = 1000;
unsigned long lastMQTTPublish = 0;
const unsigned long mqttPublishInterval = 5000;
unsigned long lastMQTTRetry = 0;
const unsigned long mqttRetryInterval = 30000;
unsigned long lastVolumeSave = 0;
const unsigned long volumeSaveInterval = 30000; // Save volume setiap 30 detik ke EEPROM

// ==================== RELAY VARIABLES ====================
bool relayState = false;
unsigned long wifiCheckTime = 0;
bool timeSynced = false;
bool wifiConnected = false;
unsigned long relayOnTime = 0;
unsigned long autoOffTimeout = 120000;  // 2 menit dalam millisecond
unsigned long relayGracePeriod = 30000; // Grace period 30 detik setelah relay ON
float minFlowRateThreshold = 5.0;       // Threshold flowRate_IN untuk auto OFF (L/min)
float timeoutFlowThreshold = 0.0;       // Flow <= nilai ini memakai auto-off timeout (L/min)

// ==================== RESET BUTTON VARIABLES ====================
bool resetButtonPressed = false;
unsigned long resetButtonPressTime = 0;
const unsigned long FACTORY_RESET_THRESHOLD = 5000; // 5 detik
bool factoryResetTriggered = false;

// ==================== TANDON CAPACITY ====================
float maxCapacity = 1000.0;

// ==================== WIFI MANAGER VARIABLES ====================
int wifiScanCount = 0;
String scannedNetworks[20];
int scannedSignals[20];
bool isAPMode = false;
bool mdnsActive = false;
unsigned long wifiLastRetry = 0;
const unsigned long wifiRetryTimeout = 20000;
unsigned long wifiDisconnectedSince = 0;
const unsigned long wifiAPFallbackDelay = 15000;
unsigned long wifiStaConnectedSince = 0;
const unsigned long wifiStaStableTime = 5000;

// ==================== OTA UPDATE VARIABLES ====================
bool otaActive = false;
bool otaInitialized = false;
bool httpOtaActive = false;
bool httpOtaShouldReboot = false;
unsigned long otaStartTime = 0;
unsigned long otaLastRetry = 0;
const unsigned long OTA_TIMEOUT = 600000;       // 10 menit timeout untuk OTA
const unsigned long OTA_RETRY_INTERVAL = 15000; // Retry OTA every 15 seconds
String otaErrorMessage = "";
String otaStatusText = "idle";
int httpOtaStatusCode = 200;
String httpOtaResponse = "{\"status\":\"idle\"}";

// ==================== SETUP STATE MACHINE ====================
enum SetupState
{
    SETUP_PINS,
    SETUP_INTERRUPTS,
    SETUP_PREFS,
    SETUP_WIFI,
    SETUP_MQTT,
    SETUP_SERVER,
    SETUP_OTA,
    SETUP_VARS,
    SETUP_DONE
};
SetupState setupState = SETUP_PINS;
unsigned long setupTime = 0;

// ==================== FUNCTION PROTOTYPES ====================
void IRAM_ATTR countPulse_IN();
void IRAM_ATTR countPulse_OUT();
void setupWebServer();
void handleRoot();
void handleAPI();
void handleRelayControl();
void handleReset();
void handleConfigPage();
void handleSaveWiFi();
void handleWiFiStatus();
void handleScanWiFi();
void handleSelectWiFi();
void handleSettingsPage();
void handleGetSettings();
void handleSaveCapacity();
void handleSaveVolume();
void handleSaveFlowThreshold();
void handleSaveAutoOffSettings();
void saveVolumeToEEPROM();
void startAP();
void connectToWiFi();
void setupMDNS(bool enableOtaService);
void scanAvailableNetworks();
void syncTime();
void reconnectMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publishToMQTT();
void updateFlowMeasurements();
void checkAutoOffRelay();
void checkResetButton();
void factoryReset();
void setupOTA();
void handleOTAStartPage();
void handleOTAStatus();
void handleOTAUpdate();
void handleOTAUpload();
void finishHttpOTAError(int statusCode, const char *message);

// ==================== SETUP (DIPERBAIKI) ====================
void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n=== TANDON AIR METER SYSTEM (ESP32-S3) ===");
    Serial.println("[Setup] Starting initialization sequence...");
    setupTime = millis();
    // State machine akan berjalan di loop, setup hanya mencatat waktu mulai
}

// ==================== LOOP ====================
void loop()
{
    if (setupState != SETUP_DONE)
    {
        switch (setupState)
        {
        case SETUP_PINS:
            Serial.println("[Setup] Configuring pins...");
            pinMode(SENSOR_IN_PIN, INPUT_PULLUP);
            pinMode(SENSOR_OUT_PIN, INPUT_PULLUP);
            pinMode(RELAY_PIN, OUTPUT);
            pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); // Reset button dengan pull-up
            digitalWrite(RELAY_PIN, LOW);            // relay OFF (aktif HIGH)
            relayState = false;

            // --- Debug test pin (tambahan untuk verifikasi) ---
            Serial.printf("Relay pin %d initial state = %d (should be LOW)\n", RELAY_PIN, digitalRead(RELAY_PIN));
            delay(100);
            digitalWrite(RELAY_PIN, HIGH);
            Serial.printf("After HIGH, state = %d\n", digitalRead(RELAY_PIN));
            delay(100);
            digitalWrite(RELAY_PIN, LOW);
            Serial.printf("Back to LOW, state = %d\n", digitalRead(RELAY_PIN));
            // ------------------------------------------------

            setupState = SETUP_INTERRUPTS;
            break;

        case SETUP_INTERRUPTS:
            Serial.println("[Setup] Attaching interrupts...");
            attachInterrupt(digitalPinToInterrupt(SENSOR_IN_PIN), countPulse_IN, RISING);
            attachInterrupt(digitalPinToInterrupt(SENSOR_OUT_PIN), countPulse_OUT, RISING);
            setupState = SETUP_PREFS;
            break;

        case SETUP_PREFS:
            Serial.println("[Setup] Loading WiFi config...");
            prefs.begin("wifi", true);
            ssid = prefs.getString("ssid", "");
            password = prefs.getString("pass", "");
            prefs.end();

            prefs.begin("tandon", true);
            maxCapacity = prefs.getFloat("capacity", 1000.0);
            minFlowRateThreshold = prefs.getFloat("flow_threshold", 5.0);
            timeoutFlowThreshold = prefs.getFloat("timeout_flow", 0.0);
            relayGracePeriod = prefs.getULong("grace_period", 30000);
            autoOffTimeout = prefs.getULong("auto_timeout", 120000);
            volume_IN = prefs.getFloat("volume_in", 0.0);
            volume_OUT = prefs.getFloat("volume_out", 0.0);
            prefs.end();
            Serial.printf("[Setup] Capacity loaded: %.2f L\n", maxCapacity);
            Serial.printf("[Setup] Flow Threshold loaded: %.2f L/min\n", minFlowRateThreshold);
            Serial.printf("[Setup] Timeout Flow Threshold loaded: %.2f L/min\n", timeoutFlowThreshold);
            Serial.printf("[Setup] Relay Grace Period loaded: %.0f detik\n", relayGracePeriod / 1000.0);
            Serial.printf("[Setup] Auto OFF Timeout loaded: %.0f detik\n", autoOffTimeout / 1000.0);
            Serial.printf("[Setup] Volume restored: IN=%.2f L, OUT=%.2f L\n", volume_IN, volume_OUT);

            setupState = SETUP_WIFI;
            break;

        case SETUP_WIFI:
            Serial.println("[Setup] Starting WiFi...");
            WiFi.mode(WIFI_STA);
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            WiFi.setSleep(false);
            WiFi.setHostname(deviceHostname);

            if (ssid.length() > 0)
            {
                WiFi.begin(ssid.c_str(), password.c_str());
                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
                {
                    delay(100);
                    yield();
                }
                if (WiFi.status() == WL_CONNECTED)
                {
                    Serial.println("[WiFi] Connected! IP: " + WiFi.localIP().toString());
                    wifiConnected = true;
                    isAPMode = false;
                    wifiDisconnectedSince = 0;
                    wifiStaConnectedSince = millis();
                    // Initialize OTA immediately after WiFi connects
                    setupOTA();
                }
                else
                {
                    Serial.println("[WiFi] Failed to connect, starting AP mode...");
                    startAP();
                }
            }
            else
            {
                startAP();
            }
            setupState = SETUP_MQTT;
            break;

        case SETUP_MQTT:
            Serial.println("[Setup] Configuring MQTT...");
            mqtt.setServer(mqtt_server, mqtt_port);
            mqtt.setCallback(mqttCallback);
            mqtt.setKeepAlive(60);
            mqtt.setSocketTimeout(2);
            setupState = SETUP_SERVER;
            break;

        case SETUP_SERVER:
            Serial.println("[Setup] Starting web server...");
            setupWebServer();
            server.begin();
            setupState = SETUP_OTA;
            break;

        case SETUP_OTA:
            Serial.println("[Setup] OTA initialized with WiFi...");
            setupState = SETUP_VARS;
            break;

        case SETUP_VARS:
            Serial.println("[Setup] Initializing variables...");
            lastFlowTime = millis();
            lastMQTTPublish = millis();
            lastMQTTRetry = millis();
            timeSynced = false;
            setupState = SETUP_DONE;
            Serial.println("[Setup] System ready!");
            break;

        case SETUP_DONE:
            break;
        }
        delay(10);
        yield();
        return;
    }

    unsigned long currentMillis = millis();

    // *** CRITICAL: Handle OTA first - skip ALL other operations during active transfer ***
    if (otaActive)
    {
        // HTTP OTA still needs the web server to receive firmware chunks.
        if (httpOtaActive)
        {
            server.handleClient();

            if (millis() - otaStartTime >= 900000) // 15 minutes
            {
                Serial.println("[OTA HTTP] Timeout - upload session closed");
                Update.abort();
                otaActive = false;
                httpOtaActive = false;
                otaStatusText = "error";
                otaErrorMessage = "HTTP OTA timeout";
            }

            delay(1);
            yield();
            return;
        }

        // During ArduinoOTA transfer, only call OTA handler - nothing else
        if (otaInitialized)
        {
            ArduinoOTA.handle();

            // Debug heartbeat every 2 seconds during OTA (reduced frequency)
            static unsigned long lastHeartbeat = 0;
            if (millis() - lastHeartbeat >= 2000)
            {
                Serial.printf("[OTA] Heartbeat - WiFi: %d, RSSI: %d dBm, Time: %lu ms\n",
                              WiFi.status(), WiFi.RSSI(), millis() - otaStartTime);
                lastHeartbeat = millis();
            }
        }

        // Check timeout
        if (millis() - otaStartTime >= 900000) // 15 minutes
        {
            Serial.println("[OTA] Timeout - OTA session closed");
            otaActive = false;
            otaInitialized = false;
        }

        // Reduced delay for better responsiveness
        delayMicroseconds(50);
        yield();
        return; // EXIT LOOP - Don't do anything else during OTA
    }

    // Normal operation (not in active OTA transfer)
    // Always call ArduinoOTA.handle() when initialized to listen for OTA requests
    if (wifiConnected && !isAPMode && otaInitialized)
    {
        ArduinoOTA.handle();
    }

    // WiFi status check and reconnection (but don't reconnect during active OTA)
    if (currentMillis - wifiCheckTime >= 5000)
    {
        wifiCheckTime = currentMillis;
        if (WiFi.status() == WL_CONNECTED)
        {
            if (!wifiConnected)
            {
                wifiConnected = true;
                wifiDisconnectedSince = 0;
                wifiStaConnectedSince = currentMillis;
                Serial.println("[WiFi] Connected!");
                Serial.println("[WiFi] IP: " + WiFi.localIP().toString());

                if (!isAPMode && !otaInitialized)
                {
                    setupOTA();
                }
                syncTime();
                timeSynced = true;
            }

            if (isAPMode && wifiStaConnectedSince > 0 && (currentMillis - wifiStaConnectedSince >= wifiStaStableTime))
            {
                WiFi.softAPdisconnect(true);
                isAPMode = false;
                Serial.println("[WiFi] Router connection stable, fallback AP stopped");

                if (!otaInitialized)
                {
                    setupOTA();
                }
            }
        }
        else
        {
            if (wifiDisconnectedSince == 0)
            {
                wifiDisconnectedSince = currentMillis;
            }
            wifiStaConnectedSince = 0;

            if (wifiConnected)
            {
                wifiConnected = false;
                otaInitialized = false;
                ArduinoOTA.end();
                Serial.println("[WiFi] Disconnected!");
            }

            if (!otaActive && !isAPMode && (currentMillis - wifiDisconnectedSince >= wifiAPFallbackDelay))
            {
                Serial.println("[WiFi] Router/AP not reachable, starting fallback AP mode...");
                startAP();
            }

            // Keep retrying the saved network while the router/AP is down.
            if (!otaActive && ssid.length() > 0 && (currentMillis - wifiLastRetry >= wifiRetryTimeout))
            {
                wifiLastRetry = currentMillis;
                Serial.printf("[WiFi] Reconnecting to saved network: %s\n", ssid.c_str());

                if (!isAPMode)
                {
                    WiFi.mode(WIFI_STA);
                    WiFi.disconnect(false, false);
                }
                WiFi.setAutoReconnect(true);
                WiFi.setSleep(false);
                WiFi.setHostname(deviceHostname);
                WiFi.begin(ssid.c_str(), password.c_str());
            }
        }
    }

    // MQTT operations (safe to do, OTA not active)
    if (!mqtt.connected() && wifiConnected && (currentMillis - lastMQTTRetry >= mqttRetryInterval))
    {
        reconnectMQTT();
        lastMQTTRetry = currentMillis;
    }

    if (mqtt.connected())
    {
        mqtt.loop();
    }

    // Handle web server
    server.handleClient();

    // Skip heavy operations during active OTA transfer to prevent blocking
    if (!otaActive)
    {
        // Update flow measurements and check relay only when not doing OTA
        updateFlowMeasurements();
        checkAutoOffRelay(); // Check apakah relay perlu auto-off
        checkResetButton();  // Check reset button untuk factory reset

        // Publish MQTT
        if (wifiConnected && !isAPMode)
        {
            if (currentMillis - lastMQTTPublish >= mqttPublishInterval)
            {
                publishToMQTT();
                lastMQTTPublish = currentMillis;
            }
        }

        // Save volume data to EEPROM every 30 seconds
        if (currentMillis - lastVolumeSave >= volumeSaveInterval)
        {
            saveVolumeToEEPROM();
            lastVolumeSave = currentMillis;
        }
    }

    // Minimal delay for stability
    delay(5);
    yield();
}

// ==================== INTERRUPT HANDLERS ====================
void IRAM_ATTR countPulse_IN()
{
    unsigned long now = (unsigned long)esp_timer_get_time();
    if (now - lastPulseTime_IN >= 5000)
    {
        pulseCount_IN++;
        lastPulseTime_IN = now;
    }
}

void IRAM_ATTR countPulse_OUT()
{
    unsigned long now = (unsigned long)esp_timer_get_time();
    if (now - lastPulseTime_OUT >= 5000)
    {
        pulseCount_OUT++;
        lastPulseTime_OUT = now;
    }
}

// ==================== FLOW MEASUREMENTS ====================
void updateFlowMeasurements()
{
    unsigned long now = millis();
    if (now - lastFlowTime >= flowInterval)
    {
        unsigned long timeDiff = now - lastFlowTime;
        if (timeDiff > 0)
        {
            noInterrupts();
            unsigned long temp_IN = pulseCount_IN;
            unsigned long temp_OUT = pulseCount_OUT;
            pulseCount_IN = 0;
            pulseCount_OUT = 0;
            interrupts();

            float timeMinutes = timeDiff / 60000.0;
            if (timeMinutes > 0)
            {
                flowRate_IN = (temp_IN * PULSE_FACTOR / 1000.0) / timeMinutes;
                flowRate_OUT = (temp_OUT * PULSE_FACTOR / 1000.0) / timeMinutes;
                if (flowRate_IN > MAX_FLOW_RATE)
                    flowRate_IN = MAX_FLOW_RATE;
                if (flowRate_OUT > MAX_FLOW_RATE)
                    flowRate_OUT = MAX_FLOW_RATE;
            }
            volume_IN += temp_IN * PULSE_FACTOR / 1000.0;
            volume_OUT += temp_OUT * PULSE_FACTOR / 1000.0;
            if (volume_IN > 1000000)
                volume_IN = 0;
            if (volume_OUT > 1000000)
                volume_OUT = 0;
        }
        lastFlowTime = now;
        Serial.printf("[Flow] IN: %.2f L/min | OUT: %.2f L/min | Vol IN: %.2f L | Vol OUT: %.2f L\n",
                      flowRate_IN, flowRate_OUT, volume_IN, volume_OUT);
    }
}

// ==================== AUTO OFF RELAY ====================
void checkAutoOffRelay()
{
    if (relayState && relayOnTime > 0)
    {
        unsigned long elapsedTime = millis() - relayOnTime;

        // Grace period: jangan check threshold selama waktu yang disimpan setelah relay ON
        if (elapsedTime < relayGracePeriod)
        {
            // Dalam grace period - relay boleh ON, tunggu pompa mulai mengalir
            if (elapsedTime % 5000 < 100) // Log setiap 5 detik
                Serial.printf("[Relay] Grace period: %.0f detik lagi sebelum threshold check aktif\n", (relayGracePeriod - elapsedTime) / 1000.0);
            return;
        }

        // Check 1: Jika flow rate IN <= batas timeout, check timeout
        if (flowRate_IN <= timeoutFlowThreshold)
        {
            if (elapsedTime >= autoOffTimeout)
            {
                // Auto OFF - aliran di bawah batas timeout sampai durasi timeout
                digitalWrite(RELAY_PIN, LOW);
                relayState = false;
                relayOnTime = 0;
                if (mqtt.connected())
                {
                    mqtt.publish("tandon/relay/state", "off", true);
                    mqtt.publish("tandon/relay/reason", "auto_off_timeout", true);
                }
                Serial.printf("[Relay] AUTO OFF! Flow %.2f L/min <= batas timeout %.2f L/min selama %.0f detik\n",
                              flowRate_IN, timeoutFlowThreshold, elapsedTime / 1000.0);
            }
            else if (elapsedTime >= autoOffTimeout - 10000) // Warn 10 detik sebelum auto-off
            {
                unsigned long timeLeft = autoOffTimeout - elapsedTime;
                if (timeLeft % 5000 < 100) // Log setiap 5 detik
                    Serial.printf("[Relay] WARNING: Auto-off dalam %.0f detik (flow %.2f <= %.2f L/min)\n",
                                  timeLeft / 1000.0, flowRate_IN, timeoutFlowThreshold);
            }
        }
        // Check 2: Setelah grace period, jika flowRate_IN < threshold, langsung OFF
        else if (flowRate_IN < minFlowRateThreshold)
        {
            // Auto OFF - flow rate di bawah threshold
            digitalWrite(RELAY_PIN, LOW);
            relayState = false;
            relayOnTime = 0;
            if (mqtt.connected())
            {
                mqtt.publish("tandon/relay/state", "off", true);
                mqtt.publish("tandon/relay/reason", "auto_off_low_flow", true);
            }
            Serial.printf("[Relay] AUTO OFF! Flow rate (%.2f L/min) di bawah threshold (%.2f L/min)\n", flowRate_IN, minFlowRateThreshold);
        }
        // Check 3: Jika flowRate_IN >= threshold, reset timer
        else if (flowRate_IN >= minFlowRateThreshold)
        {
            // Reset timer jika ada aliran normal
            relayOnTime = millis();
            Serial.printf("[Relay] Flow detected: %.2f L/min, timer reset\n", flowRate_IN);
        }
    }
}

// ==================== RESET BUTTON HANDLER ====================
void checkResetButton()
{
    bool buttonPressed = digitalRead(RESET_BUTTON_PIN) == LOW; // LOW = pressed (pull-up active)

    if (buttonPressed && !resetButtonPressed)
    {
        // Button pressed (transition from not pressed to pressed)
        resetButtonPressed = true;
        resetButtonPressTime = millis();
        Serial.println("[Reset Button] Tombol ditekan - tahan 5 detik untuk factory reset...");
    }
    else if (!buttonPressed && resetButtonPressed)
    {
        // Button released
        unsigned long pressDuration = millis() - resetButtonPressTime;
        resetButtonPressed = false;
        Serial.printf("[Reset Button] Tombol dilepas setelah %.0f ms\n", pressDuration / 1000.0);

        if (pressDuration >= FACTORY_RESET_THRESHOLD)
        {
            Serial.println("\n\n========== FACTORY RESET DIMULAI ==========");
            factoryReset();
        }
    }
    else if (buttonPressed && resetButtonPressed)
    {
        // Button still pressed - show countdown
        unsigned long pressDuration = millis() - resetButtonPressTime;
        unsigned long timeLeft = FACTORY_RESET_THRESHOLD - pressDuration;

        // Show progress every 500ms
        if (pressDuration % 500 < 50)
        {
            Serial.printf("[Reset Button] Tahan %.1f detik lagi untuk factory reset...\n",
                          timeLeft / 1000.0);
        }

        // Alert 1 detik sebelum reset
        if (timeLeft < 1000 && timeLeft > 900)
        {
            Serial.println("\n>>> PERHATIAN: FACTORY RESET AKAN DIMULAI DALAM 1 DETIK! LEPASKAN TOMBOL UNTUK BATAL <<<\n");
        }
    }
}

// ==================== FACTORY RESET FUNCTION ====================
void factoryReset()
{
    if (factoryResetTriggered)
        return; // Prevent multiple resets
    factoryResetTriggered = true;

    Serial.println("\n>>> FACTORY RESET DALAM PROGRESS <<<");
    Serial.println("Menghapus semua konfigurasi...");

    // Turn off relay
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;

    // Clear WiFi preferences
    Serial.println("  - Menghapus WiFi settings...");
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    // Clear Tandon preferences
    Serial.println("  - Menghapus Tandon settings...");
    prefs.begin("tandon", false);
    prefs.clear();
    prefs.end();

    // Reset volume counters
    Serial.println("  - Reset volume counters...");
    volume_IN = 0.0;
    volume_OUT = 0.0;
    maxCapacity = 1000.0;
    minFlowRateThreshold = 5.0;
    timeoutFlowThreshold = 0.0;
    relayGracePeriod = 30000;
    autoOffTimeout = 120000;

    // Reset OTA status
    otaInitialized = false;
    otaActive = false;

    // Publish MQTT notification if connected
    if (mqtt.connected())
    {
        mqtt.publish("tandon/status", "factory_reset", true);
    }

    Serial.println("\n✓ FACTORY RESET SELESAI");
    Serial.println("✓ Sistem akan restart dalam 2 detik...");
    Serial.println("✓ WiFi mode akan berganti ke AP mode\n");

    delay(2000);
    ESP.restart();
}

// ==================== OTA UPDATE ====================
void setupOTA()
{
    otaInitialized = false;

    // Disable OTA if in AP mode
    if (!wifiConnected || isAPMode)
    {
        Serial.println("[OTA] WiFi not connected or in AP mode - OTA disabled");
        return;
    }

    Serial.println("\n[OTA] ========================================");
    Serial.println("[OTA] Initializing OTA configuration...");
    Serial.printf("[OTA] WiFi Status: %d (should be 3 for WL_CONNECTED)\n", WiFi.status());
    Serial.printf("[OTA] WiFi Signal: %d dBm\n", WiFi.RSSI());

    // Ensure WiFi is fully connected
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[OTA] ERROR: WiFi not connected!");
        return;
    }

    WiFi.setSleep(false);

    // Reset ArduinoOTA before configuration
    ArduinoOTA.end();
    delay(200);

    // Configure OTA dengan setting yang detail
    ArduinoOTA.setHostname(deviceHostname);
    ArduinoOTA.setPassword("admin123");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA.setMdnsEnabled(false);

    Serial.println("[OTA] Configuration set:");
    Serial.printf("      - Hostname: %s\n", deviceHostname);
    Serial.println("      - Port: 3232");
    Serial.println("      - Password: admin123");

    // ArduinoOTA event handlers
    ArduinoOTA.onStart([]()
                       {
        Serial.println("[OTA] >>> onStart callback triggered! <<<");
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else
            type = "filesystem";

        Serial.println("\n[OTA] ========================================");
        Serial.println("[OTA] Start updating " + type);
        Serial.println("[OTA] Upload in progress...");
        Serial.println("[OTA] ========================================");
        Serial.printf("[OTA] WiFi Status: %d, Signal: %d dBm\n", WiFi.status(), WiFi.RSSI());

        // Keep WiFi active during OTA - don't change sleep settings
        Serial.println("[OTA] Keeping WiFi active for transfer");

        otaErrorMessage = "";
        otaStatusText = "active";
        if (mqtt.connected())
        {
            mqtt.publish("tandon/ota/status", "active", true);
            mqtt.publish("tandon/ota/error", "", true);
        }

        otaActive = true;
        otaStartTime = millis();

        // Turn off relay during OTA
        digitalWrite(RELAY_PIN, LOW);
        relayState = false;
        Serial.println("[OTA] <<< onStart callback completed >>>"); });

    ArduinoOTA.onEnd([]()
                     {
        Serial.println("[OTA] ========================================");
        Serial.println("[OTA] Update complete! Rebooting...");
        Serial.println("[OTA] ========================================\n");

        otaStatusText = "complete";
        if (mqtt.connected())
        {
            mqtt.publish("tandon/ota/status", "complete", true);
        }
        otaActive = false; });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
        static unsigned int lastPercent = 0;
        unsigned int percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 5 == 0)
        {
            Serial.printf("[OTA] Progress: %u%% (%u / %u bytes) - WiFi: %d\n",
                         percent, progress, total, WiFi.status());
            lastPercent = percent;
        } });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        Serial.println("\n[OTA] ========================================");
        Serial.print("[OTA] ERROR: ");
        if (error == OTA_AUTH_ERROR)
        {
            Serial.println("Authentication failed!");
            Serial.println("[OTA] Troubleshooting:");
            Serial.println("  1. Pastikan password = 'admin123' (case-sensitive)");
            Serial.println("  2. Perangkat dan komputer di WiFi yang sama");
            Serial.println("  3. Restart device ESP32 dan coba lagi");
            Serial.printf("  4. Current IP: %s\n", WiFi.localIP().toString().c_str());
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            Serial.println("Begin failed - storage issue");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            Serial.println("Connect failed - network issue");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            Serial.println("Receive failed - data corruption or network issue");
            Serial.printf("[OTA] Debug Info:\n");
            Serial.printf("  - WiFi Status: %d\n", WiFi.status());
            Serial.printf("  - WiFi RSSI: %d dBm\n", WiFi.RSSI());
            Serial.printf("  - Free Heap: %d bytes\n", ESP.getFreeHeap());
            Serial.printf("  - Uptime: %lu ms\n", millis());
            Serial.println("  - Troubleshooting:");
            Serial.println("    1. Check network stability");
            Serial.println("    2. Ensure Arduino IDE and ESP32 on same network");
            Serial.println("    3. Try reducing upload speed in Arduino IDE");
            Serial.println("    4. Check firewall/antivirus blocking port 3232");
        }
        else if (error == OTA_END_ERROR)
        {
            Serial.println("End failed - finalization error");
        }

        otaErrorMessage = String("OTA error ") + error + ": " + (error == OTA_AUTH_ERROR ? "Auth Failed" :
                                error == OTA_BEGIN_ERROR ? "Begin Failed" :
                                error == OTA_CONNECT_ERROR ? "Connect Failed" :
                                error == OTA_RECEIVE_ERROR ? "Receive Failed" :
                                error == OTA_END_ERROR ? "End Failed" : "Unknown Error");
        otaStatusText = "error";
        if (mqtt.connected())
        {
            mqtt.publish("tandon/ota/status", "error", true);
            mqtt.publish("tandon/ota/error", otaErrorMessage.c_str(), true);
        }

        Serial.println("[OTA] ========================================\n");
        otaActive = false; });

    // Mulai OTA
    try
    {
        ArduinoOTA.begin();
        setupMDNS(true);
        otaInitialized = true;
        Serial.println("[OTA] ========================================");
        Serial.println("[OTA] ✓ OTA Ready for updates");
        Serial.printf("[OTA] IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[OTA] Hostname: %s\n", deviceHostname);
        Serial.println("[OTA] Port: 3232\n");
        Serial.printf("[OTA] mDNS: %s.local\n", deviceHostname);
        Serial.println("[OTA] Upload dengan: ");
        Serial.println("  Arduino IDE → Tools → Port → tandon-air");
        Serial.println("  Atau: espota.exe -i 192.168.137.86 -p 3232 --auth=admin123");
        Serial.println("[OTA] ========================================\n");
    }
    catch (...)
    {
        Serial.println("[OTA] ERROR: Failed to initialize ArduinoOTA!");
        Serial.println("[OTA] Will retry in 15 seconds...");
        otaErrorMessage = "Failed to initialize ArduinoOTA";
        otaStatusText = "error";
        if (mqtt.connected())
        {
            mqtt.publish("tandon/ota/status", "error", true);
            mqtt.publish("tandon/ota/error", otaErrorMessage.c_str(), true);
        }
        otaInitialized = false;
    }
}

// ==================== WIFI ====================
void setupMDNS(bool enableOtaService)
{
    MDNS.end();
    delay(50);

    if (MDNS.begin(deviceHostname))
    {
        mdnsActive = true;
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "path", "/");

        if (enableOtaService)
        {
            MDNS.enableArduino(3232, true);
        }

        Serial.printf("[mDNS] http://%s.local/ ready (%s mode)\n",
                      deviceHostname, isAPMode ? "AP+STA" : "STA");
    }
    else
    {
        mdnsActive = false;
        Serial.printf("[mDNS] Failed to start %s.local\n", deviceHostname);
    }
}

void startAP()
{
    if (isAPMode)
    {
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setHostname(deviceHostname);
    WiFi.softAP(apSSID, apPASS);
    Serial.printf("[AP] Started: %s IP: %s\n", apSSID, WiFi.softAPIP().toString().c_str());
    wifiConnected = false;
    isAPMode = true;
    wifiStaConnectedSince = 0;
    setupMDNS(false);

    if (ssid.length() > 0)
    {
        Serial.printf("[WiFi] Background reconnect enabled for saved network: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        wifiLastRetry = millis();
    }
}

void syncTime()
{
    if (!wifiConnected)
        return;
    Serial.print("[NTP] Syncing time");
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    unsigned long start = millis();
    const unsigned long timeout = 10000;
    while (millis() - start < timeout)
    {
        time_t now = time(nullptr);
        if (now >= 24 * 3600)
        {
            Serial.println(" OK!");
            return;
        }
        delay(100);
        yield();
    }
    Serial.println(" timeout (using system time)");
}

// ==================== MQTT ====================
void reconnectMQTT()
{
    if (!wifiConnected)
        return;
    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect(mqtt_client))
    {
        Serial.println(" Connected!");
        mqtt.subscribe("tandon/relay/cmd");
        mqtt.subscribe("tandon/sensor/reset");
        mqttConnectTime = millis();
        mqtt.publish("tandon/relay/state", relayState ? "on" : "off", true);
        mqtt.publish("tandon/status", "online", true);
    }
    else
    {
        Serial.printf(" Failed (state %d)\n", mqtt.state());
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String message = "";
    for (unsigned int i = 0; i < length; i++)
        message += (char)payload[i];
    Serial.printf("[MQTT] %s = %s\n", topic, message.c_str());

    if (strcmp(topic, "tandon/relay/cmd") == 0)
    {
        if (message == "on")
        {
            digitalWrite(RELAY_PIN, HIGH);
            relayState = true;
            relayOnTime = millis(); // Catat waktu relay dihidupkan
            if (mqtt.connected())
                mqtt.publish("tandon/relay/state", "on", true);
            Serial.println("[Relay] ON via MQTT");
        }
        else if (message == "off")
        {
            digitalWrite(RELAY_PIN, LOW);
            relayState = false;
            relayOnTime = 0; // Reset waktu relay
            if (mqtt.connected())
                mqtt.publish("tandon/relay/state", "off", true);
            Serial.println("[Relay] OFF via MQTT");
        }
    }
    else if (strcmp(topic, "tandon/sensor/reset") == 0 && message == "reset")
    {
        noInterrupts();
        volume_IN = 0.0;
        volume_OUT = 0.0;
        interrupts();
        if (mqtt.connected())
        {
            mqtt.publish("tandon/sensor/volume_in", "0", true);
            mqtt.publish("tandon/sensor/volume_out", "0", true);
        }
        Serial.println("[Reset] Volume counters reset");
    }
}

void publishToMQTT()
{
    if (!mqtt.connected())
        return;
    char buf[20];
    snprintf(buf, sizeof(buf), "%.2f", flowRate_IN);
    mqtt.publish("tandon/sensor/flowrate_in", buf, false);
    snprintf(buf, sizeof(buf), "%.2f", flowRate_OUT);
    mqtt.publish("tandon/sensor/flowrate_out", buf, false);
    snprintf(buf, sizeof(buf), "%.2f", volume_IN);
    mqtt.publish("tandon/sensor/volume_in", buf, false);
    snprintf(buf, sizeof(buf), "%.2f", volume_OUT);
    mqtt.publish("tandon/sensor/volume_out", buf, false);
}

// ==================== VOLUME PERSISTENCE ====================
void saveVolumeToEEPROM()
{
    prefs.begin("tandon", false);
    prefs.putFloat("volume_in", volume_IN);
    prefs.putFloat("volume_out", volume_OUT);
    prefs.end();
    Serial.printf("[EEPROM] Volume saved: IN=%.2f L, OUT=%.2f L\n", volume_IN, volume_OUT);
}

// ==================== WEB SERVER ====================
void setupWebServer()
{
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/data", HTTP_GET, handleAPI);
    server.on("/api/relay", HTTP_POST, handleRelayControl);
    server.on("/api/reset", HTTP_POST, handleReset);
    server.on("/config", HTTP_GET, handleConfigPage);
    server.on("/api/wifi/save", HTTP_POST, handleSaveWiFi);
    server.on("/api/wifi/status", HTTP_GET, handleWiFiStatus);
    server.on("/api/wifi/scan", HTTP_GET, handleScanWiFi);
    server.on("/api/wifi/select", HTTP_POST, handleSelectWiFi);
    server.on("/settings", HTTP_GET, handleSettingsPage);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings/capacity", HTTP_POST, handleSaveCapacity);
    server.on("/api/settings/volume", HTTP_POST, handleSaveVolume);
    server.on("/api/settings/flow-threshold", HTTP_POST, handleSaveFlowThreshold);
    server.on("/api/settings/auto-off", HTTP_POST, handleSaveAutoOffSettings);
    server.on("/ota", HTTP_GET, handleOTAStartPage);
    server.on("/api/ota/status", HTTP_GET, handleOTAStatus);
    server.on("/update", HTTP_POST, handleOTAUpdate, handleOTAUpload);
}

void handleRoot()
{
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Tandon Air Meter</title>
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background:linear-gradient(135deg,#667eea 0%,#764ba2 100%); min-height:100vh; padding:20px; }
    .container { max-width:900px; margin:0 auto; }
    .header { background:white; padding:25px; border-radius:15px; margin-bottom:20px; box-shadow:0 8px 32px rgba(0,0,0,0.1); }
    .header h1 { color:#667eea; margin-bottom:5px; font-size:32px; }
    .header p { color:#999; font-size:14px; }
    .volume-section { background:white; border-radius:15px; padding:30px; margin-bottom:20px; box-shadow:0 8px 32px rgba(0,0,0,0.1); }
    .volume-title { text-align:center; color:#667eea; font-size:18px; font-weight:bold; margin-bottom:25px; }
    .volume-flow-container { display:grid; grid-template-columns:1fr 2fr 1fr; gap:20px; margin-bottom:30px; align-items:center; }
    .volume-card { background:linear-gradient(135deg,#667eea,#764ba2); color:white; padding:20px; border-radius:12px; text-align:center; box-shadow:0 5px 15px rgba(102,126,234,0.3); }
    .volume-card h3 { font-size:14px; opacity:0.9; margin-bottom:10px; }
    .volume-card .value { font-size:32px; font-weight:bold; }
    .volume-card .unit { font-size:14px; opacity:0.8; }
    .flow-diagram { display:flex; align-items:center; justify-content:space-around; position:relative; }
    .flow-arrow { font-size:40px; animation:flow 1.5s infinite; }
    @keyframes flow { 0% { opacity:0.3; } 50% { opacity:1; } 100% { opacity:0.3; } }
    .remaining-highlight { background:linear-gradient(135deg,#4CAF50,#45a049); color:white; padding:25px; border-radius:12px; text-align:center; box-shadow:0 5px 15px rgba(76,175,80,0.3); margin-bottom:20px; }
    .remaining-highlight h2 { font-size:16px; opacity:0.9; margin-bottom:10px; }
    .remaining-highlight .big-value { font-size:48px; font-weight:bold; }
    .progress-container { margin:20px 0; }
    .progress-label { display:flex; justify-content:space-between; margin-bottom:8px; font-size:12px; color:#666; font-weight:bold; }
    .progress-bar { width:100%; height:12px; background:#f0f0f0; border-radius:10px; overflow:hidden; }
    .progress-fill { height:100%; background:linear-gradient(90deg,#4CAF50,#8bc34a); border-radius:10px; transition:width 0.5s ease; }
    .cards-grid { display:grid; grid-template-columns:1fr 1fr; gap:20px; margin-bottom:20px; }
    @media (max-width:768px) { .cards-grid { grid-template-columns:1fr; } .volume-flow-container { grid-template-columns:1fr; } }
    .sensor-card { background:white; padding:20px; border-radius:12px; box-shadow:0 5px 15px rgba(0,0,0,0.1); }
    .sensor-card h2 { color:#667eea; margin-bottom:15px; font-size:16px; }
    .data-row { display:flex; justify-content:space-between; padding:10px 0; border-bottom:1px solid #f0f0f0; }
    .data-row:last-child { border-bottom:none; }
    .data-label { color:#666; font-weight:600; }
    .data-value { color:#333; font-weight:bold; }
    .control-section { background:white; padding:20px; border-radius:12px; box-shadow:0 5px 15px rgba(0,0,0,0.1); }
    .control-section h2 { color:#667eea; margin-bottom:15px; }
    .advanced-section { background:white; padding:16px 20px; border-radius:12px; box-shadow:0 5px 15px rgba(0,0,0,0.1); margin-top:15px; }
    .button-group { display:flex; gap:10px; margin-bottom:15px; }
    button { flex:1; padding:12px; border:none; border-radius:8px; font-weight:bold; cursor:pointer; transition:all 0.3s; }
    button:hover { transform:translateY(-2px); box-shadow:0 5px 15px rgba(0,0,0,0.2); }
    .btn-on { background:linear-gradient(135deg,#4CAF50,#45a049); color:white; }
    .btn-off { background:linear-gradient(135deg,#f44336,#da190b); color:white; }
    .btn-reset { background:linear-gradient(135deg,#2196F3,#0b7dda); color:white; width:100%; }
    .btn-config { background:linear-gradient(135deg,#FF9800,#F57C00); color:white; width:100%; margin-top:10px; }
    .status { padding:12px; background:#f0f0f0; border-radius:8px; text-align:center; margin-top:15px; font-weight:bold; border-left:4px solid #667eea; }
    .status.on { background:#e8f5e9; border-left-color:#4CAF50; color:#2e7d32; }
    .status.off { background:#ffebee; border-left-color:#f44336; color:#c62828; }
  </style>
</head>
<body>
<div class=container>
  <div class=header>
    <h1>💧 Tandon Air Meter</h1>
    <p>Real-time Water Monitoring System</p>
  </div>
  <div class=volume-section>
    <div class=volume-title>📊 Kondisi Tandon</div>
    <div class=volume-flow-container>
      <div class=volume-card><h3>🔵 Air Masuk</h3><div class=value id=vol-in>0.00</div><div class=unit>Liter</div></div>
      <div class=flow-diagram><div class=flow-arrow>↓</div><div class=flow-arrow>➖</div><div class=flow-arrow>↓</div></div>
      <div class=volume-card><h3>🔴 Air Keluar</h3><div class=value id=vol-out>0.00</div><div class=unit>Liter</div></div>
    </div>
    <div class=remaining-highlight>
      <h2>💧 Sisa Air di Tandon</h2>
      <div class=big-value id=remaining-value>0.00</div><div class=unit>Liter</div>
      <div class=remaining-percent id=remaining-info>Tandon kosong</div>
    </div>
    <div class=progress-container>
      <div class=progress-label><span>Kapasitas Tandon</span><span id=progress-text>0%</span></div>
      <div class=progress-bar><div class=progress-fill id=progress-fill style="width:0%"></div></div>
    </div>
  </div>
  <div class=cards-grid>
    <div class=sensor-card><h2>📥 Input Sensor</h2><div class=data-row><span class=data-label>Flow Rate:</span><span class=data-value><span id=flowrate_in>0.00</span> L/min</span></div><div class=data-row><span class=data-label>Total Volume:</span><span class=data-value><span id=volume_in>0.00</span> L</span></div></div>
    <div class=sensor-card><h2>📤 Output Sensor</h2><div class=data-row><span class=data-label>Flow Rate:</span><span class=data-value><span id=flowrate_out>0.00</span> L/min</span></div><div class=data-row><span class=data-label>Total Volume:</span><span class=data-value><span id=volume_out>0.00</span> L</span></div></div>
  </div>
  <div class=control-section>
    <h2>🎛️ POMPA AIR Relay Control</h2>
    <div class=button-group><button class=btn-on onclick=relayOn()>🔛 ON</button><button class=btn-off onclick=relayOff()>🔴 OFF</button></div>
    <div class=status id=status>Status: Loading...</div>
  </div>
  <div class=advanced-section>
    <button class=btn-config onclick="window.location='/settings'" style="background:linear-gradient(135deg, #9C27B0, #7B1FA2);">Pengaturan Lanjutan</button>
  </div>
</div>
<script>
let MAX_CAPACITY = 1000;
function updateData() {
    fetch('/api/data').then(r=>r.json()).then(d=>{
        if(d.max_capacity) MAX_CAPACITY = d.max_capacity;
        document.getElementById('flowrate_in').innerText = d.flowrate_in.toFixed(2);
        document.getElementById('flowrate_out').innerText = d.flowrate_out.toFixed(2);
        document.getElementById('volume_in').innerText = d.volume_in.toFixed(2);
        document.getElementById('volume_out').innerText = d.volume_out.toFixed(2);
        document.getElementById('vol-in').innerText = d.volume_in.toFixed(2);
        document.getElementById('vol-out').innerText = d.volume_out.toFixed(2);
        let remaining = d.volume_remaining;
        document.getElementById('remaining-value').innerText = remaining.toFixed(2);
        let percent = Math.min(remaining/MAX_CAPACITY*100,100);
        document.getElementById('progress-fill').style.width = percent+'%';
        document.getElementById('progress-text').innerText = percent.toFixed(0)+'%';
        let info = document.getElementById('remaining-info');
        if(remaining<50) info.innerText = '⚠️ Tandon hampir kosong!';
        else if(remaining<100) info.innerText = '⚡ Kapasitas rendah';
        else if(remaining>MAX_CAPACITY*0.9) info.innerText = '✓ Tandon hampir penuh';
        else info.innerText = '✓ Normal';
        let s = document.getElementById('status');
        s.innerText = 'Status: Relay '+(d.relay_state?'ON':'OFF');
        s.className = 'status '+(d.relay_state?'on':'off');
    });
}
function relayOn(){ fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'on'})}).then(()=>updateData()); }
function relayOff(){ fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'off'})}).then(()=>updateData()); }
function resetCounters(){ if(confirm('Reset semua counter volume?')) fetch('/api/reset',{method:'POST'}).then(()=>updateData()); }
updateData(); setInterval(updateData,2000);
</script>
</body>
</html>
)=====";
    server.send(200, "text/html", html);
}

void handleAPI()
{
    float remaining = volume_IN - volume_OUT;
    if (remaining < 0)
        remaining = 0;

    String json = "{\"flowrate_in\":" + String(flowRate_IN, 2) + ",\"flowrate_out\":" + String(flowRate_OUT, 2) +
                  ",\"volume_in\":" + String(volume_IN, 2) + ",\"volume_out\":" + String(volume_OUT, 2) +
                  ",\"volume_remaining\":" + String(remaining, 2) +
                  ",\"max_capacity\":" + String(maxCapacity, 2) +
                  ",\"relay_state\":" + (relayState ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

// ==================== HANDLE RELAY (DENGAN VERIFIKASI PIN) ====================
void handleRelayControl()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    String body = server.arg("plain");
    Serial.println("[Relay] Received POST body: " + body);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error)
    {
        Serial.printf("[Relay] JSON parse error: %s\n", error.c_str());
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char *cmd = doc["cmd"];
    if (cmd == nullptr)
    {
        server.send(400, "application/json", "{\"error\":\"Missing 'cmd' field\"}");
        return;
    }

    if (strcmp(cmd, "on") == 0)
    {
        digitalWrite(RELAY_PIN, HIGH);
        delay(10);
        Serial.printf("[Relay] ON command: pin %d now reads %d (should be 1)\n", RELAY_PIN, digitalRead(RELAY_PIN));
        relayState = true;
        relayOnTime = millis(); // Catat waktu relay dihidupkan
        if (mqtt.connected())
            mqtt.publish("tandon/relay/state", "on", true);
    }
    else if (strcmp(cmd, "off") == 0)
    {
        digitalWrite(RELAY_PIN, LOW);
        delay(10);
        Serial.printf("[Relay] OFF command: pin %d now reads %d (should be 0)\n", RELAY_PIN, digitalRead(RELAY_PIN));
        relayState = false;
        relayOnTime = 0; // Reset waktu relay
        if (mqtt.connected())
            mqtt.publish("tandon/relay/state", "off", true);
    }
    else
    {
        server.send(400, "application/json", "{\"error\":\"Invalid command\"}");
        return;
    }

    server.send(200, "application/json", "{\"status\":\"ok\", \"relay_state\":" + String(relayState ? "true" : "false") + "}");
}

void handleReset()
{
    noInterrupts();
    volume_IN = 0.0;
    volume_OUT = 0.0;
    interrupts();
    saveVolumeToEEPROM();
    if (mqtt.connected())
    {
        mqtt.publish("tandon/sensor/volume_in", "0", true);
        mqtt.publish("tandon/sensor/volume_out", "0", true);
    }
    server.send(200, "application/json", "{\"status\":\"reset\"}");
}

void handleConfigPage()
{
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WiFi Manager</title>
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:Arial; padding:15px; background:#f5f5f5; }
    .container { max-width:500px; margin:0 auto; background:white; padding:20px; border-radius:10px; box-shadow:0 2px 10px rgba(0,0,0,0.1); }
    h1 { color:#667eea; margin-bottom:20px; font-size:24px; }
    .section { margin-bottom:20px; }
    .section h2 { font-size:16px; color:#333; margin-bottom:10px; border-bottom:2px solid #667eea; padding-bottom:5px; }
    .network-list { max-height:300px; overflow-y:auto; border:1px solid #ddd; border-radius:5px; }
    .network-item { padding:12px; border-bottom:1px solid #eee; cursor:pointer; display:flex; justify-content:space-between; align-items:center; }
    .network-item:hover { background:#f9f9f9; }
    .network-name { font-weight:bold; color:#333; }
    .network-signal { font-size:12px; color:#999; }
    .network-selected { background:#e8f0fe; border-left:4px solid #667eea; }
    input { width:100%; padding:10px; margin:8px 0; border:1px solid #ddd; border-radius:5px; font-size:14px; }
    button { width:100%; padding:12px; margin:8px 0; border:none; border-radius:5px; font-weight:bold; cursor:pointer; font-size:14px; }
    .mode-actions { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-bottom:10px; }
    .mode-actions button { margin:0; }
    .manual-entry { display:none; margin-top:10px; padding:12px; background:#fff8e1; border-radius:8px; border-left:4px solid #FF9800; }
    .btn-scan { background:#2196F3; color:white; }
    .btn-connect { background:#4CAF50; color:white; }
    .btn-manual { background:#FF9800; color:white; }
    .loading { text-align:center; color:#999; padding:20px; }
    .error { color:#d32f2f; font-size:12px; margin-top:5px; }
    .status-msg { padding:15px; margin:10px 0; border-radius:5px; text-align:center; font-weight:bold; display:none; }
    .status-wait { background:#fff3e0; color:#e65100; }
    .status-success { background:#e8f5e9; color:#2e7d32; }
    .wifi-status { background:#f8f9fa; border-left:4px solid #667eea; padding:12px; border-radius:8px; margin-bottom:15px; }
    .wifi-status.connected { background:#e8f5e9; border-left-color:#4CAF50; }
    .wifi-status.disconnected { background:#ffebee; border-left-color:#f44336; }
    .wifi-row { display:flex; justify-content:space-between; gap:12px; padding:4px 0; font-size:13px; }
    .wifi-label { color:#666; font-weight:bold; }
    .wifi-value { color:#333; text-align:right; word-break:break-word; }
  </style>
</head>
<body>
<div class=container>
  <h1>⚙️ WiFi Manager</h1>
  <div id="status-wait" class="status-msg status-wait">⏳ Device connecting to new WiFi...<br><small>Tunggu 20 detik...</small></div>
  <div id="status-success" class="status-msg status-success">✓ Reconnecting...<br><small id=new-ip-info></small></div>
  <div id="main-ui">
    <div class=section>
      <h2>Status WiFi Saat Ini</h2>
      <div id=wifi-status class="wifi-status">
        <div class="wifi-row"><span class="wifi-label">Status</span><span class="wifi-value" id=wifi-state>Loading...</span></div>
        <div class="wifi-row"><span class="wifi-label">SSID</span><span class="wifi-value" id=wifi-ssid>-</span></div>
        <div class="wifi-row"><span class="wifi-label">IP</span><span class="wifi-value" id=wifi-ip>-</span></div>
        <div class="wifi-row"><span class="wifi-label">Signal</span><span class="wifi-value" id=wifi-rssi>-</span></div>
      </div>
    </div>
    <div class=section>
      <h2>📡 Available Networks</h2>
      <div class=mode-actions>
        <button class=btn-scan onclick=scanNetworks()>🔄 Scan</button>
        <button class=btn-manual onclick=toggleManualEntry()>⌨️ Manual Entry</button>
      </div>
      <div class=network-list id=networks><div class=loading>Scanning...</div></div>
      <input type=password id=password placeholder="WiFi Password">
      <button class=btn-connect onclick=connectSelected()>✓ Connect Selected</button>
      <div class=manual-entry id=manual-entry>
        <input type=text id=manual_ssid placeholder="SSID">
        <input type=password id=manual_pass placeholder="Password">
        <button class=btn-manual onclick=connectManual()>Connect Manually</button>
      </div>
    </div>
    <hr>
    <div class=section><button class=btn-scan onclick="window.location='/settings'" style="background:#9C27B0;">🎛️ Pengaturan Tandon</button><button class=btn-scan onclick="window.location='/'" style="background:#666;">← Kembali</button></div>
  </div>
</div>
<script>
let selectedSSID = null, currentIP = window.location.hostname;
function loadWiFiStatus(){
    fetch('/api/wifi/status').then(r=>r.json()).then(d=>{
        let box=document.getElementById('wifi-status');
        let connected=d.connected===true;
        box.className='wifi-status '+(connected?'connected':'disconnected');
        document.getElementById('wifi-state').innerText=d.mode==='AP'?'AP Mode':(connected?'Terhubung':'Tidak terhubung');
        document.getElementById('wifi-ssid').innerText=d.ssid||'-';
        document.getElementById('wifi-ip').innerText=d.ip||'-';
        document.getElementById('wifi-rssi').innerText=connected?d.rssi+' dBm':'-';
    }).catch(()=>{
        document.getElementById('wifi-status').className='wifi-status disconnected';
        document.getElementById('wifi-state').innerText='Tidak bisa membaca status';
    });
}
function scanNetworks(){
    document.getElementById('networks').innerHTML='<div class=loading>Scanning...</div>';
    fetch('/api/wifi/scan').then(r=>r.json()).then(d=>{
        let html=''; if(d.networks.length===0) html='<div class=loading>No networks</div>';
        else d.networks.forEach(net=>{ html+='<div class="network-item" onclick="selectNetwork(\''+net.ssid+'\')"><div><div class=network-name>'+net.ssid+'</div><div class=network-signal>Signal: '+net.rssi+' dBm</div></div></div>'; });
        document.getElementById('networks').innerHTML=html;
    }).catch(e=>{ document.getElementById('networks').innerHTML='<div class=error>Scan failed</div>'; });
}
function selectNetwork(ssid){ selectedSSID=ssid; document.querySelectorAll('.network-item').forEach(el=>el.classList.remove('network-selected')); event.target.closest('.network-item').classList.add('network-selected'); document.getElementById('password').focus(); }
function toggleManualEntry(){ let el=document.getElementById('manual-entry'); let open=el.style.display==='block'; el.style.display=open?'none':'block'; if(!open) document.getElementById('manual_ssid').focus(); }
function showWaitingScreen(){ document.getElementById('main-ui').style.display='none'; document.getElementById('status-wait').style.display='block'; setTimeout(()=>{ window.location.href='http://'+currentIP; },20000); }
function connectSelected(){ if(!selectedSSID){ alert('Select network'); return; } fetch('/api/wifi/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:selectedSSID, password:document.getElementById('password').value})}).then(()=>showWaitingScreen()).catch(e=>alert(e)); }
function connectManual(){ let ssid=document.getElementById('manual_ssid').value, pass=document.getElementById('manual_pass').value; if(!ssid){ alert('Enter SSID'); return; } fetch('/api/wifi/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid, password:pass})}).then(()=>showWaitingScreen()).catch(e=>alert(e)); }
loadWiFiStatus(); scanNetworks(); setInterval(loadWiFiStatus,5000);
</script>
</body>
</html>
)=====";
    server.send(200, "text/html", html);
}

void handleSaveWiFi()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    const char *newSSID = doc["ssid"];
    const char *newPass = doc["password"];
    if (newSSID == nullptr || newPass == nullptr || strlen(newSSID) == 0 || strlen(newPass) == 0)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid credentials\"}");
        return;
    }
    prefs.begin("wifi", false);
    prefs.putString("ssid", newSSID);
    prefs.putString("pass", newPass);
    prefs.end();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
    delay(1000);
    ESP.restart();
}

void handleWiFiStatus()
{
    DynamicJsonDocument doc(384);
    bool connected = WiFi.status() == WL_CONNECTED;

    doc["connected"] = connected;
    doc["ap_mode"] = isAPMode;
    doc["mode"] = isAPMode ? (connected ? "AP+STA" : "AP") : "STA";
    doc["saved_ssid"] = ssid;

    if (connected)
    {
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["ap_ssid"] = isAPMode ? apSSID : "";
        doc["ap_ip"] = isAPMode ? WiFi.softAPIP().toString() : "";
    }
    else if (isAPMode)
    {
        doc["ssid"] = apSSID;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["rssi"] = 0;
    }
    else
    {
        doc["ssid"] = ssid.length() > 0 ? ssid : "";
        doc["ip"] = "";
        doc["rssi"] = 0;
    }

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void scanAvailableNetworks()
{
    Serial.println("[WiFi] Scanning...");
    int n = WiFi.scanNetworks();
    wifiScanCount = min(n, 20);
    for (int i = 0; i < wifiScanCount; i++)
    {
        scannedNetworks[i] = WiFi.SSID(i);
        scannedSignals[i] = WiFi.RSSI(i);
        Serial.printf("  %s (%d dBm)\n", scannedNetworks[i].c_str(), scannedSignals[i]);
    }
}

void handleScanWiFi()
{
    scanAvailableNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < wifiScanCount; i++)
    {
        if (i > 0)
            json += ",";
        json += "{\"ssid\":\"" + scannedNetworks[i] + "\",\"rssi\":" + String(scannedSignals[i]) + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleSelectWiFi()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    const char *selectedSSID = doc["ssid"];
    const char *selectedPass = doc["password"];
    if (selectedSSID == nullptr || strlen(selectedSSID) == 0)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid SSID\"}");
        return;
    }
    Serial.printf("[WiFi] Saving & connecting to %s\n", selectedSSID);
    prefs.begin("wifi", false);
    prefs.putString("ssid", selectedSSID);
    prefs.putString("pass", selectedPass != nullptr ? selectedPass : "");
    prefs.end();
    server.send(200, "application/json", "{\"status\":\"connecting\"}");
    delay(500);
    ESP.restart();
}

void handleSettingsPage()
{
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>⚙️ Pengaturan Tandon</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg,#667eea 0%,#764ba2 100%); min-height:100vh; display:flex; justify-content:center; align-items:center; padding:20px; }
        .container { background:white; border-radius:15px; padding:30px; max-width:500px; width:100%; }
        h1 { color:#333; margin-bottom:10px; }
        .subtitle { color:#888; margin-bottom:30px; }
        .setting-group { margin-bottom:25px; }
        label { display:block; color:#555; font-weight:600; margin-bottom:10px; }
        .input-group { display:flex; gap:10px; align-items:center; }
        input[type="number"] { flex:1; padding:12px; border:2px solid #ddd; border-radius:8px; font-size:16px; }
        input:focus { border-color:#667eea; outline:none; }
        .unit { color:#888; }
        .current-value { background:#f0f4ff; padding:15px; border-radius:8px; margin-bottom:15px; border-left:4px solid #667eea; }
        .current-value .value { font-size:24px; font-weight:bold; color:#667eea; }
        button { width:100%; padding:12px; border:none; border-radius:8px; font-size:16px; font-weight:600; cursor:pointer; margin-top:10px; transition:0.3s; }
        .btn-save { background:linear-gradient(135deg,#667eea,#764ba2); color:white; }
        .btn-back { background:#f0f0f0; color:#333; }
        .message { padding:12px; border-radius:8px; margin-bottom:15px; text-align:center; display:none; }
        .message.success { background:#d4edda; color:#155724; display:block; }
        .message.error { background:#f8d7da; color:#721c24; display:block; }
        .loading { display:none; text-align:center; color:#667eea; }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ Pengaturan Tandon Air</h1>
        <p class="subtitle">Atur kapasitas dan volume air</p>
        <div id="message" class="message"></div>
        <div class="setting-group">
            <label>Kapasitas Maksimal</label>
            <div class="current-value"><span>Kapasitas saat ini:</span><br><span class="value" id="currentCapacity">1000</span> <span class="unit">L</span></div>
            <div class="input-group"><input type="number" id="capacityInput" min="100" max="10000" value="1000"><span class="unit">L</span></div>
            <button class="btn-save" onclick="saveCapacity()">💾 Simpan Kapasitas</button>
        </div>
        <div class="setting-group">
            <label>Volume Air Saat Ini</label>
            <div class="current-value"><span>Volume saat ini:</span><br><span class="value" id="currentVolume">0</span> <span class="unit">L</span></div>
            <div class="input-group"><input type="number" id="volumeInput" min="0" max="10000" value="0"><span class="unit">L</span></div>
            <button class="btn-save" onclick="saveVolume()" style="background:linear-gradient(135deg,#f093fb,#f5576c);">💧 Simpan Volume</button>
        </div>
        <div class="setting-group">
            <label>⚡ Flow Rate Threshold (Auto OFF)</label>
            <div class="current-value"><span>Threshold saat ini:</span><br><span class="value" id="currentThreshold">5.0</span> <span class="unit">L/min</span></div>
            <div style="background:#fff3cd; padding:10px; border-radius:8px; margin-bottom:15px; font-size:12px; color:#856404;"><strong>ℹ️ Info:</strong> Setelah relay ON, sistem menunggu grace period agar pompa mulai mengalir. Jika setelah grace period flow rate masih kurang dari threshold, relay akan OFF. Timeout dipakai untuk batas waktu auto-off saat tidak ada aliran.</div>
            <div class="input-group"><input type="number" id="thresholdInput" min="0.1" max="30" step="0.1" value="5.0"><span class="unit">L/min</span></div>
            <button class="btn-save" onclick="saveThreshold()" style="background:linear-gradient(135deg,#ffa500,#ff8c00);">⚡ Simpan Threshold</button>
        </div>
        <div class="setting-group">
            <label>Waktu Auto OFF</label>
            <div class="current-value"><span>Grace period saat ini:</span><br><span class="value" id="currentGracePeriod">30</span> <span class="unit">detik</span><br><span>Timeout saat ini:</span><br><span class="value" id="currentAutoTimeout">120</span> <span class="unit">detik</span><br><span>Batas flow timeout:</span><br><span class="value" id="currentTimeoutFlow">0.0</span> <span class="unit">L/min</span></div>
            <div class="input-group"><input type="number" id="gracePeriodInput" min="1" max="600" step="1" value="30"><span class="unit">detik</span></div>
            <div class="input-group"><input type="number" id="autoTimeoutInput" min="10" max="3600" step="1" value="120"><span class="unit">detik</span></div>
            <div class="input-group"><input type="number" id="timeoutFlowInput" min="0" max="30" step="0.1" value="0.0"><span class="unit">L/min</span></div>
            <button class="btn-save" onclick="saveAutoOffSettings()" style="background:linear-gradient(135deg,#00BCD4,#00838F);">Simpan Waktu Auto OFF</button>
        </div>
        <div class="setting-group">
            <label>Pengaturan Lanjutan</label>
            <button class="btn-save" onclick="resetCounters()" style="background:linear-gradient(135deg,#2196F3,#0b7dda);">Reset Counters</button>
            <button class="btn-save" onclick="window.location='/config'" style="background:linear-gradient(135deg,#FF9800,#F57C00);">WiFi Settings</button>
            <button class="btn-save" onclick="window.location='/ota'" style="background:linear-gradient(135deg,#FF6B6B,#EE5A6F);">OTA Update</button>
        </div>
        <div class="loading" id="loading"><div class="spinner"></div> Menyimpan...</div>
        <button class="btn-back" onclick="goBack()">← Kembali</button>
    </div>
    <script>
        function showMessage(text,type){ let m=document.getElementById('message'); m.innerText=text; m.className='message '+type; setTimeout(()=>m.className='message',5000); }
        function loadSettings(){ fetch('/api/settings').then(r=>r.json()).then(d=>{ let cap=d.capacity||1000; let vol=d.volume||0; let thresh=d.flow_threshold||5.0; let grace=d.grace_period||30; let timeout=d.auto_off_timeout||120; let timeoutFlow=(d.timeout_flow_threshold ?? 0.0); document.getElementById('currentCapacity').innerText=cap; document.getElementById('capacityInput').value=cap; document.getElementById('currentVolume').innerText=vol; document.getElementById('volumeInput').value=vol; document.getElementById('currentThreshold').innerText=thresh; document.getElementById('thresholdInput').value=thresh; document.getElementById('currentGracePeriod').innerText=grace; document.getElementById('gracePeriodInput').value=grace; document.getElementById('currentAutoTimeout').innerText=timeout; document.getElementById('autoTimeoutInput').value=timeout; document.getElementById('currentTimeoutFlow').innerText=parseFloat(timeoutFlow).toFixed(1); document.getElementById('timeoutFlowInput').value=timeoutFlow; }).catch(()=>showMessage('Gagal memuat','error')); }
        function saveCapacity(){ let cap=parseFloat(document.getElementById('capacityInput').value); if(isNaN(cap)||cap<100||cap>10000){ showMessage('Kapasitas harus 100-10000 L','error'); return; } document.getElementById('loading').style.display='block'; fetch('/api/settings/capacity',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({capacity:cap})}).then(r=>r.json()).then(d=>{ document.getElementById('loading').style.display='none'; if(d.status==='success'){ showMessage('Kapasitas tersimpan!','success'); document.getElementById('currentCapacity').innerText=cap; } else showMessage('Gagal menyimpan','error'); }).catch(e=>{ document.getElementById('loading').style.display='none'; showMessage('Error','error'); }); }
        function saveVolume(){ let vol=parseFloat(document.getElementById('volumeInput').value); if(isNaN(vol)||vol<0||vol>10000){ showMessage('Volume harus 0-10000 L','error'); return; } document.getElementById('loading').style.display='block'; fetch('/api/settings/volume',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({volume:vol})}).then(r=>r.json()).then(d=>{ document.getElementById('loading').style.display='none'; if(d.status==='success'){ showMessage('Volume tersimpan!','success'); document.getElementById('currentVolume').innerText=vol; } else showMessage('Gagal menyimpan','error'); }).catch(e=>{ document.getElementById('loading').style.display='none'; showMessage('Error','error'); }); }
        function saveThreshold(){ let thresh=parseFloat(document.getElementById('thresholdInput').value); if(isNaN(thresh)||thresh<0.1||thresh>30){ showMessage('Threshold harus 0.1-30 L/min','error'); return; } document.getElementById('loading').style.display='block'; fetch('/api/settings/flow-threshold',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({threshold:thresh})}).then(r=>r.json()).then(d=>{ document.getElementById('loading').style.display='none'; if(d.status==='success'){ showMessage('Threshold tersimpan!','success'); document.getElementById('currentThreshold').innerText=thresh; } else showMessage('Gagal menyimpan','error'); }).catch(e=>{ document.getElementById('loading').style.display='none'; showMessage('Error','error'); }); }
        function saveAutoOffSettings(){ let grace=parseInt(document.getElementById('gracePeriodInput').value); let timeout=parseInt(document.getElementById('autoTimeoutInput').value); let timeoutFlow=parseFloat(document.getElementById('timeoutFlowInput').value); if(isNaN(grace)||grace<1||grace>600){ showMessage('Grace period harus 1-600 detik','error'); return; } if(isNaN(timeout)||timeout<10||timeout>3600){ showMessage('Timeout harus 10-3600 detik','error'); return; } if(isNaN(timeoutFlow)||timeoutFlow<0||timeoutFlow>30){ showMessage('Batas flow timeout harus 0-30 L/min','error'); return; } document.getElementById('loading').style.display='block'; fetch('/api/settings/auto-off',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({grace_period:grace, auto_off_timeout:timeout, timeout_flow_threshold:timeoutFlow})}).then(r=>r.json()).then(d=>{ document.getElementById('loading').style.display='none'; if(d.status==='success'){ showMessage('Waktu auto-off tersimpan!','success'); document.getElementById('currentGracePeriod').innerText=grace; document.getElementById('currentAutoTimeout').innerText=timeout; document.getElementById('currentTimeoutFlow').innerText=timeoutFlow.toFixed(1); } else showMessage('Gagal menyimpan','error'); }).catch(e=>{ document.getElementById('loading').style.display='none'; showMessage('Error','error'); }); }
        function resetCounters(){ if(confirm('Reset semua counter volume?')) fetch('/api/reset',{method:'POST'}).then(()=>{ showMessage('Counter berhasil direset','success'); loadSettings(); }).catch(()=>showMessage('Gagal reset counter','error')); }
        function goBack(){ window.location='/'; }
        loadSettings();
    </script>
</body>
</html>
)=====";
    server.send(200, "text/html", html);
}

void handleGetSettings()
{
    String json = "{\"capacity\":" + String(maxCapacity, 2) +
                  ",\"volume\":" + String(volume_IN, 2) +
                  ",\"flow_threshold\":" + String(minFlowRateThreshold, 2) +
                  ",\"timeout_flow_threshold\":" + String(timeoutFlowThreshold, 2) +
                  ",\"grace_period\":" + String(relayGracePeriod / 1000) +
                  ",\"auto_off_timeout\":" + String(autoOffTimeout / 1000) + "}";
    server.send(200, "application/json", json);
}

void handleSaveCapacity()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No body\"}");
        return;
    }
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    float capacity = doc["capacity"];
    if (capacity < 100 || capacity > 10000)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Kapasitas 100-10000 L\"}");
        return;
    }
    prefs.begin("tandon", false);
    prefs.putFloat("capacity", capacity);
    prefs.end();
    maxCapacity = capacity;
    Serial.printf("[EEPROM] Capacity saved: %.2f L\n", maxCapacity);
    server.send(200, "application/json", "{\"status\":\"success\",\"capacity\":" + String(capacity, 2) + "}");
}

void handleSaveVolume()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No body\"}");
        return;
    }
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    float volume = doc["volume"];
    if (volume < 0 || volume > 10000)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Volume harus 0-10000 L\"}");
        return;
    }
    noInterrupts();
    volume_IN = volume;
    interrupts();
    saveVolumeToEEPROM();
    Serial.printf("[MANUAL] Volume set to: %.2f L\n", volume_IN);
    server.send(200, "application/json", "{\"status\":\"success\",\"volume\":" + String(volume, 2) + "}");
}

void handleSaveFlowThreshold()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No body\"}");
        return;
    }
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    float threshold = doc["threshold"];

    if (threshold < 0.1 || threshold > 30.0)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Threshold harus 0.1-30 L/min\"}");
        return;
    }

    prefs.begin("tandon", false);
    prefs.putFloat("flow_threshold", threshold);
    prefs.end();

    minFlowRateThreshold = threshold;
    Serial.printf("[EEPROM] Flow Threshold saved: %.2f L/min\n", minFlowRateThreshold);
    server.send(200, "application/json", "{\"status\":\"success\",\"threshold\":" + String(threshold, 2) + "}");
}

void handleSaveAutoOffSettings()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No body\"}");
        return;
    }

    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    unsigned long graceSeconds = doc["grace_period"] | 0;
    unsigned long timeoutSeconds = doc["auto_off_timeout"] | 0;
    float timeoutFlow = doc["timeout_flow_threshold"] | 0.0;

    if (graceSeconds < 1 || graceSeconds > 600)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Grace period harus 1-600 detik\"}");
        return;
    }

    if (timeoutSeconds < 10 || timeoutSeconds > 3600)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Timeout harus 10-3600 detik\"}");
        return;
    }

    if (timeoutFlow < 0.0 || timeoutFlow > 30.0)
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Batas flow timeout harus 0-30 L/min\"}");
        return;
    }

    relayGracePeriod = graceSeconds * 1000UL;
    autoOffTimeout = timeoutSeconds * 1000UL;
    timeoutFlowThreshold = timeoutFlow;

    prefs.begin("tandon", false);
    prefs.putULong("grace_period", relayGracePeriod);
    prefs.putULong("auto_timeout", autoOffTimeout);
    prefs.putFloat("timeout_flow", timeoutFlowThreshold);
    prefs.end();

    Serial.printf("[EEPROM] Auto OFF settings saved: grace=%lu detik, timeout=%lu detik, timeout flow=%.2f L/min\n",
                  graceSeconds, timeoutSeconds, timeoutFlowThreshold);
    server.send(200, "application/json", "{\"status\":\"success\",\"grace_period\":" + String(graceSeconds) +
                                           ",\"auto_off_timeout\":" + String(timeoutSeconds) +
                                           ",\"timeout_flow_threshold\":" + String(timeoutFlowThreshold, 2) + "}");
}

// ==================== OTA HANDLERS ====================
void handleOTAStartPage()
{
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>OTA Update - Tandon Air</title>
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background:linear-gradient(135deg,#667eea 0%,#764ba2 100%); min-height:100vh; padding:20px; }
    .container { max-width:600px; margin:50px auto; background:white; padding:40px; border-radius:15px; box-shadow:0 8px 32px rgba(0,0,0,0.1); }
    h1 { color:#667eea; margin-bottom:10px; font-size:28px; }
    .info { color:#999; margin-bottom:30px; font-size:14px; }
    .settings-box { background:#f8f9fa; padding:20px; border-radius:10px; margin-bottom:20px; border-left:4px solid #667eea; }
    .setting-item { display:flex; justify-content:space-between; padding:10px 0; border-bottom:1px solid #e0e0e0; }
    .setting-item:last-child { border-bottom:none; }
    .label { color:#666; font-weight:500; }
    .value { color:#333; font-family:monospace; }
    .status-box { padding:15px; border-radius:8px; margin-bottom:20px; text-align:center; font-weight:500; }
    .status-ready { background:#d4edda; color:#155724; }
    .status-updating { background:#fff3cd; color:#856404; }
    .btn { width:100%; padding:12px; border:none; border-radius:8px; font-size:16px; font-weight:600; cursor:pointer; margin-top:10px; transition:0.3s; }
    .btn-primary { background:#667eea; color:white; }
    .btn-primary:hover { background:#5568d3; }
    .btn-secondary { background:#6c757d; color:white; }
    .btn-secondary:hover { background:#5a6268; }
    .progress-box { display:none; margin-top:20px; }
    .progress { width:100%; height:30px; background:#e0e0e0; border-radius:8px; overflow:hidden; }
    .progress-bar { height:100%; background:#667eea; width:0%; transition:width 0.3s; display:flex; align-items:center; justify-content:center; color:white; font-weight:bold; }
    .method-tabs { display:flex; gap:10px; margin-bottom:20px; }
    .tab-btn { flex:1; padding:10px; border:2px solid #e0e0e0; background:white; border-radius:8px; cursor:pointer; font-weight:500; transition:0.3s; }
    .tab-btn.active { border-color:#667eea; background:#667eea; color:white; }
    .method-content { display:none; }
    .method-content.active { display:block; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📡 OTA Firmware Update</h1>
    <p class="info">Update firmware tanpa kabel USB</p>
    
    <div class="settings-box">
      <div class="setting-item">
        <span class="label">Device IP:</span>
        <span class="value" id="device-ip">Loading...</span>
      </div>
      <div class="setting-item">
        <span class="label">Hostname:</span>
        <span class="value">tandon-air</span>
      </div>
      <div class="setting-item">
        <span class="label">Status:</span>
        <span class="value" id="ota-status">Checking...</span>
      </div>
    </div>

    <div id="status-box" class="status-box" style="display:none;"></div>

    <div class="method-tabs">
      <button class="tab-btn active" onclick="switchMethod('wireless')">✨ Wireless OTA</button>
      <button class="tab-btn" onclick="switchMethod('manual')">📁 Manual Upload</button>
    </div>

    <div id="wireless" class="method-content active">
      <h3 style="margin-bottom:15px; color:#333;">Metode Wireless (Arduino IDE)</h3>
      <ol style="margin-left:20px; line-height:1.8; color:#666;">
        <li>Buka Arduino IDE → Tools → Port</li>
        <li>Pilih "tandon-air at [IP Address]"</li>
        <li>Klik Upload sketch (Ctrl+U)</li>
        <li>Masukkan password: <code style="background:#f0f0f0; padding:2px 6px; border-radius:3px;">admin123</code></li>
      </ol>
      <button class="btn btn-primary" onclick="copyConfig()">📋 Copy Config</button>
    </div>

    <div id="manual" class="method-content">
      <h3 style="margin-bottom:15px; color:#333;">Upload File Binary</h3>
      <input type="file" id="file-input" accept=".bin" style="width:100%; padding:10px; border:2px dashed #667eea; border-radius:8px; cursor:pointer;">
      <button class="btn btn-primary" onclick="uploadBinary()">🚀 Upload Binary</button>
    </div>

    <div class="progress-box" id="progress-box">
      <div class="progress">
        <div class="progress-bar" id="progress-bar">0%</div>
      </div>
      <p id="progress-text" style="text-align:center; margin-top:10px; color:#666;"></p>
    </div>

    <button class="btn btn-secondary" onclick="goBack()">← Kembali</button>
  </div>

  <script>
    function switchMethod(method) {
      document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.method-content').forEach(el => el.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById(method).classList.add('active');
    }

    async function updateOTAStatus() {
      try {
        const response = await fetch('/api/ota/status');
        const data = await response.json();
        
        document.getElementById('device-ip').textContent = data.ip || 'Unknown';
        
        const statusEl = document.getElementById('ota-status');
        const statusBox = document.getElementById('status-box');
        
        if (data.ota_ready) {
          statusEl.textContent = '✅ Ready';
          statusBox.className = 'status-box status-ready';
          statusBox.textContent = '✓ OTA siap untuk update';
          statusBox.style.display = 'block';
        } else if (data.ota_active) {
          statusEl.textContent = '⏳ Updating';
          statusBox.className = 'status-box status-updating';
          statusBox.textContent = '⏳ OTA dalam proses...';
          statusBox.style.display = 'block';
        } else {
          statusEl.textContent = '❌ Not Ready';
          statusBox.textContent = '❌ OTA tidak siap (WiFi tidak terhubung)';
          statusBox.style.display = 'block';
        }
      } catch (e) {
        console.error('Status error:', e);
        document.getElementById('ota-status').textContent = '⚠️ Error';
      }
    }

    function copyConfig() {
      const config = `Hostname: tandon-air\nPassword: admin123`;
      navigator.clipboard.writeText(config).then(() => {
        alert('✓ Konfigurasi disalin ke clipboard');
      });
    }

    async function uploadBinary() {
      const fileInput = document.getElementById('file-input');
      if (!fileInput.files.length) {
        alert('Pilih file binary terlebih dahulu');
        return;
      }

      const file = fileInput.files[0];
      const formData = new FormData();
      formData.append('file', file);

      const progressBox = document.getElementById('progress-box');
      progressBox.style.display = 'block';

      try {
        const xhr = new XMLHttpRequest();
        
        xhr.upload.addEventListener('progress', (e) => {
          if (e.lengthComputable) {
            const percentComplete = (e.loaded / e.total) * 100;
            document.getElementById('progress-bar').style.width = percentComplete + '%';
            document.getElementById('progress-bar').textContent = Math.round(percentComplete) + '%';
            document.getElementById('progress-text').textContent = `${Math.round(e.loaded / 1024)} KB / ${Math.round(e.total / 1024)} KB`;
          }
        });

        xhr.addEventListener('load', () => {
          if (xhr.status === 200) {
            alert('✓ Update berhasil! Perangkat sedang restart...');
            setTimeout(() => location.reload(), 3000);
          } else {
            alert('❌ Upload gagal: ' + xhr.response);
          }
          progressBox.style.display = 'none';
        });

        xhr.addEventListener('error', () => {
          alert('❌ Error saat upload');
          progressBox.style.display = 'none';
        });

        xhr.open('POST', '/update');
        xhr.send(formData);
      } catch (e) {
        alert('❌ Error: ' + e.message);
        progressBox.style.display = 'none';
      }
    }

    function goBack() {
      window.location.href = '/';
    }

    // Update status setiap 2 detik
    updateOTAStatus();
    setInterval(updateOTAStatus, 2000);
  </script>
</body>
</html>
    )=====";
    server.send(200, "text/html", html);
}

void handleOTAStatus()
{
    DynamicJsonDocument doc(384);
    doc["ip"] = WiFi.localIP().toString();
    doc["ota_ready"] = (wifiConnected && !isAPMode && !otaActive && !httpOtaActive);
    doc["ota_active"] = (otaActive || httpOtaActive);
    doc["http_ota_active"] = httpOtaActive;
    doc["ota_initialized"] = otaInitialized;
    doc["ota_status"] = otaStatusText;
    doc["ota_error"] = otaErrorMessage;
    doc["hostname"] = deviceHostname;
    doc["version"] = "1.0.0";

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void finishHttpOTAError(int statusCode, const char *message)
{
    otaActive = false;
    httpOtaActive = false;
    otaStatusText = "error";
    otaErrorMessage = message;
    httpOtaShouldReboot = false;
    httpOtaStatusCode = statusCode;
    httpOtaResponse = String("{\"status\":\"error\",\"error\":\"") + message + "\"}";

    if (wifiConnected && !isAPMode && WiFi.status() == WL_CONNECTED)
    {
        setupOTA();
    }
}

// ==================== HTTP OTA FIRMWARE UPLOAD ====================
// Receive firmware file data
void handleOTAUpload()
{
    HTTPUpload &upload = server.upload();
    static uint32_t uploadedBytes = 0;
    static uint32_t totalFileSize = 0;
    static bool updateStarted = false;
    static bool sessionHadError = false;
    static uint32_t lastPercent = 0;

    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("[OTA HTTP] Upload started: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
        Serial.printf("[OTA HTTP] Free heap before: %d bytes\n", ESP.getFreeHeap());

        uploadedBytes = 0;
        totalFileSize = upload.totalSize;
        updateStarted = false;
        sessionHadError = false;
        lastPercent = 0;
        httpOtaShouldReboot = false;
        httpOtaStatusCode = 200;
        httpOtaResponse = "{\"status\":\"uploading\"}";
        otaErrorMessage = "";

        // Stop ArduinoOTA listener temporarily
        Serial.println("[OTA HTTP] Stopping ArduinoOTA...");
        ArduinoOTA.end();
        delay(100);

        // Disconnect MQTT
        if (mqtt.connected())
        {
            Serial.println("[OTA HTTP] Disconnecting MQTT...");
            mqtt.disconnect();
            delay(100);
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[OTA HTTP] ERROR: WiFi is not connected!");
            finishHttpOTAError(503, "WiFi is not connected");
            sessionHadError = true;
            return;
        }

        Serial.println("[OTA HTTP] Preparing Update...");
        Serial.printf("[OTA HTTP] Partition space check...\n");

        // Multipart uploads can report totalSize as 0 on ESP32 WebServer.
        // Let Update use the full OTA partition instead of trusting that value.
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            Serial.println("[OTA HTTP] ERROR: Update.begin() failed!");
            Serial.printf("[OTA HTTP] Debug info:\n");
            Serial.printf("  - Reported file size: %u bytes\n", totalFileSize);
            Serial.printf("  - Free heap: %u bytes\n", ESP.getFreeHeap());
            Serial.printf("  - Free PSRAM: %u bytes\n", ESP.getFreePsram());
            Update.printError(Serial);

            finishHttpOTAError(500, "Update.begin failed - check flash partition");
            sessionHadError = true;
            return;
        }

        updateStarted = true;
        otaActive = true;
        httpOtaActive = true;
        otaStartTime = millis();
        otaStatusText = "uploading";
        Serial.println("[OTA HTTP] Update.begin() successful");
        Serial.printf("[OTA HTTP] Ready to receive %u bytes\n", totalFileSize);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!updateStarted)
        {
            if (sessionHadError)
            {
                Serial.println("[OTA HTTP] Ignoring write after earlier session error");
                return;
            }
            Serial.println("[OTA HTTP] ERROR: Update not properly started!");
            Update.abort();
            finishHttpOTAError(500, "Update session error");
            sessionHadError = true;
            return;
        }

        uploadedBytes += upload.currentSize;
        uint32_t percent = totalFileSize > 0 ? (uploadedBytes * 100) / totalFileSize : 0;

        // Write data to flash with error handling
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize)
        {
            Serial.printf("[OTA HTTP] ERROR: Update.write() failed! Expected %d, got %d\n", upload.currentSize, written);
            Serial.printf("[OTA HTTP] At byte position: %u / %u\n", uploadedBytes, totalFileSize);
            Update.printError(Serial);
            Update.abort();
            updateStarted = false;
            finishHttpOTAError(500, "Update.write failed");
            sessionHadError = true;
            return;
        }

        // Log progress every 10%
        if (percent % 10 == 0 && percent != lastPercent)
        {
            Serial.printf("[OTA HTTP] Progress: %u%% (%u / %u bytes) - Free heap: %u\n",
                          percent, uploadedBytes, totalFileSize, ESP.getFreeHeap());
            lastPercent = percent;
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        Serial.printf("[OTA HTTP] Upload finished: %u / %u bytes\n", uploadedBytes, totalFileSize);

        if (!updateStarted)
        {
            if (sessionHadError)
            {
                Serial.println("[OTA HTTP] Upload ended after earlier session error");
                return;
            }
            Serial.println("[OTA HTTP] ERROR: Update session invalid!");
            finishHttpOTAError(500, "Update session error");
            sessionHadError = true;
            return;
        }

        // Finalize update
        if (Update.end(true)) // true = check MD5
        {
            Serial.println("[OTA HTTP] Update successful!");
            Serial.println("[OTA HTTP] MD5 verified - firmware is valid");
            Serial.println("[OTA HTTP] Device will restart after response is sent...");
            otaStatusText = "complete";
            otaActive = false;
            httpOtaActive = false;
            updateStarted = false;
            sessionHadError = false;
            httpOtaShouldReboot = true;
            httpOtaStatusCode = 200;
            httpOtaResponse = "{\"status\":\"success\",\"message\":\"Update completed, restarting...\"}";
        }
        else
        {
            Serial.println("[OTA HTTP] ERROR: Update.end() failed!");
            Serial.printf("[OTA HTTP] Last written: %u / %u bytes\n", uploadedBytes, totalFileSize);
            Update.printError(Serial);
            updateStarted = false;
            finishHttpOTAError(500, "Update finalization failed");
            sessionHadError = true;
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Serial.println("[OTA HTTP] Upload aborted!");
        Update.abort();
        updateStarted = false;
        finishHttpOTAError(500, "Upload aborted");
        sessionHadError = true;
    }
}

// Send response for OTA POST request
void handleOTAUpdate()
{
    Serial.printf("[OTA HTTP] Sending final response: %d %s\n", httpOtaStatusCode, httpOtaResponse.c_str());
    server.sendHeader("Connection", "close");
    server.send(httpOtaStatusCode, "application/json", httpOtaResponse);

    if (httpOtaShouldReboot)
    {
        delay(1000);
        ESP.restart();
    }
}
