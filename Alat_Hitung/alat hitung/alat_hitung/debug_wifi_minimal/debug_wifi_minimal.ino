// Test WiFi AP Minimal - ESP32S3 Debug
// Upload ini dulu untuk test basic WiFi

#include <WiFi.h>

void setup()
{
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("=== ESP32S3 WiFi Test Debug ===");
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("Free Heap: %d\n", ESP.getFreeHeap());
    Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());

    // Test 1: Basic WiFi capability
    Serial.println("\n--- Test 1: WiFi Mode ---");
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_AP);
    delay(1000);
    Serial.println("WiFi mode set to AP");

    // Test 2: Simple AP without password
    Serial.println("\n--- Test 2: Open AP ---");
    bool result1 = WiFi.softAP("ESP32_TEST_OPEN");
    delay(3000);
    if (result1)
    {
        Serial.println("✅ Open AP SUCCESS");
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    else
    {
        Serial.println("❌ Open AP FAILED");
    }

    // Test 3: AP with password
    Serial.println("\n--- Test 3: Password AP ---");
    WiFi.softAPdisconnect(true);
    delay(1000);
    bool result2 = WiFi.softAP("ESP32_TEST_PASS", "12345678");
    delay(3000);
    if (result2)
    {
        Serial.println("✅ Password AP SUCCESS");
        Serial.printf("SSID: ESP32_TEST_PASS\n");
        Serial.printf("Pass: 12345678\n");
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    else
    {
        Serial.println("❌ Password AP FAILED");
    }

    // Test 4: WiFi Status
    Serial.println("\n--- Test 4: WiFi Status ---");
    Serial.printf("WiFi Mode: %d\n", WiFi.getMode());
    Serial.printf("AP Active: %s\n", WiFi.softAPgetStationNum() >= 0 ? "YES" : "NO");
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    Serial.println("\n=================================");
    Serial.println("Cari WiFi di smartphone:");
    Serial.println("- ESP32_TEST_OPEN (tanpa password)");
    Serial.println("- ESP32_TEST_PASS (password: 12345678)");
    Serial.println("=================================");
}

void loop()
{
    static unsigned long last = 0;
    if (millis() - last > 10000)
    {
        last = millis();
        Serial.printf("[%lu] Connected clients: %d\n", millis() / 1000, WiFi.softAPgetStationNum());
    }
    delay(1000);
}
