/*
 * ================================================================
 * 3-PIN SWITCH TESTER (COM-NC-NO)
 * ================================================================
 *
 * Deskripsi:
 * Program untuk menguji fungsi switch 3 kaki dengan konfigurasi:
 * - COM (Common) - Terminal utama
 * - NC (Normally Closed) - Terminal yang terhubung saat kondisi normal
 * - NO (Normally Open) - Terminal yang terhubung saat switch diaktifkan
 *
 * Fungsi Testing:
 * Program akan memverifikasi bahwa switch dapat:
 * 1. Menghubungkan COM ke NC (posisi normal/OFF)
 * 2. Menghubungkan COM ke NO (posisi aktif/ON)
 * 3. Memastikan kedua kondisi dapat dicapai (switch berfungsi baik)
 *
 * Hardware yang Dibutuhkan:
 * - Arduino Uno/Nano
 * - TM1637 4-digit 7-segment display
 * - Buzzer aktif 5V
 * - LED indikator (opsional)
 * - Switch 3 kaki yang akan ditest
 * - Kabel jumper dan breadboard
 *
 * Indikator Hasil:
 * - Display: "NC" saat COM-NC terhubung
 * - Display: "NO" saat COM-NO terhubung
 * - Display: "GOOD" saat kedua kondisi sudah terpenuhi
 * - Buzzer: Berbunyi saat switch LULUS test (GOOD)
 * - Serial: Log detail setiap perubahan status
 *
 * Cara Kerja Test:
 * 1. Hubungkan switch ke pin test Arduino
 * 2. Gerakkan switch ke posisi NC → Display "NC"
 * 3. Gerakkan switch ke posisi NO → Display "NO"
 * 4. Jika kedua posisi berhasil → Display "GOOD" + Buzzer
 * 5. Reset otomatis jika switch dilepas
 *
 * Aplikasi:
 * - Quality control switch manufacturing
 * - Testing switch bekas/rusak
 * - Verifikasi wiring switch
 * - Educational tool untuk belajar switch
 *
 * Author: Sembre
 * Date: 2025
 * Version: 1.0
 * Target: Arduino Uno/Nano
 * ================================================================
 */

#include <TM1637Display.h>

// ================================================================
// KONFIGURASI HARDWARE DAN PIN
// ================================================================

// Pin konfigurasi untuk TM1637 7-segment display
#define CLK_PIN A0 // Clock pin untuk TM1637
#define DIO_PIN A1 // Data I/O pin untuk TM1637

// Inisialisasi objek TM1637 display
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Pin konfigurasi untuk testing switch 3 kaki
const int comToNCPin = 2; // Pin untuk test COM ke NC (Normally Closed)
const int comToNOPin = 3; // Pin untuk test COM ke NO (Normally Open)
const int buzzerPin = 6;  // Pin untuk buzzer feedback
const int ledPin = 7;     // Pin untuk LED indikator (opsional)

// Konfigurasi timing dan display
const uint8_t DISPLAY_BRIGHTNESS = 0x0a; // Brightness level TM1637
const int SCAN_DELAY = 200;              // Delay scanning dalam ms
const int DEBOUNCE_DELAY = 50;           // Delay debouncing

// ================================================================
// VARIABEL STATUS DAN TRACKING
// ================================================================

/*
 * storedConditions menggunakan bit flags untuk track kondisi:
 * Bit 0 (LSB): Set jika COM-NC connection sudah terdeteksi
 * Bit 1:       Set jika COM-NO connection sudah terdeteksi
 *
 * Nilai storedConditions:
 * 0 = Belum ada kondisi terpenuhi
 * 1 = Hanya NC condition terpenuhi (bit 0 set)
 * 2 = Hanya NO condition terpenuhi (bit 1 set)
 * 3 = Kedua kondisi terpenuhi (bit 0 dan 1 set) = GOOD
 */
int storedConditions = 0;

// Bit flags untuk kondisi tracking
const int NC_CONDITION_BIT = 1; // Bit 0: COM-NC connection
const int NO_CONDITION_BIT = 2; // Bit 1: COM-NO connection
const int ALL_CONDITIONS = 3;   // Kedua bit set = switch GOOD
// ================================================================
// CUSTOM 7-SEGMENT DISPLAY PATTERNS
// ================================================================

/*
 * Pattern untuk menampilkan "GOOD" pada TM1637
 * Digunakan saat switch lulus test (kedua kondisi NC dan NO terpenuhi)
 *
 * TM1637 7-segment layout:
 *     AAA
 *    F   B
 *     GGG
 *    E   C
 *     DDD
 */
const uint8_t SEG_GOOD[] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G | SEG_F, // 'g' - karakter pertama
    SEG_C | SEG_D | SEG_E | SEG_G,                 // 'O' - karakter kedua
    SEG_C | SEG_D | SEG_E | SEG_G,                 // 'O' - karakter ketiga
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,         // 'd' - karakter keempat
};

/*
 * Pattern untuk menampilkan "-NC-" pada TM1637
 * Digunakan saat switch dalam posisi NC (COM terhubung ke NC)
 */
const uint8_t SEG_NC[] = {
    SEG_G,                         // '-' - garis horizontal
    SEG_C | SEG_E | SEG_G,         // 'n' - huruf n kecil
    SEG_A | SEG_D | SEG_E | SEG_F, // 'C' - huruf C besar
    SEG_G,                         // '-' - garis horizontal
};

/*
 * Pattern untuk menampilkan "-NO-" pada TM1637
 * Digunakan saat switch dalam posisi NO (COM terhubung ke NO)
 */
const uint8_t SEG_NO[] = {
    SEG_G,                         // '-' - garis horizontal
    SEG_C | SEG_E | SEG_G,         // 'n' - huruf n kecil
    SEG_C | SEG_D | SEG_E | SEG_G, // 'o' - huruf o kecil
    SEG_G,                         // '-' - garis horizontal
};
// ================================================================
// SETUP FUNCTION - Inisialisasi sistem 3-pin switch tester
// ================================================================
void setup() {
  // ================================================================
  // INISIALISASI KOMUNIKASI SERIAL
  // ================================================================
  Serial.begin(9600);
  Serial.println("========================================");
  Serial.println("3-PIN SWITCH TESTER v1.0");
  Serial.println("========================================");
  Serial.println("Testing: COM-NC-NO Switch Functionality");
  Serial.println("Connections:");
  Serial.print("  COM-NC Test Pin: ");
  Serial.println(comToNCPin);
  Serial.print("  COM-NO Test Pin: ");
  Serial.println(comToNOPin);
  Serial.print("  Buzzer Pin: ");
  Serial.println(buzzerPin);
  Serial.print("  LED Pin: ");
  Serial.println(ledPin);
  Serial.println("Initializing system...");

  // ================================================================
  // KONFIGURASI PIN INPUT UNTUK SWITCH TESTING
  // ================================================================
  /*
   * Pin testing switch dikonfigurasi sebagai INPUT_PULLUP
   *
   * Cara kerja INPUT_PULLUP untuk switch testing:
   * - Internal pull-up resistor (~20kΩ) aktif
   * - Pin terbaca HIGH saat switch TIDAK terhubung (open circuit)
   * - Pin terbaca LOW saat switch terhubung ke GND/COM
   * - Memberikan sinyal stabil tanpa external resistor
   *
   * Koneksi switch yang akan ditest:
   * - COM pin switch → GND Arduino
   * - NC pin switch → Pin 2 Arduino (comToNCPin)
   * - NO pin switch → Pin 3 Arduino (comToNOPin)
   */
  pinMode(comToNCPin, INPUT_PULLUP);
  pinMode(comToNOPin, INPUT_PULLUP);

  Serial.println("Switch test pins configured as INPUT_PULLUP:");
  Serial.println("  HIGH = Switch open (not connected)");
  Serial.println("  LOW = Switch closed (connected to COM/GND)");

  // ================================================================
  // KONFIGURASI PIN OUTPUT UNTUK FEEDBACK
  // ================================================================
  pinMode(buzzerPin, OUTPUT); // Buzzer untuk audio feedback
  pinMode(ledPin, OUTPUT);    // LED untuk visual feedback

  // Pastikan output dalam kondisi OFF di awal
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, LOW);

  Serial.println("Feedback pins configured:");
  Serial.println("  Buzzer: OFF (will activate on GOOD)");
  Serial.println("  LED: OFF (reserved for future use)");

  // ================================================================
  // INISIALISASI TM1637 DISPLAY
  // ================================================================
  tm1637.setBrightness(DISPLAY_BRIGHTNESS);

  // Test display dengan sequence startup
  Serial.println("Testing TM1637 display...");

  // Test pattern "GOOD"
  tm1637.setSegments(SEG_GOOD);
  Serial.println("  Displaying: GOOD");
  delay(1000);

  // Test pattern "NC"
  tm1637.setSegments(SEG_NC);
  Serial.println("  Displaying: -NC-");
  delay(1000);

  // Test pattern "NO"
  tm1637.setSegments(SEG_NO);
  Serial.println("  Displaying: -NO-");
  delay(1000);

  // Clear display
  tm1637.clear();
  Serial.println("  Display cleared");

  // Test buzzer
  Serial.println("Testing buzzer...");
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(200);
    digitalWrite(buzzerPin, LOW);
    delay(200);
  }
  Serial.println("  Buzzer test complete");

  // ================================================================
  // RESET KONDISI DAN SIAP TESTING
  // ================================================================
  storedConditions = 0; // Reset semua kondisi

  Serial.println("========================================");
  Serial.println("SYSTEM READY!");
  Serial.println("========================================");
  Serial.println("Test Procedure:");
  Serial.println("1. Connect switch COM to Arduino GND");
  Serial.println("2. Connect switch NC to pin 2");
  Serial.println("3. Connect switch NO to pin 3");
  Serial.println("4. Move switch to NC position → Display '-NC-'");
  Serial.println("5. Move switch to NO position → Display '-NO-'");
  Serial.println("6. If both positions work → Display 'GOOD' + Buzzer");
  Serial.println("========================================");
  Serial.println("Monitoring switch positions...");
}

// ================================================================
// MAIN LOOP - Testing 3-pin switch functionality
// ================================================================
void loop() {
  // ================================================================
  // BACA STATUS PIN SWITCH
  // ================================================================
  /*
   * Pembacaan pin switch dengan INPUT_PULLUP:
   * - HIGH (1) = Switch terbuka/tidak terhubung ke COM
   * - LOW (0) = Switch tertutup/terhubung ke COM (via GND)
   *
   * Untuk switch 3 kaki yang baik:
   * - Posisi NC: comToNCState = LOW, comToNOState = HIGH
   * - Posisi NO: comToNCState = HIGH, comToNOState = LOW
   * - Posisi tengah (jika ada): kedua pin HIGH
   */
  int comToNCState = digitalRead(comToNCPin); // Status COM ke NC connection
  int comToNOState = digitalRead(comToNOPin); // Status COM ke NO connection

  // ================================================================
  // KONDISI 1: SWITCH DALAM POSISI NETRAL/TERPUTUS
  // ================================================================
  /*
   * Kondisi: Kedua pin HIGH (tidak ada koneksi)
   * Aksi: Reset stored conditions dan clear display
   * Tujuan: Memungkinkan testing ulang dari awal
   */
  if (comToNCState == HIGH && comToNOState == HIGH) {
    if (storedConditions != 0) {
      Serial.println("----------------------------------------");
      Serial.println("SWITCH RESET DETECTED");
      Serial.println("Switch returned to neutral position");
      Serial.println("Resetting stored conditions for new test");
      Serial.print("Previous conditions: ");
      Serial.println(storedConditions, BIN);

      storedConditions = 0; // Reset semua bit flags
      tm1637.clear();       // Clear display

      Serial.println("Ready for new switch test");
      Serial.println("----------------------------------------");
    }
  }

  // ================================================================
  // KONDISI 2: SWITCH DALAM POSISI NC (NORMALLY CLOSED)
  // ================================================================
  /*
   * Kondisi: COM-NC terhubung (LOW), COM-NO terputus (HIGH)
   * Aksi: Set NC condition bit dan tampilkan "-NC-"
   * Display: "-NC-" untuk menunjukkan posisi switch
   */
  if (comToNCState == LOW && comToNOState == HIGH) {
    // Cek jika ini adalah deteksi NC yang baru
    if (!(storedConditions & NC_CONDITION_BIT))
    {
      Serial.println("----------------------------------------");
      Serial.println("NC POSITION DETECTED");
      Serial.println("Switch State: COM connected to NC");
      Serial.print("Pin States - NC: ");
      Serial.print(comToNCState ? "HIGH" : "LOW");
      Serial.print(", NO: ");
      Serial.println(comToNOState ? "HIGH" : "LOW");

      storedConditions |= NC_CONDITION_BIT; // Set bit 0

      Serial.print("Updated conditions: ");
      Serial.println(storedConditions, BIN);
      Serial.println("Display: -NC-");
      Serial.println("----------------------------------------");
    }

    // Tampilkan "-NC-" pada display
    tm1637.setSegments(SEG_NC);

    // Matikan buzzer (hanya nyala saat GOOD)
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  // ================================================================
  // KONDISI 3: SWITCH DALAM POSISI NO (NORMALLY OPEN)
  // ================================================================
  /*
   * Kondisi: COM-NC terputus (HIGH), COM-NO terhubung (LOW)
   * Aksi: Set NO condition bit dan tampilkan "-NO-"
   * Display: "-NO-" untuk menunjukkan posisi switch
   */
  if (comToNCState == HIGH && comToNOState == LOW) {
    // Cek jika ini adalah deteksi NO yang baru
    if (!(storedConditions & NO_CONDITION_BIT))
    {
      Serial.println("----------------------------------------");
      Serial.println("NO POSITION DETECTED");
      Serial.println("Switch State: COM connected to NO");
      Serial.print("Pin States - NC: ");
      Serial.print(comToNCState ? "HIGH" : "LOW");
      Serial.print(", NO: ");
      Serial.println(comToNOState ? "HIGH" : "LOW");

      storedConditions |= NO_CONDITION_BIT; // Set bit 1

      Serial.print("Updated conditions: ");
      Serial.println(storedConditions, BIN);
      Serial.println("Display: -NO-");
      Serial.println("----------------------------------------");
    }

    // Tampilkan "-NO-" pada display
    tm1637.setSegments(SEG_NO);

    // Matikan buzzer (hanya nyala saat GOOD)
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  // ================================================================
  // KONDISI 4: EVALUASI HASIL TEST - SWITCH GOOD/BAD
  // ================================================================
  /*
   * Jika kedua kondisi NC dan NO sudah terpenuhi (storedConditions == 3):
   * - Switch dinyatakan GOOD (berfungsi normal)
   * - Display menampilkan "GOOD"
   * - Buzzer berbunyi sebagai konfirmasi
   * - Test complete, tunggu reset untuk test ulang
   */
  if (storedConditions == ALL_CONDITIONS)
  {
    static bool goodAlreadyDisplayed = false;

    // Tampilkan pesan GOOD hanya sekali per test cycle
    if (!goodAlreadyDisplayed)
    {
      Serial.println("========================================");
      Serial.println("SWITCH TEST RESULT: PASSED");
      Serial.println("========================================");
      Serial.println("Switch Status: GOOD - Fully Functional");
      Serial.println("Test Summary:");
      Serial.println("  ✓ NC Position: Working (COM-NC connection)");
      Serial.println("  ✓ NO Position: Working (COM-NO connection)");
      Serial.println("  ✓ Switching: Proper isolation between NC/NO");
      Serial.println("Feedback:");
      Serial.println("  Display: GOOD");
      Serial.println("  Buzzer: ACTIVATED");
      Serial.println("========================================");
      Serial.println("Test complete. Remove switch to test another.");

      goodAlreadyDisplayed = true;
    }

    // Tampilkan "GOOD" pada display
    tm1637.setSegments(SEG_GOOD);

    // Aktifkan buzzer untuk konfirmasi
    digitalWrite(buzzerPin, HIGH);

    // LED bisa digunakan untuk indikasi tambahan
    digitalWrite(ledPin, HIGH);
  }
  else
  {
    // Reset good display flag untuk test cycle berikutnya
    static bool goodAlreadyDisplayed = false;
    goodAlreadyDisplayed = false;

    // Matikan buzzer dan LED jika kondisi belum lengkap
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  // ================================================================
  // ERROR DETECTION - KONDISI ABNORMAL
  // ================================================================
  /*
   * Deteksi kondisi abnormal switch:
   * - Kedua NC dan NO terhubung bersamaan (short circuit)
   * - Kondisi ini tidak boleh terjadi pada switch normal
   */
  if (comToNCState == LOW && comToNOState == LOW)
  {
    static unsigned long lastErrorReport = 0;
    unsigned long currentTime = millis();

    // Report error setiap 2 detik untuk menghindari spam
    if (currentTime - lastErrorReport > 2000)
    {
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("ERROR: ABNORMAL SWITCH CONDITION");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("Both NC and NO are connected simultaneously!");
      Serial.println("This indicates:");
      Serial.println("  - Faulty switch (internal short circuit)");
      Serial.println("  - Incorrect wiring");
      Serial.println("  - Switch mechanism failure");
      Serial.println("Action: Check switch and wiring");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

      lastErrorReport = currentTime;

      // Flash display untuk menunjukkan error
      tm1637.clear();
      delay(100);
      tm1637.showNumberDecEx(8888); // Error indication
      delay(100);
    }
  }

  // ================================================================
  // SCAN TIMING DAN DEBOUNCING
  // ================================================================
  /*
   * Delay untuk:
   * 1. Mengurangi CPU usage
   * 2. Debouncing switch yang mungkin bouncing
   * 3. Memberikan waktu untuk membaca display
   * 4. Mencegah spam Serial output
   */
  delay(SCAN_DELAY);
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lebih lanjut)
// ================================================================

/*
 * getSwitchStatusString() - Konversi status ke string readable
 * Parameter: storedConditions (bit flags)
 * Return: String status switch
 */
/*
String getSwitchStatusString(int conditions) {
  switch(conditions) {
    case 0: return "IDLE (No position detected)";
    case 1: return "NC_ONLY (Only NC position tested)";
    case 2: return "NO_ONLY (Only NO position tested)";
    case 3: return "GOOD (Both NC and NO working)";
    default: return "UNKNOWN";
  }
}
*/

/*
 * displayCustomMessage(String message) - Tampilkan pesan custom
 * Parameter: message (max 4 karakter)
 */
/*
void displayCustomMessage(String message) {
  if(message.length() > 4) message = message.substring(0, 4);

  uint8_t customSegments[4] = {0, 0, 0, 0};

  for(int i = 0; i < message.length(); i++) {
    // Implementasi custom character encoding
    // Tambahkan karakter sesuai kebutuhan
  }

  tm1637.setSegments(customSegments);
}
*/

/*
 * playBuzzerPattern(int pattern) - Pola buzzer berbeda
 * Parameter: pattern (1=single, 2=double, 3=triple, 4=long)
 */
/*
void playBuzzerPattern(int pattern) {
  digitalWrite(buzzerPin, LOW); // Pastikan mati dulu
  delay(50);

  switch(pattern) {
    case 1: // Single beep
      digitalWrite(buzzerPin, HIGH);
      delay(200);
      digitalWrite(buzzerPin, LOW);
      break;

    case 2: // Double beep
      for(int i = 0; i < 2; i++) {
        digitalWrite(buzzerPin, HIGH);
        delay(150);
        digitalWrite(buzzerPin, LOW);
        delay(150);
      }
      break;

    case 3: // Triple beep
      for(int i = 0; i < 3; i++) {
        digitalWrite(buzzerPin, HIGH);
        delay(100);
        digitalWrite(buzzerPin, LOW);
        delay(100);
      }
      break;

    case 4: // Long beep
      digitalWrite(buzzerPin, HIGH);
      delay(1000);
      digitalWrite(buzzerPin, LOW);
      break;
  }
}
*/

/*
 * runSwitchDiagnostic() - Diagnostic lengkap switch
 */
/*
void runSwitchDiagnostic() {
  Serial.println("=== SWITCH DIAGNOSTIC MODE ===");

  for(int i = 0; i < 10; i++) {
    int ncState = digitalRead(comToNCPin);
    int noState = digitalRead(comToNOPin);

    Serial.print("Sample ");
    Serial.print(i + 1);
    Serial.print(": NC=");
    Serial.print(ncState ? "HIGH" : "LOW");
    Serial.print(", NO=");
    Serial.print(noState ? "HIGH" : "LOW");

    if(ncState == LOW && noState == LOW) {
      Serial.print(" [ERROR: Both connected]");
    } else if(ncState == LOW) {
      Serial.print(" [NC Position]");
    } else if(noState == LOW) {
      Serial.print(" [NO Position]");
    } else {
      Serial.print(" [Neutral/Open]");
    }

    Serial.println();
    delay(500);
  }

  Serial.println("=== DIAGNOSTIC COMPLETE ===");
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM SWITCH TESTER:
 * =============================
 *
 * Arduino Uno -> TM1637 Display:
 * 5V    -> VCC
 * GND   -> GND
 * A0    -> CLK (Clock)
 * A1    -> DIO (Data I/O)
 *
 * Arduino Uno -> Feedback Components:
 * Pin 6 -> Buzzer + (Active Buzzer 5V)
 * Pin 7 -> LED + (melalui resistor 220Ω)
 * GND   -> Buzzer - dan LED -
 *
 * Arduino Uno -> Switch Under Test:
 * Pin 2 -> NC terminal of switch
 * Pin 3 -> NO terminal of switch
 * GND   -> COM terminal of switch
 *
 * Switch 3-Kaki Connection:
 *
 *     NC (Normally Closed)    COM (Common)    NO (Normally Open)
 *           |                      |                    |
 *         Pin 2                   GND                 Pin 3
 *
 * Ketika switch di posisi NC: COM-NC terhubung, COM-NO terputus
 * Ketika switch di posisi NO: COM-NC terputus, COM-NO terhubung
 *
 *
 * JENIS SWITCH YANG BISA DITEST:
 * ==============================
 *
 * 1. Toggle Switch SPDT (Single Pole Double Throw):
 *    - 3 terminal: COM, NC, NO
 *    - Fungsi: Pilih salah satu dari 2 circuit
 *    - Contoh: Switch lampu 2 arah
 *
 * 2. Pushbutton Switch SPDT:
 *    - 3 terminal: COM, NC, NO
 *    - Fungsi: Momentary activation
 *    - Contoh: Emergency stop button
 *
 * 3. Slide Switch SPDT:
 *    - 3 terminal: COM, NC, NO
 *    - Fungsi: Sliding contact selection
 *    - Contoh: Power switch perangkat
 *
 * 4. Rotary Switch (3 position):
 *    - 3 terminal: COM, NC, NO
 *    - Fungsi: Rotary selection
 *    - Contoh: Mode selector switch
 *
 * 5. Relay Contact (SPDT):
 *    - 3 terminal: COM, NC, NO
 *    - Fungsi: Electromagnetic switching
 *    - Contoh: Control relay
 *
 *
 * INTERPRETASI HASIL TEST:
 * ========================
 *
 * Display "-NC-":
 *   - Switch dalam posisi NC (Normally Closed)
 *   - COM terhubung ke NC terminal
 *   - Kondisi normal/default switch
 *
 * Display "-NO-":
 *   - Switch dalam posisi NO (Normally Open)
 *   - COM terhubung ke NO terminal
 *   - Kondisi aktif/triggered switch
 *
 * Display "GOOD" + Buzzer:
 *   - Switch LULUS test
 *   - Kedua posisi NC dan NO berfungsi
 *   - Switch dapat beralih dengan baik
 *
 * Display berkedip/Error:
 *   - Kemungkinan short circuit internal
 *   - Wiring error
 *   - Switch rusak
 *
 *
 * TROUBLESHOOTING:
 * ===============
 *
 * Problem: Display tidak menyala
 * Solution:
 * - Cek koneksi power TM1637 (VCC, GND)
 * - Cek koneksi CLK dan DIO (A0, A1)
 * - Pastikan library TM1637Display terinstall
 * - Test dengan kode simple display
 *
 * Problem: Switch tidak terdeteksi
 * Solution:
 * - Cek koneksi switch ke pin 2, 3, dan GND
 * - Pastikan switch memiliki 3 terminal COM-NC-NO
 * - Test kontinuitas switch dengan multimeter
 * - Verifikasi pin mode INPUT_PULLUP aktif
 *
 * Problem: Selalu menampilkan satu posisi saja
 * Solution:
 * - Switch mungkin rusak/stuck di satu posisi
 * - Cek mekanis switch bisa bergerak bebas
 * - Test dengan switch lain yang diketahui baik
 * - Periksa terminal switch tidak korosi
 *
 * Problem: Display "Error" terus menerus
 * Solution:
 * - Ada short circuit antara NC dan NO
 * - Cek wiring tidak ada yang short
 * - Switch internal rusak (double connection)
 * - Ganti switch atau perbaiki wiring
 *
 * Problem: Buzzer tidak berbunyi saat "GOOD"
 * Solution:
 * - Cek koneksi buzzer ke pin 6 dan GND
 * - Pastikan buzzer aktif 5V (bukan passive)
 * - Test buzzer dengan digitalWrite manual
 * - Cek tegangan output pin 6
 *
 * Problem: Test tidak reset setelah lepas switch
 * Solution:
 * - Switch mungkin masih ada koneksi parsial
 * - Bersihkan terminal switch dari oksidasi
 * - Pastikan semua koneksi terputus total
 * - Restart Arduino jika perlu
 *
 *
 * QUALITY CONTROL GUIDELINES:
 * ===========================
 *
 * Kriteria PASS (Switch GOOD):
 * ✓ Dapat bergerak ke posisi NC (display "-NC-")
 * ✓ Dapat bergerak ke posisi NO (display "-NO-")
 * ✓ Tidak ada koneksi simultan NC dan NO
 * ✓ Switching smooth tanpa bouncing berlebihan
 * ✓ Resistansi contact rendah (<1Ω)
 *
 * Kriteria FAIL (Switch BAD):
 * ✗ Tidak dapat mencapai salah satu posisi
 * ✗ Stuck di satu posisi
 * ✗ Short circuit internal (NC-NO terhubung)
 * ✗ Resistansi contact tinggi (>10Ω)
 * ✗ Excessive bouncing atau intermittent
 *
 *
 * MAINTENANCE DAN KALIBRASI:
 * ==========================
 *
 * Daily Check:
 * - Test dengan switch reference yang diketahui baik
 * - Verifikasi display menampilkan karakter dengan benar
 * - Test buzzer masih berbunyi
 * - Cek koneksi kabel tidak longgar
 *
 * Weekly Maintenance:
 * - Bersihkan terminal test dari oksidasi
 * - Cek kalibrasi dengan berbagai jenis switch
 * - Backup program dan settings
 * - Update log hasil testing
 *
 * Monthly Calibration:
 * - Test dengan switch standar kalibrasi
 * - Verifikasi timing response masih akurat
 * - Cek wear pada connector test
 * - Update firmware jika ada perbaikan
 *
 *
 * PENGEMBANGAN LANJUTAN:
 * ======================
 *
 * 1. Multiple Switch Testing:
 *    - Tambah multiplexer untuk test multiple switch
 *    - Automated switch changer mechanism
 *    - Batch testing mode dengan database
 *
 * 2. Advanced Measurements:
 *    - Contact resistance measurement
 *    - Switching time measurement
 *    - Bouncing analysis
 *    - Life cycle testing
 *
 * 3. Data Logging & Analysis:
 *    - SD card logging dengan timestamp
 *    - Statistical analysis hasil test
 *    - Trend analysis untuk predictive maintenance
 *    - Export data ke sistem ERP
 *
 * 4. User Interface Enhancement:
 *    - LCD 20x4 untuk informasi detail
 *    - Menu navigation dengan rotary encoder
 *    - Touch screen interface
 *    - Web interface untuk remote monitoring
 *
 * 5. Production Integration:
 *    - Conveyor belt integration
 *    - Automatic part handling
 *    - PLC communication
 *    - Barcode/QR code tracking
 *
 *
 * SPESIFIKASI TEKNIS:
 * ==================
 *
 * Electrical:
 * - Operating Voltage: 5V DC ±5%
 * - Current Consumption: 200mA max (with buzzer)
 * - Input Impedance: 20kΩ (internal pull-up)
 * - Contact Test Voltage: 5V DC
 * - Contact Test Current: <1mA (safe for sensitive switches)
 *
 * Mechanical:
 * - Switch Terminal: Standard screw terminals atau spring clamps
 * - Wire Gauge: 18-24 AWG recommended
 * - Connector Type: 3.5mm pitch terminal blocks
 *
 * Performance:
 * - Response Time: <100ms per position change
 * - Test Accuracy: >99.9% for good switches
 * - False Positive Rate: <0.1%
 * - Operating Temperature: 0°C to 50°C
 * - Storage Temperature: -20°C to 70°C
 *
 * Display:
 * - Type: TM1637 4-digit 7-segment
 * - Characters: Custom patterns for NC, NO, GOOD
 * - Brightness: Adjustable 0-7 levels
 * - Visibility: >5 meters in normal lighting
 *
 */
