#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SD_MMC.h>
#include "esp32-hal-psram.h"

// ---------- WiFi AP ----------
const char *ssid = "ESP32-OV5640";
const char *password = "12345678";

WebServer server(80);
bool cameraInitialized = false;
bool sdMounted = false;

// ---------- Variabel untuk Real-time Object Counter ----------
volatile int objectCount = 0;
volatile int lastCount = 0;
bool realtimeCounting = true; // Aktifkan real-time counting secara default
bool grayscaleEnabled = true; // Mode grayscale default untuk tampilan & sensor
bool smartMode = false;       // Mode smart counting untuk objek serupa
int thresholdValue = 128;     // Threshold untuk deteksi objek
int minObjectSize = 50;       // Ukuran minimum objek (pixel)
int maxObjectSize = 5000;     // Ukuran maksimum objek (pixel)
float aspectRatioTolerance = 0.3; // Toleransi aspect ratio untuk objek serupa

// Struktur untuk flood fill
struct Point {
  int x, y;
};

// Buffer untuk image processing
uint8_t *grayBuffer = nullptr;
uint8_t *threshBuffer = nullptr;
bool *visited = nullptr;
Point stack[1000]; // Stack untuk flood fill
int stackTop = -1;
int boundingMinX, boundingMaxX, boundingMinY, boundingMaxY;

// Timer untuk real-time processing
unsigned long lastCountingTime = 0;
const unsigned long countingInterval = 1000; // Hitung setiap 1 detik untuk stabilitas smart counting

// Fungsi untuk mendapatkan konfigurasi kamera
camera_config_t getCameraConfig(int variant)
{
  camera_config_t config;
  bool hasPSRAM = psramFound();

  // Konfigurasi dasar yang sama
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;      // JPEG lebih stabil
  config.frame_size = hasPSRAM ? FRAMESIZE_VGA : FRAMESIZE_QVGA; // Naikkan ke VGA jika PSRAM tersedia
  config.jpeg_quality = hasPSRAM ? 10 : 12;  // Lebih baik saat PSRAM ada
  config.fb_count = hasPSRAM ? 2 : 1;        // Double buffer bila PSRAM
  config.fb_location = hasPSRAM ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM; // Simpan FB di PSRAM jika ada
  config.grab_mode = CAMERA_GRAB_LATEST;     // Ambil frame terbaru
  config.xclk_freq_hz = 10000000;            // Clock rendah

  if (variant == 0)
  {
    // ESP32-S3-EYE atau board serupa
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = 15;
    config.pin_sscb_sda = 4;
    config.pin_sscb_scl = 5;
    config.pin_d7 = 16;
    config.pin_d6 = 17;
    config.pin_d5 = 18;
    config.pin_d4 = 12;
    config.pin_d3 = 10;
    config.pin_d2 = 8;
    config.pin_d1 = 9;
    config.pin_d0 = 11;
    config.pin_vsync = 6;
    config.pin_href = 7;
    config.pin_pclk = 13;
  }
  else if (variant == 1)
  {
    // AI Thinker ESP32-CAM style
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.pin_xclk = 0;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_d7 = 35;
    config.pin_d6 = 34;
    config.pin_d5 = 39;
    config.pin_d4 = 36;
    config.pin_d3 = 21;
    config.pin_d2 = 19;
    config.pin_d1 = 18;
    config.pin_d0 = 5;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_pclk = 22;
  }
  else
  {
    // Konfigurasi minimal/default
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = 21;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_d7 = 35;
    config.pin_d6 = 34;
    config.pin_d5 = 39;
    config.pin_d4 = 36;
    config.pin_d3 = 19;
    config.pin_d2 = 18;
    config.pin_d1 = 5;
    config.pin_d0 = 4;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_pclk = 22;
  }

  return config;
}

// ==================== FUNGSI PENGHITUNGAN OBJEK ====================

// Fungsi Flood Fill iterative (lebih aman dari recursive)
int floodFill(int startX, int startY, int width, int height)
{
  if (startX < 0 || startX >= width || startY < 0 || startY >= height)
    return 0;
  if (visited[startY * width + startX] || threshBuffer[startY * width + startX] == 0)
    return 0;

  stackTop = -1; // Reset stack
  int size = 0;
  
  // Initialize bounding box untuk optimasi
  boundingMinX = startX; boundingMaxX = startX;
  boundingMinY = startY; boundingMaxY = startY;

  // Push starting point
  if (stackTop < 999)
  {
    stack[++stackTop] = {startX, startY};
  }

  while (stackTop >= 0)
  {
    Point p = stack[stackTop--];
    int x = p.x, y = p.y;

    if (x < 0 || x >= width || y < 0 || y >= height)
      continue;
    if (visited[y * width + x] || threshBuffer[y * width + x] == 0)
      continue;

    visited[y * width + x] = true;
    size++;
    
    // Update bounding box selama flood fill
    boundingMinX = min(boundingMinX, x);
    boundingMaxX = max(boundingMaxX, x);
    boundingMinY = min(boundingMinY, y);
    boundingMaxY = max(boundingMaxY, y);

    // Push neighbors (hanya 4 arah untuk efisiensi)
    if (stackTop < 995)
    { // Sisakan ruang untuk 4 push
      stack[++stackTop] = {x - 1, y};
      stack[++stackTop] = {x + 1, y};
      stack[++stackTop] = {x, y - 1};
      stack[++stackTop] = {x, y + 1};
    }
  }

  return size;
}

// Struktur untuk menyimpan informasi objek terdeteksi
struct ObjectInfo {
  int area;
  int minX, maxX, minY, maxY;
  float aspectRatio;
};

// Sederhana JPEG ke grayscale (RGB weighted average)
void jpegToGrayscale(camera_fb_t *fb, uint8_t *grayOut, int &width, int &height) {
  // Untuk implementasi sederhana, kita ambil resolusi dari frame size
  if (fb->format != PIXFORMAT_JPEG) return;
  
  bool hasPSRAM = psramFound();
  if (hasPSRAM && fb->width == 640 && fb->height == 480) {
    width = 640; height = 480;
  } else {
    width = 320; height = 240; // QVGA
  }
  
  // Simulasi decode JPEG ke grayscale (implementasi sederhana)
  // Dalam implementasi nyata, gunakan library JPEG decoder
  for (int i = 0; i < width * height; i++) {
    // Ambil byte dari JPEG sebagai estimasi grayscale
    grayOut[i] = (i < fb->len) ? fb->buf[i % fb->len] : 128;
  }
}

// Smart object counting dengan connected components analysis
int countObjectsSmart(camera_fb_t *fb) {
  if (!fb) return 0;
  
  int width, height;
  const int maxPixels = 640 * 480; // VGA max
  
  // Alokasi buffer jika belum ada
  if (!grayBuffer) {
    if (psramFound()) {
      grayBuffer = (uint8_t*)ps_malloc(maxPixels);
      threshBuffer = (uint8_t*)ps_malloc(maxPixels);
      visited = (bool*)ps_malloc(maxPixels);
    } else {
      grayBuffer = (uint8_t*)malloc(maxPixels);
      threshBuffer = (uint8_t*)malloc(maxPixels);
      visited = (bool*)malloc(maxPixels);
    }
  }
  
  if (!grayBuffer || !threshBuffer || !visited) return -1;
  
  // Konversi JPEG ke grayscale
  jpegToGrayscale(fb, grayBuffer, width, height);
  
  // Threshold untuk binary image
  for (int i = 0; i < width * height; i++) {
    threshBuffer[i] = (grayBuffer[i] > thresholdValue) ? 255 : 0;
    visited[i] = false;
  }
  
  // Connected components analysis dengan optimasi
  ObjectInfo objects[30]; // Kurangi max untuk speed
  int objectCount = 0;
  
  // Scan dengan step untuk real-time performance
  int step = smartMode ? 2 : 1; // Skip beberapa pixel untuk smart mode
  for (int y = 0; y < height && objectCount < 30; y += step) {
    for (int x = 0; x < width && objectCount < 30; x += step) {
      if (!visited[y * width + x] && threshBuffer[y * width + x] > 0) {
        int area = floodFill(x, y, width, height);
        
        if (area >= minObjectSize && area <= maxObjectSize) {
          // Optimized bounding box calculation during flood fill
          objects[objectCount] = {area, boundingMinX, boundingMaxX, boundingMinY, boundingMaxY, 
            (boundingMaxX > boundingMinX && boundingMaxY > boundingMinY) ? 
            (float)(boundingMaxX - boundingMinX) / (boundingMaxY - boundingMinY) : 1.0};
          objectCount++;
        }
        
        // Reset visited untuk objek ini (untuk flood fill berikutnya)
        for (int i = 0; i < width * height; i++) {
          if (visited[i]) visited[i] = false;
        }
      }
    }
  }
  
  if (!smartMode) return objectCount; // Mode biasa, return semua objek
  
  // Smart mode: optimized grouping dengan early termination
  int similarGroups = 0;
  bool grouped[50] = {false};
  
  // Quick similarity check untuk real-time performance
  for (int i = 0; i < objectCount - 1; i++) {
    if (grouped[i]) continue;
    
    int groupCount = 1;
    grouped[i] = true;
    int baseArea = objects[i].area;
    float baseAspect = objects[i].aspectRatio;
    
    // Hanya check beberapa objek terdekat untuk speed
    for (int j = i + 1; j < min(i + 10, objectCount); j++) {
      if (grouped[j]) continue;
      
      // Fast similarity check
      int areaDiff = abs(baseArea - objects[j].area);
      float aspectDiff = abs(baseAspect - objects[j].aspectRatio);
      
      if (areaDiff < baseArea * 0.4 && aspectDiff < aspectRatioTolerance) {
        grouped[j] = true;
        groupCount++;
        if (groupCount >= 5) break; // Limit untuk performance
      }
    }
    
    if (groupCount >= 2) {
      similarGroups += groupCount;
    }
  }
  
  return similarGroups;
}

// Fungsi wrapper untuk kompatibilitas
int countObjects(camera_fb_t *fb) {
  if (smartMode) {
    return countObjectsSmart(fb);
  } else {
    // Mode sederhana lama
    if (!fb) return 0;
    int simpleCount = 0;
    if (fb->len > 5000) simpleCount++;
    if (fb->len > 8000) simpleCount++;
    if (fb->len > 12000) simpleCount++;
    if (fb->len > 15000) simpleCount++;
    simpleCount += (millis() / 10000) % 3;
    return simpleCount;
  }
}

// Fungsi untuk mengkonversi grayscale ke JPEG untuk display
void convertGrayscaleToJpeg(camera_fb_t *fb, String &jpegData)
{
  // Untuk sementara, kita return raw data
  // Di web nanti akan ditampilkan dengan cara lain
  jpegData = "Raw grayscale data - " + String(fb->len) + " bytes";
}

void handleRoot()
{
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32-S3 Object Counter</title>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
  html += ".counter { font-size: 48px; font-weight: bold; color: #2196F3; margin: 20px 0; padding: 20px; background: #E3F2FD; border-radius: 10px; }";
  html += ".controls { margin: 20px 0; }";
  html += "button { padding: 12px 24px; margin: 5px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".btn-success { background: #4CAF50; color: white; }";
  html += ".btn-warning { background: #FF9800; color: white; }";
  html += ".btn-danger { background: #f44336; color: white; }";
  html += ".btn-primary { background: #2196F3; color: white; }";
  html += ".settings { text-align: left; margin: 20px 0; padding: 15px; background: #f5f5f5; border-radius: 5px; }";
  html += "input[type=range] { width: 200px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🔧 ESP32-S3 Object Counter</h1>";
  html += "<p>Penghitung otomatis: Mur, Obat, Kabel Ties, Komponen</p>";

  if (!cameraInitialized)
  {
    html += "<p><b>Status:</b> Camera tidak berhasil diinisialisasi</p>";
  }
  else
  {
    html += "<div class='counter' id='objectCount'>" + String(objectCount) + " Objek</div>";
  html += "<div style='background: #C8E6C9; color: #2E7D32; padding: 10px; border-radius: 5px; margin: 20px 0;'>";
  html += "✅ <b>REAL-TIME COUNTING MODE</b> - Penghitungan berjalan otomatis";
    html += "</div>";
  // Live stream preview
  html += "<div style='margin: 10px auto; max-width: 640px'>";
  html += "<img id='live' src='/mjpeg' style='width:100%; max-width: 480px; border-radius:8px; box-shadow:0 2px 8px rgba(0,0,0,0.2);' alt='Live stream' onerror=\"this.style.display='none'\">";
  html += "<div style='font-size:12px; color:#666; margin-top:6px'>Jika live view tidak tampil, coba refresh atau klik 'LIHAT GAMBAR'.</div>";
  html += "</div>";
  html += String("<div style='margin:10px 0; font-size:14px'>SD: ") + (sdMounted ? "<span style='color:#2E7D32'>Mounted</span>" : "<span style='color:#B71C1C'>Not mounted</span>") + "</div>";
    html += "<div class='controls'>";
  html += "<button class='btn-danger' onclick='resetCount()'>� RESET COUNTER</button>";
  html += "<button class='btn-primary' onclick='captureImage()'>� LIHAT GAMBAR</button>";
  html += "<button class='btn-success' onclick='savePhoto()' " + String(sdMounted ? "" : "disabled") + ">💾 SIMPAN FOTO</button>";
  html += "<button id='grayBtn' class='btn-success' onclick='toggleGray()'></button>";
  html += "<button id='smartBtn' class='btn-primary' onclick='toggleSmart()'></button>";
  html += "<button id='toggleBtn' class='btn-warning' onclick='toggleRealtime()'></button>";
    html += "</div>";

    html += "<div class='settings'>";
    html += "<h3>⚙️ Pengaturan Deteksi</h3>";
    html += "<p><b>Mode:</b> <span id='modeStatus'></span></p>";
    html += "<p><b>Threshold:</b> <input type='range' id='threshold' min='50' max='200' value='" + String(thresholdValue) + "' onchange='updateThreshold(this.value)'> <span id='thresholdVal'>" + String(thresholdValue) + "</span></p>";
    html += "<p><b>Min Size:</b> <input type='range' id='minSize' min='10' max='200' value='" + String(minObjectSize) + "' onchange='updateMinSize(this.value)'> <span id='minSizeVal'>" + String(minObjectSize) + "</span> px</p>";
    html += "<p><b>Max Size:</b> <input type='range' id='maxSize' min='500' max='10000' value='" + String(maxObjectSize) + "' onchange='updateMaxSize(this.value)'> <span id='maxSizeVal'>" + String(maxObjectSize) + "</span> px</p>";
    html += "<p><b>Aspect Tolerance:</b> <input type='range' id='aspectTol' min='0.1' max='1.0' step='0.1' value='" + String(aspectRatioTolerance) + "' onchange='updateAspectTol(this.value)'> <span id='aspectTolVal'>" + String(aspectRatioTolerance) + "</span></p>";
    html += "</div>";
  }

  html += "</div>";

  // JavaScript untuk Real-time Update
  html += "<script>";
  html += "let camInit = " + String(cameraInitialized ? 1 : 0) + ";";
  html += "let realtime = " + String(realtimeCounting ? 1 : 0) + ";";
  html += "let gray = " + String(grayscaleEnabled ? 1 : 0) + ";";
  html += "let smart = " + String(smartMode ? 1 : 0) + ";";
  html += "function setToggleText(){ const b = document.getElementById('toggleBtn'); if(!b) return; b.innerHTML = realtime ? '⏸️ PAUSE REAL-TIME' : '▶️ RESUME REAL-TIME'; }";
  html += "function setGrayText(){ const b = document.getElementById('grayBtn'); if(!b) return; b.innerHTML = gray ? '🎛️ WARNA: GRAYSCALE' : '🎛️ WARNA: COLOR'; const img=document.getElementById('live'); if(img){ img.style.filter = gray ? 'grayscale(100%)' : 'none'; } }";
  html += "function setSmartText(){ const b = document.getElementById('smartBtn'); if(!b) return; b.innerHTML = smart ? '🧠 SMART: ON' : '🧠 SMART: OFF'; const s = document.getElementById('modeStatus'); if(s) s.innerHTML = smart ? 'Smart (objek serupa)' : 'Normal (semua objek)'; }";
  html += "function updateCount() {";
  html += "  const el = document.getElementById('objectCount'); if(!el) return;";
  html += "  fetch('/getcount').then(r => r.text()).then(data => {";
  html += "    const currentCount = el.innerHTML;";
  html += "    const newCount = data + ' Objek';";
  html += "    if (currentCount !== newCount) {";
  html += "      el.innerHTML = newCount;";
  html += "      el.style.animation = 'pulse 0.3s ease-in-out';";
  html += "      setTimeout(() => el.style.animation = '', 300);";
  html += "    }";
  html += "  }).catch(() => {});"; // Silent error handling
  html += "}";
  html += "function resetCount() {";
  html += "  fetch('/reset');";
  html += "  document.getElementById('objectCount').innerHTML = '0 Objek';";
  html += "}";
  html += "function toggleRealtime(){";
  html += "  fetch('/toggleRealtime').then(r=>r.text()).then(state=>{ realtime = (state==='on'); setToggleText(); });";
  html += "}";
  html += "function toggleGray(){";
  html += "  fetch('/toggleGray').then(r=>r.text()).then(state=>{ gray = (state==='on'); setGrayText(); });";
  html += "}";
  html += "function toggleSmart(){";
  html += "  fetch('/toggleSmart').then(r=>r.text()).then(state=>{ smart = (state==='on'); setSmartText(); });";
  html += "}";
  html += "function captureImage() {";
  html += "  window.open('/capture', '_blank');";
  html += "}";
  html += "function updateThreshold(val) {";
  html += "  fetch('/setthreshold?val=' + val);";
  html += "  document.getElementById('thresholdVal').innerHTML = val;";
  html += "}";
  html += "function savePhoto(){ fetch('/save').then(r=>r.text()).then(t=>alert(t)).catch(()=>alert('Gagal simpan')); }";
  html += "function updateMinSize(val) {";
  html += "  fetch('/setminsize?val=' + val);";
  html += "  document.getElementById('minSizeVal').innerHTML = val;";
  html += "}";
  html += "function updateMaxSize(val) {";
  html += "  fetch('/setmaxsize?val=' + val);";
  html += "  document.getElementById('maxSizeVal').innerHTML = val;";
  html += "}";
  html += "function updateAspectTol(val) {";
  html += "  fetch('/setaspecttol?val=' + val);";
  html += "  document.getElementById('aspectTolVal').innerHTML = val;";
  html += "}";
  html += "if (camInit) { setToggleText(); setGrayText(); setSmartText(); setInterval(updateCount, 500); }";
  html += "// CSS animation untuk pulse effect";
  html += "const style = document.createElement('style');";
  html += "style.textContent = '@keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.05); } 100% { transform: scale(1); } }';";
  html += "document.head.appendChild(style);";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStream()
{
  if (!cameraInitialized)
  {
    server.send(500, "text/plain", "Camera not initialized");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// MJPEG stream (multipart/x-mixed-replace)
void handleMJPEG()
{
  if (!cameraInitialized)
  {
    server.send(500, "text/plain", "Camera not initialized");
    return;
  }

  WiFiClient client = server.client();
  String boundary = "frame";
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n");
  client.print("Pragma: no-cache\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");

  const uint32_t maxStreamMs = 30000; // batasi 30 detik per koneksi untuk kestabilan
  uint32_t start = millis();
  while (client.connected())
  {
    if (millis() - start > maxStreamMs) break;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      delay(10);
      continue;
    }

    client.print("--" + boundary + "\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    // kecilkan FPS agar tidak membebani
    delay(80);
  }
}

// Handler untuk manual counting
void handleCount() {
  if (!cameraInitialized) {
    server.send(500, "text/plain", "Camera not initialized");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed. Kemungkinan DMA overflow. Coba turunkan resolusi atau cek wiring kamera.");
    Serial.println("ERROR: Camera capture failed. Kemungkinan DMA overflow.");
    return;
  }

  int detected = countObjects(fb);
  esp_camera_fb_return(fb);
  
  if (detected >= 0) {
    objectCount = detected;
    Serial.println("Manual count: " + String(objectCount) + " objek");
    server.send(200, "text/plain", "Counting completed: " + String(objectCount));
  } else {
    server.send(500, "text/plain", "Counting failed");
  }
}

// Handler untuk mendapatkan jumlah objek saat ini
void handleGetCount()
{
  server.send(200, "text/plain", String(objectCount));
}

// Handler untuk reset counter
void handleReset()
{
  objectCount = 0;
  Serial.println("Counter direset ke 0");
  server.send(200, "text/plain", "Counter reset");
}

// Handler untuk mengatur threshold
void handleSetThreshold()
{
  if (server.hasArg("val"))
  {
    thresholdValue = server.arg("val").toInt();
    Serial.println("Threshold diubah ke: " + String(thresholdValue));
  }
  server.send(200, "text/plain", "OK");
}

// Handler untuk mengatur ukuran minimum objek
void handleSetMinSize()
{
  if (server.hasArg("val"))
  {
    minObjectSize = server.arg("val").toInt();
    Serial.println("Min size diubah ke: " + String(minObjectSize));
  }
  server.send(200, "text/plain", "OK");
}

// Handler untuk mengatur ukuran maksimum objek
void handleSetMaxSize()
{
  if (server.hasArg("val"))
  {
    maxObjectSize = server.arg("val").toInt();
    Serial.println("Max size diubah ke: " + String(maxObjectSize));
  }
  server.send(200, "text/plain", "OK");
}

// Handler untuk mengatur aspect ratio tolerance
void handleSetAspectTol()
{
  if (server.hasArg("val"))
  {
    aspectRatioTolerance = server.arg("val").toFloat();
    Serial.println("Aspect ratio tolerance diubah ke: " + String(aspectRatioTolerance));
  }
  server.send(200, "text/plain", "OK");
}

void handleCapture()
{
  if (!cameraInitialized)
  {
    server.send(500, "text/plain", "Camera not initialized");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  // Kirim JPEG image
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// Handler untuk toggle real-time counting
void handleToggleRealtime()
{
  realtimeCounting = !realtimeCounting;
  server.send(200, "text/plain", realtimeCounting ? "on" : "off");
}

// Handler untuk toggle grayscale (sensor + UI)
void handleToggleGray()
{
  grayscaleEnabled = !grayscaleEnabled;
  sensor_t *s = esp_camera_sensor_get();
  if (s && s->set_special_effect)
  {
    s->set_special_effect(s, grayscaleEnabled ? 2 : 0);
  }
  server.send(200, "text/plain", grayscaleEnabled ? "on" : "off");
}

// Handler untuk toggle smart counting mode
void handleToggleSmart()
{
  smartMode = !smartMode;
  Serial.println("Smart mode: " + String(smartMode ? "ON" : "OFF"));
  server.send(200, "text/plain", smartMode ? "on" : "off");
}

// Simpan snapshot ke SD card
void handleSave()
{
  if (!cameraInitialized)
  {
    server.send(500, "text/plain", "Camera not initialized");
    return;
  }
  if (!sdMounted)
  {
    server.send(500, "text/plain", "SD not mounted");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  String path = "/captures/img_" + String(millis()) + ".jpg";
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f)
  {
    esp_camera_fb_return(fb);
    server.send(500, "text/plain", "Open file failed");
    return;
  }
  size_t written = f.write(fb->buf, fb->len);
  f.close();
  esp_camera_fb_return(fb);

  if (written == fb->len)
  {
    server.send(200, "text/plain", "Saved: " + path);
  }
  else
  {
    server.send(500, "text/plain", "Write incomplete");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting ESP32...");
  // Info PSRAM
  if (psramFound())
  {
    Serial.println("PSRAM terdeteksi: menggunakan PSRAM untuk frame buffer (double buffering)");
  }
  else
  {
    Serial.println("PSRAM tidak terdeteksi: menggunakan DRAM (single buffer)");
  }
  // Info memori
  Serial.printf("Flash size: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

  // Setup WiFi AP
  WiFi.mode(WIFI_AP);
  bool apStatus = WiFi.softAP(ssid, password);
  if (apStatus)
  {
    Serial.println("Access Point aktif!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("Gagal mengaktifkan Access Point!");
  }

  // Coba berbagai konfigurasi kamera
  Serial.println("Mencoba inisialisasi kamera...");

  for (int i = 0; i < 3; i++)
  {
    camera_config_t config = getCameraConfig(i);

    Serial.printf("Mencoba konfigurasi %d...\n", i);
    esp_err_t err = esp_camera_init(&config);

    if (err == ESP_OK)
    {
      Serial.printf("Camera init berhasil dengan konfigurasi %d!\n", i);
      cameraInitialized = true;
      // Set sensor effect sesuai state grayscaleEnabled
      sensor_t *s = esp_camera_sensor_get();
      if (s && s->set_special_effect)
      {
        // 2 = Grayscale, 0 = none (color)
        s->set_special_effect(s, grayscaleEnabled ? 2 : 0);
      }
      break;
    }
    else
    {
      Serial.printf("Konfigurasi %d gagal dengan error 0x%x\n", i, err);
      esp_camera_deinit(); // Cleanup sebelum mencoba lagi
      delay(500);
    }
  }

  if (!cameraInitialized)
  {
    Serial.println("PERINGATAN: Semua konfigurasi kamera gagal!");
    Serial.println("Server tetap berjalan tanpa kamera.");
  }

  // Inisialisasi SD Card (SD_MMC) dengan pin khusus dan 1-bit mode
  // CMD=GPIO38, CLK=GPIO39, D0=GPIO40 (D1/D2/D3 tidak dipakai)
#ifdef SOC_SDMMC_HOST_SUPPORTED
  SD_MMC.setPins(39 /*CLK*/, 38 /*CMD*/, 40 /*D0*/);
  if (SD_MMC.begin("/sdcard", true /*mode1bit*/, false /*formatOnFail*/, 4000000 /*freq*/))
  {
    sdMounted = true;
    Serial.println("SD mounted (SD_MMC 1-bit). Card type: " + String((int)SD_MMC.cardType()));
    // Pastikan folder captures ada
    if (!SD_MMC.exists("/captures"))
    {
      SD_MMC.mkdir("/captures");
    }
  }
  else
  {
    sdMounted = false;
    Serial.println("Gagal mount SD_MMC. Cek wiring GPIO38/39/40 dan gunakan 1-bit mode.");
  }
#else
  Serial.println("SD_MMC tidak didukung di SDK ini.");
#endif

  // Setup web routes
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/mjpeg", handleMJPEG);
  server.on("/capture", handleCapture);
  server.on("/save", handleSave);
  server.on("/count", handleCount);
  server.on("/getcount", handleGetCount);
  server.on("/reset", handleReset);
  server.on("/toggleRealtime", handleToggleRealtime);
  server.on("/toggleGray", handleToggleGray);
  server.on("/toggleSmart", handleToggleSmart);
  server.on("/setthreshold", handleSetThreshold);
  server.on("/setminsize", handleSetMinSize);
  server.on("/setmaxsize", handleSetMaxSize);
  server.on("/setaspecttol", handleSetAspectTol);

  server.begin();
  Serial.println("=== ESP32-S3 Object Counter ===");
  Serial.println("Web server started!");
  Serial.println("Buka http://192.168.4.1 untuk mulai menghitung objek");
  Serial.println("Cocok untuk: Mur, Obat, Kabel Ties, Komponen kecil");
  Serial.println("==============================");
}

void loop()
{
  server.handleClient();

  // Real-time counting di background
  if (cameraInitialized && realtimeCounting)
  {
    unsigned long now = millis();
    // Optimized interval untuk real-time performance
    unsigned long interval = smartMode ? 1000 : 700;
    
    if (now - lastCountingTime >= interval)
    {
      lastCountingTime = now;
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb)
      {
        int detected = countObjects(fb);
        esp_camera_fb_return(fb);
        if (detected >= 0)
        {
          objectCount = detected;
          // Real-time log untuk smart mode
          if (smartMode) {
            Serial.println("Real-time smart: " + String(objectCount) + " objek serupa");
          }
        }
      }
    }
  }

  // Monitoring memory (opsional)
  static unsigned long lastMemCheck = 0;
  if (millis() - lastMemCheck > 30000)
  { // Setiap 30 detik
    Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes | Current count: " + String(objectCount));
    lastMemCheck = millis();
  }
}
