#include <EEPROM.h>
#include <TM1637Display.h>

// Konfigurasi untuk boot lock
#define MAGIC_NUMBER 1234 // Ubah angka ini untuk mereset boot lock
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_COUNT 2
#define MAX_BOOT_COUNT 5000

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Deklarasi pin secara individu
const int pins[] = {
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53,
  2,  3,  4,  5,  6
};

void lockSystem() {
  // Tampilkan "dIE" di TM1637
  uint8_t dieSegments[] = {0x5e, 0x30, 0x79, 0x00}; // d, I, E, (blank)
  tm1637.setSegments(dieSegments);
  
  // Masuk ke loop tak terbatas untuk mengunci sistem
  while (true) {
    // Sistem terkunci
  }
}

void setup() {
  // Inisialisasi TM1637
  tm1637.setBrightness(7); // Set brightness level (0 - 7)

  // Inisialisasi Serial Monitor
  Serial.begin(115200);

  // Cek dan update boot count
  int magic;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);

  unsigned int bootCount;
  if (magic == MAGIC_NUMBER) {
    EEPROM.get(EEPROM_ADDR_COUNT, bootCount);

    if (bootCount >= MAX_BOOT_COUNT) {
      Serial.println("Max boot count reached. System locked.");
      lockSystem();
    }

    bootCount++;
    EEPROM.put(EEPROM_ADDR_COUNT, bootCount);
  } else {
    // Magic number tidak cocok, reset boot count
    EEPROM.put(EEPROM_ADDR_MAGIC, MAGIC_NUMBER);
    bootCount = 1;
    EEPROM.put(EEPROM_ADDR_COUNT, bootCount);
  }
  
  Serial.print("Boot count: ");
  Serial.println(bootCount);

  // Deklarasi pin sebagai INPUT_PULLUP agar pin terbaca HIGH saat tidak tersambung ke GND
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }
}

void loop() {
  // Cek setiap pin
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if (digitalRead(pins[i]) == LOW) {
      // Jika pin terhubung ke GND, tampilkan angka pada TM1637 display
      int valueToShow = i + 1; // Start numbering from 1
      tm1637.showNumberDecEx(valueToShow);

      // Tampilkan angka juga di Serial Monitor
      Serial.print("Pin ");
      Serial.print(pins[i]);
      Serial.print(" connected, displaying number: ");
      Serial.println(valueToShow);

      delay(1000); // Display for 1 second
      tm1637.clear(); // Clear the display after showing
      break; // Exit loop after detecting the first active pin
    }
  }
}
