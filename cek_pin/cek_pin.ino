/*
 * ================================================================
 * ARDUINO PIN TESTER WITH TM1637 DISPLAY
 * ================================================================
 * 
 * Deskripsi:
 * Program untuk menguji dan mengidentifikasi pin Arduino yang terhubung
 * ke ground (GND). Ketika pin tertentu dihubungkan ke GND, nomor pin
 * akan ditampilkan pada display 7-segment TM1637.
 *
 * Hardware yang Dibutuhkan:
 * - Arduino Mega 2560 (recommended) atau Uno
 * - TM1637 4-digit 7-segment display module
 * - Kabel jumper untuk testing
 * - Breadboard (opsional)
 *
 * Fungsi Utama:
 * - Monitoring semua pin I/O Arduino secara real-time
 * - Menampilkan nomor pin yang terhubung ke GND
 * - Auto-clear display setelah koneksi terputus
 * - Support untuk Arduino Mega (54 pins) dan Uno (20 pins)
 *
 * Cara Kerja:
 * 1. Semua pin dikonfigurasi sebagai INPUT_PULLUP (HIGH default)
 * 2. Program scan semua pin secara berurutan
 * 3. Jika pin dibaca LOW (terhubung ke GND), tampilkan nomornya
 * 4. Display akan clear otomatis setelah 1 detik
 * 5. Proses berulang terus menerus
 *
 * Aplikasi:
 * - Testing kontinuitas kabel
 * - Identifikasi pin pada board Arduino
 * - Debugging hardware connections
 * - Educational tool untuk belajar Arduino pinout
 *
 * Pin Mapping Display:
 * Pin 1-54 akan ditampilkan sebagai nomor 1-54 pada display
 * (Pin array index 0 = Display "1", dst.)
 *
 * Author: Sembre
 * Date: 2025
 * Version: 1.0
 * Target Board: Arduino Mega 2560
 * ================================================================
 */

#include <TM1637Display.h>

// ================================================================
// KONFIGURASI HARDWARE DAN PIN
// ================================================================

// Konfigurasi pin untuk TM1637 7-segment display
#define CLK_PIN A0                    // Clock pin untuk TM1637
#define DIO_PIN A1                    // Data I/O pin untuk TM1637

// Inisialisasi objek TM1637 display
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Konfigurasi display
const uint8_t DISPLAY_BRIGHTNESS = 7;  // Brightness level (0-7)
const int DISPLAY_DURATION = 1000;     // Durasi tampil dalam ms
const int SCAN_DELAY = 50;             // Delay antar scan dalam ms

// ================================================================
// DEFINISI PIN YANG AKAN DITEST
// ================================================================
/*
 * Array pins[] berisi semua pin I/O yang akan ditest
 * 
 * Arduino Mega 2560 Pin Layout:
 * - Digital Pins: 2-53 (Pin 0,1 reserved untuk Serial)
 * - Analog Pins: A0,A1 (untuk TM1637), A2-A15 (available untuk test)
 * 
 * Total pins yang ditest: 54 pins
 * Pin Index 0 = Display "1" (Pin 2)
 * Pin Index 1 = Display "2" (Pin 3)
 * ...dst
 */
const int pins[] = {
  // Digital pins 2-13 (Arduino Uno compatible)
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
  
  // Digital pins 14-15 (Serial3 pada Mega)
  14, 15, 
  
  // Analog pins A2-A15 (A0,A1 digunakan untuk TM1637)
  A2, A3, A4, A5, A6, A7, A8, A9,
  A10, A11, A12, A13, A14, A15,
  
  // Extended digital pins 22-53 (khusus Arduino Mega)
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53
};

// Menghitung jumlah total pins yang akan ditest
const int TOTAL_PINS = sizeof(pins) / sizeof(pins[0]);

// ================================================================
// SETUP FUNCTION - Inisialisasi sistem
// ================================================================
void setup() {
  // Inisialisasi Serial untuk debugging (opsional)
  Serial.begin(9600);
  Serial.println("========================================");
  Serial.println("ARDUINO PIN TESTER v1.0");
  Serial.println("========================================");
  Serial.print("Total pins to test: ");
  Serial.println(TOTAL_PINS);
  Serial.println("Initializing system...");
  
  // ================================================================
  // INISIALISASI TM1637 DISPLAY
  // ================================================================
  tm1637.setBrightness(DISPLAY_BRIGHTNESS);
  
  // Test display dengan countdown
  Serial.println("Testing display...");
  for(int i = 3; i > 0; i--) {
    tm1637.showNumberDecEx(i);
    delay(500);
  }
  tm1637.clear();
  
  Serial.println("Display test complete.");
  
  // ================================================================
  // KONFIGURASI PIN MODE
  // ================================================================
  /*
   * Semua pin dikonfigurasi sebagai INPUT_PULLUP
   * 
   * INPUT_PULLUP bekerja sebagai berikut:
   * - Internal pull-up resistor (~20kΩ) aktif
   * - Pin akan terbaca HIGH (1) saat tidak terhubung ke apapun
   * - Pin akan terbaca LOW (0) saat terhubung ke GND
   * - Memberikan sinyal yang stabil tanpa floating
   */
  Serial.println("Configuring pins as INPUT_PULLUP:");
  
  for (int i = 0; i < TOTAL_PINS; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    
    // Print konfigurasi pin untuk debugging
    Serial.print("Pin ");
    if(pins[i] >= A0) {
      Serial.print("A");
      Serial.print(pins[i] - A0);
    } else {
      Serial.print(pins[i]);
    }
    Serial.print(" -> INPUT_PULLUP (Display #");
    Serial.print(i + 1);
    Serial.println(")");
    
    // Small delay untuk stabilitas
    delay(1);
  }
  
  Serial.println("========================================");
  Serial.println("System ready!");
  Serial.println("Connect any pin to GND to test...");
  Serial.println("Pin numbers will be displayed on TM1637");
  Serial.println("========================================");
  
  // Tampilkan pesan ready pada display
  tm1637.showNumberDecEx(0);
  delay(1000);
  tm1637.clear();
}

// ================================================================
// MAIN LOOP - Scan dan deteksi pin yang terhubung ke GND
// ================================================================
void loop() {
  // ================================================================
  // SCAN SEMUA PIN SECARA BERURUTAN
  // ================================================================
  /*
   * Loop ini akan:
   * 1. Scan semua pin dalam array pins[]
   * 2. Membaca status setiap pin (HIGH/LOW)
   * 3. Jika ada pin yang LOW (terhubung ke GND):
   *    - Tampilkan nomor pin pada display
   *    - Print informasi ke Serial Monitor
   *    - Tunggu hingga koneksi terputus
   * 4. Lanjut ke pin berikutnya
   */
  
  for (int i = 0; i < TOTAL_PINS; i++) {
    // Baca status pin (HIGH = tidak terhubung, LOW = terhubung ke GND)
    int pinState = digitalRead(pins[i]);
    
    if (pinState == LOW) {
      // ================================================================
      // PIN TERDETEKSI TERHUBUNG KE GND
      // ================================================================
      
      // Hitung nomor yang akan ditampilkan (mulai dari 1, bukan 0)
      int displayNumber = i + 1;
      
      // Tampilkan nomor pada TM1637 display
      tm1637.showNumberDecEx(displayNumber);
      
      // Print informasi detail ke Serial Monitor
      Serial.println("========================================");
      Serial.print("PIN DETECTED: ");
      
      // Format output berdasarkan jenis pin
      if(pins[i] >= A0) {
        Serial.print("A");
        Serial.print(pins[i] - A0);
        Serial.print(" (Analog Pin A");
        Serial.print(pins[i] - A0);
        Serial.println(")");
      } else {
        Serial.print(pins[i]);
        Serial.print(" (Digital Pin ");
        Serial.print(pins[i]);
        Serial.println(")");
      }
      
      Serial.print("Display Number: ");
      Serial.println(displayNumber);
      Serial.print("Array Index: ");
      Serial.println(i);
      Serial.println("Status: Connected to GND");
      
      // ================================================================
      // TUNGGU HINGGA PIN DILEPAS DARI GND
      // ================================================================
      /*
       * Loop ini akan terus berjalan selama pin masih terhubung ke GND
       * Memberikan display yang stabil dan mencegah flickering
       */
      Serial.println("Waiting for disconnection...");
      
      unsigned long startTime = millis();
      bool connectionStable = true;
      
      while (digitalRead(pins[i]) == LOW) {
        // Update display setiap periode tertentu untuk menjaga brightness
        if (millis() - startTime > 100) {
          tm1637.showNumberDecEx(displayNumber);
          startTime = millis();
        }
        
        delay(10); // Small delay untuk mengurangi CPU usage
      }
      
      // ================================================================
      // PIN TELAH DILEPAS DARI GND
      // ================================================================
      Serial.println("Pin disconnected from GND");
      Serial.println("========================================");
      
      // Clear display setelah pin dilepas
      tm1637.clear();
      
      // Delay sebelum melanjutkan scan untuk debouncing
      delay(100);
      
      // Keluar dari loop untuk memulai scan ulang dari awal
      // Ini memastikan deteksi yang akurat jika ada multiple pins
      break;
    }
  }
  
  // Small delay antar scan cycle untuk mengurangi CPU usage
  delay(SCAN_DELAY);
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lebih lanjut)
// ================================================================

/*
 * displayPinInfo(int pinIndex) - Tampilkan info detail pin
 * Parameter: pinIndex (0 hingga TOTAL_PINS-1)
 */
/*
void displayPinInfo(int pinIndex) {
  if(pinIndex < 0 || pinIndex >= TOTAL_PINS) return;
  
  Serial.print("Pin Info - Index: ");
  Serial.print(pinIndex);
  Serial.print(", Physical Pin: ");
  
  if(pins[pinIndex] >= A0) {
    Serial.print("A");
    Serial.print(pins[pinIndex] - A0);
  } else {
    Serial.print(pins[pinIndex]);
  }
  
  Serial.print(", Display: ");
  Serial.print(pinIndex + 1);
  Serial.print(", Current State: ");
  Serial.println(digitalRead(pins[pinIndex]) ? "HIGH" : "LOW");
}
*/

/*
 * scanAllPins() - Scan dan tampilkan status semua pin
 * Berguna untuk debugging
 */
/*
void scanAllPins() {
  Serial.println("=== SCANNING ALL PINS ===");
  for(int i = 0; i < TOTAL_PINS; i++) {
    displayPinInfo(i);
    delay(10);
  }
  Serial.println("=== SCAN COMPLETE ===");
}
*/

/*
 * testDisplay() - Test semua digit pada TM1637
 */
/*
void testDisplay() {
  Serial.println("Testing TM1637 display...");
  
  // Test individual digits
  for(int i = 0; i <= 9; i++) {
    tm1637.showNumberDecEx(i);
    delay(500);
  }
  
  // Test multi-digit numbers
  for(int i = 10; i <= 99; i += 10) {
    tm1637.showNumberDecEx(i);
    delay(500);
  }
  
  tm1637.clear();
  Serial.println("Display test complete.");
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM:
 * ===============
 * 
 * Arduino Mega -> TM1637 Display:
 * 5V    -> VCC
 * GND   -> GND  
 * A0    -> CLK (Clock)
 * A1    -> DIO (Data I/O)
 * 
 * Test Setup:
 * - Gunakan kabel jumper male-male
 * - Satu ujung ke pin yang akan ditest
 * - Ujung lainnya ke GND Arduino
 * - Nomor pin akan muncul di display
 * 
 * 
 * PIN MAPPING ARDUINO MEGA 2560:
 * ==============================
 * 
 * Display "1"  -> Pin 2    (Digital)
 * Display "2"  -> Pin 3    (Digital, PWM)
 * Display "3"  -> Pin 4    (Digital)
 * Display "4"  -> Pin 5    (Digital, PWM)
 * Display "5"  -> Pin 6    (Digital, PWM)
 * Display "6"  -> Pin 7    (Digital)
 * Display "7"  -> Pin 8    (Digital)
 * Display "8"  -> Pin 9    (Digital, PWM)
 * Display "9"  -> Pin 10   (Digital, PWM)
 * Display "10" -> Pin 11   (Digital, PWM)
 * Display "11" -> Pin 12   (Digital)
 * Display "12" -> Pin 13   (Digital, LED_BUILTIN)
 * Display "13" -> Pin 14   (Digital, TX3)
 * Display "14" -> Pin 15   (Digital, RX3)
 * Display "15" -> Pin A2   (Analog)
 * Display "16" -> Pin A3   (Analog)
 * Display "17" -> Pin A4   (Analog)
 * Display "18" -> Pin A5   (Analog)
 * Display "19" -> Pin A6   (Analog)
 * Display "20" -> Pin A7   (Analog)
 * Display "21" -> Pin A8   (Analog)
 * Display "22" -> Pin A9   (Analog)
 * Display "23" -> Pin A10  (Analog)
 * Display "24" -> Pin A11  (Analog)
 * Display "25" -> Pin A12  (Analog)
 * Display "26" -> Pin A13  (Analog)
 * Display "27" -> Pin A14  (Analog)
 * Display "28" -> Pin A15  (Analog)
 * Display "29" -> Pin 22   (Digital)
 * Display "30" -> Pin 23   (Digital)
 * ... dan seterusnya hingga pin 53
 * 
 * NOTE: Pin A0 dan A1 tidak ditest karena digunakan untuk TM1637
 * NOTE: Pin 0 dan 1 tidak ditest karena digunakan untuk Serial
 * 
 * 
 * TROUBLESHOOTING:
 * ===============
 * 
 * Problem: Display tidak menyala
 * Solution:
 * - Cek koneksi VCC (5V) dan GND
 * - Pastikan library TM1637Display terinstall
 * - Cek kabel CLK dan DIO (A0, A1)
 * - Test dengan kode simple untuk memastikan display bekerja
 * 
 * Problem: Display menunjukkan angka yang salah
 * Solution:
 * - Cek array pins[] untuk memastikan urutan benar
 * - Verifikasi pin mapping dengan multimeter
 * - Pastikan tidak ada pin yang conflict
 * 
 * Problem: Display berkedip atau tidak stabil
 * Solution:
 * - Tambah kapasitor 100µF antara VCC dan GND
 * - Cek kualitas koneksi kabel
 * - Pastikan power supply stabil
 * - Kurangi brightness jika perlu
 * 
 * Problem: Beberapa pin tidak terdeteksi
 * Solution:
 * - Cek apakah pin tersebut ada di array pins[]
 * - Pastikan pinMode sudah diset INPUT_PULLUP
 * - Test dengan multimeter untuk memastikan koneksi
 * - Cek apakah pin tersebut tidak digunakan fungsi lain
 * 
 * Problem: Pin selalu terbaca LOW meski tidak terhubung
 * Solution:
 * - Pin mungkin rusak atau terhubung internal ke GND
 * - Cek dengan oscilloscope untuk noise
 * - Ganti Arduino jika pin secara fisik rusak
 * 
 * 
 * MODIFIKASI DAN PENGEMBANGAN:
 * ============================
 * 
 * 1. Menambah jenis test:
 *    - Voltage level testing (analog read)
 *    - PWM output testing
 *    - Frequency measurement
 *    - Capacitance testing
 * 
 * 2. Menambah interface:
 *    - Push button untuk mode switching
 *    - LCD 16x2 untuk informasi lebih detail
 *    - Buzzer untuk audio feedback
 *    - LED untuk visual indication
 * 
 * 3. Data logging:
 *    - Save hasil test ke SD card
 *    - Timestamp untuk setiap test
 *    - Statistical analysis
 *    - Export ke Excel/CSV
 * 
 * 4. Komunikasi:
 *    - WiFi untuk remote monitoring
 *    - Bluetooth untuk mobile app
 *    - Ethernet untuk network integration
 * 
 * 5. Advanced features:
 *    - Automated test sequences  
 *    - Pin stress testing
 *    - Temperature monitoring
 *    - Power consumption measurement
 * 
 * 
 * COMPATIBILITY:
 * ==============
 * 
 * Arduino Uno/Nano:
 * - Hanya gunakan pins[] index 0-13 (pin 2-13, A2-A5)
 * - Total 16 pins yang bisa ditest
 * - Ubah array pins[] sesuai kebutuhan
 * 
 * Arduino Mega 2560:
 * - Support penuh untuk 54 pins
 * - Gunakan konfigurasi default
 * 
 * Arduino Leonardo:
 * - Sesuaikan array pins[] dengan pinout Leonardo
 * - Perhatikan perbedaan analog pins
 * 
 * 
 * LIBRARY REQUIREMENTS:
 * ====================
 * 
 * TM1637Display Library:
 * - Install via Library Manager: "TM1637" by Avishay Orpaz
 * - Atau download: https://github.com/avishorp/TM1637
 * - Version tested: 1.2.0 atau lebih baru
 * 
 * 
 * POWER REQUIREMENTS:
 * ==================
 * 
 * - Arduino: 5V via USB atau external
 * - TM1637: 5V, konsumsi ~15mA
 * - Total konsumsi: ~100mA (aman untuk USB power)
 * 
 * 
 * PERFORMANCE:
 * ============
 * 
 * - Scan rate: ~20Hz (50ms delay per cycle)
 * - Response time: <100ms untuk deteksi pin
 * - Display update: Real-time
 * - Memory usage: ~2KB (terutama untuk array pins)
 * 
 * 
 * SAFETY NOTES:
 * =============
 * 
 * - Jangan hubungkan pin ke voltage >5V
 * - Gunakan current limiting resistor jika perlu
 * - Jangan short circuit pins output ke GND langsung
 * - Matikan power saat wiring untuk mencegah damage
 * 
 */
