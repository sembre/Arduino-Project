#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <Preferences.h>

// ================= RTC MEMORY (untuk detect hard reset) =================
RTC_DATA_ATTR unsigned int resetCount = 0;
RTC_DATA_ATTR unsigned long lastResetTime = 0;

// ================= WIFI MANAGER =================
String ssid = "";
String password = "";
WebServer server(80);
Preferences prefs;

String ap_ssid = ""; // Di-generate otomatis dari MAC address
const char *ap_password = "12345678";
bool wifiConfigured = false;
int numberOfNetworks = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000; // Cek koneksi WiFi setiap 30 detik

// WiFi Scan - ASYNC (non-blocking)
unsigned long lastScanTime = 0;
const unsigned long SCAN_CACHE_DURATION = 15000; // Cache results 15 seconds
bool isScanRunning = false;

// Auto-connect retry settings
unsigned long lastConnectionAttempt = 0;
unsigned long connectionRetryInterval = 5000;   // Start with 5 seconds
const unsigned long MAX_RETRY_INTERVAL = 60000; // Max 60 seconds

// ================= MQTT =================
const char *mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);
const char *publish_topic = "panggilan/alat";
const char *response_topic = "panggilan/respons";

// ================= ID MEJA =================
String ID_MEJA = "MEJA01"; // Dapat diubah via web interface

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= PIN =================
#define BTN_DUMMY 4          // Tombol Dummy (GPIO 4) ✓
#define BTN_PAKU 5           // Tombol Paku Palu (GPIO 5) ✓
#define BTN_LEM_LILIN 6      // Tombol Lem Lilin (GPIO 6) ✓
#define BTN_ADJUST 7         // Tombol Adjust Twisting (GPIO 7) ✓
#define BTN_REPAIR 8         // Tombol Repair Board (GPIO 8) ✓
#define BTN_BATAL 18         // Tombol Batal (GPIO 18) ✓
#define BTN_BOOT 0           // GPIO0 - BOOT button (untuk hard reset) ✓
#define BUZZER 19            // Buzzer (GPIO 19) ✓

// ================= STATUS =================
bool sedangMemanggil = false;
String jenisPanggilan = "";

// ================= ANTRIAN / QUEUE SYSTEM =================
#define MAX_QUEUE 20
#define QUEUE_TIMEOUT 300000 // 5 menit timeout untuk auto-remove
struct QueueItem
{
  String type; // "DUMMY", "PAKU_PALU", "LEM_LILIN", "ADJUST_TWISTING", "REPAIR_BOARD"
  unsigned long timeAdded;
  bool acknowledged;  // Sudah di-ack oleh penerima?
  String respondedBy; // Penerima mana yang respond
};

QueueItem callQueue[MAX_QUEUE];
int queueCount = 0;        // Jumlah antrian menunggu
int currentCallIndex = -1; // Index call yang sedang berjalan (-1 jika tidak ada)
unsigned long lastQueueUpdate = 0;
const unsigned long QUEUE_UPDATE_INTERVAL = 5000; // Print queue info setiap 5 detik

// ================= OTW STATE TRACKING =================
#define STATUS_WAITING 0 // Menunggu penerima
#define STATUS_OTW 1     // Penerima sedang dalam perjalanan (On The Way)
#define STATUS_IDLE 2    // Tidak ada panggilan
int currentStatus = STATUS_IDLE;
unsigned long otwStartTime = 0;
const unsigned long OTW_DURATION = 10000; // 10 detik tampil OTW status

// debounce
unsigned long lastPress = 0;
int delayDebounce = 300;

// Restart flag
bool restartPending = false;
unsigned long restartTime = 0;

// Track if normal mode routes are registered
bool normalModeRoutesRegistered = false;

// Track if AP mode is setup
bool apModeSetup = false;

// ================= FUNCTION PROTOTYPES =================
void tampilkanStatus();
void kirimMQTT(String jenis, String status);
void printQueueInfo();
void cancelAllCalls();
void savePendingCall();
void clearPendingCall();
void restorePendingCall();

// ================= FUNGSI HARD RESET DETECTION =================
void checkHardResetButton()
{
  // Check BOOT button (GPIO0)
  static unsigned long bootButtonPressTime = 0;
  static bool bootButtonPressed = false;
  const unsigned long HARD_RESET_HOLD_TIME = 5000; // 5 detik

  if (digitalRead(BTN_BOOT) == LOW)
  {
    // BOOT button sedang ditekan
    if (!bootButtonPressed)
    {
      // Pertama kali ditekan
      bootButtonPressed = true;
      bootButtonPressTime = millis();
      Serial.println("[HardReset] BOOT button pressed (hold 5s for hard reset)");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("HOLD BOOT 5s");
      lcd.setCursor(0, 1);
      lcd.print("0s...");
    }
    else
    {
      // Masih ditekan - update countdown
      unsigned long holdTime = millis() - bootButtonPressTime;
      unsigned long seconds = holdTime / 1000;

      // Update LCD setiap 500ms
      if (holdTime % 500 < 100)
      {
        lcd.setCursor(0, 1);
        lcd.print(String(seconds) + "s...         ");
      }

      // Jika sudah 5 detik, trigger hard reset
      if (holdTime >= HARD_RESET_HOLD_TIME)
      {
        Serial.println("[HardReset] ✓ BOOT button held for 5 seconds - HARD RESET TRIGGERED!");
        performHardReset();
        bootButtonPressed = false;
      }
    }
  }
  else
  {
    // BOOT button dilepas
    if (bootButtonPressed)
    {
      unsigned long holdTime = millis() - bootButtonPressTime;
      bootButtonPressed = false;

      if (holdTime < HARD_RESET_HOLD_TIME)
      {
        Serial.println("[HardReset] BOOT button released (hold time: " + String(holdTime) + "ms - too short)");
        // Restore LCD
        if (wifiConfigured)
        {
          tampilkanStatus();
        }
      }
    }
  }
}

void performHardReset()
{
  Serial.println("\n========================================");
  Serial.println("     HARD RESET - SEMUA PENGATURAN");
  Serial.println("========================================\n");

  // Display di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HARD RESET");
  lcd.setCursor(0, 1);
  lcd.print("Clearing...");

  // Buzzer sequence - panjang untuk konfirmasi hard reset
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }

  delay(500);

  // Clear WiFi credentials & preferences
  Serial.println("[HardReset] Clearing WiFi credentials...");
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  Serial.println("[HardReset] Clearing device settings...");
  prefs.begin("device", false);
  prefs.clear();
  prefs.end();

  Serial.println("[HardReset] ✓ Semua pengaturan telah dihapus!");
  Serial.println("[HardReset] Preferences cleared, perangkat akan restart...");
  Serial.println("\n========================================");
  Serial.println("     RESTART PERANGKAT (dalam 2 detik)");
  Serial.println("========================================\n");

  // Display success
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HARD RESET OK");
  lcd.setCursor(0, 1);
  lcd.print("Restarting...");

  delay(2000);

  // Soft restart
  ESP.restart();
}

// ================= FUNGSI PREFERENCES (STORAGE) =================
void saveWiFiCredentials(String ssidParam, String passParam)
{
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssidParam);
  prefs.putString("pass", passParam);
  prefs.end();

  Serial.println("[WiFi] Credentials saved to Preferences");
  Serial.println("[WiFi] SSID: " + ssidParam);
}

void loadWiFiCredentials()
{
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  prefs.end();

  if (ssid.length() > 0)
  {
    Serial.println("[WiFi] Credentials loaded from Preferences");
    Serial.println("[WiFi] SSID: " + ssid);
  }
  else
  {
    Serial.println("[WiFi] No saved credentials found");
  }
}

void saveDeviceSettings(String nama_meja, String mqtt_topic)
{
  prefs.begin("device", false);
  prefs.putString("nama_meja", nama_meja);
  prefs.putString("mqtt_topic", mqtt_topic);
  prefs.end();

  Serial.println("[Device] Settings saved");
  Serial.println("[Device] Nama Meja: " + nama_meja);
}

void loadDeviceSettings()
{
  prefs.begin("device", true);
  String nama = prefs.getString("nama_meja", "");
  prefs.end();

  if (nama.length() > 0)
  {
    // Load dari preferences jika sudah pernah di-save
    ID_MEJA = nama;
    ap_ssid = nama + "_SETUP";
    Serial.println("[Device] Settings loaded from Preferences");
    Serial.println("[Device] Nama Meja: " + ID_MEJA);
  }
  else
  {
    // Generate random ID dari MAC address (unik tapi konsisten)
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String mac_suffix = String(mac[4], HEX) + String(mac[5], HEX);
    mac_suffix.toUpperCase();

    ID_MEJA = "MEJA_" + mac_suffix;
    ap_ssid = ID_MEJA + "_SETUP";

    Serial.println("[Device] Generated random ID from MAC address");
    Serial.println("[Device] Nama Meja: " + ID_MEJA);
    Serial.println("[Device] AP SSID: " + ap_ssid);
  }
}

// ================= FUNGSI SCAN WIFI =================
String getNetworksHTML()
{
  String html = "";
  int n = WiFi.scanComplete();

  if (n == 0)
  {
    html = "<option>Tidak ada jaringan ditemukan</option>";
  }
  else if (n > 0)
  {
    numberOfNetworks = n;
    for (int i = 0; i < n; i++)
    {
      String ssidName = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      ssidName.replace("\"", "&quot;");
      html += "<option value='" + ssidName + "'>";
      html += ssidName + " (" + String(rssi) + " dBm)";
      html += "</option>";
    }
  }
  return html;
}

// ================= WEB SERVER HANDLER =================
void handleRoot()
{
  // Jika WiFi sudah terhubung dan BUKAN dalam mode AP saja, serve dashboard
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() != WIFI_AP)
  {
    handleMainPage();
    return;
  }

  // Tampilkan halaman WiFi setup (mode AP)
  String html = "<!DOCTYPE html><html lang='id'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Pengaturan WiFi</title>";
  html += "<style>";
  html += "body {font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
  html += "display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}";
  html += ".container {background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 30px rgba(0,0,0,0.3);";
  html += "max-width: 400px; width: 100%;}";
  html += "h1 {color: #333; text-align: center; font-size: 24px; margin: 0 0 10px 0;}";
  html += ".subtitle {text-align: center; color: #666; font-size: 14px; margin-bottom: 30px;}";
  html += ".form-group {margin-bottom: 20px;}";
  html += "label {display: block; margin-bottom: 5px; color: #333; font-weight: bold; font-size: 14px;}";
  html += "select, input[type='password'] {width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 5px;";
  html += "font-size: 14px; box-sizing: border-box; transition: border-color 0.3s;}";
  html += "select:focus, input[type='password']:focus {outline: none; border-color: #667eea;}";
  html += "button {width: 100%; padding: 12px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
  html += "color: white; border: none; border-radius: 5px; font-size: 16px; font-weight: bold; cursor: pointer;";
  html += "transition: transform 0.2s;}";
  html += "button:hover {transform: translateY(-2px);}";
  html += ".refresh-btn {background: #28a745; margin-bottom: 20px; padding: 10px; font-size: 14px;}";
  html += ".refresh-btn:hover {background: #218838;}";
  html += ".info {background: #e7f3ff; border-left: 4px solid #2196F3; padding: 15px; margin-bottom: 20px;";
  html += "border-radius: 5px; font-size: 13px; color: #1565c0;}";
  html += ".separator {text-align: center; color: #999; margin: 25px 0; font-size: 12px;}";
  html += ".status-info {background: #e8f5e9; border-left: 4px solid #4CAF50; padding: 15px; margin-bottom: 20px; border-radius: 5px; font-size: 12px; color: #2e7d32;}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Pengaturan WiFi</h1>";
  html += "<div class='subtitle'>" + ap_ssid + "</div>";

  // Show status info
  html += "<div class='status-info'>";
  html += "<strong>ℹ️ Instruksi Pertama Kali</strong><br>";
  html += "1. Terhubung ke Access Point: <strong>" + ap_ssid + "</strong><br>";
  html += "2. Password: 12345678<br>";
  html += "3. Pilih jaringan WiFi dan masukkan password<br>";
  html += "4. Klik 'Ganti WiFi & Terhubung'<br>";
  html += "5. Tunggu redirect otomatis ke IP baru";
  html += "</div>";

  html += "<div class='info'><strong>Info:</strong><br>";
  html += "Pilih jaringan WiFi rumah/kantor Anda dan masukkan password.<br>";
  html += "Perangkat akan otomatis terkoneksi dan membuka pengaturan.</div>";
  html += "<form method='POST' action='/save'>";
  html += "<div class='form-group'>";
  html += "<label for='ssid'>Pilih Jaringan WiFi</label>";
  html += "<select id='ssid' name='ssid' required>";
  html += "<option value=''>Memindai jaringan...</option>";
  html += "</select>";
  html += "<button type='button' class='refresh-btn' onclick='refreshNetworks()'>";
  html += "Scan Ulang Jaringan</button></div>";
  html += "<div class='form-group'>";
  html += "<label for='pass'>Password WiFi</label>";
  html += "<input type='password' id='pass' name='pass' placeholder='Masukkan password' required>";
  html += "</div>";
  html += "<button type='submit'>Ganti WiFi &amp; Terhubung</button>";
  html += "</form>";
  html += "<div class='separator'>Kolom di atas adalah halaman pertama kali setup</div>";
  html += "</div>";
  html += "<div id='scan-status' style='margin-top: 10px; padding: 10px; background: #f0f0f0; border-radius: 5px; font-size: 12px; color: #666; display: none;'>";
  html += "⏳ Memindai jaringan...";
  html += "</div>";
  html += "<script>";
  html += "let pollTimeout;";
  html += "let maxRetries = 60;";
  html += "let retryCount = 0;";
  html += "let forceMode = false;";
  html += "async function loadNetworks(){";
  html += "  try {";
  html += "    let url = '/networks?t=' + Date.now();";
  html += "    if (forceMode) url += '&force=1';";
  html += "    const response = await fetch(url, {cache: 'no-store'});";
  html += "    if (!response.ok) throw new Error('HTTP ' + response.status);";
  html += "    const text = await response.text();";
  html += "    const select = document.getElementById('ssid');";
  html += "    const isScanning = text.includes('Memindai');";
  html += "    select.innerHTML = text;";
  html += "    retryCount = 0;";
  html += "    if (isScanning) {";
  html += "      pollTimeout = setTimeout(loadNetworks, 1000);";
  html += "    } else {";
  html += "      forceMode = false;";
  html += "    }";
  html += "  } catch(e) {";
  html += "    console.error('Network error:', e);";
  html += "    retryCount++;";
  html += "    if (retryCount < maxRetries) {";
  html += "      pollTimeout = setTimeout(loadNetworks, 1000);";
  html += "    }";
  html += "  }";
  html += "}";
  html += "function refreshNetworks() {";
  html += "  const select = document.getElementById('ssid');";
  html += "  select.innerHTML = '<option value=\\\"\\\">Memindai jaringan...</option>';";
  html += "  retryCount = 0;";
  html += "  forceMode = true;";
  html += "  loadNetworks();";
  html += "}";
  html += "loadNetworks();";
  html += "</script>";
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleNetworks()
{
  // Check if user is forcing a fresh scan
  bool forceNewScan = server.hasArg("force");

  if (forceNewScan)
  {
    // Force reset cache and start new scan
    Serial.println("[WiFi] Force new scan requested");
    isScanRunning = false;
    lastScanTime = 0;
    WiFi.scanNetworks(true); // Start async scan
    isScanRunning = true;
    server.send(200, "text/html", "<option>Memindai jaringan...</option>");
    return;
  }

  int scanStatus = WiFi.scanComplete();

  // No scan running, start one
  if (scanStatus == -2)
  {
    Serial.println("[WiFi] Starting async WiFi scan");
    WiFi.scanNetworks(true); // Start async scan
    isScanRunning = true;
    server.send(200, "text/html", "<option>Memindai jaringan...</option>");
    return;
  }

  // Scan still in progress
  if (scanStatus == -1)
  {
    server.send(200, "text/html", "<option>Memindai jaringan...</option>");
    return;
  }

  // Scan complete (scanStatus >= 0)
  isScanRunning = false;
  lastScanTime = millis();
  String html = getNetworksHTML();
  server.send(200, "text/html", html);
}

// ================= SETTINGS WEB INTERFACE (AFTER WIFI CONNECTED) =================
void handleStatus()
{
  // Return device status as JSON
  // Check WiFi status directly, not just wifiConfigured flag
  String response = "{";
  response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  response += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  response += "\"ssid\":\"" + WiFi.SSID() + "\"";
  response += "}";

  server.send(200, "application/json", response);
}

void handleQueueInfo()
{
  // Return queue information dari matching Penerima, bukan local queue
  String statusName = "IDLE";
  if (currentStatus == STATUS_WAITING)
    statusName = "WAITING";
  else if (currentStatus == STATUS_OTW)
    statusName = "OTW";

  String response = "{";
  response += "\"callType\":\"" + (sedangMemanggil ? jenisPanggilan : "-") + "\",";
  response += "\"status\":\"" + statusName + "\",";

  // Jika sedang memanggil, tampilkan info dari matching Penerima
  if (sedangMemanggil && jenisPanggilan != "")
  {
    response += "\"penerimaNama\":\"-\",";
    response += "\"penerimarIP\":\"-\",";
    response += "\"queueCount\":0,";
    response += "\"matchFound\":false";
  }
  else
  {
    response += "\"penerimaNama\":\"-\",";
    response += "\"penerimarIP\":\"-\",";
    response += "\"queueCount\":0,";
    response += "\"matchFound\":false";
  }

  response += "}";

  server.send(200, "application/json", response);
}

void handleGetIP()
{
  // Return current IP address - validate it's not 0.0.0.0 or AP IP
  String currentIP = WiFi.localIP().toString();

  // Don't return 0.0.0.0 or AP IP to browser
  if (currentIP == "0.0.0.0" || currentIP == "192.168.4.1")
  {
    server.send(200, "application/json", "{\"ip\":\"\"}");
  }
  else
  {
    String response = "{\"ip\":\"" + currentIP + "\"}";
    server.send(200, "application/json", response);
  }
}

void handleCall()
{
  if (server.method() == HTTP_POST)
  {
    String callType = server.arg("type");

    if (callType == "DUMMY")
    {
      if (!sedangMemanggil)
      {
        // Panggilan pertama: queue dan process
        addToQueue("DUMMY");
        bunyiBuzzer(500);
        processQueue();
      }
      else
      {
        // Panggilan kedua dst: langsung kirim tanpa queue
        kirimMQTT("DUMMY", "PANGGIL");
        bunyiBuzzer(500);
        Serial.println("[Call] ➤ Langsung kirim DUMMY (tanpa queue)");
      }
    }
    else if (callType == "PAKU_PALU")
    {
      if (!sedangMemanggil)
      {
        addToQueue("PAKU_PALU");
        bunyiBuzzer(700);
        processQueue();
      }
      else
      {
        kirimMQTT("PAKU_PALU", "PANGGIL");
        bunyiBuzzer(700);
        Serial.println("[Call] ➤ Langsung kirim PAKU_PALU (tanpa queue)");
      }
    }
    else if (callType == "LEM_LILIN")
    {
      if (!sedangMemanggil)
      {
        addToQueue("LEM_LILIN");
        bunyiBuzzer(600);
        processQueue();
      }
      else
      {
        kirimMQTT("LEM_LILIN", "PANGGIL");
        bunyiBuzzer(600);
        Serial.println("[Call] ➤ Langsung kirim LEM_LILIN (tanpa queue)");
      }
    }
    else if (callType == "ADJUST_TWISTING")
    {
      if (!sedangMemanggil)
      {
        addToQueue("ADJUST_TWISTING");
        bunyiBuzzer(650);
        processQueue();
      }
      else
      {
        kirimMQTT("ADJUST_TWISTING", "PANGGIL");
        bunyiBuzzer(650);
        Serial.println("[Call] ➤ Langsung kirim ADJUST_TWISTING (tanpa queue)");
      }
    }
    else if (callType == "REPAIR_BOARD")
    {
      if (!sedangMemanggil)
      {
        addToQueue("REPAIR_BOARD");
        bunyiBuzzer(750);
        processQueue();
      }
      else
      {
        kirimMQTT("REPAIR_BOARD", "PANGGIL");
        bunyiBuzzer(750);
        Serial.println("[Call] ➤ Langsung kirim REPAIR_BOARD (tanpa queue)");
      }
    }
    else if (callType == "BATAL")
    {
      // Cancel semua: current call + clear queue
      cancelAllCalls();
      bunyiBuzzer(200);
    }

    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void handleSettings()
{
  if (server.method() == HTTP_GET)
  {
    String html = "<!DOCTYPE html><html lang='id'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Pengaturan Perangkat</title>";
    html += "<style>";
    html += "body {font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
    html += "display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}";
    html += ".container {background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 30px rgba(0,0,0,0.3);";
    html += "max-width: 400px; width: 100%;}";
    html += "h1 {color: #333; text-align: center; font-size: 24px; margin: 0 0 10px 0;}";
    html += ".subtitle {text-align: center; color: #666; font-size: 14px; margin-bottom: 30px;}";
    html += ".form-group {margin-bottom: 20px;}";
    html += "label {display: block; margin-bottom: 5px; color: #333; font-weight: bold; font-size: 14px;}";
    html += "input[type='text'] {width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 5px;";
    html += "font-size: 14px; box-sizing: border-box; transition: border-color 0.3s;}";
    html += "input[type='text']:focus {outline: none; border-color: #667eea;}";
    html += "button {width: 100%; padding: 12px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
    html += "color: white; border: none; border-radius: 5px; font-size: 16px; font-weight: bold; cursor: pointer;";
    html += "transition: transform 0.2s;}";
    html += "button:hover {transform: translateY(-2px);}";
    html += ".info {background: #e7f3ff; border-left: 4px solid #2196F3; padding: 15px; margin-bottom: 20px;";
    html += "border-radius: 5px; font-size: 13px; color: #1565c0;}";
    html += ".status {background: #f0f0f0; padding: 15px; border-radius: 5px; margin-bottom: 20px; font-size: 13px;}";
    html += ".status-item {display: flex; justify-content: space-between; margin-bottom: 8px;}";
    html += ".status-label {color: #666; font-weight: bold;}";
    html += ".status-value {color: #333;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Pengaturan Perangkat</h1>";
    html += "<div class='subtitle'>Konfigurasi Nama & MQTT</div>";
    html += "<div class='info'><strong>Info:</strong><br>";
    html += "Atur nama meja dan topik MQTT.<br>";
    html += "Perubahan akan langsung tersimpan.</div>";

    html += "<div class='status'>";
    html += "<div class='status-item'>";
    html += "<span class='status-label'>WiFi:</span>";
    html += "<span class='status-value'>" + WiFi.SSID() + "</span>";
    html += "</div>";
    html += "<div class='status-item'>";
    html += "<span class='status-label'>IP Address:</span>";
    html += "<span class='status-value'>" + WiFi.localIP().toString() + "</span>";
    html += "</div>";
    html += "<div class='status-item'>";
    html += "<span class='status-label'>Signal:</span>";
    html += "<span class='status-value'>" + String(WiFi.RSSI()) + " dBm</span>";
    html += "</div>";
    html += "</div>";

    html += "<form method='POST' action='/savesettings'>";
    html += "<div class='form-group'>";
    html += "<label for='nama'>Nama Meja</label>";
    html += "<input type='text' id='nama' name='nama' value='" + ID_MEJA + "' required>";
    html += "</div>";
    html += "<button type='submit'>Simpan Pengaturan</button>";
    html += "</form>";
    html += "</div>";
    html += "</body></html>";

    server.send(200, "text/html", html);
  }
}

void handleSaveSettings()
{
  if (server.method() == HTTP_POST)
  {
    String newNama = server.arg("nama");

    if (newNama.length() > 0 && newNama.length() <= 16)
    {
      ID_MEJA = newNama;
      saveDeviceSettings(newNama, "panggilan/alat");

      String response = "<!DOCTYPE html><html><head>";
      response += "<meta charset='UTF-8'><title>Berhasil</title>";
      response += "<style>";
      response += "body {font-family: Arial; background: #4CAF50; display: flex; justify-content: center;";
      response += "align-items: center; min-height: 100vh; color: white; text-align: center; margin: 0;}";
      response += ".container {padding: 40px;}";
      response += "h1 {font-size: 28px;} p {font-size: 16px; margin: 20px 0;}";
      response += "</style></head><body><div class='container'>";
      response += "<h1>✓ Berhasil Disimpan!</h1>";
      response += "<p>Nama Meja: <strong>" + newNama + "</strong></p>";
      response += "<p>Akan kembali ke halaman utama dalam 2 detik...</p>";
      response += "<script>setTimeout(function() { window.location.href = '/'; }, 2000);</script>";
      response += "</div></body></html>";

      server.send(200, "text/html", response);
      return;
    }
    else
    {
      server.send(400, "text/plain", "Nama meja tidak boleh kosong atau terlalu panjang (max 16 karakter)");
    }
  }
}

// ================= PENERIMA CONFIGURATION HANDLERS =================


void handleMainPage()
{
  String html = "<!DOCTYPE html><html lang='id'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>" + ID_MEJA + "</title>";
  html += "<style>";
  html += "body {font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
  html += "display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}";
  html += ".container {background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 30px rgba(0,0,0,0.3);";
  html += "max-width: 450px; width: 100%;}";
  html += "h1 {color: #333; font-size: 28px; margin: 0 0 10px 0; text-align: center;}";
  html += ".subtitle {color: #666; font-size: 16px; margin-bottom: 30px; text-align: center;}";
  html += ".status-card {background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 20px;}";
  html += ".status-item {display: flex; justify-content: space-between; margin-bottom: 12px; font-size: 14px;}";
  html += ".status-label {color: #666; font-weight: bold;}";
  html += ".status-value {color: #333;}";
  html += ".call-buttons {display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; margin-bottom: 20px;}";
  html += ".btn-call {padding: 20px 10px; border-radius: 8px; font-size: 14px; font-weight: bold;";
  html += "cursor: pointer; border: none; color: white; transition: all 0.3s;";
  html += "display: flex; flex-direction: column; align-items: center; justify-content: center;}";
  html += ".btn-call:active {transform: scale(0.95);}";
  html += ".btn-dummy {background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%);}";
  html += ".btn-dummy:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(76,175,80,0.4);}";
  html += ".btn-paku {background: linear-gradient(135deg, #FF9800 0%, #e68900 100%);}";
  html += ".btn-paku:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(255,152,0,0.4);}";
  html += ".btn-lem {background: linear-gradient(135deg, #E91E63 0%, #c2185b 100%);}";
  html += ".btn-lem:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(233,30,99,0.4);}";
  html += ".btn-adjust {background: linear-gradient(135deg, #2196F3 0%, #1976D2 100%);}";
  html += ".btn-adjust:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(33,150,243,0.4);}";
  html += ".btn-repair {background: linear-gradient(135deg, #9C27B0 0%, #7B1FA2 100%);}";
  html += ".btn-repair:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(156,39,176,0.4);}";
  html += ".btn-batal {background: linear-gradient(135deg, #f44336 0%, #da190b 100%);}";
  html += ".btn-batal:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(244,67,54,0.4);}";
  html += ".btn-label {font-size: 10px; margin-top: 6px;}";
  html += ".button-group {display: grid; gap: 10px; margin-bottom: 10px;}";
  html += "a, .btn-nav {";
  html += "padding: 12px; border-radius: 5px; text-decoration: none; font-size: 16px;";
  html += "font-weight: bold; cursor: pointer; border: none; transition: transform 0.2s;}";
  html += ".btn-settings {background: #2196F3; color: white;}";
  html += ".btn-settings:hover {transform: translateY(-2px); background: #1976D2;}";
  html += ".btn-resetwifi {background: #FF9800; color: white;}";
  html += ".btn-resetwifi:hover {transform: translateY(-2px); background: #F57C00;}";
  html += ".info {background: #e7f3ff; border-left: 4px solid #2196F3; padding: 15px;";
  html += "border-radius: 5px; font-size: 13px; color: #1565c0; text-align: left;}";
  html += ".feedback {display: none; padding: 10px; margin-bottom: 15px; border-radius: 5px; text-align: center; font-weight: bold;}";
  html += ".feedback.success {display: block; background: #d4edda; color: #155724; border: 1px solid #c3e6cb;}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>" + ID_MEJA + "</h1>";
  html += "<div class='subtitle'>Sistem Pemanggil ENGINEERING</div>";

  html += "<div id='feedback' class='feedback'></div>";

  html += "<div class='status-card'>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Jaringan:</span>";
  html += "<span class='status-value'>" + WiFi.SSID() + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>IP:</span>";
  html += "<span class='status-value'>" + WiFi.localIP().toString() + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Signal:</span>";
  html += "<span class='status-value'>" + String(WiFi.RSSI()) + " dBm</span>";
  html += "</div>";
  html += "</div>";

  // Call Status Card
  html += "<div class='status-card' style='background: #f3e5f5; border-left: 4px solid #9c27b0;'>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Jenis Panggilan:</span>";
  html += "<span class='status-value' id='currentCall'>" + (sedangMemanggil ? jenisPanggilan : "TIDAK ADA") + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Status:</span>";
  html += "<span class='status-value' id='status'>IDLE</span>";
  html += "</div>";
  html += "</div>";

  html += "<div class='call-buttons'>";
  html += "<button class='btn-call btn-dummy' onclick='sendCall(\"DUMMY\")' title='Tombol Dummy'>";
  html += "<span style='font-size: 20px;'>�</span>";
  html += "<span class='btn-label'>DUMMY</span>";
  html += "</button>";
  html += "<button class='btn-call btn-paku' onclick='sendCall(\"PAKU_PALU\")' title='Tombol Paku Palu'>";
  html += "<span style='font-size: 20px;'>�</span>";
  html += "<span class='btn-label'>PAKU PALU</span>";
  html += "</button>";
  html += "<button class='btn-call btn-lem' onclick='sendCall(\"LEM_LILIN\")' title='Panggil Lem Lilin'>";
  html += "<span style='font-size: 18px;'>🔥</span>";
  html += "<span class='btn-label'>LEM LILIN</span>";
  html += "</button>";
  html += "<button class='btn-call btn-adjust' onclick='sendCall(\"ADJUST_TWISTING\")' title='Panggil Adjust Twisting'>";
  html += "<span style='font-size: 18px;'>⚙️</span>";
  html += "<span class='btn-label'>ADJUST TWIST</span>";
  html += "</button>";
  html += "<button class='btn-call btn-repair' onclick='sendCall(\"REPAIR_BOARD\")' title='Panggil Repair Board'>";
  html += "<span style='font-size: 18px;'>🔧</span>";
  html += "<span class='btn-label'>REPAIR BOARD</span>";
  html += "</button>";
  html += "<button class='btn-call btn-batal' onclick='sendCall(\"BATAL\")' title='Batal Panggilan'>";
  html += "<span style='font-size: 18px;'>✕</span>";
  html += "<span class='btn-label'>BATAL</span>";
  html += "</button>";
  html += "</div>";

  html += "<div class='button-group'>";
  html += "<a href='/settings' class='btn-settings'>Pengaturan</a>";
  html += "<a href='/resetwifi' class='btn-resetwifi'>Reset WiFi</a>";
  html += "</div>";
  html += "<div class='info'><strong>Tip:</strong><br>";
  html += "Gunakan tombol di atas untuk memanggil.<br>";
  html += "Klik 'Pengaturan' untuk mengubah nama meja.</div>";
  html += "</div>";

  html += "<script>";
  html += "async function sendCall(type) {";
  html += "  try {";
  html += "    const response = await fetch('/call?type=' + type, {method: 'POST'});";
  html += "    const data = await response.json();";
  html += "    const fb = document.getElementById('feedback');";
  html += "    if (type === 'BATAL') {";
  html += "      fb.textContent = '✓ Panggilan DIBATALKAN';";
  html += "    } else {";
  html += "      fb.textContent = '✓ Panggilan ' + type + ' TERKIRIM';";
  html += "    }";
  html += "    fb.className = 'feedback success';";
  html += "    setTimeout(() => { fb.className = 'feedback'; }, 2000);";
  html += "    await updateQueueStatus();";
  html += "  } catch(e) {";
  html += "    console.error('Error:', e);";
  html += "  }";
  html += "}";
  html += "async function updateQueueStatus() {";
  html += "  try {";
  html += "    const response = await fetch('/queueinfo');";
  html += "    const data = await response.json();";
  html += "    document.getElementById('currentCall').textContent = data.callType || '-';";
  html += "    document.getElementById('status').textContent = data.status || 'IDLE';";
  html += "  } catch(e) {";
  html += "    console.error('Queue update error:', e);";
  html += "  }";
  html += "}";
  html += "updateQueueStatus();";
  html += "setInterval(updateQueueStatus, 1000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleResetWiFi()
{
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Reset WiFi</title>
  <style>
    body {
      font-family: Arial;
      background: linear-gradient(135deg, #FF6B6B 0%, #FF5252 100%);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      color: white;
      text-align: center;
      margin: 0;
    }
    .container { 
      padding: 40px; 
      background: rgba(255,255,255,0.1);
      border-radius: 10px;
      backdrop-filter: blur(10px);
      max-width: 400px;
    }
    h1 { font-size: 28px; margin: 0 0 20px 0; }
    p { font-size: 16px; margin: 15px 0; }
    .spinner {
      border: 4px solid rgba(255,255,255,0.3);
      border-top: 4px solid white;
      border-radius: 50%;
      width: 40px;
      height: 40px;
      animation: spin 1s linear infinite;
      margin: 20px auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    #countdown {
      font-weight: bold;
      font-size: 24px;
      margin: 20px 0;
      color: #FFE082;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Reset WiFi</h1>
    <div class="spinner"></div>
    <p>WiFi akan direset ke mode AP</p>
    <p>Perangkat akan restart...</p>
    <p id="countdown">Redirect dalam 30 detik...</p>
  </div>
  <script>
    let count = 30;
    
    const countdownInterval = setInterval(function() {
      count--;
      document.getElementById('countdown').textContent = 'Redirect dalam ' + count + ' detik...';
      if (count <= 0) {
        clearInterval(countdownInterval);
        window.location.href = 'http://192.168.4.1/';
      }
    }, 1000);
  </script>
</body>
</html>
  )";

  server.send(200, "text/html", html);

  // Clear WiFi credentials
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  Serial.println("[WiFi] WiFi credentials cleared, restart pending...");

  // Set pending restart dengan delay agar AP bisa startup terlebih dahulu
  restartPending = true;
  restartTime = millis() + 3000;
}

void handleSave()
{
  if (server.method() == HTTP_POST)
  {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("pass");

    if (newSSID.length() > 0 && newPassword.length() > 0)
    {
      saveWiFiCredentials(newSSID, newPassword);

      // Send response dengan auto-detect IP mechanism
      String response = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Menghubungkan ke WiFi...</title>
  <style>
    body {
      font-family: Arial;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      color: white;
      text-align: center;
      margin: 0;
    }
    .container { 
      padding: 40px; 
      background: rgba(255,255,255,0.1);
      border-radius: 10px;
      backdrop-filter: blur(10px);
      max-width: 450px;
    }
    h1 { font-size: 28px; margin: 0 0 20px 0; }
    p { font-size: 16px; margin: 15px 0; }
    .spinner {
      border: 4px solid rgba(255,255,255,0.3);
      border-top: 4px solid white;
      border-radius: 50%;
      width: 50px;
      height: 50px;
      animation: spin 1s linear infinite;
      margin: 20px auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .status {
      background: rgba(255,255,255,0.1);
      padding: 15px;
      border-radius: 8px;
      margin-top: 20px;
      font-size: 14px;
    }
    #countdown {
      font-weight: bold;
      color: #FFE082;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>⏳ Menghubungkan ke WiFi</h1>
    <div class="spinner"></div>
    <p id="status-text">Menghubungkan ke jaringan...</p>
    <div class="status">
      <p>Proses ini mungkin memakan waktu 10-20 detik</p>
      <p id="countdown">Silahkan tunggu...</p>
    </div>
  </div>
  <script>
    let attemptCount = 0;
    const maxAttempts = 40;
    const apIP = "192.168.4.1";
    let startPolling = false;
    
    async function checkIP() {
      attemptCount++;
      try {
        const response = await fetch('http://' + apIP + '/getip');
        if (response.ok) {
          const data = await response.json();
          if (data.ip && data.ip !== "" && data.ip !== apIP) {
            // Valid IP detected - WiFi connected!
            document.getElementById('status-text').textContent = '✓ Terhubung ke WiFi!';
            document.getElementById('countdown').textContent = 'Buka pengaturan...';
            setTimeout(() => {
              window.location.href = 'http://' + data.ip + '/settings';
            }, 2000);
            return;
          }
        }
      } catch(e) {
        console.log('Still waiting for WiFi connection...');
      }
      
      if (attemptCount < maxAttempts) {
        const remaining = Math.ceil((maxAttempts - attemptCount) * 0.5);
        document.getElementById('countdown').textContent = 'Tunggu: ' + remaining + ' detik...';
        setTimeout(checkIP, 500);
      } else {
        document.getElementById('status-text').textContent = '❌ Koneksi Gagal';
        document.getElementById('countdown').textContent = 'Silahkan kembali ke 192.168.4.1 dan coba lagi';
      }
    }
    
    // Tunggu 3 detik sebelum mulai polling - beri waktu device untuk connect
    setTimeout(() => {
      startPolling = true;
      checkIP();
    }, 3000);
  </script>
</body>
</html>
      )";

      server.send(200, "text/html", response);

      // Setelah response terkirim, switch ke dual mode AP+STA untuk connect ke WiFi baru
      delay(500);

      Serial.println("[WiFi] Switching to AP+STA mode and attempting connection to: " + newSSID);
      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(newSSID.c_str(), newPassword.c_str());

      wifiConfigured = false;
    }
    else
    {
      server.send(400, "text/plain", "SSID dan Password tidak boleh kosong");
    }
  }
}

// ================= SETUP ACCESS POINT =================
void setupAccessPoint()
{
  if (apModeSetup)
    return; // Sudah setup, jangan setup lagi

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP MODE");
  lcd.setCursor(0, 1);
  lcd.print("192.168.4.1");

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("     WiFi SETUP MODE AKTIF");
  Serial.println("========================================");
  Serial.print("Access Point: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.println("URL: http://192.168.4.1");
  Serial.println("========================================\n");

  // Setup web server untuk AP mode
  server.on("/", handleRoot);
  server.on("/networks", handleNetworks);
  server.on("/save", handleSave);
  server.on("/status", handleStatus);
  server.on("/queueinfo", handleQueueInfo);
  server.on("/getip", handleGetIP);
  server.begin();
  apModeSetup = true;

  Serial.println("[WebServer] Started on port 80");
}

// ================= SETUP WIFI NORMAL =================
void setup_wifi()
{
  loadWiFiCredentials();

  // Jika belum ada kredensial, masuk AP mode
  if (ssid.length() == 0 || password.length() == 0)
  {
    Serial.println("[WiFi] No credentials found, starting AP mode");
    setupAccessPoint();
    return;
  }

  // Coba koneksi dengan kredensial yang tersimpan
  Serial.println("[WiFi] Attempting connection to: " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  lcd.setCursor(0, 1);
  lcd.print(ssid.substring(0, 16));

  // Tunggu koneksi dengan timeout lebih lama: 30 detik (60 attempt x 500ms)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConfigured = true;
    connectionRetryInterval = 5000; // Reset retry interval on success
    lastConnectionAttempt = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().substring(0, 16).c_str());

    Serial.println("[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    delay(2000);
  }
  else
  {
    // Jika gagal koneksi, kembali ke AP mode
    Serial.println("[WiFi] Connection failed after 30s, entering AP mode");
    setupAccessPoint();
  }
}

// ================= CEK DAN RECONNECT WIFI =================
void checkWiFiConnection()
{
  // Jika tidak dalam mode AP, cek koneksi secara berkala
  if (wifiConfigured && WiFi.status() != WL_CONNECTED)
  {
    // Check if it's time to retry based on exponential backoff
    if (millis() - lastConnectionAttempt < connectionRetryInterval)
    {
      return; // Not time to retry yet
    }

    Serial.println("[WiFi] Connection lost at " + String(millis()) + "ms, attempting to reconnect...");
    Serial.println("[WiFi] Retry interval: " + String(connectionRetryInterval / 1000) + " seconds");

    WiFi.begin(ssid.c_str(), password.c_str());
    lastConnectionAttempt = millis();

    // Tunggu reconnect selama hingga 20 detik
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40)
    {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("[WiFi] Reconnection attempt failed");

      // Increase retry interval with exponential backoff
      connectionRetryInterval = connectionRetryInterval * 1.5;
      if (connectionRetryInterval > MAX_RETRY_INTERVAL)
      {
        connectionRetryInterval = MAX_RETRY_INTERVAL;
      }

      Serial.println("[WiFi] Next retry in " + String(connectionRetryInterval / 1000) + " seconds");
    }
    else
    {
      // Successful reconnect - reset retry interval
      connectionRetryInterval = 5000;
      Serial.println("[WiFi] Reconnected! IP: " + WiFi.localIP().toString());

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi OK");
      lcd.setCursor(0, 1);
      lcd.print(WiFi.localIP().toString().substring(0, 16).c_str());
      delay(2000);
    }
  }
}

// ================= MQTT =================
void reconnect()
{
  while (!client.connected())
  {
    if (client.connect(ID_MEJA.c_str()))
    {
      lcd.clear();
      lcd.print("MQTT Connected");
      delay(1000);

      // Subscribe untuk receive responses
      client.subscribe(response_topic);
      Serial.println("[MQTT] Subscribed to: " + String(response_topic));
      
      // Restore pending call jika ada
      static bool firstMQTTConnect = true;
      if (firstMQTTConnect) {
        firstMQTTConnect = false;
        delay(500);
        restorePendingCall();
      }
    }
    else
    {
      delay(2000);
    }
  }
}

// ================= MQTT CALLBACK - Handle responses =================
void mqttMessageCallback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  Serial.println("[MQTT-RX] Topic: " + String(topic) + " | Payload: " + message);

  // Parse response: {"meja":"MEJA01","penerima":"PENERIMA01","status":"OK"}
  if (String(topic) == response_topic)
  {
    String penerimaNama = "";
    String status = "";

    // Extract penerima field
    int penerimaStart = message.indexOf("\"penerima\":\"") + 12;
    int penerimaEnd = message.indexOf("\"", penerimaStart);
    if (penerimaStart > 12 && penerimaEnd > penerimaStart)
    {
      penerimaNama = message.substring(penerimaStart, penerimaEnd);
    }

    // Extract status field
    int statusStart = message.indexOf("\"status\":\"") + 10;
    int statusEnd = message.indexOf("\"", statusStart);
    if (statusStart > 10 && statusEnd > statusStart)
    {
      status = message.substring(statusStart, statusEnd);
    }

    if (status == "OK" && currentCallIndex != -1)
    {
      callQueue[currentCallIndex].acknowledged = true;
      callQueue[currentCallIndex].respondedBy = penerimaNama;

      // ===== SWITCH TO OTW STATUS =====
      currentStatus = STATUS_OTW;
      otwStartTime = millis();

      Serial.println("[MQTT] ✓ Acknowledged by: " + penerimaNama);
      Serial.println("[Status] ► Switching to OTW mode for 10 seconds");

      // Tampil OTW di LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("OTW >> " + penerimaNama);
      lcd.setCursor(0, 1);
      lcd.print("Tunggu...");
    }
  }
}

// ================= AUTO CLEAR OTW STATUS =================
void autoClearOTW()
{
  // Jika sedang dalam status OTW
  if (currentStatus == STATUS_OTW && currentCallIndex != -1)
  {
    unsigned long elapsed = millis() - otwStartTime;

    // Jika sudah 10 detik, clear dan proses antrian berikutnya
    if (elapsed >= OTW_DURATION)
    {
      Serial.println("[Status] ✓ OTW duration expired (10 sec), clearing current call");

      // Clear current call
      currentCallIndex = -1;
      sedangMemanggil = false;
      jenisPanggilan = "";
      currentStatus = STATUS_IDLE;

      // Display idle state
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(ID_MEJA);
      lcd.setCursor(0, 1);
      lcd.print("Siap");

      // Proses antrian berikutnya
      if (queueCount > 0)
      {
        Serial.println("[Queue] ► Processing next call from queue");
        processQueue();
      }
      else
      {
        Serial.println("[Queue] ► No more calls in queue");
      }
    }
  }
}

// ================= BUZZER =================
void bunyiBuzzer(int durasi)
{
  digitalWrite(BUZZER, HIGH);
  delay(durasi);
  digitalWrite(BUZZER, LOW);
}

// ================= ANTRIAN / QUEUE FUNCTIONS =================
void addToQueue(String callType)
{
  if (queueCount >= MAX_QUEUE)
  {
    Serial.println("[Queue] ❌ Queue penuh! (max " + String(MAX_QUEUE) + ")");
    return;
  }

  callQueue[queueCount].type = callType;
  callQueue[queueCount].timeAdded = millis();
  callQueue[queueCount].acknowledged = false;
  callQueue[queueCount].respondedBy = "";
  queueCount++;

  Serial.println("[Queue] ✓ Panggilan ditambahkan: " + callType);
  Serial.println("[Queue] Total antrian: " + String(queueCount));
  printQueueInfo();
}

void processQueue()
{
  // Jika tidak ada yang sedang diproses, ambil dari queue
  if (currentCallIndex == -1 && queueCount > 0)
  {
    // Ambil panggilan pertama dari queue
    String nextCall = callQueue[0].type;

    // Shift queue (hapus item pertama)
    for (int i = 0; i < queueCount - 1; i++)
    {
      callQueue[i] = callQueue[i + 1];
    }
    queueCount--;

    // Set sebagai current call
    currentCallIndex = 0;
    sedangMemanggil = true;
    jenisPanggilan = nextCall;
    currentStatus = STATUS_WAITING; // Set to WAITING status

    Serial.println("[Queue] ► Processing: " + nextCall);
    Serial.println("[Queue] Sisa antrian: " + String(queueCount));
    Serial.println("[Status] ► Set to WAITING for confirmation");

    kirimMQTT(nextCall, "PANGGIL");
    tampilkanStatus();
    savePendingCall();
  }
}

void cancelAllCalls()
{
  Serial.println("[Queue] ✗ Membatalkan semua panggilan");

  // Cancel current call jika ada
  if (sedangMemanggil)
  {
    kirimMQTT("", "BATAL");
    sedangMemanggil = false;
    jenisPanggilan = "";
    currentCallIndex = -1;
    currentStatus = STATUS_IDLE;
  }

  // Clear queue
  queueCount = 0;

  Serial.println("[Queue] Semua panggilan dibatalkan, queue dikosongkan");
  clearPendingCall();
}

void printQueueInfo()
{
  Serial.println("\n════════════════════════════════════════");
  Serial.println("📊 STATUS ANTRIAN - " + String(millis() / 1000) + "s");
  Serial.println("════════════════════════════════════════");
  Serial.println("Antrian menunggu: " + String(queueCount));

  if (currentCallIndex != -1)
  {
    Serial.println("Sedang diproses: " + jenisPanggilan);
    Serial.println("  Status: " + String(callQueue[currentCallIndex].acknowledged ? "ACK" : "WAIT"));
    if (callQueue[currentCallIndex].respondedBy.length() > 0)
    {
      Serial.println("  Responded by: " + callQueue[currentCallIndex].respondedBy);
    }
  }
  else
  {
    Serial.println("Sedang diproses: TIDAK ADA");
  }

  if (queueCount > 0)
  {
    Serial.println("\n📋 Daftar Antrian:");
    for (int i = 0; i < queueCount; i++)
    {
      unsigned long waitTime = (millis() - callQueue[i].timeAdded) / 1000;
      Serial.println("  [" + String(i + 1) + "] " + callQueue[i].type +
                     " (wait: " + String(waitTime) + "s, ack: " +
                     String(callQueue[i].acknowledged ? "YES" : "NO") + ")");
    }
  }
  Serial.println("════════════════════════════════════════\n");
}

// ================= LCD =================
void tampilkanStatus()
{
  lcd.clear();

  if (sedangMemanggil)
  {
    // Line 1: Nama meja
    lcd.setCursor(0, 0);
    lcd.print(ID_MEJA);

    // Line 1: Jumlah antrian (di sebelah kanan)
    String queueDisplay = "Q:" + String(queueCount);
    lcd.setCursor(16 - queueDisplay.length(), 0);
    lcd.print(queueDisplay);

    // Line 2: Panggilan yang sedang berjalan
    lcd.setCursor(0, 1);
    lcd.print(jenisPanggilan);
  }
  else
  {
    // Tidak ada panggilan
    lcd.setCursor(0, 0);
    lcd.print(ID_MEJA);

    // Tampilkan jumlah antrian di sebelah kanan
    String queueDisplay = "Q:" + String(queueCount);
    lcd.setCursor(16 - queueDisplay.length(), 0);
    lcd.print(queueDisplay);

    // Line 2: Status siap
    if (queueCount == 0)
    {
      lcd.setCursor(0, 1);
      lcd.print("SIAP...");
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("Loading Q" + String(queueCount));
    }
  }
}

// ================= KIRIM MQTT =================
void kirimMQTT(String jenis, String status)
{
  String payload = "{";
  payload += "\"meja\":\"" + String(ID_MEJA) + "\",";
  payload += "\"timestamp\":" + String(millis()) + ",";
  if (jenis != "")
  {
    payload += "\"jenis\":\"" + jenis + "\",";
  }
  payload += "\"status\":\"" + status + "\"";
  payload += "}";

  if (client.publish(publish_topic, payload.c_str(), (status == "PANGGIL")))
  {
    Serial.println("[MQTT-TX] Published: " + payload);
  }
  else
  {
    Serial.println("[MQTT-TX] Failed: " + payload);
  }
}

// ================= SAVE/RESTORE PENDING CALL =================
void savePendingCall()
{
  if (sedangMemanggil && jenisPanggilan != "")
  {
    prefs.begin("call", false);
    prefs.putString("pending_type", jenisPanggilan);
    prefs.putULong("pending_time", millis());
    prefs.end();
    
    Serial.println("[Pending] Saved pending call: " + jenisPanggilan);
  }
}

void clearPendingCall()
{
  prefs.begin("call", false);
  prefs.putString("pending_type", "");
  prefs.end();
  
  Serial.println("[Pending] Cleared pending call");
}

void restorePendingCall()
{
  prefs.begin("call", true);
  String pendingType = prefs.getString("pending_type", "");
  prefs.end();

  if (pendingType.length() > 0)
  {
    Serial.println("[Pending] Found pending call on startup: " + pendingType);
    Serial.println("[Pending] Re-publishing to Penerima...");
    
    sedangMemanggil = true;
    jenisPanggilan = pendingType;
    currentStatus = STATUS_WAITING;
    currentCallIndex = 0;
    
    kirimMQTT(pendingType, "PANGGIL");
    tampilkanStatus();
  }
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);
  delay(1000); // Tunggu serial ready
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("     PEMANGGIL STARTING UP");
  Serial.println("========================================");
  Serial.println("[SETUP] Serial initialized");

  Serial.println("[SETUP] Setting up GPIO pins...");
  pinMode(BTN_DUMMY, INPUT_PULLUP);
  pinMode(BTN_PAKU, INPUT_PULLUP);
  pinMode(BTN_LEM_LILIN, INPUT_PULLUP);
  pinMode(BTN_ADJUST, INPUT_PULLUP);
  pinMode(BTN_REPAIR, INPUT_PULLUP);
  pinMode(BTN_BATAL, INPUT_PULLUP);
  pinMode(BTN_BOOT, INPUT_PULLUP); // BOOT button untuk hard reset
  pinMode(BUZZER, OUTPUT);
  Serial.println("[SETUP] ✓ GPIO pins ready");

  Serial.println("[SETUP] Initializing I2C (SDA=21, SCL=47)...");
  Wire.begin(21, 47); // SDA=GPIO21, SCL=GPIO47
  Serial.println("[SETUP] ✓ I2C initialized");
  
  Serial.println("[SETUP] Initializing LCD at 0x27...");
  lcd.init();
  delay(500); // Tunggu LCD siap
  Serial.println("[SETUP] ✓ LCD initialized");
  
  Serial.println("[SETUP] Enabling LCD backlight...");
  lcd.backlight();
  delay(200);
  lcd.home(); // Set cursor ke posisi awal
  Serial.println("[SETUP] ✓ LCD backlight ready");

  // Load atau generate device settings sebelum setup WiFi
  Serial.println("[SETUP] Loading device settings...");
  loadDeviceSettings();
  Serial.println("[SETUP] ✓ Device settings loaded");
  
  Serial.println("[SETUP] Starting WiFi setup...");
  setup_wifi();
  Serial.println("[SETUP] ✓ WiFi setup complete");

  // Setup web server untuk mode normal (setelah WiFi terkoneksi)
  if (wifiConfigured)
  {
    server.on("/", handleMainPage);
    server.on("/call", handleCall);
    server.on("/settings", handleSettings);
    server.on("/savesettings", handleSaveSettings);
    server.on("/resetwifi", handleResetWiFi);
    server.on("/status", handleStatus);
    server.on("/queueinfo", handleQueueInfo);
    server.on("/getip", handleGetIP);
    server.begin();

    normalModeRoutesRegistered = true;

    Serial.println("\n========================================");
    Serial.println("     PERANGKAT SIAP DIGUNAKAN");
    Serial.println("========================================");
    Serial.print("Buka browser ke: http://");
    Serial.println(WiFi.localIP().toString());
    Serial.println("========================================\n");
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttMessageCallback);
  Serial.println("[SETUP] ✓ MQTT configured");

  tampilkanStatus();
  
  Serial.println("[SETUP] ✓✓✓ SETUP COMPLETE - Entering loop");
  Serial.println("========================================\n");
}

// ================= LOOP =================
void loop()
{
  static bool firstLoopRun = true;
  if (firstLoopRun) {
    firstLoopRun = false;
    Serial.println("[LOOP] ✓ Entered main loop successfully");
  }
  
  // Handle pending restart
  if (restartPending && millis() >= restartTime)
  {
    Serial.println("[System] Restarting device...");
    ESP.restart();
  }

  // Check hard reset button (BOOT / GPIO0) - cek setiap loop
  checkHardResetButton();

  // Background WiFi scan refresh (only if not currently scanning and cache expired)
  if (!isScanRunning && (millis() - lastScanTime > SCAN_CACHE_DURATION))
  {
    int scanStatus = WiFi.scanComplete();
    if (scanStatus == -2)
    {
      // No scan running, start background refresh
      WiFi.scanNetworks(true);
      isScanRunning = true;
      Serial.println("[WiFi] Background scan started");
    }
  }

  // Handle web server requests
  server.handleClient();

  // Check if WiFi connected in AP+STA mode (from handleSave)
  if (!wifiConfigured && WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] Connected to WiFi in AP+STA mode!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    wifiConfigured = true;

    // Update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().substring(0, 16).c_str());

    // Register normal mode routes jika belum terdaftar
    if (!normalModeRoutesRegistered)
    {
      Serial.println("[WebServer] Registering normal mode routes");
      server.on("/", handleMainPage); // Override handleRoot dengan handleMainPage
      server.on("/call", handleCall);
      server.on("/settings", handleSettings);
      server.on("/savesettings", handleSaveSettings);
      server.on("/resetwifi", handleResetWiFi);
      server.on("/status", handleStatus);
      server.on("/queueinfo", handleQueueInfo);
      server.on("/getip", handleGetIP);

      normalModeRoutesRegistered = true;
      Serial.println("[WebServer] Normal mode routes registered");
    }

    // Setup MQTT
    client.setServer(mqtt_server, 1883);

    Serial.println("\n========================================");
    Serial.println("     PERANGKAT SIAP DIGUNAKAN");
    Serial.println("========================================");
    Serial.print("Buka browser ke: http://");
    Serial.println(WiFi.localIP().toString());
    Serial.println("========================================\n");
  }

  if (!wifiConfigured)
  {
    delay(10);
    return;
  }

  // Cek koneksi WiFi secara berkala
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = millis();
    checkWiFiConnection();
  }

  // Normal operation ketika WiFi terkoneksi
  if (!client.connected())
    reconnect();
  client.loop();

  // ===== AUTO CLEAR OTW STATUS (setiap loop untuk responsif) =====
  autoClearOTW();

  // ===== PERIODIC QUEUE STATUS PRINT (setiap 5 detik) =====
  if (millis() - lastQueueUpdate > QUEUE_UPDATE_INTERVAL)
  {
    lastQueueUpdate = millis();
    printQueueInfo();
  }

  // ===== TOMBOL DUMMY =====
  if (digitalRead(BTN_DUMMY) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    addToQueue("DUMMY");
    bunyiBuzzer(500);
    processQueue();
  }

  // ===== TOMBOL PAKU PALU =====
  if (digitalRead(BTN_PAKU) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    addToQueue("PAKU_PALU");
    bunyiBuzzer(700);
    processQueue();
  }

  // ===== TOMBOL LEM LILIN =====
  if (digitalRead(BTN_LEM_LILIN) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    addToQueue("LEM_LILIN");
    bunyiBuzzer(600);
    processQueue();
  }

  // ===== TOMBOL ADJUST TWISTING =====
  if (digitalRead(BTN_ADJUST) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    addToQueue("ADJUST_TWISTING");
    bunyiBuzzer(650);
    processQueue();
  }

  // ===== TOMBOL REPAIR BOARD =====
  if (digitalRead(BTN_REPAIR) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    addToQueue("REPAIR_BOARD");
    bunyiBuzzer(750);
    processQueue();
  }

  // ===== TOMBOL BATAL =====
  if (digitalRead(BTN_BATAL) == LOW && millis() - lastPress > delayDebounce)
  {
    lastPress = millis();

    cancelAllCalls();
    bunyiBuzzer(200);
  }
}
