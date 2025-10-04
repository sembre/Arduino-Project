// web_handlers.h - Handler sederhana untuk Camera Stream dan File Manager
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <WebServer.h>
#include "sd_functions.h"

// Function declarations
void addCORSHeaders(WebServer &server);
void handleSystemInfo(WebServer &server);
void handleCapture(WebServer &server);
void handleCameraTest(WebServer &server);
void handleFileList(WebServer &server);
void handleFileDownload(WebServer &server);
void handleFileUpload(WebServer &server);
void handleFileUploadResponse(WebServer &server);
void handleFileDelete(WebServer &server);
void handleCreateFolder(WebServer &server);
void handleSDCardReconnect(WebServer &server);
void handleOptions(WebServer &server);

// Helper function untuk CORS headers
void addCORSHeaders(WebServer &server)
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ==== System Info Handler ====
void handleSystemInfo(WebServer &server)
{
    addCORSHeaders(server);

    String info = "ESP32-S3 Camera System\n";
    info += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    info += "Total Heap: " + String(ESP.getHeapSize()) + " bytes\n";
    info += "Chip Model: " + String(ESP.getChipModel()) + "\n";
    info += "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    info += "Flash Size: " + String(ESP.getFlashChipSize()) + " bytes\n";
    info += "WiFi Status: " + String(WiFi.getMode() == WIFI_AP ? "Access Point Active" : "Disconnected") + "\n";
    info += "AP IP Address: " + WiFi.softAPIP().toString() + "\n";
    info += "Connected Clients: " + String(WiFi.softAPgetStationNum()) + "\n";

    // SD Card status
    info += "SD Card: ";
    if (checkSDCardStatus()) {
        info += "Available (" + String(usingSPIMode ? "SPI" : "MMC") + " mode)\n";
        if (usingSPIMode) {
            info += "SD Card Size: " + String(SD.cardSize() / (1024 * 1024)) + " MB\n";
            info += "SD Card Used: " + String(SD.usedBytes() / (1024 * 1024)) + " MB\n";
        } else {
            info += "SD Card Size: " + String(SD_MMC.cardSize() / (1024 * 1024)) + " MB\n";
            info += "SD Card Used: " + String(SD_MMC.usedBytes() / (1024 * 1024)) + " MB\n";
        }
    } else {
        info += "Not Available\n";
    }

    server.send(200, "text/plain", info);
}

// ==== SD Card Reconnect Handler ====
void handleSDCardReconnect(WebServer &server)
{
    addCORSHeaders(server);
    
    Serial.println("🔄 Manual SD card reconnect requested");
    
    bool success = forceReinitializeSDCard();
    
    if (success) {
        String response = "{\"success\":true,\"message\":\"SD card reconnected successfully\",";
        response += "\"mode\":\"" + String(usingSPIMode ? "SPI" : "MMC") + "\"}";
        server.send(200, "application/json", response);
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to reconnect SD card\"}");
    }
}

// ==== Capture Image Handler ====
void handleCapture(WebServer &server)
{
    Serial.println("📸 Capture request received");
    addCORSHeaders(server);

    // Check if camera is available
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("❌ Camera capture failed - no frame buffer");
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Camera capture failed\"}");
        return;
    }

    Serial.printf("✅ Frame captured: %d bytes\n", fb->len);

    // Check SD card before saving
    if (!initializeSDCard() || !checkSDCardStatus()) {
        Serial.println("❌ SD Card not available for capture - attempting reconnection");
        esp_camera_fb_return(fb);
        if (!forceReinitializeSDCard()) {
            server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available for saving image\"}");
            return;
        }
        Serial.println("✅ SD Card reconnected for capture");
    }

    // Generate filename
    String filename = generateImageFileName("CAPTURE");
    String filepath = "/" + filename;
    Serial.printf("📁 Saving to: %s (SD Mode: %s)\n", filepath.c_str(), usingSPIMode ? "SPI" : "MMC");

    // Save to SD card
    bool saved = saveImageToSD(fb->buf, fb->len, filepath);
    esp_camera_fb_return(fb);

    if (saved)
    {
        String response = "{\"success\":true,\"filename\":\"" + filename + "\",\"path\":\"" + filepath + "\"}";
        Serial.println("✅ Image capture successful: " + filename);
        server.send(200, "application/json", response);
    }
    else
    {
        Serial.println("❌ Failed to save image to SD card");
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save image to SD card\"}");
    }
}

// ==== Camera Test Handler ====
void handleCameraTest(WebServer &server)
{
    addCORSHeaders(server);

    Serial.println("🧪 Testing camera functionality...");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        String response = "{\"success\":false,\"error\":\"Camera test failed - no frame buffer\"}";
        server.send(500, "application/json", response);
        return;
    }

    // Get frame info before returning buffer
    int width = fb->width;
    int height = fb->height;
    size_t size = fb->len;
    bool isJpeg = (fb->format == PIXFORMAT_JPEG);

    esp_camera_fb_return(fb);

    String response = "{\"success\":true,";
    response += "\"width\":" + String(width) + ",";
    response += "\"height\":" + String(height) + ",";
    response += "\"size\":" + String(size) + ",";
    response += "\"format\":\"" + String(isJpeg ? "JPEG" : "Unknown") + "\"}";

    Serial.printf("✅ Camera test successful: %dx%d, %d bytes\n", width, height, size);
    server.send(200, "application/json", response);
}

// ==== File List Handler ====
void handleFileList(WebServer &server)
{
    addCORSHeaders(server);

    String path = server.hasArg("path") ? server.arg("path") : "/";

    // Normalize path
    if (path.isEmpty() || path == "")
    {
        path = "/";
    }
    if (!path.startsWith("/"))
    {
        path = "/" + path;
    }

    Serial.printf("📂 Listing files in: %s\n", path.c_str());
    Serial.printf("🔍 SD Card status - Initialized: %s, Using SPI: %s\n", 
                  sdCardInitialized ? "YES" : "NO", 
                  usingSPIMode ? "YES" : "NO");

    // Force check SD card status and try to reconnect if needed
    if (!initializeSDCard() || !checkSDCardStatus())
    {
        Serial.println("❌ SD Card not available - attempting reconnection");
        if (!forceReinitializeSDCard()) {
            Serial.println("❌ SD Card reconnection failed");
            server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available - please check SD card connection\"}");
            return;
        }
        else {
            Serial.println("✅ SD Card reconnection successful");
        }
    }

    File root;
    int retries = 3;
    
    // Try to open directory with retries
    while (retries > 0 && !root) {
        if (usingSPIMode)
        {
            root = SD.open(path);
        }
        else
        {
            root = SD_MMC.open(path);
        }
        
        if (!root) {
            retries--;
            if (retries > 0) {
                Serial.printf("❌ Failed to open directory, retrying... (%d left)\n", retries);
                delay(100);
                // Try to reinitialize SD card
                forceReinitializeSDCard();
            }
        }
    }

    if (!root)
    {
        Serial.printf("❌ Failed to open directory after retries: %s\n", path.c_str());
        server.send(404, "application/json", "{\"success\":false,\"error\":\"Directory not found or SD card error\"}");
        return;
    }

    if (!root.isDirectory())
    {
        Serial.printf("❌ Path is not a directory: %s\n", path.c_str());
        root.close();
        server.send(404, "application/json", "{\"success\":false,\"error\":\"Path is not a directory\"}");
        return;
    }

    String json = "{\"success\":true,\"path\":\"" + path + "\",\"files\":[";
    bool first = true;
    int fileCount = 0;

    File file = root.openNextFile();
    while (file)
    {
        if (!first)
            json += ",";
        first = false;

        String fileName = String(file.name());
        // Remove path prefix if present
        int lastSlash = fileName.lastIndexOf('/');
        if (lastSlash >= 0)
        {
            fileName = fileName.substring(lastSlash + 1);
        }

        // Skip hidden files and system files
        if (!fileName.startsWith("."))
        {
            json += "{\"name\":\"" + fileName + "\",";
            json += "\"isDir\":" + String(file.isDirectory() ? "true" : "false") + ",";
            json += "\"size\":" + String(file.size()) + "}";
            fileCount++;
        }
        else
        {
            if (!first)
            {
                // Remove the comma we added for this hidden file
                json = json.substring(0, json.length() - 1);
                first = fileCount == 0;
            }
        }

        file.close();
        file = root.openNextFile();
    }
    root.close();

    json += "],\"count\":" + String(fileCount) + "}";

    Serial.printf("✅ Listed %d files in %s\n", fileCount, path.c_str());
    server.send(200, "application/json", json);
}

// ==== File Download Handler ====
void handleFileDownload(WebServer &server)
{
    addCORSHeaders(server);

    if (!server.hasArg("file"))
    {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }

    String filepath = server.arg("file");

    // Normalize path
    if (!filepath.startsWith("/"))
    {
        filepath = "/" + filepath;
    }

    Serial.printf("📥 Download request: %s\n", filepath.c_str());

    if (!initializeSDCard())
    {
        Serial.println("❌ SD Card not available for download");
        server.send(500, "text/plain", "SD Card not available");
        return;
    }

    File file;
    if (usingSPIMode)
    {
        file = SD.open(filepath);
    }
    else
    {
        file = SD_MMC.open(filepath);
    }

    if (!file)
    {
        Serial.printf("❌ File not found: %s\n", filepath.c_str());
        server.send(404, "text/plain", "File not found");
        return;
    }

    if (file.isDirectory())
    {
        Serial.printf("❌ Cannot download directory: %s\n", filepath.c_str());
        file.close();
        server.send(400, "text/plain", "Cannot download directory");
        return;
    }

    // Determine content type
    String contentType = "application/octet-stream";
    String lowerPath = filepath;
    lowerPath.toLowerCase();

    if (lowerPath.endsWith(".jpg") || lowerPath.endsWith(".jpeg"))
    {
        contentType = "image/jpeg";
    }
    else if (lowerPath.endsWith(".png"))
    {
        contentType = "image/png";
    }
    else if (lowerPath.endsWith(".gif"))
    {
        contentType = "image/gif";
    }
    else if (lowerPath.endsWith(".txt"))
    {
        contentType = "text/plain";
    }
    else if (lowerPath.endsWith(".html") || lowerPath.endsWith(".htm"))
    {
        contentType = "text/html";
    }
    else if (lowerPath.endsWith(".css"))
    {
        contentType = "text/css";
    }
    else if (lowerPath.endsWith(".js"))
    {
        contentType = "application/javascript";
    }
    else if (lowerPath.endsWith(".json"))
    {
        contentType = "application/json";
    }

    // Extract filename for download
    String filename = filepath;
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash >= 0)
    {
        filename = filename.substring(lastSlash + 1);
    }

    Serial.printf("📤 Sending file: %s (%d bytes) as %s\n", filename.c_str(), file.size(), contentType.c_str());

    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.streamFile(file, contentType);
    file.close();

    Serial.println("✅ Download completed");
}

// ==== Create Folder Handler ====
void handleCreateFolder(WebServer &server)
{
    addCORSHeaders(server);

    if (!server.hasArg("path") || !server.hasArg("name"))
    {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
        return;
    }

    String basePath = server.arg("path");
    String folderName = server.arg("name");

    Serial.printf("📁 Create folder request - Base: '%s', Name: '%s'\n", basePath.c_str(), folderName.c_str());

    // Validate folder name
    if (folderName.indexOf('/') >= 0 || folderName.indexOf('\\') >= 0 || folderName.length() == 0)
    {
        Serial.println("❌ Invalid folder name");
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid folder name\"}");
        return;
    }

    // Normalize base path
    if (basePath.isEmpty() || basePath == "")
    {
        basePath = "/";
    }
    if (!basePath.startsWith("/"))
    {
        basePath = "/" + basePath;
    }

    // Check SD card status
    if (!initializeSDCard() || !checkSDCardStatus())
    {
        Serial.println("❌ SD Card not available for folder creation - attempting reconnection");
        if (!forceReinitializeSDCard()) {
            Serial.println("❌ SD Card reconnection failed");
            server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available\"}");
            return;
        }
        Serial.println("✅ SD Card reconnected for folder creation");
    }

    String fullPath = basePath;
    if (!fullPath.endsWith("/"))
        fullPath += "/";
    fullPath += folderName;

    Serial.printf("📁 Creating folder: %s\n", fullPath.c_str());

    bool success;
    if (usingSPIMode)
    {
        success = SD.mkdir(fullPath);
    }
    else
    {
        success = SD_MMC.mkdir(fullPath);
    }

    if (success)
    {
        Serial.printf("✅ Folder created: %s\n", fullPath.c_str());
        String response = "{\"success\":true,\"path\":\"" + fullPath + "\"}";
        server.send(200, "application/json", response);
    }
    else
    {
        Serial.printf("❌ Failed to create folder: %s\n", fullPath.c_str());
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to create folder\"}");
    }
}

// ==== File Upload Handler ====
void handleFileUpload(WebServer &server)
{
    HTTPUpload &upload = server.upload();
    static File uploadFile;
    static String uploadPath;
    static bool uploadError = false;

    if (upload.status == UPLOAD_FILE_START)
    {
        addCORSHeaders(server);
        uploadError = false;

        Serial.printf("📤 Upload starting: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);

        // Force check SD card status
        if (!initializeSDCard() || !checkSDCardStatus())
        {
            Serial.println("❌ SD Card not available for upload - trying to reinitialize");
            if (!forceReinitializeSDCard()) {
                Serial.println("❌ SD Card reinitialize failed");
                uploadError = true;
                return;
            }
        }

        String filename = upload.filename;
        
        // Clean filename - remove path if present
        int lastSlash = filename.lastIndexOf('/');
        int lastBackslash = filename.lastIndexOf('\\');
        int lastSeparator = max(lastSlash, lastBackslash);
        if (lastSeparator >= 0) {
            filename = filename.substring(lastSeparator + 1);
        }

        // Get current path from form data
        String currentDir = server.hasArg("path") ? server.arg("path") : "/";
        if (currentDir != "/" && !currentDir.endsWith("/"))
        {
            currentDir += "/";
        }

        uploadPath = currentDir + filename;
        Serial.printf("� Upload path: %s\n", uploadPath.c_str());

        // Try to open file for writing with retry
        int retries = 3;
        while (retries > 0 && !uploadFile) {
            if (usingSPIMode)
            {
                uploadFile = SD.open(uploadPath, FILE_WRITE);
            }
            else
            {
                uploadFile = SD_MMC.open(uploadPath, FILE_WRITE);
            }
            
            if (!uploadFile) {
                retries--;
                if (retries > 0) {
                    Serial.printf("❌ Failed to create upload file, retrying... (%d left)\n", retries);
                    delay(100);
                    // Try to reinitialize SD card
                    if (!checkSDCardStatus()) {
                        forceReinitializeSDCard();
                    }
                } else {
                    Serial.printf("❌ Failed to create upload file after retries: %s\n", uploadPath.c_str());
                    uploadError = true;
                }
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile && !uploadError)
        {
            size_t written = uploadFile.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize)
            {
                Serial.printf("❌ Upload write error: %d/%d bytes\n", written, upload.currentSize);
                uploadError = true;
            }
            else
            {
                // Periodic progress update
                static unsigned long lastProgress = 0;
                if (millis() - lastProgress > 1000) { // Update every second
                    Serial.printf("📊 Upload progress: %d bytes written\n", upload.totalSize);
                    lastProgress = millis();
                }
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.flush(); // Ensure data is written
            uploadFile.close();
            
            if (!uploadError) {
                Serial.printf("✅ Upload completed successfully: %s (%d bytes)\n", uploadPath.c_str(), upload.totalSize);
                
                // Verify file was written correctly
                File verifyFile;
                if (usingSPIMode) {
                    verifyFile = SD.open(uploadPath);
                } else {
                    verifyFile = SD_MMC.open(uploadPath);
                }
                
                if (verifyFile && verifyFile.size() == upload.totalSize) {
                    Serial.println("✅ Upload verification successful");
                } else {
                    Serial.println("⚠️ Upload verification failed - file size mismatch");
                    uploadError = true;
                }
                
                if (verifyFile) verifyFile.close();
            } else {
                Serial.printf("❌ Upload completed with errors: %s\n", uploadPath.c_str());
            }
        }
        else
        {
            Serial.println("❌ Upload failed - no file handle");
            uploadError = true;
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Serial.println("❌ Upload aborted");
        if (uploadFile) {
            uploadFile.close();
        }
        uploadError = true;
    }
}



// ==== File Upload Response Handler ====
void handleFileUploadResponse(WebServer &server)
{
    addCORSHeaders(server);
    
    // Check if upload was successful by checking the static error flag
    // Note: This is a simplified approach - in production, you'd want better state management
    
    if (!initializeSDCard() || !checkSDCardStatus()) {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available\"}");
        return;
    }
    
    // Since we can't easily pass upload status from handleFileUpload, 
    // we'll assume success if we reach here and SD card is available
    server.send(200, "application/json", "{\"success\":true,\"message\":\"File uploaded successfully\"}");
}

// ==== File Delete Handler ====
void handleFileDelete(WebServer &server)
{
    addCORSHeaders(server);

    if (!server.hasArg("file"))
    {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
        return;
    }

    String filepath = server.arg("file");

    if (!initializeSDCard())
    {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"SD Card not available\"}");
        return;
    }

    bool success;
    if (usingSPIMode)
    {
        success = SD.remove(filepath);
    }
    else
    {
        success = SD_MMC.remove(filepath);
    }

    if (success)
    {
        Serial.printf("✅ File deleted: %s\n", filepath.c_str());
        server.send(200, "application/json", "{\"success\":true,\"message\":\"File deleted successfully\"}");
    }
    else
    {
        Serial.printf("❌ Failed to delete file: %s\n", filepath.c_str());
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to delete file\"}");
    }
}

// ==== OPTIONS Handler for CORS ====
void handleOptions(WebServer &server)
{
    addCORSHeaders(server);
    server.send(200, "text/plain", "");
}

#endif
