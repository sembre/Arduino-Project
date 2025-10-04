// ESP32_S3_OV5640_Counter.ino
// Versi: 1.3 - Fixed pin mapping dan WiFi AP untuk ESP32S3
// Fungsi: Web AP, snapshot, object counting dengan OV5640 camera

#include "esp_camera.h"
#include "WiFi.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"

// ---------------- CONFIG ----------------
const char *ap_ssid = "ESP32_CAMERA_COUNT";
const char *ap_pass = "12345678"; // minimal 8 karakter

// Default processing parameters
int g_threshold = 70;  // 0-255, ambang biner
int g_minArea = 30;    // piksel, minimal blob dihitung
int g_maxArea = 20000; // piksel, maksimal blob dihitung

// ==== Pin kamera ESP32-S3 + OV5640 ====
// Pin mapping sesuai dengan board ESP32S3 + OV5640 Anda
#define PWDN_GPIO_NUM -1  // Power down (not used)
#define RESET_GPIO_NUM -1 // Reset (not used)
#define XCLK_GPIO_NUM 15  // External clock
#define SIOD_GPIO_NUM 4   // I2C Data (SDA)
#define SIOC_GPIO_NUM 5   // I2C Clock (SCL)

#define Y9_GPIO_NUM 16   // Data bit 9
#define Y8_GPIO_NUM 17   // Data bit 8
#define Y7_GPIO_NUM 18   // Data bit 7
#define Y6_GPIO_NUM 12   // Data bit 6
#define Y5_GPIO_NUM 10   // Data bit 5
#define Y4_GPIO_NUM 8    // Data bit 4
#define Y3_GPIO_NUM 9    // Data bit 3
#define Y2_GPIO_NUM 11   // Data bit 2
#define VSYNC_GPIO_NUM 6 // Vertical sync
#define HREF_GPIO_NUM 7  // Horizontal reference
#define PCLK_GPIO_NUM 13 // Pixel clock

// Resolusi untuk penghitungan (cepat)
#define PROC_FRAMESIZE FRAMESIZE_QQVGA // 160x120
#define PROC_PIXELFORMAT PIXFORMAT_RGB565

httpd_handle_t camera_httpd = NULL;

// Function declarations
void startCameraServer();
bool initCamera();
bool setupWiFiAP();

// Minimal helper: convert RGB565 -> grayscale
static inline uint8_t rgb565_to_gray(uint16_t c)
{
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >> 5) & 0x3F;
    uint8_t b = c & 0x1F;
    // expand to 8-bit
    uint8_t R = (r * 255) / 31;
    uint8_t G = (g * 255) / 63;
    uint8_t B = (b * 255) / 31;
    return (uint8_t)((R * 299 + G * 587 + B * 114) / 1000);
}

// Simple connected-component labeling via BFS; returns vector of areas
#include <vector>
struct Blob
{
    int area;
    int minx, miny, maxx, maxy;
};

std::vector<Blob> detect_blobs(uint8_t *gray, int w, int h, int threshold, int minArea, int maxArea)
{
    std::vector<Blob> blobs;
    // labels: 0 unlabeled, 1 labeled
    uint8_t *vis = (uint8_t *)malloc(w * h);
    if (!vis)
        return blobs;
    memset(vis, 0, w * h);

    // stack for BFS (store index)
    int maxStack = w * h;
    int *stack = (int *)malloc(sizeof(int) * maxStack);
    if (!stack)
    {
        free(vis);
        return blobs;
    }

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (vis[idx])
                continue;
            if (gray[idx] < threshold)
            { // foreground = dark (adjust if background dark)
                // BFS
                int sp = 0;
                stack[sp++] = idx;
                vis[idx] = 1;
                int area = 0;
                int minx = x, miny = y, maxx = x, maxy = y;
                while (sp > 0)
                {
                    int cur = stack[--sp];
                    int cy = cur / w;
                    int cx = cur % w;
                    area++;
                    if (cx < minx)
                        minx = cx;
                    if (cx > maxx)
                        maxx = cx;
                    if (cy < miny)
                        miny = cy;
                    if (cy > maxy)
                        maxy = cy;
                    // 4-neighbors
                    const int dx[4] = {-1, 1, 0, 0};
                    const int dy[4] = {0, 0, -1, 1};
                    for (int k = 0; k < 4; k++)
                    {
                        int nx = cx + dx[k];
                        int ny = cy + dy[k];
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                            continue;
                        int nidx = ny * w + nx;
                        if (vis[nidx])
                            continue;
                        if (gray[nidx] < threshold)
                        {
                            vis[nidx] = 1;
                            stack[sp++] = nidx;
                        }
                        else
                        {
                            vis[nidx] = 1; // mark background too so we skip later (optional)
                        }
                    }
                }
                if (area >= minArea && area <= maxArea)
                {
                    Blob b;
                    b.area = area;
                    b.minx = minx;
                    b.miny = miny;
                    b.maxx = maxx;
                    b.maxy = maxy;
                    blobs.push_back(b);
                }
            }
            else
            {
                vis[idx] = 1; // background
            }
        }
    }
    free(stack);
    free(vis);
    return blobs;
}

// Serve main HTML UI (futuristic, lightweight)
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Object Counter</title>
<style>
  :root{--bg:#0b0f1a;--card:#0f1724;--accent:#00ffd5;--muted:#98a0b3}
  body{margin:0;font-family:system-ui,Segoe UI,Roboto,Helvetica,Arial;background:linear-gradient(180deg,#02040a,#071026);color:#e6eef8;display:flex;flex-direction:column;align-items:center;gap:16px;padding:18px}
  .card{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.04);backdrop-filter:blur(6px);padding:14px;border-radius:12px;max-width:960px;width:100%}
  h1{margin:0;font-size:20px;color:var(--accent)}
  .row{display:flex;gap:12px;flex-wrap:wrap}
  img#snap{border-radius:8px;border:1px solid rgba(255,255,255,0.05);width:320px;height:auto;display:block;background:#000}
  .controls{display:flex;flex-direction:column;gap:8px;min-width:220px}
  label{font-size:12px;color:var(--muted)}
  input[type=range]{width:100%}
  button{background:linear-gradient(90deg,var(--accent),#6bf0ff);border:none;padding:8px 12px;border-radius:8px;color:#001;cursor:pointer;font-weight:600}
  .status{font-size:14px;color:#c8d6e5}
  .result{font-size:32px;color:var(--accent);font-weight:700;margin-top:6px}
  .small{font-size:12px;color:var(--muted)}
  footer{font-size:12px;color:var(--muted);margin-top:8px}
</style>
</head>
<body>
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <h1>ESP32 Camera Counter</h1>
      <div class="small">Mode: AP (SSID: <b>ESP32_CAMERA_COUNT</b>)</div>
    </div>
    <div class="row" style="margin-top:12px">
      <div>
        <img id="snap" src="/snapshot?ts=0">
        <div style="display:flex;gap:8px;margin-top:8px">
          <button onclick="refresh()">Refresh</button>
          <button onclick="doCount()">Count</button>
          <button onclick="save()">Save Snapshot</button>
        </div>
      </div>
      <div class="controls">
        <label>Threshold: <span id="thVal">70</span></label>
        <input id="th" type="range" min="10" max="250" value="70" oninput="document.getElementById('thVal').innerText=this.value">
        <label>Min Area (px): <span id="minVal">30</span></label>
        <input id="minA" type="range" min="1" max="1000" value="30" oninput="document.getElementById('minVal').innerText=this.value">
        <label>Max Area (px): <span id="maxVal">20000</span></label>
        <input id="maxA" type="range" min="100" max="50000" value="20000" oninput="document.getElementById('maxVal').innerText=this.value">
        <div style="margin-top:6px">
          <div class="small">Detected:</div>
          <div id="count" class="result">—</div>
          <div id="sizes" class="small"></div>
        </div>
      </div>
    </div>
    <div style="margin-top:12px" class="small">Tips: Letakkan objek di permukaan polos kontras tinggi. Sesuaikan threshold dan min/max area untuk tipe barang berbeda (mur, paku, konektor).</div>
  </div>
  <footer>ESP32S3 OV5640 Object Counter • Pin mapping verified</footer>

<script>
function refresh(){ document.getElementById('snap').src = '/snapshot?ts='+Date.now(); }
async function doCount(){
  const th = document.getElementById('th').value;
  const minA = document.getElementById('minA').value;
  const maxA = document.getElementById('maxA').value;
  const res = await fetch(`/count?threshold=${th}&min=${minA}&max=${maxA}`);
  const j = await res.json();
  document.getElementById('count').innerText = j.count;
  document.getElementById('sizes').innerText = j.sizes.join(', ');
}
async function save(){
  const res = await fetch('/save_snapshot');
  const j = await res.json();
  alert(j.message);
}
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawliteral";

// --------- camera init ----------
bool initCamera()
{
    Serial.println("🔧 Initializing camera with verified pin mapping...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;         // GPIO 11
    config.pin_d1 = Y3_GPIO_NUM;         // GPIO 9
    config.pin_d2 = Y4_GPIO_NUM;         // GPIO 8
    config.pin_d3 = Y5_GPIO_NUM;         // GPIO 10
    config.pin_d4 = Y6_GPIO_NUM;         // GPIO 12
    config.pin_d5 = Y7_GPIO_NUM;         // GPIO 18
    config.pin_d6 = Y8_GPIO_NUM;         // GPIO 17
    config.pin_d7 = Y9_GPIO_NUM;         // GPIO 16
    config.pin_xclk = XCLK_GPIO_NUM;     // GPIO 15
    config.pin_pclk = PCLK_GPIO_NUM;     // GPIO 13
    config.pin_vsync = VSYNC_GPIO_NUM;   // GPIO 6
    config.pin_href = HREF_GPIO_NUM;     // GPIO 7
    config.pin_sscb_sda = SIOD_GPIO_NUM; // GPIO 4
    config.pin_sscb_scl = SIOC_GPIO_NUM; // GPIO 5
    config.pin_pwdn = PWDN_GPIO_NUM;     // -1 (not used)
    config.pin_reset = RESET_GPIO_NUM;   // -1 (not used)

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PROC_PIXELFORMAT; // RGB565
    config.frame_size = PROC_FRAMESIZE;     // QQVGA 160x120
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("❌ Camera init failed with error 0x%x\n", err);
        return false;
    }

    Serial.println("✅ Camera initialized successfully!");
    return true;
}

// Setup WiFi AP dengan multiple fallback methods
bool setupWiFiAP()
{
    Serial.println("🔧 Setting up WiFi AP...");

    // Reset WiFi to clean state
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_AP);
    delay(500);

    bool success = false;

    // Method 1: Standard AP setup
    Serial.println("📡 Trying standard AP setup...");
    success = WiFi.softAP(ap_ssid, ap_pass);
    delay(2000);

    if (!success)
    {
        Serial.println("📡 Trying with explicit channel 1...");
        success = WiFi.softAP(ap_ssid, ap_pass, 1, 0, 4);
        delay(2000);
    }

    if (!success)
    {
        Serial.println("📡 Trying with channel 6...");
        success = WiFi.softAP(ap_ssid, ap_pass, 6, 0, 4);
        delay(2000);
    }

    if (!success)
    {
        Serial.println("📡 Trying without password...");
        success = WiFi.softAP(ap_ssid);
        delay(2000);
    }

    if (success)
    {
        IPAddress IP = WiFi.softAPIP();
        Serial.println("✅ WiFi AP started successfully!");
        Serial.printf("📶 SSID: %s\n", ap_ssid);
        Serial.printf("🔑 Password: %s\n", ap_pass);
        Serial.printf("🌐 IP: %s\n", IP.toString().c_str());
        Serial.printf("📱 Connect and visit: http://%s\n", IP.toString().c_str());
        return true;
    }
    else
    {
        Serial.println("❌ All WiFi AP methods failed!");
        return false;
    }
}

// HTTP handlers
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t snapshot_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int w = fb->width;
    int h = fb->height;
    int rowSize = ((w * 3 + 3) / 4) * 4;
    int imgSize = rowSize * h;
    int fileSize = 54 + imgSize;

    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=snapshot.bmp");

    // BMP header
    uint8_t bmpHeader[54] = {0};
    bmpHeader[0] = 'B';
    bmpHeader[1] = 'M';
    bmpHeader[2] = fileSize & 0xFF;
    bmpHeader[3] = (fileSize >> 8) & 0xFF;
    bmpHeader[4] = (fileSize >> 16) & 0xFF;
    bmpHeader[5] = (fileSize >> 24) & 0xFF;
    bmpHeader[10] = 54;
    bmpHeader[14] = 40;
    bmpHeader[18] = w & 0xFF;
    bmpHeader[19] = (w >> 8) & 0xFF;
    bmpHeader[20] = (w >> 16) & 0xFF;
    bmpHeader[21] = (w >> 24) & 0xFF;
    bmpHeader[22] = h & 0xFF;
    bmpHeader[23] = (h >> 8) & 0xFF;
    bmpHeader[24] = (h >> 16) & 0xFF;
    bmpHeader[25] = (h >> 24) & 0xFF;
    bmpHeader[26] = 1;
    bmpHeader[28] = 24;

    httpd_resp_send_chunk(req, (const char *)bmpHeader, 54);

    // pixel data
    uint8_t *rgb565 = fb->buf;
    for (int y = h - 1; y >= 0; y--)
    {
        for (int x = 0; x < w; x++)
        {
            int idx = (y * w + x) * 2;
            uint16_t px = rgb565[idx] | (rgb565[idx + 1] << 8);
            uint8_t r5 = (px >> 11) & 0x1F;
            uint8_t g6 = (px >> 5) & 0x3F;
            uint8_t b5 = px & 0x1F;
            uint8_t R = (r5 * 255) / 31;
            uint8_t G = (g6 * 255) / 63;
            uint8_t B = (b5 * 255) / 31;
            uint8_t bgr[3] = {B, G, R};
            httpd_resp_send_chunk(req, (const char *)bgr, 3);
        }
        int pad = rowSize - w * 3;
        if (pad > 0)
        {
            uint8_t padding[4] = {0};
            httpd_resp_send_chunk(req, (const char *)padding, pad);
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

static esp_err_t count_handler(httpd_req_t *req)
{
    char query[200];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char param[32];
        if (httpd_query_key_value(query, "threshold", param, sizeof(param)) == ESP_OK)
            g_threshold = atoi(param);
        if (httpd_query_key_value(query, "min", param, sizeof(param)) == ESP_OK)
            g_minArea = atoi(param);
        if (httpd_query_key_value(query, "max", param, sizeof(param)) == ESP_OK)
            g_maxArea = atoi(param);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int w = fb->width;
    int h = fb->height;
    uint8_t *gray = (uint8_t *)malloc(w * h);
    if (!gray)
    {
        esp_camera_fb_return(fb);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint8_t *buf = fb->buf;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            int idx = i * 2;
            uint16_t px = buf[idx] | (buf[idx + 1] << 8);
            gray[i] = rgb565_to_gray(px);
        }
    }

    std::vector<Blob> blobs = detect_blobs(gray, w, h, g_threshold, g_minArea, g_maxArea);

    String json = "{";
    json += "\"count\":" + String((int)blobs.size()) + ",";
    json += "\"sizes\":[";
    for (size_t i = 0; i < blobs.size(); i++)
    {
        json += String(blobs[i].area);
        if (i + 1 < blobs.size())
            json += ",";
    }
    json += "],";
    json += "\"w\":" + String(w) + ",\"h\":" + String(h);
    json += "}";

    free(gray);
    esp_camera_fb_return(fb);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

static esp_err_t save_snapshot_handler(httpd_req_t *req)
{
    if (!SD_MMC.begin())
    {
        const char *resp = "{\"error\":\"SD card init failed\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        const char *resp = "{\"error\":\"camera capture failed\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }

    int w = fb->width, h = fb->height;
    String fn = "/snapshot_" + String(millis()) + ".bmp";
    File f = SD_MMC.open(fn, FILE_WRITE);
    if (!f)
    {
        esp_camera_fb_return(fb);
        const char *resp = "{\"error\":\"SD open failed\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }

    int rowSize = ((w * 3 + 3) / 4) * 4;
    int imgSize = rowSize * h;
    int fileSize = 54 + imgSize;
    uint8_t bmpHeader[54] = {0};
    bmpHeader[0] = 'B';
    bmpHeader[1] = 'M';
    bmpHeader[2] = fileSize & 0xFF;
    bmpHeader[3] = (fileSize >> 8) & 0xFF;
    bmpHeader[4] = (fileSize >> 16) & 0xFF;
    bmpHeader[5] = (fileSize >> 24) & 0xFF;
    bmpHeader[10] = 54;
    bmpHeader[14] = 40;
    bmpHeader[18] = w & 0xFF;
    bmpHeader[19] = (w >> 8) & 0xFF;
    bmpHeader[20] = (w >> 16) & 0xFF;
    bmpHeader[21] = (w >> 24) & 0xFF;
    bmpHeader[22] = h & 0xFF;
    bmpHeader[23] = (h >> 8) & 0xFF;
    bmpHeader[24] = (h >> 16) & 0xFF;
    bmpHeader[25] = (h >> 24) & 0xFF;
    bmpHeader[26] = 1;
    bmpHeader[28] = 24;
    f.write(bmpHeader, 54);

    uint8_t *rgb565 = fb->buf;
    for (int y = h - 1; y >= 0; y--)
    {
        for (int x = 0; x < w; x++)
        {
            int idx = (y * w + x) * 2;
            uint16_t px = rgb565[idx] | (rgb565[idx + 1] << 8);
            uint8_t r5 = (px >> 11) & 0x1F;
            uint8_t g6 = (px >> 5) & 0x3F;
            uint8_t b5 = px & 0x1F;
            uint8_t R = (r5 * 255) / 31;
            uint8_t G = (g6 * 255) / 63;
            uint8_t B = (b5 * 255) / 31;
            f.write(&B, 1);
            f.write(&G, 1);
            f.write(&R, 1);
        }
        int pad = rowSize - w * 3;
        for (int p = 0; p < pad; p++)
        {
            uint8_t z = 0;
            f.write(&z, 1);
        }
    }

    f.close();
    esp_camera_fb_return(fb);

    String resp = "{\"message\":\"saved as " + fn + "\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp.c_str(), resp.length());
    return ESP_OK;
}

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
    httpd_uri_t snapshot_uri = {.uri = "/snapshot", .method = HTTP_GET, .handler = snapshot_handler, .user_ctx = NULL};
    httpd_uri_t count_uri = {.uri = "/count", .method = HTTP_GET, .handler = count_handler, .user_ctx = NULL};
    httpd_uri_t save_uri = {.uri = "/save_snapshot", .method = HTTP_GET, .handler = save_snapshot_handler, .user_ctx = NULL};

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &snapshot_uri);
        httpd_register_uri_handler(camera_httpd, &count_uri);
        httpd_register_uri_handler(camera_httpd, &save_uri);
        Serial.println("✅ Web server started successfully!");
    }
    else
    {
        Serial.println("❌ Web server failed to start!");
    }
}

void setup()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

    Serial.begin(115200);
    delay(2000);
    Serial.println();
    Serial.println("======================================");
    Serial.println("🚀 ESP32S3 Camera Object Counter v1.3");
    Serial.println("📷 OV5640 Camera Module");
    Serial.println("======================================");
    Serial.printf("ESP32 Chip: %s\n", ESP.getChipModel());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println();

    // Step 1: Setup WiFi AP
    if (!setupWiFiAP())
    {
        Serial.println("⚠️  WiFi AP failed, but continuing with camera init...");
    }

    // Step 2: Initialize camera
    delay(1000);
    if (!initCamera())
    {
        Serial.println("⚠️  Camera init failed, continuing with web server only...");
    }

    // Step 3: Start web server
    Serial.println("🌐 Starting web server...");
    startCameraServer();

    Serial.println();
    Serial.println("======================================");
    Serial.println("✅ Setup Complete!");
    Serial.println("1. 📱 Search WiFi: ESP32_CAMERA_COUNT");
    Serial.println("2. 🔑 Password: 12345678");
    Serial.println("3. 🌐 Browser: http://192.168.4.1");
    Serial.println("======================================");
}

void loop()
{
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000)
    {
        lastCheck = millis();
        Serial.printf("📊 Status - WiFi clients: %d, Free heap: %d bytes\n",
                      WiFi.softAPgetStationNum(), ESP.getFreeHeap());
    }
    delay(1000);
}
