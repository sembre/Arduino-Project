# ESP32-S3 Camera & File Manager - AI Coding Instructions

## Architecture Overview

This is a **modular ESP32-S3 Arduino project** combining live camera streaming with SD card file management via a web interface. The system creates a WiFi Access Point and serves a single-page web application.

### Core Components & Data Flow

- **Main App** (`Hitung_Konektor_Modular.ino`) → Orchestrates initialization and web server routing
- **Camera Module** (`camera_functions.h`) → Handles OV5640 camera init, capture, and streaming
- **SD Storage** (`sd_functions.h`) → Manages dual-mode SD card access (MMC built-in + SPI fallback)
- **Web Handlers** (`web_handlers.h`) → HTTP endpoint implementations with CORS support
- **Web Interface** (`web_interface.h`) → Single-page HTML/CSS/JS embedded as PROGMEM string
- **Configuration** (`config.h`) → Hardware pin mappings and system constants

### Critical Architectural Patterns

#### 1. **Dual SD Card Strategy** (Key Differentiator)

```cpp
// Always try SD_MMC (built-in) first, fallback to SPI
bool usingSPIMode = false; // Global state tracker
if (SD_MMC.setPins() && SD_MMC.begin()) {
    usingSPIMode = false; // Built-in MMC mode
} else {
    SPI.begin(); SD.begin(); usingSPIMode = true; // SPI fallback
}
```

#### 2. **Embedded Web Interface Pattern**

- Entire UI stored as `const char index_html[] PROGMEM` string in flash memory
- Single-page app with live camera stream + file manager panels
- JavaScript handles all dynamic content (file listings, uploads, navigation)

#### 3. **Modular Handler Architecture**

- All HTTP endpoints defined as standalone functions in `web_handlers.h`
- Consistent pattern: `addCORSHeaders() → validate → SD/camera operation → JSON response`
- Lambda functions in main app route to handler functions

## Development Workflows

### Building & Deployment

```bash
# Arduino IDE workflow - no makefile/build system
# 1. Select "ESP32S3 Dev Module" board
# 2. Set partition scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
# 3. PSRAM: "OPI PSRAM" (required for high-res camera)
# 4. Upload via USB-C cable
```

### Debugging Strategy

```cpp
// Serial output is primary debugging tool
Serial.printf("📤 Sending file: %s (%d bytes)\n", filename.c_str(), file.size());
// Use emoji prefixes for log categorization: 📸 🔄 ❌ ✅ 📂 📤 📥
```

### Testing Endpoints

```bash
# Connect to WiFi AP: "ESP32-CAM-AP" / "12345678"
# Base URL: http://192.168.4.1
curl http://192.168.4.1/system_info  # System status
curl http://192.168.4.1/files?path=/  # File listing
curl -X POST http://192.168.4.1/capture  # Take photo
```

## Project-Specific Conventions

### File Operation Patterns

```cpp
// Always check SD initialization before file ops
if (!initializeSDCard()) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available\"}");
    return;
}

// Dual-mode file operations
File file = usingSPIMode ? SD.open(filepath) : SD_MMC.open(filepath);
```

### Path Normalization

```cpp
// All paths must start with "/" for consistency
if (!filepath.startsWith("/")) {
    filepath = "/" + filepath;
}
```

### Error Handling Convention

```cpp
// Structured JSON responses with success/error fields
String response = "{\"success\":true,\"filename\":\"" + filename + "\"}";
// vs
server.send(500, "application/json", "{\"success\":false,\"error\":\"" + errorMsg + "\"}");
```

### Camera Stream Pattern

```cpp
// handleCameraStream() uses multipart/x-mixed-replace for live streaming
// Frame capture must always call esp_camera_fb_return(fb) to prevent memory leaks
camera_fb_t *fb = esp_camera_fb_get();
server.streamFile(); // Stream the frame
esp_camera_fb_return(fb); // Critical: always return buffer
```

## Integration Points & Dependencies

### Hardware Dependencies

- **PSRAM detection** drives camera quality settings (`psramFound()` check)
- **Pin conflicts**: Camera uses GPIO 18, SD SPI also uses GPIO 18 (fallback mode issue)
- **Power requirements**: Both camera and SD card need stable 3.3V

### Cross-Component Communication

```cpp
// Global state variables for coordination
extern bool usingSPIMode;        // SD card mode indicator
extern bool sdCardInitialized;   // Initialization state
// Web handlers read these to choose SD_MMC vs SD operations
```

### Critical File Dependencies

- `config.h` → Must be included first (provides pin definitions)
- `web_interface.h` → Contains entire HTML/CSS/JS as PROGMEM string
- `camera_functions.h` → Must call initializeCamera() before any stream operations

## Common Pitfalls & Solutions

### Memory Management

```cpp
// PROGMEM strings for large HTML content (not regular String)
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML>...)rawliteral";
server.send_P(200, "text/html", index_html); // Use send_P for PROGMEM
```

### SD Card Initialization Timing

```cpp
// Always allow camera to stabilize before SD init
delay(2000); // Camera stabilization
initializeSDCard(); // Then try SD card
```

### File Upload Handler Pattern

```cpp
// File uploads require both upload handler AND response handler
server.on("/upload", HTTP_POST, []() { handleFileUploadResponse(server); },
                                []() { handleFileUpload(server); });
//                                ↑ Upload handler    ↑ Response handler
```
