// Hitung_Konektor_Modular.ino - ESP32-S3 Camera & File Manager (Simplified)
// Sistem yang ringan hanya untuk Live Camera Stream dan File Manager SD Card

#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>

// Include custom headers
#include "config.h"
#include "camera_functions.h"
#include "sd_functions.h"
#include "web_interface.h"
#include "web_handlers.h"

// External variable declaration
extern bool usingSPIMode;

// Global objects
WebServer server(80);

// ==== SETUP ====
void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== ESP32-S3 Camera & File Manager ===");
    Serial.println("Starting system...");

    // Initialize components
    if (!initializeCamera())
    {
        Serial.println("❌ Camera initialization failed");
        while (1)
            delay(1000);
    }

    delay(2000); // Wait for camera to stabilize

    if (!initializeSDCard())
    {
        Serial.println("⚠️ SD Card not available, some features may not work");
    }
    else
    {
        printSDCardInfo();
    }

    // Start WiFi Access Point
    WiFi.softAP(ssid, password);
    Serial.println("✅ WiFi AP started");
    Serial.print("🌐 IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("📡 SSID: ");
    Serial.println(ssid);
    Serial.print("🔐 Password: ");
    Serial.println(password);

    // Setup web routes
    setupWebRoutes();

    // Start web server
    server.begin();
    Serial.println("✅ Web server started");
    Serial.println("🎯 Ready! Open http://192.168.4.1 in your browser");
    Serial.println("========================================");
}

// ==== MAIN LOOP ====
void loop()
{
    server.handleClient();
    delay(2); // Small delay for stability
}

// ==== Setup Web Routes ====
void setupWebRoutes()
{
    // Main page
    server.on("/", HTTP_GET, []()
              { server.send_P(200, "text/html", index_html); });

    // Camera stream
    server.on("/stream", HTTP_GET, []()
              { handleCameraStream(server); });

    // System info
    server.on("/system_info", HTTP_GET, []()
              { handleSystemInfo(server); });

    // SD Card reconnect
    server.on("/sd_reconnect", HTTP_POST, []()
              { handleSDCardReconnect(server); });

    // Image capture
    server.on("/capture", HTTP_POST, []()
              { handleCapture(server); });

    // Camera test endpoint
    server.on("/camera_test", HTTP_GET, []()
              { handleCameraTest(server); });

    // File operations
    server.on("/files", HTTP_GET, []()
              { handleFileList(server); });

    server.on("/download", HTTP_GET, []()
              { handleFileDownload(server); });

    server.on("/upload", HTTP_POST, []()
              { handleFileUploadResponse(server); }, []()
              { handleFileUpload(server); });

    server.on("/delete", HTTP_DELETE, []()
              { handleFileDelete(server); });

    server.on("/create_folder", HTTP_POST, []()
              { handleCreateFolder(server); });

    // CORS support
    server.on("/", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/files", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/capture", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/upload", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/delete", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/create_folder", HTTP_OPTIONS, []()
              { handleOptions(server); });

    server.on("/sd_reconnect", HTTP_OPTIONS, []()
              { handleOptions(server); });

    // 404 handler
    server.onNotFound([]()
                      { server.send(404, "text/plain", "404: Not Found"); });
}
