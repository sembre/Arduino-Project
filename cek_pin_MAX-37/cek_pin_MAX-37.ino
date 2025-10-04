/*
  ------------------------------------------------------------------------
  Project   : TM1637 Input Display Checker
  Date      : 2024-08-09
  Author    : AGUS FITRIYANTO
  GitHub    : https://github.com/sembre
  License   : Creative Commons Attribution 4.0 International (CC BY 4.0)
  License URL: https://creativecommons.org/licenses/by/4.0/

  ------------------------------------------------------------------------
  🇮🇩 Bahasa Indonesia:
  Deskripsi :
  Kode ini membaca status dari sejumlah pin digital pada Arduino dan
  menampilkan nomor pin yang aktif (terhubung ke GND) pada display
  7-segmen TM1637. Selain itu, nomor tersebut juga akan ditampilkan
  pada Serial Monitor.

  Hak Cipta & Lisensi:
  - Anda bebas menggunakan, membagikan, memodifikasi, dan mendistribusikan
    ulang kode ini untuk keperluan apa pun, termasuk komersial.
  - Syaratnya: Anda **wajib memberikan atribusi yang sesuai** kepada penulis
    asli (AGUS FITRIYANTO) dan menyertakan pernyataan lisensi ini.
  - Detail lisensi dapat dibaca di URL di atas.
  - Untuk update atau versi terbaru, kunjungi GitHub: https://github.com/sembre

  ------------------------------------------------------------------------
  🇬🇧 English Version:
  Description:
  This code reads the status of multiple digital pins on the Arduino and
  displays the active pin number (connected to GND) on a TM1637 7-segment
  display. The pin number is also printed on the Serial Monitor.

  Copyright & License:
  - You are free to use, share, modify, and redistribute this code for any
    purpose, including commercial use.
  - Condition: You **must give appropriate credit** to the original author
    (AGUS FITRIYANTO) and include this license statement.
  - For more details, please visit the license URL above.
  - For updates or the latest version, visit GitHub: https://github.com/sembre
  ------------------------------------------------------------------------
*/

// 2024-08-09 add serial monitor
// 2025-10-03 add boot counter with EEPROM and device lock

#include <TM1637Display.h>
#include <EEPROM.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Konstanta untuk boot counter
#define EEPROM_BOOT_ADDRESS 0 // Alamat EEPROM untuk menyimpan boot counter
#define MAX_BOOT_COUNT 7000   // Maksimal boot count sebelum terkunci
#define MAGIC_NUMBER 0xAB     // Magic number untuk validasi EEPROM

// Variabel global
unsigned int bootCount = 0;
bool deviceLocked = false;

// Custom segments untuk menampilkan "dIE" pada TM1637
const uint8_t dieSegments[] = {
    0x5E, // d
    0x30, // I
    0x79  // E
};

// Deklarasi pin secara individu
const int pins[] = {
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53,
    2, 3, 4, 5, 6};

// Fungsi untuk membaca boot count dari EEPROM
unsigned int readBootCount()
{
  unsigned int count = 0;

  // Cek magic number untuk validasi
  if (EEPROM.read(EEPROM_BOOT_ADDRESS + 4) != MAGIC_NUMBER)
  {
    // Jika magic number tidak cocok, inisialisasi EEPROM
    writeBootCount(0);
    return 0;
  }

  // Baca boot count (4 bytes)
  count = (unsigned int)EEPROM.read(EEPROM_BOOT_ADDRESS) |
          ((unsigned int)EEPROM.read(EEPROM_BOOT_ADDRESS + 1) << 8) |
          ((unsigned int)EEPROM.read(EEPROM_BOOT_ADDRESS + 2) << 16) |
          ((unsigned int)EEPROM.read(EEPROM_BOOT_ADDRESS + 3) << 24);

  return count;
}

// Fungsi untuk menulis boot count ke EEPROM
void writeBootCount(unsigned int count)
{
  // Tulis boot count (4 bytes)
  EEPROM.write(EEPROM_BOOT_ADDRESS, count & 0xFF);
  EEPROM.write(EEPROM_BOOT_ADDRESS + 1, (count >> 8) & 0xFF);
  EEPROM.write(EEPROM_BOOT_ADDRESS + 2, (count >> 16) & 0xFF);
  EEPROM.write(EEPROM_BOOT_ADDRESS + 3, (count >> 24) & 0xFF);

  // Tulis magic number untuk validasi
  EEPROM.write(EEPROM_BOOT_ADDRESS + 4, MAGIC_NUMBER);
}

// Fungsi untuk menampilkan "dIE" pada TM1637
void showDieMessage()
{
  tm1637.setSegments(dieSegments, 3, 1); // Tampilkan "dIE" mulai dari posisi 1
}

// Fungsi untuk increment boot count
void incrementBootCount()
{
  bootCount++;
  writeBootCount(bootCount);

  Serial.print("Boot count: ");
  Serial.print(bootCount);
  Serial.print("/");
  Serial.println(MAX_BOOT_COUNT);

  if (bootCount >= MAX_BOOT_COUNT)
  {
    deviceLocked = true;
    Serial.println("DEVICE LOCKED! Maximum boot count reached.");
  }
}

void setup()
{
  // Inisialisasi TM1637
  tm1637.setBrightness(7); // Set brightness level (0 - 7)

  // Inisialisasi Serial Monitor
  Serial.begin(9600);

  // Baca boot count dari EEPROM
  bootCount = readBootCount();

  // Increment boot count dan cek apakah device terkunci
  incrementBootCount();

  // Jika device terkunci, tampilkan "dIE" dan hentikan eksekusi
  if (deviceLocked)
  {
    showDieMessage();
    Serial.println("Device is permanently locked. Showing 'dIE' message.");
    while (1)
    {
      // Loop tak terbatas - device terkunci
      delay(1000);
    }
  }

  // Deklarasi pin sebagai INPUT_PULLUP agar pin terbaca HIGH saat tidak tersambung ke GND
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
  {
    pinMode(pins[i], INPUT_PULLUP);
  }

  Serial.println("Device initialized successfully.");
  Serial.println("Pin checker is ready.");
}

void loop()
{
  // Jika device terkunci, hanya tampilkan "dIE" dan tidak berfungsi
  if (deviceLocked)
  {
    showDieMessage();
    delay(1000);
    return;
  }

  // Cek setiap pin
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
  {
    if (digitalRead(pins[i]) == LOW)
    {
      // Jika pin terhubung ke GND, tampilkan angka pada TM1637 display
      int valueToShow = i + 1; // Start numbering from 1
      tm1637.showNumberDecEx(valueToShow);

      // Tampilkan angka juga di Serial Monitor
      Serial.print("Pin ");
      Serial.print(pins[i]);
      Serial.print(" connected, displaying number: ");
      Serial.println(valueToShow);

      delay(1000);    // Display for 1 second
      tm1637.clear(); // Clear the display after showing
      break;          // Exit loop after detecting the first active pin
    }
  }
}
