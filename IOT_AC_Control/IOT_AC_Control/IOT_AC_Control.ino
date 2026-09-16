#include <WiFi.h>
#include <WiFiMulti.h>     // <--- Library baru untuk Multi-WiFi
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "I2CKeyPad.h"
#include <time.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SD_MMC.h> 

// ================= INSTANCE & IDENTITAS =================
WiFiMulti wifiMulti; // Objek pengelola Multi-WiFi
String DEVICE_NAME = ""; 
String wifiAPName;
String networkHost;
String mqtt_topic_status;
String mqtt_topic_cmd;

// ================= KONFIGURASI HARDWARE ================
#define I2C_SDA 8 
#define I2C_SCL 9 
#define RELAY_ON_PIN 4  
#define RELAY_OFF_PIN 5 

#define BUTTON_ON_PIN 6  
#define BUTTON_OFF_PIN 7 
#define BOOT_BUTTON_PIN 0 

#define LCD_ADDR 0x27 
#define KEYPAD_ADDR 0x20 

// Pin Khusus MicroSD Freenove ESP32-S3
#define SD_CMD 38
#define SD_CLK 39
#define SD_D0  40

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
I2CKeyPad keypad(KEYPAD_ADDR);
WebServer server(80);
Preferences preferences;

// ================= MQTT BROKER =================
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

// ================= VARIABEL SISTEM =================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200; 
const int   daylightOffset_sec = 0;

int onH[5]  = {6, -1, -1, -1, -1};   
int onM[5]  = {30, -1, -1, -1, -1};
int offH[5] = {17, -1, -1, -1, -1}; 
int offM[5] = {0, -1, -1, -1, -1};

int daysMask[5] = {127, 127, 127, 127, 127}; 

int currentDay = -1;
int currentMonth = -1;
int currentYear = -1;

unsigned long dailyRuntimeMinutes = 0;
unsigned long monthRuntimeMinutes = 0; 
unsigned long yearlyRuntimeMinutes = 0;
unsigned long totalRuntimeMinutes = 0;

bool acStatus = false; 
bool sdCardReady = false;

int lastMinute = -1;
char lastKey = 'N'; 
char currentTimeStr[9] = "00:00:00";
int keyState = 0; 
int selectedSched = 0;
String inputBuffer = "";

bool lastBtnOnState = HIGH; int btnOnState = HIGH; unsigned long lastDebounceOn = 0;
bool lastBtnOffState = HIGH; int btnOffState = HIGH; unsigned long lastDebounceOff = 0;
unsigned long debounceDelay = 50;

bool isBootButtonPressed = false;
unsigned long bootButtonPressTime = 0;

void logToSD(String action) {
  if (!sdCardReady) return; 
  File file = SD_MMC.open("/log_ac.csv", FILE_APPEND);
  if (file) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeLog[25];
      strftime(timeLog, sizeof(timeLog), "%Y-%m-%d %H:%M:%S", &timeinfo);
      char statLog[150];
      sprintf(statLog, "%s, %s, %02d:%02d, %02d:%02d, %02d:%02d, %02d:%02d",
              timeLog, action.c_str(),
              (int)(dailyRuntimeMinutes / 60), (int)(dailyRuntimeMinutes % 60),
              (int)(monthRuntimeMinutes / 60), (int)(monthRuntimeMinutes % 60),
              (int)(yearlyRuntimeMinutes / 60), (int)(yearlyRuntimeMinutes % 60),
              (int)(totalRuntimeMinutes / 60), (int)(totalRuntimeMinutes % 60));
      file.println(statLog);
    }
    file.close();
  }
}

void triggerAC(bool turnOn, String pemicu = "Sistem") {
  if (turnOn && !acStatus) {
    lcd.clear(); lcd.print("Menyalakan AC...");
    digitalWrite(RELAY_ON_PIN, HIGH); 
    acStatus = true;
    logToSD("MENYALA (" + pemicu + ")");
  } else if (!turnOn && acStatus) {
    lcd.clear(); lcd.print("Mematikan AC...");
    digitalWrite(RELAY_ON_PIN, LOW); delay(100); 
    digitalWrite(RELAY_OFF_PIN, HIGH); delay(1000); digitalWrite(RELAY_OFF_PIN, LOW); 
    acStatus = false;
    preferences.putUInt("rt_day", dailyRuntimeMinutes);
    preferences.putUInt("rt_mon", monthRuntimeMinutes);
    preferences.putUInt("rt_yr", yearlyRuntimeMinutes);
    preferences.putUInt("rt_tot", totalRuntimeMinutes);
    logToSD("MATI (" + pemicu + ")");
  }
  delay(500); lcd.clear();
  publishMQTTStatus(); 
}

// =========================================================================
//  PENGELOLA DAFTAR WIFI TERSIMPAN (Preferences / memori abadi ESP32)
// =========================================================================
// Daftar inilah yang dipakai wifiMulti untuk otomatis pindah ke WiFi lain
// begitu WiFi yang sedang dipakai hilang. Sebelumnya daftar ini HANYA bisa
// ditambah lewat captive portal - dan portal itu cuma terbuka kalau SEMUA
// WiFi tersimpan mati sekaligus (lihat setup()). Akibatnya, selama alat masih
// tersambung ke WiFi A, tidak ada satu pun cara menambahkan WiFi B sebagai
// cadangan, jadi praktis daftarnya seumur hidup cuma berisi satu WiFi -
// itulah yang bikin alat gampang kehilangan koneksi. Fungsi di bawah membuka
// jalan menambah/menghapus WiFi kapan saja lewat dashboard, TANPA perlu
// mematikan WiFi utama lebih dulu.

#define MAX_APS 5

int wifiApCount() {
  int n = preferences.getInt("saved_aps", 0);
  if (n < 0) n = 0;
  if (n > MAX_APS) n = MAX_APS;
  return n;
}

String wifiApSsid(int i) { return preferences.getString(("ssid" + String(i)).c_str(), ""); }
String wifiApPass(int i) { return preferences.getString(("pass" + String(i)).c_str(), ""); }

int wifiApFind(const String &ssid) {
  int n = wifiApCount();
  for (int i = 0; i < n; i++) if (wifiApSsid(i) == ssid) return i;
  return -1;
}

// Simpan (atau perbarui) satu WiFi. Return 1 = diperbarui, 0 = ditambah baru.
// Kalau daftar sudah penuh, yang PALING LAMA digeser keluar - sama seperti
// perilaku captive portal yang sudah ada, supaya tidak membingungkan.
int wifiApSave(const String &ssid, const String &pass) {
  if (ssid == "") return -1;

  int idx = wifiApFind(ssid);
  if (idx >= 0) {                       // sudah ada -> cukup perbarui sandinya
    preferences.putString(("pass" + String(idx)).c_str(), pass);
    return 1;
  }

  int n = wifiApCount();
  if (n < MAX_APS) {
    preferences.putString(("ssid" + String(n)).c_str(), ssid);
    preferences.putString(("pass" + String(n)).c_str(), pass);
    preferences.putInt("saved_aps", n + 1);
  } else {
    for (int i = 1; i < MAX_APS; i++) { // geser ke atas, buang yang paling lama
      preferences.putString(("ssid" + String(i - 1)).c_str(), wifiApSsid(i));
      preferences.putString(("pass" + String(i - 1)).c_str(), wifiApPass(i));
    }
    preferences.putString(("ssid" + String(MAX_APS - 1)).c_str(), ssid);
    preferences.putString(("pass" + String(MAX_APS - 1)).c_str(), pass);
  }
  return 0;
}

bool wifiApDelete(const String &ssid) {
  int idx = wifiApFind(ssid);
  if (idx < 0) return false;

  int n = wifiApCount();
  // Rapatkan daftar: yang di bawah naik satu. Aman karena yang dibaca (i+1)
  // selalu belum tertimpa pada saat ditulis ke i.
  for (int i = idx; i < n - 1; i++) {
    preferences.putString(("ssid" + String(i)).c_str(), wifiApSsid(i + 1));
    preferences.putString(("pass" + String(i)).c_str(), wifiApPass(i + 1));
  }
  preferences.remove(("ssid" + String(n - 1)).c_str());
  preferences.remove(("pass" + String(n - 1)).c_str());
  preferences.putInt("saved_aps", n - 1);
  return true;
}

// Pastikan WiFi yang SEDANG dipakai ikut tercatat di daftar abadi.
// PENTING: WiFi hasil captive portal / bawaan ESP dulu cuma hidup di
// pengaturan bawaan (WiFi.SSID()/WiFi.psk()) dan di daftar runtime wifiMulti,
// TIDAK pernah masuk daftar tersimpan. Jadi "saved_aps" sering benar-benar
// kosong - alat kelihatan punya WiFi, padahal daftar cadangannya nol.
void wifiApSimpanYangAktif() {
  String s = WiFi.SSID();
  if (s == "") return;
  if (wifiApFind(s) >= 0) return;
  wifiApSave(s, WiFi.psk());
}

void publishMQTTStatus() {
  if (!client.connected()) return;

  StaticJsonDocument<1536> doc;
  doc["id"] = networkHost;
  doc["title"] = DEVICE_NAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["time"] = currentTimeStr;
  doc["status"] = acStatus;
  doc["online"] = true;
  doc["runtime"] = monthRuntimeMinutes;

  // ---- Info WiFi untuk panel "WiFi Cadangan" di dashboard ----
  // SENGAJA HANYA NAMA (SSID), TIDAK PERNAH SANDINYA: topik status ini
  // retained di broker PUBLIK, jadi apa pun yang ditulis di sini bisa dibaca
  // siapa saja yang tahu nama topiknya, dan tersimpan di broker terus-menerus.
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  JsonArray aps = doc.createNestedArray("aps");
  int nAp = wifiApCount();
  for (int i = 0; i < nAp; i++) {
    String s = wifiApSsid(i);
    if (s != "") aps.add(s);
  }

  JsonArray scheds = doc.createNestedArray("schedules");
  for(int i=0; i<5; i++) {
    JsonObject s = scheds.createNestedObject();
    s["on_h"] = onH[i]; s["on_m"] = onM[i];
    s["off_h"] = offH[i]; s["off_m"] = offM[i];
    s["days"] = daysMask[i];
  }

  char jsonBuffer[1536];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_topic_status.c_str(), jsonBuffer, true);
}

// Kirim hasil pindaian WiFi di sekitar alat ke topik terpisah (TIDAK retained,
// karena ini data sesaat - kalau retained, hasil pindaian basi akan nempel
// selamanya di broker).
void publishWifiScan() {
  if (!client.connected()) return;

  int n = WiFi.scanNetworks();          // memblokir ~2-4 detik, hanya sesekali

  // DynamicJsonDocument (heap), BUKAN StaticJsonDocument (stack): hasil pindai
  // bisa belasan jaringan, dan stack task Arduino di ESP32 cuma 8 KB - menaruh
  // 1,5 KB dokumen + 1,5 KB buffer di stack berbarengan dengan pemakaian
  // WiFi.scanNetworks() terlalu berisiko bikin stack overflow.
  DynamicJsonDocument doc(2048);
  doc["id"] = networkHost;
  JsonArray arr = doc.createNestedArray("nets");

  int batas = (n > 15) ? 15 : n;        // jaga jangan sampai melebihi buffer
  for (int i = 0; i < batas; i++) {
    if (WiFi.SSID(i) == "") continue;
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["lock"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();

  String buf;
  serializeJson(doc, buf);
  client.publish((String("agus_smart_ac/") + networkHost + "/wifi_scan").c_str(), buf.c_str(), false);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  for (int i = 0; i < length; i++) { msg[i] = (char)payload[i]; }
  msg[length] = '\0';

  // 256 dulu terlalu kecil begitu ada perintah wifi_add (nama WiFi + sandi
  // bisa masing-masing sampai 32/63 karakter).
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) return;

  String cmd = doc["cmd"].as<String>();
  if (cmd == "toggle") {
    triggerAC(!acStatus, "Dashboard Web");
  } 
  else if (cmd == "set_name") {
    if (doc.containsKey("new_name")) {
      DEVICE_NAME = doc["new_name"].as<String>();
      preferences.putString("dev_name", DEVICE_NAME);
      lcd.clear(); lcd.print("Nama Diubah!");
      
      // Update Wasiat (Last Will) ke Broker
      client.disconnect(); 
      
      delay(1500); lcd.clear();
    }
  }
  else if (cmd == "set_sched") {
    int idx = doc["idx"].as<int>();
    if(idx >= 0 && idx < 5) {
      onH[idx] = doc["on_h"].as<int>(); onM[idx] = doc["on_m"].as<int>();
      offH[idx] = doc["off_h"].as<int>(); offM[idx] = doc["off_m"].as<int>();
      if (doc.containsKey("days")) daysMask[idx] = doc["days"].as<int>();
      preferences.putInt(("onH"+String(idx)).c_str(), onH[idx]);
      preferences.putInt(("onM"+String(idx)).c_str(), onM[idx]);
      preferences.putInt(("offH"+String(idx)).c_str(), offH[idx]);
      preferences.putInt(("offM"+String(idx)).c_str(), offM[idx]);
      preferences.putInt(("days"+String(idx)).c_str(), daysMask[idx]);
      publishMQTTStatus();
    }
  }
  // ================= PERINTAH WIFI CADANGAN =================
  // Jalur MQTT ini memungkinkan menambah WiFi dari mana saja lewat internet,
  // TAPI sandinya melewati broker PUBLIK tanpa autentikasi - siapa pun yang
  // tahu nama topik bisa membacanya. Dashboard sudah memberi peringatan dan
  // menjadikan jalur LOKAL (/wifi_add lewat IP alat, lihat setup()) sebagai
  // pilihan bawaan yang lebih aman.
  else if (cmd == "wifi_add") {
    String s = doc["ssid"].as<String>();
    String p = doc["pass"].as<String>();
    if (s != "") {
      wifiApSave(s, p);
      wifiMulti.addAP(s.c_str(), p.c_str());   // langsung aktif, tanpa restart
      lcd.clear(); lcd.print("WiFi cadangan+"); lcd.setCursor(0,1); lcd.print(s.substring(0,16));
      delay(1200); lcd.clear();
      publishMQTTStatus();
    }
  }
  else if (cmd == "wifi_del") {
    String s = doc["ssid"].as<String>();
    if (s != "" && wifiApDelete(s)) publishMQTTStatus();
    // Catatan: daftar runtime wifiMulti tidak bisa dicabut satu per satu
    // (APlistClean() bukan fungsi publik di core ESP32), jadi WiFi yang baru
    // dihapus masih mungkin dipakai sampai alat restart. Tidak berbahaya -
    // dia cuma cadangan - dan sesudah restart daftarnya sudah bersih.
  }
  else if (cmd == "wifi_scan") {
    publishWifiScan();
  }
}

// ---- Penolong endpoint HTTP lokal ----
void kirimCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void kirimDaftarWifi() {
  StaticJsonDocument<768> doc;
  doc["ok"] = true;
  doc["ssid"] = WiFi.SSID();            // WiFi yang sedang dipakai
  doc["rssi"] = WiFi.RSSI();
  doc["max"] = MAX_APS;
  JsonArray aps = doc.createNestedArray("aps");
  int n = wifiApCount();
  for (int i = 0; i < n; i++) {
    String s = wifiApSsid(i);
    if (s != "") aps.add(s);            // sekali lagi: nama saja, TANPA sandi
  }
  String out; serializeJson(doc, out);
  kirimCORS();
  server.send(200, "application/json", out);
}

void reconnectMQTT() {
  if (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    String offlinePayload = "{\"id\":\"" + networkHost + "\",\"title\":\"" + DEVICE_NAME + "\",\"online\":false,\"status\":" + (acStatus ? "true" : "false") + "}";
    
    if (client.connect(clientId.c_str(), NULL, NULL, mqtt_topic_status.c_str(), 1, true, offlinePayload.c_str())) {
      client.subscribe(mqtt_topic_cmd.c_str());
      publishMQTTStatus();
    }
  }
}

void setup() {
  Serial.begin(115200);
  preferences.begin("ac_timer", false);

  // === GENERATE KTP ABADI ESP32 ===
  networkHost = preferences.getString("dev_id", "");
  if (networkHost == "") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(); 
    delay(1000); 
    
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    if (mac == "000000000000" || mac.length() < 12) {
      randomSeed(analogRead(0) + millis());
      mac = String(random(10000, 99999)) + String(random(10000, 99999));
    }
    networkHost = "ac-" + mac;
    networkHost.toLowerCase();
    preferences.putString("dev_id", networkHost); 
  }
  
  DEVICE_NAME = preferences.getString("dev_name", "");
  if (DEVICE_NAME == "") {
    DEVICE_NAME = "AC Baru " + networkHost.substring(networkHost.length() - 4); 
    preferences.putString("dev_name", DEVICE_NAME);
  }
  wifiAPName = "Setup_" + DEVICE_NAME; 
  wifiAPName.replace(" ", "_"); 
  
  mqtt_topic_status = "agus_smart_ac/" + networkHost + "/status";
  mqtt_topic_cmd = "agus_smart_ac/" + networkHost + "/cmd";

  // === INISIALISASI HARDWARE ===
  pinMode(RELAY_ON_PIN, OUTPUT); pinMode(RELAY_OFF_PIN, OUTPUT);
  digitalWrite(RELAY_ON_PIN, LOW); digitalWrite(RELAY_OFF_PIN, LOW);
  pinMode(BUTTON_ON_PIN, INPUT_PULLUP); pinMode(BUTTON_OFF_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  if (SD_MMC.begin("/sdcard", true)) { sdCardReady = true; } 

  // === MUAT PREFERENSI DURASI & JADWAL ===
  currentDay = preferences.getInt("day", -1);
  currentMonth = preferences.getInt("month", -1);
  currentYear = preferences.getInt("year", -1);
  dailyRuntimeMinutes = preferences.getUInt("rt_day", 0);
  monthRuntimeMinutes = preferences.getUInt("rt_mon", 0);
  yearlyRuntimeMinutes = preferences.getUInt("rt_yr", 0);
  totalRuntimeMinutes = preferences.getUInt("rt_tot", 0);

  for(int i=0; i<5; i++) {
    onH[i] = preferences.getInt(("onH"+String(i)).c_str(), onH[i]);
    onM[i] = preferences.getInt(("onM"+String(i)).c_str(), onM[i]);
    offH[i] = preferences.getInt(("offH"+String(i)).c_str(), offH[i]);
    offM[i] = preferences.getInt(("offM"+String(i)).c_str(), offM[i]);
    daysMask[i] = preferences.getInt(("days"+String(i)).c_str(), 127); 
  }

  if (acStatus) digitalWrite(RELAY_ON_PIN, HIGH);
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); lcd.backlight(); keypad.begin();
  lcd.setCursor(0, 0); lcd.print(DEVICE_NAME.substring(0,16)); 
  lcd.setCursor(0, 1); lcd.print("Memuat MultiWiFi");
  delay(1500);

  // =========================================================================
  // SISTEM MULTI-WIFI ROAMING (MENGINGAT HINGGA 5 JARINGAN)
  // =========================================================================
  WiFi.mode(WIFI_STA);

  // 1. Masukkan WiFi terakhir (bawaan pabrik ESP) ke dalam daftar
  if (WiFi.SSID() != "") {
    wifiMulti.addAP(WiFi.SSID().c_str(), WiFi.psk().c_str());
  }

  // 2. Masukkan WiFi lain dari memori cadangan
  int savedAPs = preferences.getInt("saved_aps", 0);
  for (int i = 0; i < savedAPs; i++) {
    String s = preferences.getString(("ssid" + String(i)).c_str(), "");
    String p = preferences.getString(("pass" + String(i)).c_str(), "");
    if (s != "") wifiMulti.addAP(s.c_str(), p.c_str());
  }

  lcd.clear(); lcd.print("Mencari WiFi...");
  bool wifiConnected = false;
  unsigned long startWifiTimeout = millis();

  // 3. Scan & coba sambung ke salah satu WiFi di daftar selama max 15 detik
  while (millis() - startWifiTimeout < 15000) {
    if (wifiMulti.run() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(200); 
  }

  // 4. Jika semua memori WiFi mati/tidak ada, buka Portal Setup
  if (!wifiConnected) {
    lcd.clear();
    lcd.print("Buka WiFi HP:");
    lcd.setCursor(0, 1);
    lcd.print(wifiAPName.substring(0, 16));

    WiFiManager wm;
    // Blokir sistem sampai ada yang memasukkan WiFi baru
    if (!wm.startConfigPortal(wifiAPName.c_str())) {
      delay(3000);
      ESP.restart();
    }

    // Jika berhasil tersambung ke WiFi baru via HP:
    String newSSID = WiFi.SSID();
    String newPass = WiFi.psk();

    if (newSSID != "") {
      wifiMulti.addAP(newSSID.c_str(), newPass.c_str()); // Masukkan ke status aktif
      
      // Simpan ke memori abadi (Cegah duplikat)
      bool exists = false;
      for (int i = 0; i < savedAPs; i++) {
        if (preferences.getString(("ssid" + String(i)).c_str(), "") == newSSID) {
          exists = true; break;
        }
      }
      
      if (!exists) {
        if (savedAPs < 5) {
          preferences.putString(("ssid" + String(savedAPs)).c_str(), newSSID);
          preferences.putString(("pass" + String(savedAPs)).c_str(), newPass);
          preferences.putInt("saved_aps", savedAPs + 1);
        } else {
          // Jika memori 5 WiFi sudah penuh, geser dan timpa yang paling lama
          for(int i = 1; i < 5; i++) {
            preferences.putString(("ssid" + String(i-1)).c_str(), preferences.getString(("ssid" + String(i)).c_str(), ""));
            preferences.putString(("pass" + String(i-1)).c_str(), preferences.getString(("pass" + String(i)).c_str(), ""));
          }
          preferences.putString("ssid4", newSSID);
          preferences.putString("pass4", newPass);
        }
      }
    }
  }

  lcd.clear(); lcd.print("WiFi Terhubung!");

  // INTI PERBAIKAN: catat WiFi yang barusan berhasil dipakai ke daftar abadi.
  // Tanpa baris ini, WiFi bawaan/hasil captive portal cuma hidup di pengaturan
  // internal ESP dan di daftar runtime wifiMulti - "saved_aps" tetap 0, jadi
  // dashboard tidak punya apa-apa untuk ditampilkan dan daftar cadangannya
  // tidak pernah benar-benar terbentuk. Dipanggil tiap kali berhasil connect,
  // jadi WiFi mana pun yang PERNAH tersambung otomatis ikut tersimpan.
  wifiApSimpanYangAktif();

  MDNS.begin(networkHost.c_str());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  server.on("/", HTTP_GET, []() { server.send(200, "text/plain", "ESP32 AC Controller Online\nNama Perangkat: " + DEVICE_NAME); });
  server.on("/log_ac.csv", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*"); 
    if (!sdCardReady || !SD_MMC.exists("/log_ac.csv")) { server.send(404, "text/plain", "File log tidak ditemukan."); return; }
    File file = SD_MMC.open("/log_ac.csv", FILE_READ);
    server.streamFile(file, "text/csv");
    file.close();
  });

  // =======================================================================
  //  ENDPOINT WIFI CADANGAN - JALUR LOKAL (TIDAK LEWAT BROKER SAMA SEKALI)
  // =======================================================================
  // Ini jalur bawaan yang dipakai dashboard untuk menambah WiFi: sandi
  // dikirim LANGSUNG dari HP/PC ke alat lewat jaringan lokal, jadi tidak
  // pernah menyentuh broker publik. Syaratnya HP/PC harus satu jaringan
  // dengan alat pada saat menambah.
  //
  // Sandi dikirim di BODY (bukan di alamat URL) supaya tidak ikut tercatat di
  // riwayat browser / log server. Content-Type sengaja text/plain karena itu
  // termasuk "simple request" CORS - browser jadi tidak perlu kirim preflight
  // OPTIONS lebih dulu, sehingga bisa langsung jalan dari halaman dashboard.
  server.on("/wifi_list", HTTP_GET, []() { kirimDaftarWifi(); });

  server.on("/wifi_add", HTTP_OPTIONS, []() { kirimCORS(); server.send(204); });
  server.on("/wifi_add", HTTP_POST, []() {
    StaticJsonDocument<512> in;
    if (deserializeJson(in, server.arg("plain"))) {
      kirimCORS(); server.send(400, "application/json", "{\"ok\":false,\"error\":\"JSON tidak terbaca\"}");
      return;
    }
    String s = in["ssid"].as<String>();
    String p = in["pass"].as<String>();
    if (s == "") {
      kirimCORS(); server.send(400, "application/json", "{\"ok\":false,\"error\":\"Nama WiFi (SSID) kosong\"}");
      return;
    }
    int hasil = wifiApSave(s, p);
    wifiMulti.addAP(s.c_str(), p.c_str());     // langsung aktif tanpa restart
    lcd.clear(); lcd.print("WiFi cadangan+"); lcd.setCursor(0,1); lcd.print(s.substring(0,16));
    delay(1200); lcd.clear();
    publishMQTTStatus();
    kirimCORS();
    server.send(200, "application/json",
                String("{\"ok\":true,\"diperbarui\":") + (hasil == 1 ? "true" : "false") + "}");
  });

  server.on("/wifi_del", HTTP_OPTIONS, []() { kirimCORS(); server.send(204); });
  server.on("/wifi_del", HTTP_POST, []() {
    StaticJsonDocument<256> in;
    if (deserializeJson(in, server.arg("plain"))) {
      kirimCORS(); server.send(400, "application/json", "{\"ok\":false,\"error\":\"JSON tidak terbaca\"}");
      return;
    }
    String s = in["ssid"].as<String>();
    bool ok = (s != "") && wifiApDelete(s);
    if (ok) publishMQTTStatus();
    kirimCORS();
    server.send(ok ? 200 : 404, "application/json",
                ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"WiFi itu tidak ada di daftar\"}");
  });

  server.on("/wifi_scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(2048);   // heap, alasannya sama seperti publishWifiScan()
    doc["ok"] = true;
    JsonArray arr = doc.createNestedArray("nets");
    int batas = (n > 15) ? 15 : n;
    for (int i = 0; i < batas; i++) {
      if (WiFi.SSID(i) == "") continue;
      JsonObject o = arr.createNestedObject();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["lock"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    String out; serializeJson(doc, out);
    kirimCORS();
    server.send(200, "application/json", out);
  });

  server.begin();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  // Dinaikkan dari 1024: status sekarang ikut membawa SSID aktif + daftar WiFi
  // tersimpan, dan hasil pindaian WiFi bisa sampai 15 jaringan.
  client.setBufferSize(2048);
  ArduinoOTA.setHostname(networkHost.c_str()); 
  ArduinoOTA.begin();
  
  if (sdCardReady) { logToSD("SISTEM RESTART"); }
  delay(1000); lcd.clear();
}

void loop() {
  ArduinoOTA.handle(); 
  server.handleClient(); 
  
  // ================= CEK STATUS KONEKSI WIFIMULTI =================
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnectMQTT();
    client.loop();
  } else {
    // Jika WiFi tiba-tiba putus, otomatis memindai dan menyambung ke WiFi lain di daftar memori
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 5000) { // Cek tiap 5 detik agar tidak membebani ESP32
      lastWifiCheck = millis();
      if (wifiMulti.run() == WL_CONNECTED) {
        // Berhasil pindah ke WiFi cadangan -> pastikan yang ini juga tercatat,
        // supaya WiFi yang PERNAH menyelamatkan koneksi tidak hilang dari daftar.
        // Aman dipanggil berulang: kalau sudah ada di daftar, fungsinya langsung
        // keluar tanpa menulis apa pun ke flash.
        wifiApSimpanYangAktif();
      }
    }
  }

  // Tombol Reset Pabrik (Tahan 5 Detik)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (!isBootButtonPressed) {
      isBootButtonPressed = true;
      bootButtonPressTime = millis();
    } else {
      if (millis() - bootButtonPressTime >= 5000) { 
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("RESET PABRIK...");
        lcd.setCursor(0, 1); lcd.print("Menghapus Data..");
        
        WiFiManager wm;
        wm.resetSettings();   
        preferences.clear(); // Ini juga akan menghapus daftar ke-5 Multi WiFi
        
        delay(3000);
        ESP.restart(); 
      }
    }
  } else {
    isBootButtonPressed = false;
  }

  int readOn = digitalRead(BUTTON_ON_PIN);
  if (readOn != lastBtnOnState) lastDebounceOn = millis();
  if ((millis() - lastDebounceOn) > debounceDelay) {
    if (readOn != btnOnState) { btnOnState = readOn; if (btnOnState == LOW) triggerAC(true, "Tombol Manual ON"); }
  }
  lastBtnOnState = readOn;

  int readOff = digitalRead(BUTTON_OFF_PIN);
  if (readOff != lastBtnOffState) lastDebounceOff = millis();
  if ((millis() - lastDebounceOff) > debounceDelay) {
    if (readOff != btnOffState) { btnOffState = readOff; if (btnOffState == LOW) triggerAC(false, "Tombol Manual OFF"); }
  }
  lastBtnOffState = readOff;

  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 10);
  if (timeValid) sprintf(currentTimeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  unsigned long now = millis();
  if (now - lastMsg > 5000) { lastMsg = now; publishMQTTStatus(); }

  if (timeValid && timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;
    if (timeinfo.tm_yday != currentDay) {
      currentDay = timeinfo.tm_yday; dailyRuntimeMinutes = 0;
      preferences.putInt("day", currentDay); preferences.putUInt("rt_day", dailyRuntimeMinutes);
    }
    if (timeinfo.tm_mon != currentMonth && timeinfo.tm_year > 100) {
      currentMonth = timeinfo.tm_mon; monthRuntimeMinutes = 0;
      preferences.putInt("month", currentMonth); preferences.putUInt("rt_mon", monthRuntimeMinutes);
    }
    if (timeinfo.tm_year != currentYear && timeinfo.tm_year > 100) {
      currentYear = timeinfo.tm_year; yearlyRuntimeMinutes = 0;
      preferences.putInt("year", currentYear); preferences.putUInt("rt_yr", yearlyRuntimeMinutes);
    }

    if (acStatus) {
      dailyRuntimeMinutes++; monthRuntimeMinutes++; yearlyRuntimeMinutes++; totalRuntimeMinutes++;
      if (totalRuntimeMinutes % 60 == 0) {
        preferences.putUInt("rt_day", dailyRuntimeMinutes); preferences.putUInt("rt_mon", monthRuntimeMinutes);
        preferences.putUInt("rt_yr", yearlyRuntimeMinutes); preferences.putUInt("rt_tot", totalRuntimeMinutes);
      }
    }
    
    for(int i=0; i<5; i++) {
      bool isDayActive = (daysMask[i] & (1 << timeinfo.tm_wday)) != 0;
      if (isDayActive) {
        if (onH[i] != -1 && timeinfo.tm_hour == onH[i] && timeinfo.tm_min == onM[i]) triggerAC(true, "Jadwal Otomatis");
        else if (offH[i] != -1 && timeinfo.tm_hour == offH[i] && timeinfo.tm_min == offM[i]) triggerAC(false, "Jadwal Otomatis");
      }
    }
  }

  uint8_t keyIdx = keypad.getKey(); 
  char key = 'N'; 
  if (keyIdx < 16) { char keys[] = "123A456B789C*0#D"; key = keys[keyIdx]; }
  
  if (key != 'N' && key != lastKey) {
    if (keyState == 0) {
      if (key == 'A') { keyState = 1; lcd.clear(); lcd.print("Set ON Jadwal:"); lcd.setCursor(0,1); lcd.print("Pilih (1-5)"); } 
      else if (key == 'B') { keyState = 3; lcd.clear(); lcd.print("Set OFF Jadwal:"); lcd.setCursor(0,1); lcd.print("Pilih (1-5)"); }
    } else if (key == 'C') { keyState = 0; inputBuffer = ""; lcd.clear(); } 
    else {
      if ((keyState == 1 || keyState == 3) && key >= '1' && key <= '5') {
        selectedSched = (key - '0') - 1; keyState++; inputBuffer = "";
        lcd.clear(); lcd.print(keyState == 2 ? "J" + String(selectedSched+1) + " ON (HHMM):" : "J" + String(selectedSched+1) + " OFF (HHMM):");
      } else if ((keyState == 2 || keyState == 4) && key >= '0' && key <= '9') {
        inputBuffer += key; lcd.setCursor(0, 1); lcd.print(inputBuffer);
        if (inputBuffer.length() == 4) {
          int h = inputBuffer.substring(0, 2).toInt(); int m = inputBuffer.substring(2, 4).toInt();
          if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            if (keyState == 2) { onH[selectedSched] = h; onM[selectedSched] = m; preferences.putInt(("onH"+String(selectedSched)).c_str(), h); preferences.putInt(("onM"+String(selectedSched)).c_str(), m); }
            if (keyState == 4) { offH[selectedSched] = h; offM[selectedSched] = m; preferences.putInt(("offH"+String(selectedSched)).c_str(), h); preferences.putInt(("offM"+String(selectedSched)).c_str(), m); }
            lcd.clear(); lcd.print("Tersimpan!"); publishMQTTStatus(); 
          } else { 
            if (inputBuffer == "9999") {
              if (keyState == 2) { onH[selectedSched] = -1; preferences.putInt(("onH"+String(selectedSched)).c_str(), -1); }
              if (keyState == 4) { offH[selectedSched] = -1; preferences.putInt(("offH"+String(selectedSched)).c_str(), -1); }
              lcd.clear(); lcd.print("Dinonaktifkan!"); publishMQTTStatus();
            } else { lcd.clear(); lcd.print("Wkt Tdk Sah!"); }
          }
          delay(1500); keyState = 0; inputBuffer = ""; lcd.clear();
        }
      }
    }
  }
  lastKey = key;

  if (keyState == 0) {
    if (timeValid) {
      int activeIdx[5]; int activeCount = 0;
      for(int i=0; i<5; i++) { if (onH[i] != -1 || offH[i] != -1) { activeIdx[activeCount] = i; activeCount++; } }
      int totalCycles = activeCount + 1; int cycle = (millis() / 3000) % totalCycles; 
      static int lastCycle = -1; if (cycle != lastCycle) { lcd.clear(); lastCycle = cycle; }

      lcd.setCursor(0, 0); lcd.print("Jam: "); lcd.print(currentTimeStr);
      lcd.setCursor(0, 1);
      
      if (cycle == 0) { 
        char statStr[17]; sprintf(statStr, "Total: %02d J.%02d M", (int)(monthRuntimeMinutes / 60), (int)(monthRuntimeMinutes % 60)); lcd.print(statStr);
      } else { 
        int idx = activeIdx[cycle - 1]; char dayBuf[5] = "    "; 
        if (daysMask[idx] == 127) strcpy(dayBuf, "FULL");
        else if (daysMask[idx] == 62) strcpy(dayBuf, "KRJA"); 
        else if (daysMask[idx] == 65) strcpy(dayBuf, "LBUR"); 
        else if (daysMask[idx] == 0)  strcpy(dayBuf, "OFF "); 
        else {
          String d = "";
          if (daysMask[idx] & (1<<1)) d += "SN"; if (daysMask[idx] & (1<<2)) d += "SL";
          if (daysMask[idx] & (1<<3)) d += "RB"; if (daysMask[idx] & (1<<4)) d += "KM";
          if (daysMask[idx] & (1<<5)) d += "JM"; if (daysMask[idx] & (1<<6)) d += "SB";
          if (daysMask[idx] & (1<<0)) d += "MG";
          if (d.length() > 4) d = d.substring(0, 4); strcpy(dayBuf, d.c_str());
        }
        char schedStr[17];
        sprintf(schedStr, "%-4s %02d:%02d-%02d:%02d", dayBuf, onH[idx]==-1?0:onH[idx], onM[idx]==-1?0:onM[idx], offH[idx]==-1?0:offH[idx], offM[idx]==-1?0:offM[idx]);
        lcd.print(schedStr);
      }
    } else { lcd.setCursor(0, 0); lcd.print("Sync Waktu..."); }
  }
  delay(10); 
}
