// ESP32S3 WiFi AP Test dengan WebServer
// Test WiFi + Web interface tanpa camera

#include <WiFi.h>
#include <WebServer.h>

// Konfigurasi WiFi
const char *ssid = "ESP32_WEB_TEST";
const char *password = "12345678";

WebServer server(80);

// HTML page untuk test
const char *html_test = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32S3 WiFi Test</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
        body { font-family: Arial; background: #1a1a2e; color: #eee; text-align: center; padding: 20px; }
        .container { max-width: 500px; margin: auto; background: #16213e; padding: 30px; border-radius: 15px; }
        h1 { color: #0f4c75; margin-bottom: 20px; }
        .status { background: #4caf50; color: white; padding: 15px; border-radius: 8px; margin: 15px 0; }
        .info { background: #2196f3; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; }
        button { background: #ff6b6b; color: white; border: none; padding: 12px 24px; border-radius: 8px; cursor: pointer; font-size: 16px; margin: 5px; }
        button:hover { background: #ff5252; }
        .clients { font-size: 24px; color: #4caf50; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 ESP32S3 WiFi Test</h1>
        <div class="status">✅ WiFi AP Connected!</div>
        <div class="info">📡 SSID: ESP32_WEB_TEST</div>
        <div class="info">🔑 Password: 12345678</div>
        <div class="info">🌐 IP: 192.168.4.1</div>
        <div class="info">👥 Connected Clients: <span class="clients" id="clients">0</span></div>
        <button onclick="updateStatus()" >🔄 Refresh Status</ button>
                        <button onclick = "testAlert()">🧪 Test Alert</ button>
                        </ div>

                        <script>
                            function updateStatus()
{
    fetch('/status')
        .then(response = > response.json())
        .then(data = > {
            document.getElementById('clients').innerText = data.clients;
        })
        .catch(err = > console.log('Error:', err));
}

function testAlert()
{
    alert('🎉 ESP32S3 WiFi Working Perfectly!');
}

// Auto refresh every 5 seconds
setInterval(updateStatus, 5000);
updateStatus(); // Initial load
    </script>
</body>
</html>
)";

void handleRoot()
    {
        server.send(200, "text/html", html_test);
    }

    void handleStatus()
    {
        String json = "{";
        json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
        json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
        json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "}";

        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", json);
    }

    void handleNotFound()
    {
        server.send(404, "text/plain", "404 Not Found");
    }

    void setup()
    {
        Serial.begin(115200);
        delay(2000);

        Serial.println();
        Serial.println("==============================");
        Serial.println("🚀 ESP32S3 WiFi + Web Test");
        Serial.println("==============================");
        Serial.printf("Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
        Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
        Serial.println();

        // Reset WiFi
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(1000);

        // Setup WiFi AP
        Serial.println("🔧 Setting up WiFi AP...");
        WiFi.mode(WIFI_AP);
        delay(500);

        // Try multiple methods
        bool success = false;

        Serial.println("📡 Method 1: Basic softAP");
        success = WiFi.softAP(ssid, password);
        delay(2000);

        if (!success)
        {
            Serial.println("📡 Method 2: Channel 1");
            success = WiFi.softAP(ssid, password, 1);
            delay(2000);
        }

        if (!success)
        {
            Serial.println("📡 Method 3: Channel 6");
            success = WiFi.softAP(ssid, password, 6);
            delay(2000);
        }

        if (!success)
        {
            Serial.println("📡 Method 4: Open network");
            success = WiFi.softAP(ssid);
            delay(2000);
        }

        if (success)
        {
            IPAddress IP = WiFi.softAPIP();
            Serial.println("✅ WiFi AP Started Successfully!");
            Serial.printf("📶 SSID: %s\n", ssid);
            Serial.printf("🔑 Password: %s\n", password);
            Serial.printf("🌐 IP Address: %s\n", IP.toString().c_str());
            Serial.printf("🔧 Gateway: %s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("📱 Connect and visit: http://%s\n", IP.toString().c_str());

            // Setup web server
            server.on("/", handleRoot);
            server.on("/status", handleStatus);
            server.onNotFound(handleNotFound);
            server.begin();

            Serial.println("🌐 Web Server Started!");
            Serial.println("==============================");
            Serial.println("📋 INSTRUCTIONS:");
            Serial.println("1. Connect to WiFi: ESP32_WEB_TEST");
            Serial.println("2. Password: 12345678");
            Serial.println("3. Open browser: http://192.168.4.1");
            Serial.println("==============================");
        }
        else
        {
            Serial.println("❌ All WiFi methods failed!");
            Serial.println("🔧 Possible issues:");
            Serial.println("   - Wrong board selection");
            Serial.println("   - ESP32 board package not installed");
            Serial.println("   - Hardware problem");
            Serial.println("   - Power supply insufficient");
        }
    }

    void loop()
    {
        server.handleClient();

        static unsigned long lastStatus = 0;
        if (millis() - lastStatus > 30000)
        { // Every 30 seconds
            lastStatus = millis();
            Serial.printf("📊 [%lu] Clients: %d, Heap: %d\n",
                          millis() / 1000,
                          WiFi.softAPgetStationNum(),
                          ESP.getFreeHeap());
        }

        delay(100);
    }
