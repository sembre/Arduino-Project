#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <PCF8574.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include <time.h>
#include <HTTPClient.h>

// Pin mapping for ESP32-S3 WROOM CAM (OV5640)
#ifndef CAM_PIN_PWDN
#define CAM_PIN_PWDN 38
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#endif

Preferences prefs;
WebServer server(80);

//////////////// WIFI MANAGER //////////////////
String ssid = "";
String password = "";

const char *apSSID = "ESP32-Setup";
const char *apPASS = "12345678";

//////////////// MQTT //////////////////
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_client = "LampuRumahAgus";

WiFiClient espClient;
PubSubClient mqtt(espClient);
bool inMQTTCallback = false;           // Flag to prevent feedback loops
unsigned long mqttConnectTime = 0;     // Timestamp when MQTT connected
const long MQTT_SETTLING_TIME = 10000; // Wait 10 sec to ignore old retained messages
bool mqttSubscribed = false;           // Flag to track subscription state

//////////////// DDNS //////////////////
String ddnsProvider = "";                          // "duckdns", "noip", "dyndns"
String ddnsDomain = "";                            // domain name
String ddnsToken = "";                             // API token/key
bool ddnsEnabled = false;                          // enable/disable DDNS
unsigned long lastDDNSUpdate = 0;                  // timestamp of last DDNS update
const unsigned long DDNS_UPDATE_INTERVAL = 300000; // update every 5 minutes
String lastDDNSIP = "";                            // last known public IP
bool ddnsUpdateSuccess = false;                    // status of last update

//////////////// RELAY //////////////////
PCF8574 board1(0x20);
PCF8574 board2(0x21);
PCF8574 board3(0x22);
PCF8574 board4(0x23);

bool relayState[32];
int relayCount = 32;
String relayNames[32];

// Auto-light control (camera as light sensor)
bool autoLightMode[32];      // per relay auto-light mode
int autoLightThreshold = 60; // 0..255 (rendah = lebih gelap)
unsigned long lastLightCheck = 0;
const unsigned long LIGHT_CHECK_INTERVAL = 10000; // cek setiap 10 detik
bool autoLightActive[32];                         // status terakhir per relay: apakah auto telah menyalakan lampu
bool cameraAvailable = false;                     // apakah kamera berhasil inisialisasi dan siap pakai
float lastLightAvg = 0.0;                         // cahaya terakhir diukur (0..255)
bool lastLightIsDark = false;                     // apakah kondisi terakhir dianggap gelap

struct Schedule
{
  int onHour = -1;
  int onMin = 0;
  int offHour = -1;
  int offMin = 0;
  bool enabled = false;
};

#define MAX_SCHEDULES_PER_RELAY 3
Schedule schedules[32][MAX_SCHEDULES_PER_RELAY];
static unsigned long lastScheduleCheck = 0;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // UTC+7 (WIB)
const int daylightOffset_sec = 0;

////////////////////////////////////////////////
// WIFI CONNECT
void startAP()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPASS);
  Serial.println("AP Mode: ESP32-Setup");
}

void connectToWiFi()
{

  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  prefs.end();

  Serial.println("[WiFi] Mencoba terhubung...");
  Serial.println("[WiFi] SSID: " + ssid);

  if (ssid != "")
  {
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
    {
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] ✓ Terhubung!");
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("\n[WiFi] ✗ Gagal terhubung, mode AP dimulai");
    startAP();
  }
}

////////////////////////////////////////////////
// SCAN WIFI
String scanNetworks()
{
  int n = WiFi.scanNetworks();
  String html = "";
  for (int i = 0; i < n; i++)
  {
    html += "<option>" + WiFi.SSID(i) + "</option>";
  }
  return html;
}

////////////////////////////////////////////////
// NTP TIME SYNC
void syncTime()
{
  Serial.println("[NTP] Mensinkronisasi waktu...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 24 * 3600 && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }
  Serial.println();
  struct tm timeinfo = *localtime(&now);
  Serial.println("[NTP] ✓ Waktu: " + String(asctime(&timeinfo)));
}

void loadSchedules()
{
  prefs.begin("schedules", true);
  for (int i = 0; i < 32; i++)
  {
    for (int s = 0; s < MAX_SCHEDULES_PER_RELAY; s++)
    {
      String prefix = "sch_" + String(i) + "_" + String(s) + "_";
      schedules[i][s].onHour = prefs.getInt((prefix + "onH").c_str(), -1);
      schedules[i][s].onMin = prefs.getInt((prefix + "onM").c_str(), 0);
      schedules[i][s].offHour = prefs.getInt((prefix + "offH").c_str(), -1);
      schedules[i][s].offMin = prefs.getInt((prefix + "offM").c_str(), 0);
      schedules[i][s].enabled = prefs.getBool((prefix + "en").c_str(), false);
    }
  }
  prefs.end();
  Serial.println("[Config] Schedules loaded");
}

void saveSchedules()
{
  prefs.begin("schedules", false);
  for (int i = 0; i < 32; i++)
  {
    for (int s = 0; s < MAX_SCHEDULES_PER_RELAY; s++)
    {
      String prefix = "sch_" + String(i) + "_" + String(s) + "_";
      prefs.putInt((prefix + "onH").c_str(), schedules[i][s].onHour);
      prefs.putInt((prefix + "onM").c_str(), schedules[i][s].onMin);
      prefs.putInt((prefix + "offH").c_str(), schedules[i][s].offHour);
      prefs.putInt((prefix + "offM").c_str(), schedules[i][s].offMin);
      prefs.putBool((prefix + "en").c_str(), schedules[i][s].enabled);
    }
  }
  prefs.end();
}

void checkSchedules()
{
  if (millis() - lastScheduleCheck < 60000)
    return; // Check every minute
  lastScheduleCheck = millis();

  time_t now = time(nullptr);
  struct tm timeinfo = *localtime(&now);
  int currentHour = timeinfo.tm_hour;
  int currentMin = timeinfo.tm_min;

  for (int i = 0; i < relayCount; i++)
  {
    for (int s = 0; s < MAX_SCHEDULES_PER_RELAY; s++)
    {
      if (!schedules[i][s].enabled || schedules[i][s].onHour == -1)
        continue;

      // Check if it's time to turn on
      if (currentHour == schedules[i][s].onHour && currentMin == schedules[i][s].onMin)
      {
        if (!relayState[i])
        {
          setRelay(i, true);
          Serial.println("[Schedule] Relay " + String(i + 1) + " (slot " + String(s + 1) + ") ON");
        }
      }

      // Check if it's time to turn off
      if (schedules[i][s].offHour != -1 && currentHour == schedules[i][s].offHour && currentMin == schedules[i][s].offMin)
      {
        if (relayState[i])
        {
          setRelay(i, false);
          Serial.println("[Schedule] Relay " + String(i + 1) + " (slot " + String(s + 1) + ") OFF");
        }
      }
    }
  }
}

////////////////////////////////////////////////
// RELAY CONTROL
void setRelay(int ch, bool state)
{
  PCF8574 *board;
  int pin;

  if (ch < 8)
  {
    board = &board1;
    pin = ch;
  }
  else if (ch < 16)
  {
    board = &board2;
    pin = ch - 8;
  }
  else if (ch < 24)
  {
    board = &board3;
    pin = ch - 16;
  }
  else
  {
    board = &board4;
    pin = ch - 24;
  }

  Serial.println("[Relay] Ch" + String(ch + 1) + " -> " + (state ? "ON" : "OFF"));
  board->digitalWrite(pin, state);
  relayState[ch] = state;
  // Note: NOT saving to NVRAM per relay - MQTT retained messages are the state source

  // Only publish to MQTT if not already in a callback (prevent loops)
  if (!inMQTTCallback)
  {
    mqtt.publish(("rumah/lampu/" + String(ch + 1)).c_str(), state ? "ON" : "OFF");
  }
}

////////////////////////////////////////////////
// MQTT RECEIVE
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String msg = "";
  for (int i = 0; i < length; i++)
    msg += (char)payload[i];

  String t = topic;

  Serial.println("[MQTT] Topic: " + t + " -> " + msg);

  // Skip relay control during settling period (to ignore retained messages on startup)
  bool inSettlingPeriod = (millis() - mqttConnectTime) < MQTT_SETTLING_TIME;
  if (inSettlingPeriod)
  {
    Serial.println("[MQTT] ⏱ Settling period, ignoring relay command...");
    return;
  }

  inMQTTCallback = true; // Set flag to prevent re-publishing

  if (t == "rumah/lampu/threshold")
  {
    int tval = msg.toInt();
    if (tval < 0)
      tval = 0;
    if (tval > 255)
      tval = 255;
    autoLightThreshold = tval;
    saveLightConfig();
    Serial.println("[MQTT] Auto Light threshold di-set -> " + String(autoLightThreshold));
    return;
  }

  if (t == "rumah/lampu/auto")
  {
    bool enabled = (msg == "1" || msg.equalsIgnoreCase("ON") || msg.equalsIgnoreCase("TRUE"));
    for (int i = 0; i < relayCount; i++)
      autoLightMode[i] = enabled;
    saveLightConfig();
    Serial.println("[MQTT] Auto Light mode di-set untuk semua relay -> " + String(enabled ? "ON" : "OFF"));
    return;
  }

  if (t.startsWith("rumah/lampu/") && t.endsWith("/auto"))
  {
    String relayStr = t.substring(12, t.length() - 5); // extract relay number between "rumah/lampu/" and "/auto"
    int ch = relayStr.toInt() - 1;
    if (ch >= 0 && ch < relayCount)
    {
      bool enabled = (msg == "1" || msg.equalsIgnoreCase("ON") || msg.equalsIgnoreCase("TRUE"));
      autoLightMode[ch] = enabled;
      saveLightConfig();
      Serial.printf("[MQTT] Auto Light mode relay %d di-set -> %s\n", ch + 1, enabled ? "ON" : "OFF");
    }
    return;
  }

  if (t.startsWith("rumah/lampu/"))
  {
    int ch = t.substring(12).toInt() - 1;
    if (ch >= 0 && ch < relayCount)
      setRelay(ch, msg == "ON");
  }

  if (t == "rumah/lampu/all")
  {
    Serial.println("[MQTT] Mengontrol semua relay -> " + msg);
    for (int i = 0; i < relayCount; i++)
      setRelay(i, msg == "ON");
  }

  inMQTTCallback = false; // Clear flag
}

void loadRelayConfig()
{
  prefs.begin("relay_config", true);
  relayCount = prefs.getInt("count", 32);
  if (relayCount < 1)
    relayCount = 1;
  if (relayCount > 32)
    relayCount = 32;
  for (int i = 0; i < 32; i++)
  {
    relayNames[i] = prefs.getString(("name_" + String(i)).c_str(), "Lampu " + String(i + 1));
  }
  prefs.end();
  Serial.println("[Config] Loaded: " + String(relayCount) + " relays");
}

void saveRelayConfig()
{
  prefs.begin("relay_config", false);
  prefs.putInt("count", relayCount);
  for (int i = 0; i < 32; i++)
  {
    prefs.putString(("name_" + String(i)).c_str(), relayNames[i]);
  }
  prefs.end();
}

void saveRelayState()
{
  // DISABLED: NVRAM persistence causes reliability issues
  // MQTT retained messages serve as state memory
  // This function kept for future use
}

void loadRelayState()
{
  // DISABLED: NVRAM persistence causes reliability issues
  // Relays start OFF by default, MQTT retained messages restore state after settling period
  // This function kept for future use
}

void setAllRelays(bool state)
{
  for (int i = 0; i < relayCount; i++)
  {
    setRelay(i, state);
  }
}

void loadLightConfig()
{
  prefs.begin("light_cfg", true);
  String autoModes = prefs.getString("auto_modes", "");
  autoLightThreshold = prefs.getInt("threshold", autoLightThreshold);
  prefs.end();

  // Parse auto modes string - split by comma
  if (autoModes.length() > 0)
  {
    int startIdx = 0;
    int idx = 0;
    while (startIdx < autoModes.length() && idx < relayCount)
    {
      int commaIdx = autoModes.indexOf(',', startIdx);
      if (commaIdx == -1)
        commaIdx = autoModes.length();

      String token = autoModes.substring(startIdx, commaIdx);
      autoLightMode[idx] = (token == "1");

      idx++;
      startIdx = commaIdx + 1;
    }
    // Set default false for any missing entries
    for (int i = idx; i < relayCount; i++)
      autoLightMode[i] = false;
  }
  else
  {
    // Default: all auto modes off
    for (int i = 0; i < relayCount; i++)
      autoLightMode[i] = false;
  }

  Serial.printf("[Light] Config loaded: threshold=%d\n", autoLightThreshold);
  for (int i = 0; i < relayCount; i++)
    Serial.printf("  Relay %d auto: %s\n", i + 1, autoLightMode[i] ? "ON" : "OFF");
}

void saveLightConfig()
{
  prefs.begin("light_cfg", false);
  String autoModes = "";
  for (int i = 0; i < relayCount; i++)
  {
    autoModes += autoLightMode[i] ? "1" : "0";
    if (i < relayCount - 1)
      autoModes += ",";
  }
  prefs.putString("auto_modes", autoModes);
  prefs.putInt("threshold", autoLightThreshold);
  prefs.end();
  Serial.printf("[Light] Config saved: threshold=%d, auto_modes=%s\n", autoLightThreshold, autoModes.c_str());
}

void loadDDNSConfig()
{
  prefs.begin("ddns_cfg", true);
  ddnsEnabled = prefs.getBool("enabled", false);
  ddnsProvider = prefs.getString("provider", "");
  ddnsDomain = prefs.getString("domain", "");
  ddnsToken = prefs.getString("token", "");
  prefs.end();
  Serial.printf("[DDNS] Config loaded: enabled=%d provider=%s domain=%s\n", ddnsEnabled, ddnsProvider.c_str(), ddnsDomain.c_str());
}

void saveDDNSConfig()
{
  prefs.begin("ddns_cfg", false);
  prefs.putBool("enabled", ddnsEnabled);
  prefs.putString("provider", ddnsProvider);
  prefs.putString("domain", ddnsDomain);
  prefs.putString("token", ddnsToken);
  prefs.end();
  Serial.printf("[DDNS] Config saved: enabled=%d provider=%s domain=%s\n", ddnsEnabled, ddnsProvider.c_str(), ddnsDomain.c_str());
}

String getPublicIP()
{
  if (WiFi.status() != WL_CONNECTED)
    return "";

  HTTPClient http;
  http.begin("http://api.ipify.org");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String ip = http.getString();
    http.end();
    return ip;
  }
  else
  {
    Serial.printf("[DDNS] Failed to get public IP: %d\n", httpCode);
    http.end();
    return "";
  }
}

bool updateDDNS()
{
  if (!ddnsEnabled || ddnsProvider == "" || ddnsDomain == "" || ddnsToken == "")
  {
    Serial.println("[DDNS] DDNS not configured or disabled");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[DDNS] WiFi not connected");
    return false;
  }

  String currentIP = getPublicIP();
  if (currentIP == "")
  {
    Serial.println("[DDNS] Could not get current public IP");
    return false;
  }

  // Check if IP has changed
  if (currentIP == lastDDNSIP && lastDDNSUpdate > 0)
  {
    Serial.println("[DDNS] IP has not changed, skipping update");
    return true;
  }

  bool success = false;
  HTTPClient http;

  if (ddnsProvider == "duckdns")
  {
    // DuckDNS format: https://www.duckdns.org/update?domains={DOMAIN}&token={TOKEN}&ip={IP}
    String url = "https://www.duckdns.org/update?domains=" + ddnsDomain + "&token=" + ddnsToken + "&ip=" + currentIP;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      String response = http.getString();
      if (response.indexOf("OK") >= 0)
      {
        success = true;
        Serial.printf("[DDNS] DuckDNS updated successfully: %s -> %s\n", ddnsDomain.c_str(), currentIP.c_str());
      }
      else
      {
        Serial.printf("[DDNS] DuckDNS update failed: %s\n", response.c_str());
      }
    }
    else
    {
      Serial.printf("[DDNS] DuckDNS HTTP error: %d\n", httpCode);
    }
  }
  else if (ddnsProvider == "noip")
  {
    // No-IP format: https://dynupdate.no-ip.com/nic/update?hostname={DOMAIN}&myip={IP}
    // Requires Basic Auth with username:password
    String url = "https://dynupdate.no-ip.com/nic/update?hostname=" + ddnsDomain + "&myip=" + currentIP;
    http.begin(url);

    // Parse token as username:password
    int colonIndex = ddnsToken.indexOf(':');
    if (colonIndex > 0)
    {
      String username = ddnsToken.substring(0, colonIndex);
      String password = ddnsToken.substring(colonIndex + 1);
      http.setAuthorization(username.c_str(), password.c_str());
    }

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      String response = http.getString();
      if (response.indexOf("good") >= 0 || response.indexOf("nochg") >= 0)
      {
        success = true;
        Serial.printf("[DDNS] No-IP updated successfully: %s -> %s\n", ddnsDomain.c_str(), currentIP.c_str());
      }
      else
      {
        Serial.printf("[DDNS] No-IP update failed: %s\n", response.c_str());
      }
    }
    else
    {
      Serial.printf("[DDNS] No-IP HTTP error: %d\n", httpCode);
    }
  }
  else
  {
    Serial.printf("[DDNS] Unsupported provider: %s\n", ddnsProvider.c_str());
  }

  http.end();

  if (success)
  {
    lastDDNSIP = currentIP;
    lastDDNSUpdate = millis();
    ddnsUpdateSuccess = true;
  }
  else
  {
    ddnsUpdateSuccess = false;
  }

  return success;
}

void checkDDNS()
{
  if (!ddnsEnabled)
    return;

  if (millis() - lastDDNSUpdate < DDNS_UPDATE_INTERVAL)
    return;

  Serial.println("[DDNS] Checking for IP update...");
  updateDDNS();
}

bool initCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA; // smaller to reduce RAM requirement
  config.jpeg_quality = 12;
  config.fb_count = 1; // single buffer to avoid PSRAM requirement

  // If PSRAM is available, use it for the frame buffer (preferred).
  // Otherwise fall back to DRAM and reduce frame size to avoid allocation failures.
  if (psramFound())
  {
    Serial.println("[Camera] PSRAM detected, using QSPI PSRAM for framebuffer");
    config.fb_location = CAMERA_FB_IN_PSRAM;
  }
  else
  {
    Serial.println("[Camera] PSRAM not detected, using DRAM framebuffer");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_QQVGA; // smallest stable frame size for best chance of allocation
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("[Camera] Init failed with error 0x%x\n", err);
    return false;
  }

  Serial.println("[Camera] Initialized");
  return true;
}

void checkLightSensor()
{
  if (!cameraAvailable)
    return;

  if (millis() - lastLightCheck < LIGHT_CHECK_INTERVAL)
    return;

  lastLightCheck = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("[Camera] capture failed");
    return;
  }

  uint32_t sum = 0;
  for (size_t i = 0; i < fb->len; i++)
    sum += fb->buf[i];

  float avg = (float)sum / fb->len;
  esp_camera_fb_return(fb);

  bool isDark = avg < autoLightThreshold;
  lastLightAvg = avg;
  lastLightIsDark = isDark;

  Serial.printf("[Light] avg=%.1f threshold=%d dark=%d\n", avg, autoLightThreshold, isDark);

  // Kontrol auto-light per relay
  for (int i = 0; i < relayCount; i++)
  {
    Serial.printf("[Light] Relay %d: auto=%d active=%d\n", i + 1, autoLightMode[i], autoLightActive[i]);
    if (!autoLightMode[i])
      continue; // skip jika auto mode tidak aktif untuk relay ini

    if (isDark && !autoLightActive[i])
    {
      Serial.printf("[Light] Relay %d: Gelap terdeteksi -> menyalakan lampu\n", i + 1);
      setRelay(i, true);
      autoLightActive[i] = true;
    }
    else if (!isDark && autoLightActive[i])
    {
      Serial.printf("[Light] Relay %d: Terang -> mematikan lampu\n", i + 1);
      setRelay(i, false);
      autoLightActive[i] = false;
    }
  }
}

void connectMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[MQTT] WiFi tidak terhubung");
    return;
  }

  while (!mqtt.connected())
  {
    Serial.println("[MQTT] Menghubungkan ke " + String(mqtt_server) + "...");
    if (mqtt.connect(mqtt_client))
    {
      Serial.println("[MQTT] ✓ Terhubung!");
      // DO NOT subscribe yet - wait for settling period to pass
      Serial.println("[MQTT] ⏳ Akan subscribe setelah settling period " + String(MQTT_SETTLING_TIME / 1000) + "s");
      mqttConnectTime = millis(); // Start settling timer
      mqttSubscribed = false;     // Mark as not subscribed yet
    }
    else
    {
      Serial.println("[MQTT] ✗ Gagal terhubung, retry...");
      delay(2000);
    }
  }
}

////////////////////////////////////////////////
// WEB PAGE
String webpage()
{
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Lampu Rumah</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
  <style>
    body { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
    .container-main { max-width: 1000px; margin: 0 auto; }
    .header { text-align: center; color: white; margin-bottom: 30px; padding: 20px; }
    .header h1 { font-size: 2.5em; font-weight: bold; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); margin-bottom: 10px; }
    .header p { font-size: 1.1em; opacity: 0.9; }
    .wifi-config { background: white; border-radius: 15px; padding: 25px; margin-bottom: 30px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }
    .wifi-config h5 { color: #667eea; margin-bottom: 15px; font-weight: 600; }
    .form-control, .form-select { border-radius: 8px; border: 2px solid #ddd; padding: 10px 15px; transition: all 0.3s; }
    .form-control:focus, .form-select:focus { border-color: #667eea; box-shadow: 0 0 0 0.2rem rgba(102, 126, 234, 0.25); }
    .btn-config { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; padding: 10px 30px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s; }
    .btn-config:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4); color: white; }
    .relay-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 15px; margin-bottom: 20px; }
    .relay-card { background: white; border-radius: 12px; padding: 15px; text-align: center; box-shadow: 0 5px 15px rgba(0,0,0,0.1); transition: all 0.3s; cursor: pointer; }
    .relay-card:hover { transform: translateY(-5px); box-shadow: 0 10px 25px rgba(0,0,0,0.15); }
    .relay-label { font-size: 0.9em; font-weight: 600; color: #333; margin-bottom: 8px; display: block; }
    .label-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; gap: 5px; }
    .label-text { flex: 1; font-size: 0.9em; font-weight: 600; color: #333; word-break: break-word; }
    .btn-edit-inline { padding: 4px 6px; font-size: 0.8em; background: #667eea; color: white; border: none; border-radius: 4px; cursor: pointer; transition: all 0.3s; min-width: 32px; flex-shrink: 0; }
    .btn-edit-inline:hover { background: #5568d3; transform: scale(1.1); }
    .relay-btn { width: 100%; padding: 10px; border: none; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s; font-size: 0.9em; }
    .relay-btn.on { background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); color: white; box-shadow: 0 5px 15px rgba(17, 153, 142, 0.4); }
    .relay-btn.off { background: linear-gradient(135deg, #ee0979 0%, #ff6a00 100%); color: white; box-shadow: 0 5px 15px rgba(238, 9, 121, 0.4); }
    .relay-btn:active { transform: scale(0.95); }
    .control-section { background: white; border-radius: 15px; padding: 25px; margin-bottom: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }
    .control-section h5 { color: #667eea; margin-bottom: 20px; font-weight: 600; display: flex; align-items: center; gap: 10px; }
    .btn-master { width: 100%; padding: 15px; background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white; border: none; border-radius: 8px; font-weight: 600; font-size: 1.1em; cursor: pointer; transition: all 0.3s; margin-top: 10px; }
    .btn-master:hover { transform: translateY(-3px); box-shadow: 0 8px 20px rgba(245, 87, 108, 0.4); }
    .btn-schedule { padding: 4px 8px; font-size: 0.75em; background: #17a2b8; color: white; border: none; border-radius: 4px; cursor: pointer; transition: all 0.3s; }
    .btn-schedule:hover { background: #138496; }
    .schedule-info { font-size: 0.8em; color: #666; margin-top: 8px; padding-top: 8px; border-top: 1px solid #eee; font-weight: 500; }
    .wifi-status { background: white; border-radius: 15px; padding: 20px; margin-bottom: 20px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); display: flex; justify-content: space-between; align-items: center; }
    .status-info { display: flex; align-items: center; gap: 15px; flex: 1; }
    .status-icon { font-size: 2em; }
    .status-connected { color: #28a745; }
    .status-disconnected { color: #dc3545; }
    .status-text { display: flex; flex-direction: column; }
    .status-text .label { font-size: 0.85em; color: #999; }
    .status-text .value { font-size: 1.1em; font-weight: 600; color: #333; }
    .btn-settings { padding: 10px 20px; background: #667eea; color: white; border: none; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s; }
    .btn-settings:hover { background: #5568d3; transform: translateY(-2px); }
    .wifi-config-toggle { display: none; }
    .wifi-config-toggle.show { display: block; }
    .modal { display: none; position: fixed; z-index: 1000; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.5); }
    .modal.active { display: flex; justify-content: center; align-items: center; }
    .modal-content { background: white; padding: 30px; border-radius: 15px; width: 90%; max-width: 400px; box-shadow: 0 5px 20px rgba(0,0,0,0.3); }
    .modal-header { font-size: 1.5em; font-weight: bold; margin-bottom: 20px; color: #667eea; }
    .time-inputs { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px; }
    .time-input-group { display: flex; flex-direction: column; }
    .time-input-group label { font-size: 0.9em; font-weight: 600; margin-bottom: 5px; color: #333; }
    .time-input-group input { padding: 8px; border: 2px solid #ddd; border-radius: 5px; font-size: 1em; }
    .time-input-group input:focus { border-color: #667eea; outline: none; }
    .checkbox-group { display: flex; align-items: center; margin-bottom: 15px; }
    .checkbox-group input { margin-right: 10px; width: 18px; height: 18px; cursor: pointer; }
    .checkbox-group label { cursor: pointer; font-weight: 600; color: #333; }
    .modal-buttons { display: flex; gap: 10px; }
    .modal-btn { flex: 1; padding: 10px; border: none; border-radius: 5px; font-weight: 600; cursor: pointer; }
    .modal-btn-save { background: #28a745; color: white; }
    .modal-btn-save:hover { background: #218838; }
    .modal-btn-cancel { background: #6c757d; color: white; }
    .modal-btn-cancel:hover { background: #5a6268; }
    .spinner { display: inline-block; width: 12px; height: 12px; border: 2px solid rgba(255,255,255,.3); border-radius: 50%; border-top-color: #fff; animation: spin 0.6s linear infinite; }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div class="container-main">
    <div class="header">
      <h1><i class="fas fa-lightbulb"></i> Smart Lampu Rumah</h1>
      <p>Kontrol pencahayaan rumah Anda dengan mudah</p>
    </div>
    <div class="wifi-status">
      <div class="status-info">
        <div class="status-icon status-connected" id="statusIcon"><i class="fas fa-wifi"></i></div>
        <div class="status-text">
          <span class="label">WiFi Status</span>
          <span class="value" id="statusText">Memeriksa...</span>
        </div>
      </div>
      <button class="btn-settings" onclick="toggleWiFiConfig()"><i class="fas fa-cog"></i> Pengaturan</button>
    </div>
    <div class="wifi-config wifi-config-toggle" id="wifiConfigSection">
      <h5><i class="fas fa-wifi"></i> Konfigurasi WiFi</h5>
      <form onsubmit="return saveWiFi(event)">
        <div class="mb-3">
          <label class="form-label">Pilih Network</label>
          <select name="ssid" class="form-select" required>
            <option value="">-- Pilih WiFi --</option>
          </select>
        </div>
        <div class="mb-3">
          <label class="form-label">Password</label>
          <input type="password" name="pass" class="form-control" placeholder="Masukkan password WiFi" required>
        </div>
        <button type="submit" class="btn-config"><i class="fas fa-save"></i> Simpan Konfigurasi</button>
      </form>
          <div class="control-section">
      <h5><i class="fas fa-sun"></i> Auto Light</h5>
      <div class="mb-3">
        <label class="form-label">Threshold (0-255)</label>
        <input type="range" id="autoLightThreshold" min="0" max="255" value="60" oninput="updateThresholdValue(this.value)">
        <div><span id="thresholdValue">60</span></div>
      </div>
      <div id="cameraStatus" style="font-size: 0.9em; color: #666; margin-bottom: 15px;"></div>
      <div id="lightStatus" style="font-size: 0.9em; color: #666; margin-bottom: 15px;"></div>
      <button class="btn-master" onclick="saveLightSettings()">Simpan Pengaturan</button>
      <div style="margin-top: 10px;">
        <button class="btn-config" onclick="toggleAllAutoLight(true)" style="margin-right: 5px;">Auto ON Semua</button>
        <button class="btn-config" onclick="toggleAllAutoLight(false)">Auto OFF Semua</button>
      </div>
    </div>
    <div class="control-section">
      <h5><i class="fas fa-globe"></i> DDNS (Akses Internet)</h5>
      <div class="form-check form-switch" style="margin-bottom: 15px;">
        <input class="form-check-input" type="checkbox" id="ddnsEnabled" onchange="toggleDDNS(this.checked)">
        <label class="form-check-label" for="ddnsEnabled">Aktifkan DDNS</label>
      </div>
      <div class="mb-3">
        <label class="form-label">Provider</label>
        <select id="ddnsProvider" class="form-control">
          <option value="">Pilih Provider</option>
          <option value="duckdns">DuckDNS (Gratis)</option>
          <option value="noip">No-IP</option>
        </select>
      </div>
      <div class="mb-3">
        <label class="form-label">Domain</label>
        <input type="text" id="ddnsDomain" class="form-control" placeholder="yourdomain.duckdns.org">
      </div>
      <div class="mb-3">
        <label class="form-label">Token/API Key</label>
        <input type="password" id="ddnsToken" class="form-control" placeholder="Token dari provider DDNS">
      </div>
      <div id="ddnsStatus" style="font-size: 0.9em; color: #666; margin-bottom: 15px;"></div>
      <button class="btn-master" onclick="saveDDNSSettings()">Simpan Konfigurasi DDNS</button>
      <button class="btn-config" onclick="testDDNS()" style="margin-left: 10px;">Test Update</button>
    </div>
    </div>

    <div class="control-section">
      <h5><i class="fas fa-bolt"></i> Kontrol Lampu Individual</h5>
      <button class="btn-master" onclick="setRelayCount()" style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); margin-bottom: 15px;"><i class="fas fa-cog"></i> Atur Jumlah Relay</button>
      <div class="relay-grid" id="relayGrid"></div>
    </div>
    <div class="control-section">
      <button class="btn-master" onclick="controlAll(true)"><i class="fas fa-power-off"></i> NYALAKAN SEMUA</button>
      <button class="btn-master" onclick="controlAll(false)" style="background: linear-gradient(135deg, #404040 0%, #828282 100%);"><i class="fas fa-power-off"></i> MATIKAN SEMUA</button>
    </div>
    <div id="scheduleModal" class="modal">
      <div class="modal-content">
        <div class="modal-header">Atur Jadwal Lampu</div>
        <div id="schedulesList" style="max-height: 300px; overflow-y: auto; margin-bottom: 20px; border: 1px solid #ddd; border-radius: 8px; padding: 10px;"></div>
        <div style="background: #f5f5f5; padding: 15px; border-radius: 8px; margin-bottom: 15px;">
          <div style="font-weight: 600; margin-bottom: 10px; color: #667eea;">Slot Baru</div>
          <div class="time-inputs">
            <div class="time-input-group">
              <label>Jam Nyala</label>
              <input type="number" id="schedOnHour" min="0" max="23" value="0">
            </div>
            <div class="time-input-group">
              <label>Menit Nyala</label>
              <input type="number" id="schedOnMin" min="0" max="59" value="0">
            </div>
            <div class="time-input-group">
              <label>Jam Mati</label>
              <input type="number" id="schedOffHour" min="0" max="23" value="6">
            </div>
            <div class="time-input-group">
              <label>Menit Mati</label>
              <input type="number" id="schedOffMin" min="0" max="59" value="0">
            </div>
          </div>
          <div class="checkbox-group" style="margin-top: 10px;">
            <input type="checkbox" id="schedEnabled" checked>
            <label for="schedEnabled">Aktifkan Jadwal</label>
          </div>
        </div>
        <div class="modal-buttons">
          <button class="modal-btn modal-btn-save" onclick="saveSchedule()"><i class="fas fa-plus"></i> Tambah Jadwal</button>
          <button class="modal-btn modal-btn-cancel" onclick="closeScheduleModal()"><i class="fas fa-times"></i> Tutup</button>
        </div>
      </div>
    </div>
  </div>
  <script>
    const relayStates = {};
    let relayConfig = { count: 32, names: [] };
    let schedules = {};
    let autoLightModes = []; // array per relay
    let autoLightThreshold = 60;

    function loadLightConfig() {
      fetch('/getlightconfig').then(r => r.json()).then(data => {
        autoLightModes = data.auto;
        autoLightThreshold = data.threshold;
        const hasCamera = data.camera;

        document.getElementById('autoLightThreshold').value = autoLightThreshold;
        document.getElementById('thresholdValue').textContent = autoLightThreshold;

        const statusEl = document.getElementById('cameraStatus');
        if (!hasCamera) {
          statusEl.textContent = 'Kamera tidak tersedia / tidak terdeteksi (Auto Light dinonaktifkan).';
          // Disable all auto switches
          for (let i = 0; i < relayConfig.count; i++) {
            const switchEl = document.getElementById(`autoSwitch${i}`);
            if (switchEl) switchEl.disabled = true;
          }
        } else {
          statusEl.textContent = 'Kamera terdeteksi. Auto Light menggunakan cahaya dari kamera.';
          // Update switches based on loaded config
          for (let i = 0; i < relayConfig.count; i++) {
            const switchEl = document.getElementById(`autoSwitch${i}`);
            if (switchEl) {
              switchEl.checked = autoLightModes[i];
              switchEl.disabled = false;
            }
          }
        }
      }).catch(err => {
        console.error('Error loading light config:', err);
      });
    }

    function loadLightStatus() {
      fetch('/getlightstatus').then(r => r.json()).then(data => {
        const statusEl = document.getElementById('lightStatus');
        const modeText = data.dark ? 'Gelap' : 'Terang';
        statusEl.textContent = `Cahaya saat ini: ${data.avg.toFixed(1)} (Threshold: ${data.threshold}) → ${modeText}`;
      }).catch(err => {
        console.error('Error loading light status:', err);
      });
    }

    function toggleAutoLight(relayIndex, enabled) {
      if (relayIndex >= 0 && relayIndex < autoLightModes.length) {
        autoLightModes[relayIndex] = enabled;
        // Send individual change to server immediately
        const params = `auto${relayIndex}=${enabled ? 1 : 0}&threshold=${autoLightThreshold}`;
        fetch(`/setlightconfig?${params}`)
          .then(r => r.text())
          .then(() => { 
            console.log(`Relay ${relayIndex + 1} auto mode set to ${enabled ? 'ON' : 'OFF'}`);
          })
          .catch(err => { 
            console.error('Error setting auto mode:', err);
            // Revert local change on error
            autoLightModes[relayIndex] = !enabled;
            const switchEl = document.getElementById(`autoSwitch${relayIndex}`);
            if (switchEl) switchEl.checked = !enabled;
          });
      }
    }

    function toggleAllAutoLight(enabled) {
      // Update local array
      for (let i = 0; i < autoLightModes.length; i++) {
        autoLightModes[i] = enabled;
        const switchEl = document.getElementById(`autoSwitch${i}`);
        if (switchEl) switchEl.checked = enabled;
      }
      // Send to server immediately
      fetch(`/setlightconfig?auto_all=${enabled ? 1 : 0}&threshold=${autoLightThreshold}`)
        .then(r => r.text())
        .then(() => { 
          console.log(`All auto modes set to ${enabled ? 'ON' : 'OFF'}`);
        })
        .catch(err => { 
          console.error('Error setting all auto modes:', err);
          alert('Gagal mengatur auto mode semua relay');
        });
    }

    function updateThresholdValue(value) {
      autoLightThreshold = parseInt(value);
      document.getElementById('thresholdValue').textContent = value;
    }

    function saveLightSettings() {
      let params = `threshold=${autoLightThreshold}`;
      for (let i = 0; i < autoLightModes.length; i++) {
        params += `&auto${i}=${autoLightModes[i] ? 1 : 0}`;
      }
      fetch(`/setlightconfig?${params}`)
        .then(r => r.text())
        .then(() => { alert('Pengaturan Auto Light tersimpan'); })
        .catch(err => { alert('Gagal simpan: ' + err); });
    }

    function loadDDNSConfig() {
      fetch('/getddnsconfig').then(r => r.json()).then(data => {
        document.getElementById('ddnsEnabled').checked = data.enabled;
        document.getElementById('ddnsProvider').value = data.provider;
        document.getElementById('ddnsDomain').value = data.domain;
        document.getElementById('ddnsToken').value = data.token;
        
        const statusEl = document.getElementById('ddnsStatus');
        let statusText = '';
        if (data.lastIP) {
          statusText += `IP Terakhir: ${data.lastIP}`;
          if (data.lastUpdate > 0) {
            const lastUpdate = new Date(data.lastUpdate);
            statusText += ` | Update: ${lastUpdate.toLocaleString()}`;
          }
          statusText += ` | Status: ${data.success ? 'Berhasil' : 'Gagal'}`;
        } else {
          statusText = 'Belum pernah update';
        }
        statusEl.textContent = statusText;
      }).catch(err => {
        console.error('Error loading DDNS config:', err);
      });
    }

    function toggleDDNS(enabled) {
      // Optional: could save immediately, but we'll let user click save button
    }

    function saveDDNSSettings() {
      const enabled = document.getElementById('ddnsEnabled').checked ? 1 : 0;
      const provider = document.getElementById('ddnsProvider').value;
      const domain = document.getElementById('ddnsDomain').value;
      const token = document.getElementById('ddnsToken').value;
      
      fetch(`/setddnsconfig?enabled=${enabled}&provider=${encodeURIComponent(provider)}&domain=${encodeURIComponent(domain)}&token=${encodeURIComponent(token)}`)
        .then(r => r.text())
        .then(() => { 
          alert('Konfigurasi DDNS tersimpan');
          loadDDNSConfig(); // Reload to show updated status
        })
        .catch(err => { alert('Gagal simpan: ' + err); });
    }

    function testDDNS() {
      alert('Testing DDNS update... Check serial monitor for details.');
      // Force an update by calling the handler
      fetch('/setddnsconfig?enabled=1&provider=' + encodeURIComponent(document.getElementById('ddnsProvider').value) + 
            '&domain=' + encodeURIComponent(document.getElementById('ddnsDomain').value) + 
            '&token=' + encodeURIComponent(document.getElementById('ddnsToken').value))
        .then(r => r.text())
        .then(() => {
          setTimeout(() => loadDDNSConfig(), 2000); // Reload status after 2 seconds
        });
    }

    function loadRelayConfig() {
      fetch('/getrelayconfig').then(r => r.json()).then(data => {
        relayConfig = data;
        autoLightModes = new Array(relayConfig.count).fill(false); // Initialize with default false
        generateRelayButtons();
        loadRelayStates(); // Load actual relay states AFTER generating buttons
      }).catch(err => {
        console.error('Error loading relay config:', err);
      });
    }

    function loadRelayStates() {
      fetch('/getrelaystates')
        .then(r => {
          console.log('Response status:', r.status);
          return r.json();
        })
        .then(data => {
          console.log('Relay states received:', data);
          if (data.states) {
            for (let i = 0; i < data.states.length; i++) {
              relayStates[i] = data.states[i];
              updateButtonUI(i);
              console.log('Relay ' + i + ': ' + (data.states[i] ? 'ON' : 'OFF'));
            }
          }
        })
        .catch(err => {
          console.error('Error loading relay states:', err);
          console.warn('Endpoint /getrelaystates tidak ditemukan - pastikan Arduino sudah di-upload dengan kode terbaru');
        });
    }
    
    function loadNetworks() {
      fetch('/scanwifi').then(r => r.text()).then(html => {
        const select = document.querySelector('select[name="ssid"]');
        select.innerHTML = '<option value="">-- Pilih WiFi --</option>' + html;
      });
    }
    
    function toggleWiFiConfig() {
      const section = document.getElementById('wifiConfigSection');
      section.classList.toggle('show');
    }
    
    function updateWiFiStatus() {
      fetch('/getwifistatus').then(r => r.json()).then(data => {
        const statusIcon = document.getElementById('statusIcon');
        const statusText = document.getElementById('statusText');
        if (data.connected) {
          statusIcon.className = 'status-icon status-connected';
          statusIcon.innerHTML = '<i class="fas fa-check-circle"></i>';
          statusText.textContent = data.ssid || 'Terhubung';
        } else {
          statusIcon.className = 'status-icon status-disconnected';
          statusIcon.innerHTML = '<i class="fas fa-times-circle"></i>';
          statusText.textContent = 'Tidak Terhubung';
        }
      }).catch(() => {
        document.getElementById('statusText').textContent = 'Status Tidak Diketahui';
      });
    }
    
    function getScheduleDisplay(ch) {
      if (!schedules[ch]) return 'Manual';
      const active = schedules[ch].filter(s => s && s.enabled && s.onH !== -1);
      if (active.length === 0) return 'Manual';
      return active.map(s => 
        String(s.onH).padStart(2, '0') + ':' + String(s.onM).padStart(2, '0') + ' - ' +
        String(s.offH).padStart(2, '0') + ':' + String(s.offM).padStart(2, '0')
      ).join(', ');
    }
    
    function generateRelayButtons() {
      const grid = document.getElementById('relayGrid');
      grid.innerHTML = '';
      for (let i = 0; i < relayConfig.count; i++) {
        const card = document.createElement('div');
        card.className = 'relay-card';
        const name = relayConfig.names[i] || ('Lampu ' + (i+1));
        const scheduleText = getScheduleDisplay(i);
        card.innerHTML = '<div class="label-header"><span class="label-text" id="label-' + i + '">' + name + '</span><button class="btn-edit-inline" onclick="editName(' + i + ')" title="Edit nama"><i class="fas fa-pencil-alt"></i></button></div>' +
                         '<button class="relay-btn off" onclick="toggleRelay(' + i + ')" id="btn-' + i + '"><i class="fas fa-toggle-off"></i> OFF</button>' +
                         '<div class="form-check form-switch" style="margin: 5px 0;"><input class="form-check-input" type="checkbox" id="autoSwitch' + i + '" onchange="toggleAutoLight(' + i + ', this.checked)"><label class="form-check-label" for="autoSwitch' + i + '" style="font-size: 0.8em;">Auto</label></div>' +
                         '<button class="btn-schedule" style="width: 100%; margin-top: 5px;" onclick="openScheduleModal(' + i + ')" title="Jadwal"><i class="fas fa-clock"></i></button>' +
                         '<div class="schedule-info" id="sch-' + i + '">' + scheduleText + '</div>';
        grid.appendChild(card);
        relayStates[i] = false;
      }
    }
    
    function editName(ch) {
      const currentName = relayConfig.names[ch] || ('Lampu ' + (ch+1));
      const newName = prompt('Edit nama lampu:', currentName);
      if (newName && newName.length > 0 && newName.length <= 50) {
        fetch('/setrelayname?ch=' + ch + '&name=' + encodeURIComponent(newName))
          .then(r => r.text())
          .then(() => {
            relayConfig.names[ch] = newName;
            document.getElementById('label-' + ch).textContent = newName;
          });
      }
    }
    
    function setRelayCount() {
      const newCount = prompt('Jumlah relay (1-32):', relayConfig.count);
      if (newCount && newCount >= 1 && newCount <= 32) {
        fetch('/setrelaycount?count=' + newCount).then(r => r.text()).then(() => {
          window.location.reload();
        });
      }
    }
    function toggleRelay(ch) {
      const btn = document.getElementById('btn-' + ch);
      btn.disabled = true;
      fetch('/t?c=' + ch).then(() => {
        relayStates[ch] = !relayStates[ch];
        updateButtonUI(ch);
      }).finally(() => {
        btn.disabled = false;
      });
    }
    function controlAll(state) {
      const url = state ? '/onall' : '/offall';
      document.querySelectorAll('.relay-btn').forEach(btn => btn.disabled = true);
      fetch(url).then(() => {
        for (let i = 0; i < relayConfig.count; i++) {
          relayStates[i] = state;
          updateButtonUI(i);
        }
      }).finally(() => {
        document.querySelectorAll('.relay-btn').forEach(btn => btn.disabled = false);
      });
    }
    function updateButtonUI(ch) {
      const btn = document.getElementById('btn-' + ch);
      if (!btn) return;
      if (relayStates[ch]) {
        btn.className = 'relay-btn on';
        btn.innerHTML = '<i class="fas fa-toggle-on"></i> ON';
      } else {
        btn.className = 'relay-btn off';
        btn.innerHTML = '<i class="fas fa-toggle-off"></i> OFF';
      }
    }
    function saveWiFi(e) {
      e.preventDefault();
      const ssid = document.querySelector('select[name="ssid"]').value;
      const pass = document.querySelector('input[name="pass"]').value;
      if (!ssid) { alert('Pilih WiFi terlebih dahulu!'); return; }
      const btn = e.target.querySelector('button');
      btn.disabled = true;
      btn.innerHTML = '<span class="spinner"></span> Menyimpan...';
      fetch('/save?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass))
        .then(() => { alert('Konfigurasi disimpan! Perangkat akan restart...'); })
        .catch(err => {
          alert('Gagal menyimpan: ' + err);
          btn.disabled = false;
          btn.innerHTML = '<i class="fas fa-save"></i> Simpan Konfigurasi';
        });
    }
    let currentScheduleChannel = -1;
    function openScheduleModal(ch) {
      currentScheduleChannel = ch;
      const list = document.getElementById('schedulesList');
      list.innerHTML = '';
      if (schedules[ch]) {
        schedules[ch].forEach((sch, slot) => {
          if (sch && sch.onH !== -1) {
            const onTime = String(sch.onH).padStart(2, '0') + ':' + String(sch.onM).padStart(2, '0');
            const offTime = String(sch.offH).padStart(2, '0') + ':' + String(sch.offM).padStart(2, '0');
            const status = sch.enabled ? '✓' : '✗';
            const item = document.createElement('div');
            item.style.cssText = 'display: flex; justify-content: space-between; align-items: center; padding: 8px; background: white; border-bottom: 1px solid #eee; border-radius: 4px; margin-bottom: 5px;';
            item.innerHTML = '<div><span style=\"font-weight: 600;\">' + status + ' Slot ' + (slot+1) + ':</span> ' + onTime + ' - ' + offTime + '</div>' +
                             '<button type=\"button\" onclick=\"deleteSchedule(' + slot + ')\" style=\"padding: 4px 8px; background: #dc3545; color: white; border: none; border-radius: 4px; cursor: pointer;\"><i class=\"fas fa-trash\"></i></button>';
            list.appendChild(item);
          }
        });
      }
      if (list.innerHTML === '') {
        list.innerHTML = '<div style=\"color: #999; text-align: center; padding: 10px;\">Belum ada jadwal</div>';
      }
      document.getElementById('scheduleModal').classList.add('active');
    }
    function closeScheduleModal() {
      document.getElementById('scheduleModal').classList.remove('active');
      currentScheduleChannel = -1;
    }
    function deleteSchedule(slot) {
      const ch = currentScheduleChannel;
      fetch('/setschedule?ch=' + ch + '&slot=' + slot + '&onH=-1&onM=0&offH=-1&offM=0&enabled=0')
        .then(r => r.text())
        .then(() => {
          schedules[ch][slot] = { onH: -1, onM: 0, offH: -1, offM: 0, enabled: false };
          const schedElement = document.getElementById('sch-' + ch);
          if (schedElement) {
            schedElement.textContent = getScheduleDisplay(ch);
          }
          openScheduleModal(ch);
        });
    }
    function saveSchedule() {
      const ch = currentScheduleChannel;
      const onH = parseInt(document.getElementById('schedOnHour').value);
      const onM = parseInt(document.getElementById('schedOnMin').value);
      const offH = parseInt(document.getElementById('schedOffHour').value);
      const offM = parseInt(document.getElementById('schedOffMin').value);
      const enabled = document.getElementById('schedEnabled').checked;
      
      if (!schedules[ch]) schedules[ch] = [];
      let availableSlot = -1;
      for (let s = 0; s < 3; s++) {
        if (!schedules[ch][s] || schedules[ch][s].onH === -1) {
          availableSlot = s;
          break;
        }
      }
      
      if (availableSlot === -1) {
        alert('Maksimal 3 jadwal per lampu!');
        return;
      }
      
      fetch('/setschedule?ch=' + ch + '&slot=' + availableSlot + '&onH=' + onH + '&onM=' + onM + '&offH=' + offH + '&offM=' + offM + '&enabled=' + (enabled ? '1' : '0'))
        .then(r => r.text())
        .then(() => {
          schedules[ch][availableSlot] = { onH, onM, offH, offM, enabled };
          const schedElement = document.getElementById('sch-' + ch);
          if (schedElement) {
            schedElement.textContent = getScheduleDisplay(ch);
          }
          document.getElementById('schedOnHour').value = 0;
          document.getElementById('schedOnMin').value = 0;
          document.getElementById('schedOffHour').value = 6;
          document.getElementById('schedOffMin').value = 0;
          document.getElementById('schedEnabled').checked = true;
          openScheduleModal(ch);
        });
    }
    function loadSchedules() {
      fetch('/getschedules').then(r => r.json()).then(data => {
        for (let i = 0; i < 32; i++) {
          if (!schedules[i]) schedules[i] = [];
          for (let s = 0; s < 3; s++) {
            schedules[i][s] = { onH: -1, onM: 0, offH: -1, offM: 0, enabled: false };
          }
        }
        data.schedules.forEach(s => {
          schedules[s.ch][s.slot] = { onH: s.onH, onM: s.onM, offH: s.offH, offM: s.offM, enabled: s.enabled };
          const schedElement = document.getElementById('sch-' + s.ch);
          if (schedElement) {
            schedElement.textContent = getScheduleDisplay(s.ch);
          }
        });
        console.log('Schedules loaded:', schedules);
      });
    }
    window.addEventListener('load', () => {
      loadNetworks();
      loadRelayConfig();
      loadLightConfig();
      loadDDNSConfig();
      loadLightStatus();
      loadSchedules();
      updateWiFiStatus();

      // Refresh live data periodically
      setInterval(loadNetworks, 30000);
      setInterval(updateWiFiStatus, 10000);
      setInterval(loadLightStatus, 10000);
      setInterval(loadRelayStates, 5000); // keep relay UI in sync with any external changes
    });
  </script>
</body>
</html>
)rawliteral";
}

void handleRoot() { server.send(200, "text/html", webpage()); }

void handleScanWiFi()
{
  String html = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
  {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }
  server.send(200, "text/html", html);
}

void handleGetWiFiStatus()
{
  String json = "{\"connected\":";
  if (WiFi.status() == WL_CONNECTED)
  {
    json += "true,\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  }
  else
  {
    json += "false,\"ssid\":\"\",\"ip\":\"\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggle()
{
  int ch = server.arg("c").toInt();
  setRelay(ch, !relayState[ch]);
  server.send(200, "text/plain", "OK");
}

void handleOnAll()
{
  for (int i = 0; i < relayCount; i++)
    setRelay(i, true);
  server.send(200, "text/plain", "OK");
}

void handleOffAll()
{
  for (int i = 0; i < relayCount; i++)
    setRelay(i, false);
  server.send(200, "text/plain", "OK");
}

void handleSetRelayCount()
{
  int newCount = server.arg("count").toInt();
  if (newCount >= 1 && newCount <= 32)
  {
    relayCount = newCount;
    saveRelayConfig();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(400, "text/plain", "Invalid");
  }
}

void handleSetRelayName()
{
  int ch = server.arg("ch").toInt();
  String name = server.arg("name");
  if (ch >= 0 && ch < 32 && name.length() > 0 && name.length() <= 50)
  {
    relayNames[ch] = name;
    saveRelayConfig();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(400, "text/plain", "Invalid");
  }
}

void handleGetRelayConfig()
{
  String json = "{\"count\":" + String(relayCount) + ",\"names\":[";
  for (int i = 0; i < 32; i++)
  {
    json += "\"" + relayNames[i] + "\"";
    if (i < 31)
      json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleGetRelayStates()
{
  String json = "{\"states\":[";
  for (int i = 0; i < 32; i++)
  {
    json += (relayState[i] ? "true" : "false");
    if (i < 31)
      json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleGetLightConfig()
{
  String json = "{\"auto\":[";
  for (int i = 0; i < relayCount; i++)
  {
    json += autoLightMode[i] ? "true" : "false";
    if (i < relayCount - 1)
      json += ",";
  }
  json += "],\"threshold\":" + String(autoLightThreshold) + ",\"camera\":" + String(cameraAvailable ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleGetLightStatus()
{
  String json = "{\"avg\":" + String(lastLightAvg, 1) + ",\"dark\":" + String(lastLightIsDark ? "true" : "false") + ",\"threshold\":" + String(autoLightThreshold) + "}";
  server.send(200, "application/json", json);
}

void handleSetLightConfig()
{
  Serial.println("[HTTP] handleSetLightConfig called");

  // Check for auto_all parameter to set all relays
  if (server.hasArg("auto_all"))
  {
    bool enabled = server.arg("auto_all") == "1" || server.arg("auto_all") == "true";
    for (int i = 0; i < relayCount; i++)
      autoLightMode[i] = enabled;
    Serial.printf("[HTTP] Set all auto modes to %d\n", enabled);
  }
  else
  {
    // Check individual auto parameters (auto0, auto1, etc.)
    for (int i = 0; i < relayCount; i++)
    {
      String param = "auto" + String(i);
      if (server.hasArg(param))
      {
        bool enabled = server.arg(param) == "1" || server.arg(param) == "true";
        autoLightMode[i] = enabled;
        Serial.printf("[HTTP] Set relay %d auto mode to %d\n", i, enabled);
      }
    }
  }

  int t = server.arg("threshold").toInt();
  if (t < 0)
    t = 0;
  if (t > 255)
    t = 255;
  autoLightThreshold = t;
  saveLightConfig();
  server.send(200, "text/plain", "OK");
}

void handleGetDDNSConfig()
{
  String json = "{\"enabled\":" + String(ddnsEnabled ? "true" : "false") +
                ",\"provider\":\"" + ddnsProvider + "\"" +
                ",\"domain\":\"" + ddnsDomain + "\"" +
                ",\"token\":\"" + ddnsToken + "\"" +
                ",\"lastIP\":\"" + lastDDNSIP + "\"" +
                ",\"lastUpdate\":" + String(lastDDNSUpdate) +
                ",\"success\":" + String(ddnsUpdateSuccess ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleSetDDNSConfig()
{
  ddnsEnabled = server.arg("enabled") == "1" || server.arg("enabled") == "true";
  ddnsProvider = server.arg("provider");
  ddnsDomain = server.arg("domain");
  ddnsToken = server.arg("token");

  saveDDNSConfig();

  // Test update immediately if enabled
  if (ddnsEnabled)
  {
    Serial.println("[DDNS] Testing DDNS update...");
    updateDDNS();
  }

  server.send(200, "text/plain", "OK");
}

void handleSetSchedule()
{
  int ch = server.arg("ch").toInt();
  int slot = server.arg("slot").toInt();
  int onH = server.arg("onH").toInt();
  int onM = server.arg("onM").toInt();
  int offH = server.arg("offH").toInt();
  int offM = server.arg("offM").toInt();
  bool enabled = server.arg("enabled") == "1";

  if (ch >= 0 && ch < 32 && slot >= 0 && slot < MAX_SCHEDULES_PER_RELAY &&
      onH >= -1 && onH < 24 && onM >= 0 && onM < 60 &&
      offH >= -1 && offH < 24 && offM >= 0 && offM < 60)
  {
    schedules[ch][slot].onHour = onH;
    schedules[ch][slot].onMin = onM;
    schedules[ch][slot].offHour = offH;
    schedules[ch][slot].offMin = offM;
    schedules[ch][slot].enabled = enabled;
    saveSchedules();
    Serial.println("[Schedule] Relay " + String(ch + 1) + " slot " + String(slot + 1) + " set: " + String(onH) + ":" + String(onM) + " -> " + String(offH) + ":" + String(offM));
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(400, "text/plain", "Invalid");
  }
}

void handleGetSchedules()
{
  String json = "{\"schedules\":[";
  for (int i = 0; i < 32; i++)
  {
    for (int s = 0; s < MAX_SCHEDULES_PER_RELAY; s++)
    {
      json += "{\"ch\":" + String(i) + ",\"slot\":" + String(s) + ",\"onH\":" + String(schedules[i][s].onHour) + ",\"onM\":" + String(schedules[i][s].onMin) +
              ",\"offH\":" + String(schedules[i][s].offHour) + ",\"offM\":" + String(schedules[i][s].offMin) +
              ",\"enabled\":" + (schedules[i][s].enabled ? "true" : "false") + "}";
      if (i < 31 || s < MAX_SCHEDULES_PER_RELAY - 1)
        json += ",";
    }
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSave()
{
  ssid = server.arg("ssid");
  password = server.arg("pass");

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.end();

  server.send(200, "text/html", "Saved! Restarting...");
  delay(1500);
  ESP.restart();
}

////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);

  // Penting: untuk ESP32, delay panjang agar serial monitor siap
  delay(2000);

  Serial.println("\n\n========== SETUP DIMULAI ==========");
  Serial.println("[System] Smart Lampu Rumah v1.0");
  Serial.flush(); // Pastikan output dikirim

  Serial.println("[I2C] Inisialisasi...");
  Wire.begin();
  board1.begin();
  board2.begin();
  board3.begin();
  board4.begin();
  Serial.println("[I2C] ✓ 4 PCF8574 Board siap");

  Serial.println("[Config] Loading relay configuration...");
  loadRelayConfig();

  Serial.println("[Config] Loading light sensor config...");
  loadLightConfig();

  Serial.println("[Config] Loading DDNS config...");
  loadDDNSConfig();

  Serial.println("[Relay] All relays initialized to OFF");
  // Note: NVRAM relay state persistence disabled due to reliability issues
  // MQTT retained messages serve as state memory after settling period

  cameraAvailable = initCamera();
  if (!cameraAvailable)
  {
    Serial.println("[Camera] Warning: camera init failed; light sensor disabled");
    for (int i = 0; i < relayCount; i++)
      autoLightMode[i] = false;
  }

  // Initialize autoLightActive array
  for (int i = 0; i < relayCount; i++)
    autoLightActive[i] = false;

  connectToWiFi();

  Serial.println("[NTP] Mensinkronisasi waktu dengan NTP...");
  syncTime();

  Serial.println("[Config] Loading schedules...");
  loadSchedules();

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  Serial.println("[MQTT] Server: " + String(mqtt_server) + ":" + String(mqtt_port));

  server.on("/", handleRoot);
  server.on("/scanwifi", handleScanWiFi);
  server.on("/getwifistatus", handleGetWiFiStatus);
  server.on("/t", handleToggle);
  server.on("/onall", handleOnAll);
  server.on("/offall", handleOffAll);
  server.on("/save", handleSave);
  server.on("/setrelaycount", handleSetRelayCount);
  server.on("/setrelayname", handleSetRelayName);
  server.on("/getrelayconfig", handleGetRelayConfig);
  server.on("/getrelaystates", handleGetRelayStates);
  server.on("/getlightconfig", handleGetLightConfig);
  server.on("/setlightconfig", handleSetLightConfig);
  server.on("/getlightstatus", handleGetLightStatus);
  server.on("/setschedule", handleSetSchedule);
  server.on("/getschedules", handleGetSchedules);
  server.on("/getddnsconfig", handleGetDDNSConfig);
  server.on("/setddnsconfig", handleSetDDNSConfig);
  Serial.println("[Web] Routes terdaftar");

  server.begin();
  Serial.println("[Web] Server mulai di port 80");
  Serial.println("========== SETUP SELESAI ==========");
}

////////////////////////////////////////////////
void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!mqtt.connected())
    {
      connectMQTT();
    }
    else
    {
      // After connection, check if settling period is done and subscribe
      if (!mqttSubscribed && (millis() - mqttConnectTime) >= MQTT_SETTLING_TIME)
      {
        // Settling period is done - now safe to subscribe and receive retained messages
        mqtt.subscribe("rumah/lampu/+");
        mqtt.subscribe("rumah/lampu/all");
        Serial.println("[MQTT] ✓ Subscribe: rumah/lampu/+, rumah/lampu/all");
        Serial.println("[MQTT] ✓ Settling period done - receiving retained state messages...");
        mqttSubscribed = true;
      }
    }
    mqtt.loop();
  }
  else
  {
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 10000)
    {
      Serial.println("[WiFi] Putus, mencoba reconnect...");
      connectToWiFi();
      lastWiFiCheck = millis();
    }
  }

  checkSchedules();
  checkLightSensor();
  checkDDNS();
  server.handleClient();
}
