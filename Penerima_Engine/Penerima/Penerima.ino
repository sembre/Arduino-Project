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
const char *subscribe_topic = "panggilan/alat";
const char *response_topic = "panggilan/respons";

// ================= ID PENERIMA =================
String ID_PENERIMA = "PENERIMA01"; // Dapat diubah via web interface

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= PIN =================
#define BTN_KONFIRMASI 4 // Tombol untuk konfirmasi terima
#define BTN_BOOT 0       // GPIO0 - BOOT button (untuk hard reset)
#define BUZZER 19

// ================= STATUS PANGGILAN =================
#define MAX_CALL_HISTORY 10

// ================= CALL TYPE FILTER =================
struct CallTypeFilter
{
    bool allowDummy;          // DUMMY
    bool allowPakuPalu;       // PAKU_PALU
    bool allowLemLinlin;      // LEM_LILIN
    bool allowAdjustTwisting; // ADJUST_TWISTING
    bool allowRepairBoard;    // REPAIR_BOARD
};

CallTypeFilter callTypeFilter = {
    true,  // allowDummy - default all ON
    true,  // allowPakuPalu
    true,  // allowLemLinlin
    true,  // allowAdjustTwisting
    true   // allowRepairBoard
};

// ================= FILTER HELPER FUNCTIONS =================
bool isCallTypeAllowed(String jenisCall)
{
    if (jenisCall == "DUMMY") return callTypeFilter.allowDummy;
    if (jenisCall == "PAKU_PALU") return callTypeFilter.allowPakuPalu;
    if (jenisCall == "LEM_LILIN") return callTypeFilter.allowLemLinlin;
    if (jenisCall == "ADJUST_TWISTING") return callTypeFilter.allowAdjustTwisting;
    if (jenisCall == "REPAIR_BOARD") return callTypeFilter.allowRepairBoard;
    return false; // Default: tidak terima jenis yang tidak dikenal
}

void saveCallTypeFilter()
{
    Preferences prefs;
    prefs.begin("filter", false);
    prefs.putBool("dummy", callTypeFilter.allowDummy);
    prefs.putBool("paku", callTypeFilter.allowPakuPalu);
    prefs.putBool("lem", callTypeFilter.allowLemLinlin);
    prefs.putBool("adjust", callTypeFilter.allowAdjustTwisting);
    prefs.putBool("repair", callTypeFilter.allowRepairBoard);
    prefs.end();
    Serial.println("[Filter] 💾 Preferences saved");
}

void loadCallTypeFilter()
{
    Preferences prefs;
    prefs.begin("filter", true); // read-only
    callTypeFilter.allowDummy = prefs.getBool("dummy", true);
    callTypeFilter.allowPakuPalu = prefs.getBool("paku", true);
    callTypeFilter.allowLemLinlin = prefs.getBool("lem", true);
    callTypeFilter.allowAdjustTwisting = prefs.getBool("adjust", true);
    callTypeFilter.allowRepairBoard = prefs.getBool("repair", true);
    prefs.end();
    Serial.println("[Filter] 📖 Preferences loaded");
    Serial.print("[Filter] Active filters: ");
    if (callTypeFilter.allowDummy) Serial.print("DUMMY ");
    if (callTypeFilter.allowPakuPalu) Serial.print("PAKU ");
    if (callTypeFilter.allowLemLinlin) Serial.print("LEM ");
    if (callTypeFilter.allowAdjustTwisting) Serial.print("ADJUST ");
    if (callTypeFilter.allowRepairBoard) Serial.print("REPAIR");
    Serial.println();
}

struct CallInfo
{
    String mejaID;           // Siapa yang memanggil (MEJA01, MEJA02, dst)
    String jenisCall;        // Jenis panggilan (DUMMY, PAKU_PALU)
    unsigned long timestamp; // Waktu panggilan diterima
    bool sudahDikonfirmasi;  // Sudah dikonfirmasi receiver atau belum
};

CallInfo callHistory[MAX_CALL_HISTORY];
int callHistoryCount = 0;
int currentCallIndex = -1; // Index panggilan yang sedang ditampilkan (-1 jika tidak ada)
bool adaPanggilan = false;

// UI State
unsigned long lastUIUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 5000; // Update UI setiap 5 detik

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
                tampilkanStatus();
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

void saveDeviceSettings(String nama_penerima)
{
    prefs.begin("device", false);
    prefs.putString("nama_penerima", nama_penerima);
    prefs.end();

    Serial.println("[Device] Settings saved");
    Serial.println("[Device] Nama Penerima: " + nama_penerima);
}

void loadDeviceSettings()
{
    prefs.begin("device", true);
    String nama = prefs.getString("nama_penerima", "");
    prefs.end();

    if (nama.length() > 0)
    {
        // Load dari preferences jika sudah pernah di-save
        ID_PENERIMA = nama;
        ap_ssid = nama + "_SETUP";
        Serial.println("[Device] Settings loaded from Preferences");
        Serial.println("[Device] Nama Penerima: " + ID_PENERIMA);
    }
    else
    {
        // Generate random ID dari MAC address (unik tapi konsisten)
        uint8_t mac[6];
        WiFi.macAddress(mac);
        String mac_suffix = String(mac[4], HEX) + String(mac[5], HEX);
        mac_suffix.toUpperCase();

        ID_PENERIMA = "PENERIMA_" + mac_suffix;
        ap_ssid = ID_PENERIMA + "_SETUP";

        Serial.println("[Device] Generated random ID from MAC address");
        Serial.println("[Device] Nama Penerima: " + ID_PENERIMA);
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
    bool forceNewScan = server.hasArg("force");

    if (forceNewScan)
    {
        Serial.println("[WiFi] Force new scan requested");
        isScanRunning = false;
        lastScanTime = 0;
        WiFi.scanNetworks(true);
        isScanRunning = true;
        server.send(200, "text/html", "<option>Memindai jaringan...</option>");
        return;
    }

    int scanStatus = WiFi.scanComplete();

    if (scanStatus == -2)
    {
        Serial.println("[WiFi] Starting async WiFi scan");
        WiFi.scanNetworks(true);
        isScanRunning = true;
        server.send(200, "text/html", "<option>Memindai jaringan...</option>");
        return;
    }

    if (scanStatus == -1)
    {
        server.send(200, "text/html", "<option>Memindai jaringan...</option>");
        return;
    }

    isScanRunning = false;
    lastScanTime = millis();
    String html = getNetworksHTML();
    server.send(200, "text/html", html);
}

void handleStatus()
{
    String response = "{";
    response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    response += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    response += "\"ssid\":\"" + WiFi.SSID() + "\"";
    response += "}";

    server.send(200, "application/json", response);
}

void handleCallStatus()
{
    // Return incoming call information as JSON
    String response = "{";
    response += "\"adaPanggilan\":" + String(adaPanggilan ? "true" : "false") + ",";
    response += "\"callCount\":" + String(callHistoryCount) + ",";

    if (currentCallIndex >= 0 && currentCallIndex < callHistoryCount)
    {
        response += "\"currentCall\":{";
        response += "\"mejaID\":\"" + callHistory[currentCallIndex].mejaID + "\",";
        response += "\"jenisCall\":\"" + callHistory[currentCallIndex].jenisCall + "\",";
        response += "\"timestamp\":" + String(callHistory[currentCallIndex].timestamp) + ",";
        response += "\"sudahDikonfirmasi\":" + String(callHistory[currentCallIndex].sudahDikonfirmasi ? "true" : "false");
        response += "}";
    }
    else
    {
        response += "\"currentCall\":null";
    }

    response += "}";

    server.send(200, "application/json", response);
}

void handleGetIP()
{
    String currentIP = WiFi.localIP().toString();

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

// ================= FILTER INFO ENDPOINT (untuk Pemanggil) =================
void handleFilterInfo()
{
    // Return filter status + queue info untuk digunakan Pemanggil
    String response = "{";
    response += "\"nama\":\"" + ID_PENERIMA + "\",";
    response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    response += "\"filters\":{";
    response += "\"allowDummy\":" + String(callTypeFilter.allowDummy ? "true" : "false") + ",";
    response += "\"allowPakuPalu\":" + String(callTypeFilter.allowPakuPalu ? "true" : "false") + ",";
    response += "\"allowLemLinlin\":" + String(callTypeFilter.allowLemLinlin ? "true" : "false") + ",";
    response += "\"allowAdjustTwisting\":" + String(callTypeFilter.allowAdjustTwisting ? "true" : "false") + ",";
    response += "\"allowRepairBoard\":" + String(callTypeFilter.allowRepairBoard ? "true" : "false");
    response += "},";

    // Queue info untuk show posisi antrian
    response += "\"queueInfo\":{";
    response += "\"totalQueue\":" + String(callHistoryCount) + ",";
    response += "\"adaPanggilan\":" + String(adaPanggilan ? "true" : "false") + ",";
    response += "\"queue\":[";

    // Return array of queue items dengan detail
    for (int i = 0; i < callHistoryCount; i++)
    {
        if (i > 0) response += ",";
        response += "{";
        response += "\"position\":" + String(i + 1) + ",";
        response += "\"mejaID\":\"" + callHistory[i].mejaID + "\",";
        response += "\"jenisCall\":\"" + callHistory[i].jenisCall + "\",";
        response += "\"sudahDikonfirmasi\":" + String(callHistory[i].sudahDikonfirmasi ? "true" : "false");
        response += "}";
    }

    response += "]";
    response += "}";
    response += "}";

    server.send(200, "application/json", response);
}

void handleSettings()
{
    if (server.method() == HTTP_GET)
    {
        String html = "<!DOCTYPE html><html lang='id'><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Pengaturan Penerima</title>";
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
        html += "<h1>Pengaturan Penerima</h1>";
        html += "<div class='subtitle'>Konfigurasi Nama Penerima</div>";
        html += "<div class='info'><strong>Info:</strong><br>";
        html += "Atur nama penerima yang unik untuk setiap perangkat.<br>";
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
        html += "<label for='nama'>Nama Penerima</label>";
        html += "<input type='text' id='nama' name='nama' value='" + ID_PENERIMA + "' required>";
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
            ID_PENERIMA = newNama;
            saveDeviceSettings(newNama);

            String response = "<!DOCTYPE html><html><head>";
            response += "<meta charset='UTF-8'><title>Berhasil</title>";
            response += "<style>";
            response += "body {font-family: Arial; background: #4CAF50; display: flex; justify-content: center;";
            response += "align-items: center; min-height: 100vh; color: white; text-align: center; margin: 0;}";
            response += ".container {padding: 40px;}";
            response += "h1 {font-size: 28px;} p {font-size: 16px; margin: 20px 0;}";
            response += "</style></head><body><div class='container'>";
            response += "<h1>✓ Berhasil Disimpan!</h1>";
            response += "<p>Nama Penerima: <strong>" + newNama + "</strong></p>";
            response += "<p>Akan kembali ke halaman utama dalam 2 detik...</p>";
            response += "<script>setTimeout(function() { window.location.href = '/'; }, 2000);</script>";
            response += "</div></body></html>";

            server.send(200, "text/html", response);
            return;
        }
        else
        {
            server.send(400, "text/plain", "Nama penerima tidak boleh kosong atau terlalu panjang (max 16 karakter)");
        }
    }
}

// ================= CALL TYPE FILTER Web Handlers =================

void handleCallTypeFilter()
{
    if (server.method() == HTTP_GET)
    {
        String html = "<!DOCTYPE html><html lang='id'><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Filter Jenis Panggilan</title>";
        html += "<style>";
        html += "body {font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
        html += "display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}";
        html += ".container {background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 30px rgba(0,0,0,0.3);";
        html += "max-width: 500px; width: 100%;}";
        html += "h1 {color: #333; text-align: center; font-size: 24px; margin: 0 0 10px 0;}";
        html += ".subtitle {text-align: center; color: #666; font-size: 14px; margin-bottom: 30px;}";
        html += ".filter-group {background: #f9f9f9; padding: 20px; border-radius: 8px; margin-bottom: 20px;}";
        html += ".filter-item {display: flex; align-items: center; margin-bottom: 15px; padding: 10px; border-radius: 5px;";
        html += "background: white; border: 1px solid #e0e0e0;}";
        html += ".filter-item:last-child {margin-bottom: 0;}";
        html += "input[type='checkbox'] {width: 18px; height: 18px; margin-right: 12px; cursor: pointer;}";
        html += "label {flex: 1; margin: 0; cursor: pointer; color: #333; font-weight: 500; font-size: 15px;}";
        html += ".checkbox-container {display: flex; align-items: center; width: 100%;}";
        html += ".button-group {display: flex; gap: 10px; margin-top: 30px;}";
        html += "button {flex: 1; padding: 12px; border: none; border-radius: 5px; font-size: 16px; font-weight: bold;";
        html += "cursor: pointer; transition: transform 0.2s;}";
        html += ".btn-save {background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white;}";
        html += ".btn-save:hover {transform: translateY(-2px);}";
        html += ".btn-back {background: #e0e0e0; color: #333;}";
        html += ".btn-back:hover {transform: translateY(-2px);}";
        html += ".info {background: #e7f3ff; border-left: 4px solid #2196F3; padding: 15px; margin-bottom: 20px;";
        html += "border-radius: 5px; font-size: 13px; color: #1565c0;}";
        html += ".status-filters {background: #f5f5f5; padding: 15px; border-radius: 5px; margin-bottom: 20px;";
        html += "font-size: 12px; color: #666;}";
        html += "</style></head><body>";
        html += "<div class='container'>";
        html += "<h1>⚙️ Filter Jenis Panggilan</h1>";
        html += "<div class='subtitle'>Pilih jenis panggilan apa saja yang bisa diterima</div>";
        html += "<div class='info'><strong>ℹ️ Info:</strong><br>";
        html += "Centang jenis panggilan yang ingin diterima. Panggilan dengan jenis yang tidak dipilih akan diabaikan.</div>";

        // Kontrol preference
        html += "<form method='POST' action='/savefilters'>";
        html += "<div class='filter-group' style='border: 2px solid #ddd;'>";
        html += "<div style='margin-bottom: 15px; color: #666; font-weight: bold; font-size: 14px;'>Jenis Panggilan yang Diterima:</div>";

        // Checkbox untuk setiap jenis panggilan
        html += "<div class='filter-item'>";
        html += "<input type='checkbox' id='dummy' name='dummy' value='1' " + String(callTypeFilter.allowDummy ? "checked" : "") + ">";
        html += "<label for='dummy'>🤖 DUMMY - Panggilan Dummy/Test</label>";
        html += "</div>";

        html += "<div class='filter-item'>";
        html += "<input type='checkbox' id='paku' name='paku' value='1' " + String(callTypeFilter.allowPakuPalu ? "checked" : "") + ">";
        html += "<label for='paku'>🔨 PAKU PALU - Panggilan Paku Palu</label>";
        html += "</div>";

        html += "<div class='filter-item'>";
        html += "<input type='checkbox' id='lem' name='lem' value='1' " + String(callTypeFilter.allowLemLinlin ? "checked" : "") + ">";
        html += "<label for='lem'>🧴 LEM LILIN - Panggilan Lem Lilin</label>";
        html += "</div>";

        html += "<div class='filter-item'>";
        html += "<input type='checkbox' id='adjust' name='adjust' value='1' " + String(callTypeFilter.allowAdjustTwisting ? "checked" : "") + ">";
        html += "<label for='adjust'>🔧 ADJUST TWISTING - Panggilan Penyesuaian Putaran</label>";
        html += "</div>";

        html += "<div class='filter-item'>";
        html += "<input type='checkbox' id='repair' name='repair' value='1' " + String(callTypeFilter.allowRepairBoard ? "checked" : "") + ">";
        html += "<label for='repair'>🛠️ REPAIR BOARD - Panggilan Perbaikan Board</label>";
        html += "</div>";

        html += "</div>";

        // Status current filter
        html += "<div class='status-filters'>";
        html += "<strong>Status Filter Aktif:</strong><br>";
        int activeCount = (callTypeFilter.allowDummy ? 1 : 0) + 
                         (callTypeFilter.allowPakuPalu ? 1 : 0) +
                         (callTypeFilter.allowLemLinlin ? 1 : 0) +
                         (callTypeFilter.allowAdjustTwisting ? 1 : 0) +
                         (callTypeFilter.allowRepairBoard ? 1 : 0);
        html += "Menerima " + String(activeCount) + " dari 5 jenis panggilan";
        html += "</div>";

        html += "<div class='button-group'>";
        html += "<button type='submit' class='btn-save'>💾 Simpan Filter</button>";
        html += "<button type='button' class='btn-back' onclick='history.back()'>← Kembali</button>";
        html += "</div>";
        html += "</form>";
        html += "</div>";
        html += "</body></html>";

        server.send(200, "text/html", html);
    }
}

void handleSaveCallFilters()
{
    if (server.method() == HTTP_POST)
    {
        // Update filter dari form data
        callTypeFilter.allowDummy = server.hasArg("dummy");
        callTypeFilter.allowPakuPalu = server.hasArg("paku");
        callTypeFilter.allowLemLinlin = server.hasArg("lem");
        callTypeFilter.allowAdjustTwisting = server.hasArg("adjust");
        callTypeFilter.allowRepairBoard = server.hasArg("repair");

        // Save ke preferences
        saveCallTypeFilter();

        // Hitung active filters
        int activeCount = (callTypeFilter.allowDummy ? 1 : 0) + 
                         (callTypeFilter.allowPakuPalu ? 1 : 0) +
                         (callTypeFilter.allowLemLinlin ? 1 : 0) +
                         (callTypeFilter.allowAdjustTwisting ? 1 : 0) +
                         (callTypeFilter.allowRepairBoard ? 1 : 0);

        // Serial logging
        Serial.println("[Filter] ✓ Filter updated by user");
        Serial.print("[Filter] Active filters: ");
        if (callTypeFilter.allowDummy) Serial.print("DUMMY ");
        if (callTypeFilter.allowPakuPalu) Serial.print("PAKU ");
        if (callTypeFilter.allowLemLinlin) Serial.print("LEM ");
        if (callTypeFilter.allowAdjustTwisting) Serial.print("ADJUST ");
        if (callTypeFilter.allowRepairBoard) Serial.print("REPAIR");
        Serial.println();

        // Response page
        String response = "<!DOCTYPE html><html><head>";
        response += "<meta charset='UTF-8'><title>Filter Tersimpan</title>";
        response += "<style>";
        response += "body {font-family: Arial; background: #4CAF50; display: flex; justify-content: center;";
        response += "align-items: center; min-height: 100vh; color: white; text-align: center; margin: 0;}";
        response += ".container {padding: 40px;}";
        response += "h1 {font-size: 28px;} p {font-size: 16px; margin: 20px 0;} .details {font-size: 14px; margin: 20px 0;}";
        response += "</style></head><body><div class='container'>";
        response += "<h1>✓ Filter Tersimpan!</h1>";
        response += "<p>Pengaturan filter panggilan telah diperbarui</p>";
        response += "<div class='details'>";
        response += "Menerima: <strong>" + String(activeCount) + "/5</strong> jenis panggilan<br>";
        response += "Akan kembali ke halaman utama dalam 3 detik...";
        response += "</div>";
        response += "<script>setTimeout(function() { window.location.href = '/'; }, 3000);</script>";
        response += "</div></body></html>";

        server.send(200, "text/html", response);
    }
}

void handleMainPage()
{
    String html = "<!DOCTYPE html><html lang='id'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + ID_PENERIMA + "</title>";
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
    html += ".call-alert {background: #fff3cd; border-left: 4px solid #ffc107; padding: 20px; border-radius: 8px; margin-bottom: 20px; text-align: center;}";
    html += ".call-alert h2 {color: #856404; margin: 0 0 10px 0; font-size: 20px;}";
    html += ".call-alert p {color: #856404; margin: 5px 0; font-size: 16px;}";
    html += ".btn-confirm {padding: 15px; border-radius: 8px; font-size: 16px; font-weight: bold;";
    html += "cursor: pointer; border: none; color: white; transition: all 0.3s;";
    html += "background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%); width: 100%; margin-bottom: 10px;}";
    html += ".btn-confirm:hover {transform: translateY(-2px); box-shadow: 0 5px 15px rgba(76,175,80,0.4);}";
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
    html += ".no-call {background: #e8f5e9; border-left: 4px solid #4CAF50; padding: 20px; border-radius: 8px; text-align: center; color: #2e7d32;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>" + ID_PENERIMA + "</h1>";
    html += "<div class='subtitle'>Sistem Penerima Panggilan</div>";

    html += "<div id='call-display'>";
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
    html += "<div class='status-item'>";
    html += "<span class='status-label'>Total Panggilan:</span>";
    html += "<span class='status-value' id='totalCalls'>" + String(callHistoryCount) + "</span>";
    html += "</div>";
    html += "</div>";

    html += "<div id='alert-area'>";
    html += "<div class='no-call'>📭 Tidak ada panggilan</div>";
    html += "</div>";

    html += "</div>";

    html += "<div class='button-group'>";
    html += "<a href='/callfilter' class='btn-settings'>⚙️ Filter Panggilan</a>";
    html += "<a href='/settings' class='btn-settings'>Pengaturan</a>";
    html += "<a href='/resetwifi' class='btn-resetwifi'>Reset WiFi</a>";
    html += "</div>";
    html += "<div class='info'><strong>Tip:</strong><br>";
    html += "Halaman ini akan menampilkan panggilan yang masuk.<br>";
    html += "Klik 'Pengaturan' untuk mengubah nama penerima.</div>";
    html += "</div>";

    html += "<script>";
    html += "async function updateCallStatus() {";
    html += "  try {";
    html += "    const response = await fetch('/callstatus');";
    html += "    const data = await response.json();";
    html += "    const alertArea = document.getElementById('alert-area');";
    html += "    const totalCalls = document.getElementById('totalCalls');";
    html += "    totalCalls.textContent = data.callCount;";
    html += "";
    html += "    if (data.adaPanggilan && data.currentCall) {";
    html += "      const call = data.currentCall;";
    html += "      alertArea.innerHTML = '<div class=\"call-alert\">' +";
    html += "        '<h2>🔔 PANGGILAN MASUK!</h2>' +";
    html += "        '<p><strong>' + call.mejaID + '</strong></p>' +";
    html += "        '<p>Jenis: ' + call.jenisCall + '</p>' +";
    html += "        '<button class=\"btn-confirm\" onclick=\"confirmCall()\">✓ SUDAH TERIMA</button>' +";
    html += "      '</div>';";
    html += "    } else {";
    html += "      alertArea.innerHTML = '<div class=\"no-call\">📭 Tidak ada panggilan</div>';";
    html += "    }";
    html += "  } catch(e) {";
    html += "    console.error('Error:', e);";
    html += "  }";
    html += "}";
    html += "";
    html += "async function confirmCall() {";
    html += "  try {";
    html += "    const response = await fetch('/confirmcall', {method: 'POST'});";
    html += "    updateCallStatus();";
    html += "  } catch(e) {";
    html += "    console.error('Error:', e);";
    html += "  }";
    html += "}";
    html += "";
    html += "updateCallStatus();";
    html += "setInterval(updateCallStatus, 1000);  // Update setiap 1 detik";
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
    body { font-family: Arial; background: linear-gradient(135deg, #FF6B6B 0%, #FF5252 100%);
      display: flex; justify-content: center; align-items: center; min-height: 100vh;
      color: white; text-align: center; margin: 0; }
    .container { padding: 40px; background: rgba(255,255,255,0.1); border-radius: 10px;
      backdrop-filter: blur(10px); max-width: 400px; }
    h1 { font-size: 28px; margin: 0 0 20px 0; }
    p { font-size: 16px; margin: 15px 0; }
    .spinner { border: 4px solid rgba(255,255,255,0.3); border-top: 4px solid white;
      border-radius: 50%; width: 40px; height: 40px; animation: spin 1s linear infinite;
      margin: 20px auto; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    #countdown { font-weight: bold; font-size: 24px; margin: 20px 0; color: #FFE082; }
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

    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    Serial.println("[WiFi] WiFi credentials cleared, restart pending...");

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

            String response = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Menghubungkan ke WiFi...</title>
  <style>
    body { font-family: Arial; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      display: flex; justify-content: center; align-items: center; min-height: 100vh;
      color: white; text-align: center; margin: 0; }
    .container { padding: 40px; background: rgba(255,255,255,0.1); border-radius: 10px;
      backdrop-filter: blur(10px); max-width: 450px; }
    h1 { font-size: 28px; margin: 0 0 20px 0; }
    p { font-size: 16px; margin: 15px 0; }
    .spinner { border: 4px solid rgba(255,255,255,0.3); border-top: 4px solid white;
      border-radius: 50%; width: 50px; height: 50px; animation: spin 1s linear infinite;
      margin: 20px auto; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    .status { background: rgba(255,255,255,0.1); padding: 15px; border-radius: 8px;
      margin-top: 20px; font-size: 14px; }
    #countdown { font-weight: bold; color: #FFE082; margin-top: 10px; }
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
    
    setTimeout(() => {
      startPolling = true;
      checkIP();
    }, 3000);
  </script>
</body>
</html>
      )";

            server.send(200, "text/html", response);

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
        return;

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

    server.on("/", handleRoot);
    server.on("/networks", handleNetworks);
    server.on("/save", handleSave);
    server.on("/status", handleStatus);
    server.on("/callstatus", handleCallStatus);
    server.on("/filterinfo", handleFilterInfo);
    server.on("/getip", handleGetIP);
    server.begin();
    apModeSetup = true;

    Serial.println("[WebServer] Started on port 80");
}

// ================= SETUP WIFI NORMAL =================
void setup_wifi()
{
    loadWiFiCredentials();

    if (ssid.length() == 0 || password.length() == 0)
    {
        Serial.println("[WiFi] No credentials found, starting AP mode");
        setupAccessPoint();
        return;
    }

    Serial.println("[WiFi] Attempting connection to: " + ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    lcd.setCursor(0, 1);
    lcd.print(ssid.substring(0, 16));

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
        connectionRetryInterval = 5000;
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
        Serial.println("[WiFi] Connection failed after 30s, entering AP mode");
        setupAccessPoint();
    }
}

// ================= CEK DAN RECONNECT WIFI =================
void checkWiFiConnection()
{
    if (wifiConfigured && WiFi.status() != WL_CONNECTED)
    {
        if (millis() - lastConnectionAttempt < connectionRetryInterval)
        {
            return;
        }

        Serial.println("[WiFi] Connection lost at " + String(millis()) + "ms, attempting to reconnect...");
        Serial.println("[WiFi] Retry interval: " + String(connectionRetryInterval / 1000) + " seconds");

        WiFi.begin(ssid.c_str(), password.c_str());
        lastConnectionAttempt = millis();

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

            connectionRetryInterval = connectionRetryInterval * 1.5;
            if (connectionRetryInterval > MAX_RETRY_INTERVAL)
            {
                connectionRetryInterval = MAX_RETRY_INTERVAL;
            }

            Serial.println("[WiFi] Next retry in " + String(connectionRetryInterval / 1000) + " seconds");
        }
        else
        {
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

// ================= MQTT CALLBACK =================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("[MQTT] Message received on topic: " + String(topic));

    // Parse JSON payload
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }

    Serial.println("[MQTT] Payload: " + message);

    // Simple JSON parsing (cari "meja", "jenis", "status")
    String mejaID = "";
    String jenisCall = "";
    String status = "";

    // Extract meja field
    int mejaStart = message.indexOf("\"meja\":\"") + 8;
    int mejaEnd = message.indexOf("\"", mejaStart);
    if (mejaStart > 8 && mejaEnd > mejaStart)
    {
        mejaID = message.substring(mejaStart, mejaEnd);
    }

    // Extract jenis field
    int jenisStart = message.indexOf("\"jenis\":\"") + 9;
    int jenisEnd = message.indexOf("\"", jenisStart);
    if (jenisStart > 9 && jenisEnd > jenisStart)
    {
        jenisCall = message.substring(jenisStart, jenisEnd);
    }

    // Extract status field
    int statusStart = message.indexOf("\"status\":\"") + 10;
    int statusEnd = message.indexOf("\"", statusStart);
    if (statusStart > 10 && statusEnd > statusStart)
    {
        status = message.substring(statusStart, statusEnd);
    }

    Serial.println("[MQTT] Parsed - Meja: " + mejaID + ", Jenis: " + jenisCall + ", Status: " + status);

    // Handle incoming call
    if (status == "PANGGIL" && mejaID.length() > 0)
    {
        // Check filter sebelum add
        if (isCallTypeAllowed(jenisCall))
        {
            addIncomingCall(mejaID, jenisCall);
        }
        else
        {
            Serial.println("[Filter] ❌ Call filtered - Jenis: " + jenisCall + " tidak diizinkan");
        }
    }
    else if (status == "BATAL")
    {
        Serial.println("[MQTT] Panggilan dibatalkan oleh: " + mejaID);
    }
}

void addIncomingCall(String mejaID, String jenisCall)
{
    if (callHistoryCount >= MAX_CALL_HISTORY)
    {
        // Jika full, geser array
        for (int i = 0; i < MAX_CALL_HISTORY - 1; i++)
        {
            callHistory[i] = callHistory[i + 1];
        }
        callHistoryCount = MAX_CALL_HISTORY - 1;
    }

    callHistory[callHistoryCount].mejaID = mejaID;
    callHistory[callHistoryCount].jenisCall = jenisCall;
    callHistory[callHistoryCount].timestamp = millis();
    callHistory[callHistoryCount].sudahDikonfirmasi = false;

    if (currentCallIndex == -1)
    {
        currentCallIndex = callHistoryCount;
    }

    callHistoryCount++;
    adaPanggilan = true;

    Serial.println("[Queue] ✓ Panggilan dari " + mejaID + " ditambahkan (jenis: " + jenisCall + ")");
    Serial.println("[Queue] Total panggilan: " + String(callHistoryCount));

    // Buzzer untuk alert
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(BUZZER, HIGH);
        delay(200);
        digitalWrite(BUZZER, LOW);
        delay(100);
    }

    tampilkanStatus();
}

void handleConfirmCall()
{
    if (server.method() == HTTP_POST)
    {
        if (currentCallIndex >= 0 && currentCallIndex < callHistoryCount)
        {
            callHistory[currentCallIndex].sudahDikonfirmasi = true;
            Serial.println("[Confirm] ✓ Panggilan dari " + callHistory[currentCallIndex].mejaID + " dikonfirmasi");

            // Send response back to pemanggil
            sendResponse(callHistory[currentCallIndex].mejaID, "OK");

            // Pindah ke panggilan berikutnya
            currentCallIndex++;
            if (currentCallIndex >= callHistoryCount)
            {
                currentCallIndex = -1;
                adaPanggilan = false;
            }

            tampilkanStatus();
        }

        server.send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

// ================= MQTT =================
void reconnect()
{
    while (!client.connected())
    {
        Serial.print("[MQTT] Attempting MQTT connection...");

        if (client.connect(ID_PENERIMA.c_str()))
        {
            Serial.println("[MQTT] ✓ Connected!");
            lcd.clear();
            lcd.print("MQTT OK");
            delay(1000);

            // Subscribe to incoming calls
            client.subscribe(subscribe_topic);
            Serial.println("[MQTT] Subscribed to topic: " + String(subscribe_topic));
        }
        else
        {
            Serial.print("[MQTT] failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
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

// ================= SEND RESPONSE =================
void sendResponse(String mejaID, String status)
{
    String payload = "{";
    payload += "\"meja\":\"" + mejaID + "\",";
    payload += "\"penerima\":\"" + ID_PENERIMA + "\",";
    payload += "\"timestamp\":" + String(millis()) + ",";
    payload += "\"status\":\"" + status + "\"";
    payload += "}";

    if (client.publish(response_topic, payload.c_str()))
    {
        Serial.println("[MQTT-RESP] ✓ " + payload);
    }
    else
    {
        Serial.println("[MQTT-RESP] ❌ Failed: " + payload);
    }
}

// ================= LCD =================
void tampilkanStatus()
{
    lcd.clear();

    if (adaPanggilan && currentCallIndex >= 0 && currentCallIndex < callHistoryCount)
    {
        // Ada panggilan yang ditampilkan
        lcd.setCursor(0, 0);
        lcd.print("PANGGIL:" + callHistory[currentCallIndex].mejaID);

        lcd.setCursor(0, 1);
        lcd.print(callHistory[currentCallIndex].jenisCall.substring(0, 16));
    }
    else
    {
        // Tidak ada panggilan
        lcd.setCursor(0, 0);
        lcd.print(ID_PENERIMA);

        lcd.setCursor(15, 0);
        if (callHistoryCount > 0)
        {
            lcd.print(String(callHistoryCount));
        }

        lcd.setCursor(0, 1);
        if (callHistoryCount == 0)
        {
            lcd.print("SIAP...");
        }
        else
        {
            lcd.print("Calls:" + String(callHistoryCount));
        }
    }
}

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);
    delay(1000); // Tunggu serial ready

    // Check hard reset PERTAMA KALI sebelum apapun
    checkHardResetButton();

    pinMode(BTN_KONFIRMASI, INPUT_PULLUP);
    pinMode(BTN_BOOT, INPUT_PULLUP); // BOOT button untuk hard reset
    pinMode(BUZZER, OUTPUT);

    Wire.begin(21, 47); // SDA=GPIO21, SCL=GPIO47
    lcd.init();
    delay(500); // Tunggu LCD siap
    lcd.backlight();
    delay(200);
    lcd.home(); // Set cursor ke posisi awal

    // Load atau generate device settings sebelum setup WiFi
    loadDeviceSettings();
    loadCallTypeFilter(); // Load filter preferences

    setup_wifi();

    // Setup web server untuk mode normal (setelah WiFi terkoneksi)
    if (wifiConfigured)
    {
        server.on("/", handleMainPage);
        server.on("/settings", handleSettings);
        server.on("/savesettings", handleSaveSettings);
        server.on("/callfilter", handleCallTypeFilter);
        server.on("/savefilters", handleSaveCallFilters);
        server.on("/resetwifi", handleResetWiFi);
        server.on("/status", handleStatus);
        server.on("/callstatus", handleCallStatus);
        server.on("/filterinfo", handleFilterInfo);
        server.on("/confirmcall", handleConfirmCall);
        server.on("/getip", handleGetIP);
        server.begin();

        normalModeRoutesRegistered = true;

        Serial.println("\n========================================");
        Serial.println("     PENERIMA SIAP DIGUNAKAN");
        Serial.println("========================================");
        Serial.print("Buka browser ke: http://");
        Serial.println(WiFi.localIP().toString());
        Serial.println("========================================\n");
    }

    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);

    tampilkanStatus();
}

// ================= LOOP =================
void loop()
{
    // Handle pending restart
    if (restartPending && millis() >= restartTime)
    {
        Serial.println("[System] Restarting device...");
        ESP.restart();
    }

    // Check hard reset button (BOOT / GPIO0)
    checkHardResetButton();

    // Background WiFi scan refresh
    if (!isScanRunning && (millis() - lastScanTime > SCAN_CACHE_DURATION))
    {
        int scanStatus = WiFi.scanComplete();
        if (scanStatus == -2)
        {
            WiFi.scanNetworks(true);
            isScanRunning = true;
            Serial.println("[WiFi] Background scan started");
        }
    }

    // Handle web server requests
    server.handleClient();

    // Check if WiFi connected in AP+STA mode
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

        // Register normal mode routes
        if (!normalModeRoutesRegistered)
        {
            Serial.println("[WebServer] Registering normal mode routes");
            server.on("/", handleMainPage);
            server.on("/settings", handleSettings);
            server.on("/savesettings", handleSaveSettings);
            server.on("/callfilter", handleCallTypeFilter);
            server.on("/savefilters", handleSaveCallFilters);
            server.on("/resetwifi", handleResetWiFi);
            server.on("/status", handleStatus);
            server.on("/callstatus", handleCallStatus);
            server.on("/filterinfo", handleFilterInfo);
            server.on("/confirmcall", handleConfirmCall);
            server.on("/getip", handleGetIP);

            normalModeRoutesRegistered = true;
            Serial.println("[WebServer] Normal mode routes registered");
        }

        // Setup MQTT
        client.setServer(mqtt_server, 1883);
        client.setCallback(mqttCallback);

        Serial.println("\n========================================");
        Serial.println("     PENERIMA SIAP DIGUNAKAN");
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

    // MQTT connection & loop
    if (!client.connected())
        reconnect();
    client.loop();

    // ===== TOMBOL KONFIRMASI =====
    if (digitalRead(BTN_KONFIRMASI) == LOW && millis() - lastPress > delayDebounce)
    {
        lastPress = millis();

        if (adaPanggilan && currentCallIndex >= 0 && currentCallIndex < callHistoryCount)
        {
            Serial.println("[Button] Konfirmasi diterima");
            handleConfirmCall();
            bunyiBuzzer(300);
        }
    }
}
