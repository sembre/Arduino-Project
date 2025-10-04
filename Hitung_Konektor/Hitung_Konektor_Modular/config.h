// config.h - Konfigurasi pin dan konstanta
#ifndef CONFIG_H
#define CONFIG_H

// ==== Pin kamera ESP32-S3 + OV5640 ====
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

// ==== WiFi Access Point ====
const char *ssid = "ESP32-CAM-AP";
const char *password = "12345678";

// ==== SD Card Pin Configuration untuk ESP32-S3 ====
// Built-in SD card slot pins:
// CLK:  GPIO39 (automatic)
// CMD:  GPIO38 (automatic)
// DATA: GPIO40 (automatic)
#define SD_MMC_CLK_PIN 39
#define SD_MMC_CMD_PIN 38
#define SD_MMC_D0_PIN 40

// SPI SD Card pins (fallback mode)
#define SD_SPI_SCK_PIN 18
#define SD_SPI_MISO_PIN 19
#define SD_SPI_MOSI_PIN 23
#define SD_SPI_CS_PIN 5

// ==== Konstanta Training ====
const unsigned long CAPTURE_INTERVAL = 3000; // 3 detik dalam milliseconds

#endif
