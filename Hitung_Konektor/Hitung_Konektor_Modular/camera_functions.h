// camera_functions.h - Fungsi kamera yang disederhanakan
#ifndef CAMERA_FUNCTIONS_H
#define CAMERA_FUNCTIONS_H

#include <esp_camera.h>
#include <WebServer.h>

// Function declarations
bool initializeCamera();
void handleCameraStream(WebServer &server);

// Camera pin configuration untuk ESP32-S3
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// ==== Initialize Camera ====
bool initializeCamera()
{
    Serial.println("🔄 Initializing camera...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Frame size configuration - Start with smaller resolution for stability
    if (psramFound())
    {
        Serial.println("✅ PSRAM found - using high quality settings");
        config.frame_size = FRAMESIZE_SVGA; // 800x600 for better performance
        config.jpeg_quality = 10;           // Lower number = higher quality
        config.fb_count = 2;                // Frame buffer count
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }
    else
    {
        Serial.println("⚠️ PSRAM not found - using basic settings");
        config.frame_size = FRAMESIZE_VGA; // 640x480
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("❌ Camera init failed with error 0x%x\n", err);
        return false;
    }

    Serial.println("✅ Camera initialized successfully");

    // Camera sensor settings
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL)
    {
        Serial.println("🔧 Configuring camera sensor...");
        s->set_brightness(s, 0);                 // -2 to 2
        s->set_contrast(s, 0);                   // -2 to 2
        s->set_saturation(s, 0);                 // -2 to 2
        s->set_special_effect(s, 0);             // 0 to 6 (0-No Effect, 1-Negative, 2-Grayscale, 3-Red Tint, 4-Green Tint, 5-Blue Tint, 6-Sepia)
        s->set_whitebal(s, 1);                   // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);                   // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);                    // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
        s->set_exposure_ctrl(s, 1);              // 0 = disable , 1 = enable
        s->set_aec2(s, 0);                       // 0 = disable , 1 = enable
        s->set_ae_level(s, 0);                   // -2 to 2
        s->set_aec_value(s, 300);                // 0 to 1200
        s->set_gain_ctrl(s, 1);                  // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);                   // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
        s->set_bpc(s, 0);                        // 0 = disable , 1 = enable
        s->set_wpc(s, 1);                        // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);                    // 0 = disable , 1 = enable
        s->set_lenc(s, 1);                       // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);                    // 0 = disable , 1 = enable
        s->set_vflip(s, 0);                      // 0 = disable , 1 = enable
        s->set_dcw(s, 1);                        // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);                   // 0 = disable , 1 = enable
    }

    Serial.println("✅ Camera initialized successfully");

    // Test camera capture
    Serial.println("🧪 Testing camera capture...");
    camera_fb_t *test_fb = esp_camera_fb_get();
    if (test_fb)
    {
        Serial.printf("✅ Camera test successful: %dx%d, %d bytes\n", test_fb->width, test_fb->height, test_fb->len);
        esp_camera_fb_return(test_fb);
    }
    else
    {
        Serial.println("⚠️ Camera test failed - but initialization succeeded");
    }

    return true;
}

// ==== Capture image and return frame buffer ====
camera_fb_t *captureImage()
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("❌ Camera capture failed");
        return nullptr;
    }

    Serial.printf("✅ Image captured: %dx%d, %d bytes\n", fb->width, fb->height, fb->len);
    return fb;
}

// ==== Release frame buffer ====
void releaseFrameBuffer(camera_fb_t *fb)
{
    if (fb)
    {
        esp_camera_fb_return(fb);
    }
}

// ==== Stream handler for live video ====
void handleCameraStream(WebServer &server)
{
    WiFiClient client = server.client();

    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";

    client.print(response);

    while (client.connected())
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("❌ Camera capture failed in stream");
            delay(1000);
            continue;
        }

        if (fb->len > 0)
        {
            client.print("--frame\r\n");
            client.printf("Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);

            // Send image data in chunks
            uint8_t *fbBuf = fb->buf;
            size_t fbLen = fb->len;
            for (size_t n = 0; n < fbLen; n = n + 1024)
            {
                if (n + 1024 < fbLen)
                {
                    client.write(fbBuf, 1024);
                    fbBuf += 1024;
                }
                else if (fbLen % 1024 > 0)
                {
                    size_t remainder = fbLen % 1024;
                    client.write(fbBuf, remainder);
                }
            }
            client.print("\r\n");
        }

        esp_camera_fb_return(fb);

        if (!client.connected())
            break;
        delay(50); // ~20fps for stability
    }
}

#endif
