// sd_functions.h - Fungsi SD Card yang disederhanakan
#ifndef SD_FUNCTIONS_H
#define SD_FUNCTIONS_H

#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include "config.h" // Include for SD pin definitions

// Function declarations
bool initializeSDCard();
bool forceReinitializeSDCard();
bool checkSDCardStatus();
bool isSDCardAvailable();
bool saveImageToSD(uint8_t *buffer, size_t length, String filepath);
String generateImageFileName(String prefix);
void printSDCardInfo();

// Global variables
bool sdCardInitialized = false;
bool usingSPIMode = false;
unsigned long lastSDCheckTime = 0;
const unsigned long SD_CHECK_INTERVAL = 5000; // Check every 5 seconds

// ==== Force SD Card Re-initialization ====
bool forceReinitializeSDCard()
{
    Serial.println("🔄 Force re-initializing SD Card...");
    
    // Reset state
    sdCardInitialized = false;
    
    // End existing connections
    if (usingSPIMode) {
        SD.end();
    } else {
        SD_MMC.end();
    }
    
    delay(500); // Wait for cleanup
    
    return initializeSDCard();
}

// ==== Check SD Card Connection Status ====
bool checkSDCardStatus()
{
    if (!sdCardInitialized) {
        return false;
    }
    
    // Periodic check to ensure SD card is still accessible
    if (millis() - lastSDCheckTime > SD_CHECK_INTERVAL) {
        lastSDCheckTime = millis();
        
        bool isAccessible = false;
        if (usingSPIMode) {
            File testFile = SD.open("/");
            isAccessible = testFile && testFile.isDirectory();
            if (testFile) testFile.close();
        } else {
            File testFile = SD_MMC.open("/");
            isAccessible = testFile && testFile.isDirectory();
            if (testFile) testFile.close();
        }
        
        if (!isAccessible) {
            Serial.println("⚠️ SD Card connection lost, attempting reconnection...");
            return forceReinitializeSDCard();
        }
    }
    
    return true;
}

// ==== SD Card Initialization ====
bool initializeSDCard()
{
    if (sdCardInitialized && checkSDCardStatus())
    {
        return true;
    }

    Serial.println("🔄 Initializing SD Card...");

    // Reset state first
    sdCardInitialized = false;
    
    // Try built-in SD card slot first (MMC mode) with specific pins
    Serial.println("Trying SD_MMC (built-in slot)...");
    Serial.printf("Using pins - CLK: %d, CMD: %d, DATA: %d\n", SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN);

    // Configure pins for SD_MMC with retry mechanism
    int retryCount = 3;
    while (retryCount > 0) {
        if (SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN))
        {
            Serial.println("✅ SD_MMC pins configured successfully");

            // Try to begin SD_MMC with 1-bit mode for stability
            if (SD_MMC.begin("/sdcard", true, 40000000, 5)) // Mount point, 1-bit mode, freq, max files
            {
                delay(100); // Small delay for stabilization
                
                // Test if we can actually access the card
                uint8_t cardType = SD_MMC.cardType();
                if (cardType != CARD_NONE)
                {
                    // Try to list root directory as final test
                    File root = SD_MMC.open("/");
                    if (root && root.isDirectory()) {
                        root.close();
                        Serial.println("✅ SD_MMC initialized successfully");
                        Serial.printf("Card Type: %d\n", cardType);
                        Serial.printf("Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
                        sdCardInitialized = true;
                        usingSPIMode = false;
                        lastSDCheckTime = millis();
                        return true;
                    } else {
                        Serial.println("❌ SD_MMC root directory test failed");
                        if (root) root.close();
                    }
                }
                else
                {
                    Serial.println("❌ SD_MMC card not detected");
                }
                SD_MMC.end();
            }
            else
            {
                Serial.println("❌ SD_MMC begin failed");
            }
        }
        else
        {
            Serial.println("❌ SD_MMC pin configuration failed");
        }
        
        retryCount--;
        if (retryCount > 0) {
            Serial.printf("Retrying SD_MMC initialization (%d attempts left)...\n", retryCount);
            delay(1000);
        }
    }

    // If MMC fails, try SPI mode as fallback
    Serial.println("SD_MMC failed, trying SPI mode...");
    Serial.printf("Using SPI pins - SCK: %d, MISO: %d, MOSI: %d, CS: %d\n",
                  SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    retryCount = 3;
    while (retryCount > 0) {
        SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
        if (SD.begin(SD_SPI_CS_PIN, SPI, 4000000)) // Lower frequency for stability
        {
            delay(100); // Small delay for stabilization
            
            // Test if we can actually access the card
            uint8_t cardType = SD.cardType();
            if (cardType != CARD_NONE)
            {
                // Try to list root directory as final test
                File root = SD.open("/");
                if (root && root.isDirectory()) {
                    root.close();
                    Serial.println("✅ SD SPI initialized successfully");
                    Serial.printf("Card Type: %d\n", cardType);
                    Serial.printf("Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
                    sdCardInitialized = true;
                    usingSPIMode = true;
                    lastSDCheckTime = millis();
                    return true;
                } else {
                    Serial.println("❌ SD SPI root directory test failed");
                    if (root) root.close();
                }
            }
            else
            {
                Serial.println("❌ SD SPI card not detected");
            }
            SD.end();
        }
        else
        {
            Serial.println("❌ SD SPI initialization failed");
        }
        
        retryCount--;
        if (retryCount > 0) {
            Serial.printf("Retrying SD SPI initialization (%d attempts left)...\n", retryCount);
            delay(1000);
        }
    }

    Serial.println("❌ SD Card initialization failed completely");
    return false;
}

// ==== Generate filename with timestamp ====
String generateImageFileName(String prefix = "IMG")
{
    unsigned long timestamp = millis();
    return prefix + "_" + String(timestamp) + ".jpg";
}

// ==== Save image data to SD card ====
bool saveImageToSD(uint8_t *buffer, size_t length, String filepath)
{
    Serial.printf("💾 Attempting to save %d bytes to: %s\n", length, filepath.c_str());

    if (!initializeSDCard())
    {
        Serial.println("❌ SD Card not available for saving");
        return false;
    }

    File file;
    if (usingSPIMode)
    {
        Serial.println("📂 Opening file in SPI mode...");
        file = SD.open(filepath, FILE_WRITE);
    }
    else
    {
        Serial.println("📂 Opening file in MMC mode...");
        file = SD_MMC.open(filepath, FILE_WRITE);
    }

    if (!file)
    {
        Serial.println("❌ Failed to open file for writing: " + filepath);
        return false;
    }

    Serial.println("✍️ Writing image data...");
    size_t written = file.write(buffer, length);
    file.close();

    if (written == length)
    {
        Serial.println("✅ Image saved: " + filepath + " (" + String(length) + " bytes)");
        return true;
    }
    else
    {
        Serial.println("❌ Failed to write complete image. Written: " + String(written) + "/" + String(length));
        return false;
    }
}

// ==== Check if SD card is available ====
bool isSDCardAvailable()
{
    return initializeSDCard();
}

// ==== Get SD card info ====
void printSDCardInfo()
{
    if (!initializeSDCard())
    {
        Serial.println("❌ SD Card not available");
        return;
    }

    if (usingSPIMode)
    {
        Serial.println("📱 SD Card Info (SPI Mode):");
        Serial.printf("Pin Config - SCK: %d, MISO: %d, MOSI: %d, CS: %d\n",
                      SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
        Serial.printf("Type: %d\n", SD.cardType());
        Serial.printf("Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
        Serial.printf("Used: %lluMB\n", SD.usedBytes() / (1024 * 1024));
    }
    else
    {
        Serial.println("📱 SD Card Info (MMC Mode):");
        Serial.printf("Pin Config - CLK: %d, CMD: %d, DATA: %d\n",
                      SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN);
        Serial.printf("Type: %d\n", SD_MMC.cardType());
        Serial.printf("Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        Serial.printf("Used: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
    }
}

#endif
