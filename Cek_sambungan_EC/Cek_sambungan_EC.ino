/*
 * ================================================================
 * EC (ELECTRONIC COMPONENT) CONNECTION TESTER
 * ================================================================
 *
 * Deskripsi:
 * Program untuk menguji sambungan konektor EC (Electronic Component)
 * dengan sistem penomoran A/B. Setiap pasangan pin diberi identitas
 * unik dengan format "A1", "B1", "A2", "B2", dst. Program menampilkan
 * kode sambungan pada display 7-segment dan memberikan feedback audio.
 *
 * Hardware yang Dibutuhkan:
 * - Arduino Mega 2560 (recommended untuk pin yang banyak)
 * - TM1637 4-digit 7-segment display module
 * - Buzzer aktif (5V)
 * - Kabel jumper untuk testing konektor EC
 * - Breadboard (opsional)
 *
 * Fitur Utama:
 * - Testing 37 pin konektor EC (22-53, 2-6)
 * - Penomoran A/B alternating (A1, B1, A2, B2, ...)
 * - Display kode sambungan pada TM1637
 * - Audio feedback dengan buzzer
 * - Serial monitoring untuk debugging
 * - Anti-bouncing dan debouncing
 *
 * Sistem Penomoran:
 * Pin Index 0 (Pin 22) -> Display "A1"
 * Pin Index 1 (Pin 23) -> Display "B1"
 * Pin Index 2 (Pin 24) -> Display "A2"
 * Pin Index 3 (Pin 25) -> Display "B2"
 * ...dan seterusnya
 *
 * Cara Kerja:
 * 1. Semua pin dikonfigurasi sebagai INPUT_PULLUP
 * 2. Program scan pin secara berurutan
 * 3. Jika pin terhubung ke GND, tampilkan kode A/B+number
 * 4. Buzzer berbunyi setiap kali ada deteksi
 * 5. Display akan clear jika tidak ada koneksi
 *
 * Aplikasi:
 * - Testing konektor EC pada PCB
 * - Verifikasi wiring harness
 * - Quality control produksi
 * - Maintenance dan repair elektronik
 *
 * Author: Sembre
 * Date: 2025
 * Version: 1.0
 * Target: Arduino Mega 2560
 * ================================================================
 */

#include <TM1637Display.h>

// ================================================================
// KONFIGURASI HARDWARE DAN PIN
// ================================================================

// Pin konfigurasi untuk TM1637 7-segment display
#define CLK_PIN A0    // Clock pin untuk TM1637
#define DIO_PIN A1    // Data I/O pin untuk TM1637
#define BUZZER_PIN A2 // Pin untuk buzzer feedback

// Inisialisasi objek TM1637 display
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Konfigurasi display dan timing
const uint8_t DISPLAY_BRIGHTNESS = 7; // Brightness level (0-7)
const int BUZZER_DURATION = 100;      // Durasi buzzer dalam ms
const int DEBOUNCE_DELAY = 500;       // Delay untuk debouncing
const int SCAN_DELAY = 10;            // Delay antar scan

// ================================================================
// DEFINISI PIN KONEKTOR EC YANG AKAN DITEST
// ================================================================
/*
 * Array pins[] berisi pin-pin konektor EC yang akan ditest
 *
 * Layout pin konektor EC:
 * - Pin 22-53: Konektor EC utama (32 pins)
 * - Pin 2-6:   Konektor EC tambahan (5 pins)
 * Total: 37 pins konektor EC
 *
 * Sistem Penomoran Alternating A/B:
 * Index 0 (Pin 22) = A1    Index 1 (Pin 23) = B1
 * Index 2 (Pin 24) = A2    Index 3 (Pin 25) = B2
 * Index 4 (Pin 26) = A3    Index 5 (Pin 27) = B3
 * ...dst
 */
const int pins[] = {
    // Konektor EC utama - Pin 22-53 (Arduino Mega extended pins)
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53,

    // Konektor EC tambahan - Pin 2-6 (Arduino Uno compatible pins)
    2, 3, 4, 5, 6};

// Menghitung total pin yang akan ditest
const int TOTAL_PINS = sizeof(pins) / sizeof(pins[0]);

// Variabel untuk tracking pin yang terakhir terdeteksi
static int lastDetectedPin = -1;

// ================================================================
// SETUP FUNCTION - Inisialisasi sistem EC tester
// ================================================================
void setup() {
  // ================================================================
  // INISIALISASI KOMUNIKASI SERIAL
  // ================================================================
  Serial.begin(9600);
  Serial.println("========================================");
  Serial.println("EC CONNECTION TESTER v1.0");
  Serial.println("========================================");
  Serial.print("Total EC pins to test: ");
  Serial.println(TOTAL_PINS);
  Serial.println("Pin mapping: A/B alternating system");
  Serial.println("Initializing system...");

  // ================================================================
  // INISIALISASI TM1637 DISPLAY
  // ================================================================
  tm1637.setBrightness(DISPLAY_BRIGHTNESS);

  // Test display dengan sequence A-b-1-2
  Serial.println("Testing display...");

  // Test karakter A
  uint8_t testA[4] = {0b11110111, 0, 0, 0}; // Karakter 'A'
  tm1637.setSegments(testA);
  delay(500);

  // Test karakter b
  uint8_t testB[4] = {0b11111100, 0, 0, 0}; // Karakter 'b'
  tm1637.setSegments(testB);
  delay(500);

  // Test angka 1-9
  for (int i = 1; i <= 9; i++)
  {
    tm1637.showNumberDecEx(i);
    delay(300);
  }

  tm1637.clear();
  Serial.println("Display test complete.");

  // ================================================================
  // INISIALISASI BUZZER
  // ================================================================
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Pastikan buzzer mati di awal

  // Test buzzer dengan 3 beep
  Serial.println("Testing buzzer...");
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
  Serial.println("Buzzer test complete.");

  // ================================================================
  // KONFIGURASI PIN KONEKTOR EC
  // ================================================================
  /*
   * Semua pin konektor EC dikonfigurasi sebagai INPUT_PULLUP
   *
   * INPUT_PULLUP bekerja sebagai berikut:
   * - Internal pull-up resistor (~20kΩ) aktif
   * - Pin terbaca HIGH saat tidak terhubung (floating)
   * - Pin terbaca LOW saat terhubung ke GND
   * - Memberikan sinyal stabil untuk testing kontinuitas
   */
  Serial.println("Configuring EC connector pins:");
  Serial.println("Pin -> Display Code (Physical Pin)");
  Serial.println("-----------------------------------");

  for (int i = 0; i < TOTAL_PINS; i++)
  {
    pinMode(pins[i], INPUT_PULLUP);

    // Generate display code A/B + number
    char suffix = (i % 2 == 0) ? 'A' : 'B';
    int number = (i / 2) + 1;

    // Print mapping untuk reference
    Serial.print("Pin ");
    Serial.print(pins[i]);
    Serial.print(" -> ");
    Serial.print(suffix);
    Serial.print(number);
    Serial.print(" (Physical pin ");
    Serial.print(pins[i]);
    Serial.println(")");

    delay(1); // Small delay untuk stabilitas
  }

  Serial.println("-----------------------------------");
  Serial.println("System ready!");
  Serial.println("Connect EC pins to GND to test...");
  Serial.println("Format: A1, B1, A2, B2, etc.");
  Serial.println("========================================");

  // Reset variabel tracking
  lastDetectedPin = -1;

  // Tampilkan ready indication
  uint8_t readyMsg[4] = {0b00000000, 0b00000000, 0b00000000, 0b00000000};
  tm1637.setSegments(readyMsg);
  delay(1000);
  tm1637.clear();
}

// ================================================================
// MAIN LOOP - Scan dan deteksi koneksi EC
// ================================================================
void loop() {
  // ================================================================
  // SCAN SEMUA PIN KONEKTOR EC
  // ================================================================
  /*
   * Loop utama yang melakukan:
   * 1. Scan semua pin dalam array pins[]
   * 2. Deteksi pin yang terhubung ke GND (LOW state)
   * 3. Generate kode A/B berdasarkan index pin
   * 4. Tampilkan kode pada TM1637 display
   * 5. Berikan feedback audio dengan buzzer
   * 6. Anti-bouncing untuk stabilitas
   */

  for (int i = 0; i < TOTAL_PINS; i++)
  {
    // Baca status pin (HIGH = tidak terhubung, LOW = terhubung ke GND)
    int pinState = digitalRead(pins[i]);

    if (pinState == LOW)
    {
      // ================================================================
      // PIN KONEKTOR EC TERDETEKSI TERHUBUNG
      // ================================================================

      // Cek apakah ini pin yang sama dengan deteksi sebelumnya
      // Untuk mencegah multiple trigger dan bouncing
      if (pins[i] != lastDetectedPin)
      {
        lastDetectedPin = pins[i];

        // ================================================================
        // GENERATE KODE A/B BERDASARKAN INDEX
        // ================================================================
        /*
         * Sistem penomoran alternating A/B:
         * - Index genap (0,2,4,...) -> A1, A2, A3, ...
         * - Index ganjil (1,3,5,...) -> B1, B2, B3, ...
         * - Number = (index / 2) + 1
         */
        char suffix = (i % 2 == 0) ? 'A' : 'B';
        int number = (i / 2) + 1;

        // ================================================================
        // PREPARE DATA UNTUK TM1637 DISPLAY
        // ================================================================
        /*
         * TM1637 menggunakan 4 byte untuk 4 digit display
         * Format: [Digit1][Digit2][Digit3][Digit4]
         *
         * Layout display untuk kode EC:
         * Digit1: A atau b (karakter)
         * Digit2: Spasi (kosong)
         * Digit3: Puluhan (jika number >= 10)
         * Digit4: Satuan
         */
        uint8_t displayData[4] = {0, 0, 0, 0};

        // Set karakter A atau b pada digit pertama
        displayData[0] = (suffix == 'A') ? 0b11110111 : 0b11111100;

        // Set digit kedua kosong (spasi)
        displayData[1] = 0;

        // Set angka pada digit ketiga dan keempat
        if (number >= 10)
        {
          // Untuk angka 2 digit: A10, B15, dst.
          displayData[2] = tm1637.encodeDigit((number / 10) % 10); // Puluhan
          displayData[3] = tm1637.encodeDigit(number % 10);        // Satuan
        }
        else
        {
          // Untuk angka 1 digit: A1, B5, dst.
          displayData[2] = tm1637.encodeDigit(number % 10); // Satuan
          displayData[3] = 0;                               // Kosong
        }

        // Tampilkan pada TM1637 display
        tm1637.setSegments(displayData);

        // ================================================================
        // AUDIO FEEDBACK DENGAN BUZZER
        // ================================================================
        digitalWrite(BUZZER_PIN, HIGH);
        delay(BUZZER_DURATION);
        digitalWrite(BUZZER_PIN, LOW);

        // ================================================================
        // SERIAL OUTPUT UNTUK DEBUGGING DAN MONITORING
        // ================================================================
        Serial.println("========================================");
        Serial.print("EC CONNECTION DETECTED: ");
        Serial.print(suffix);
        Serial.println(number);
        Serial.print("Physical Pin: ");
        Serial.println(pins[i]);
        Serial.print("Array Index: ");
        Serial.println(i);
        Serial.print("Pin State: LOW (Connected to GND)");
        Serial.println();

        // Informasi tambahan untuk debugging
        Serial.print("Display Format: ");
        Serial.print(suffix);
        Serial.print(" + ");
        Serial.println(number);

        if (number >= 10)
        {
          Serial.print("Multi-digit display: ");
          Serial.print(suffix);
          Serial.println(number);
        }
        else
        {
          Serial.print("Single-digit display: ");
          Serial.print(suffix);
          Serial.println(number);
        }

        Serial.println("Buzzer: ACTIVATED");
        Serial.println("========================================");
      }

      // ================================================================
      // DELAY UNTUK DEBOUNCING DAN STABILITAS
      // ================================================================
      /*
       * Delay ini penting untuk:
       * 1. Mencegah multiple detection yang tidak perlu
       * 2. Memberikan waktu untuk pembacaan display
       * 3. Debouncing untuk koneksi yang tidak stabil
       * 4. Mengurangi CPU usage
       */
      delay(DEBOUNCE_DELAY);
      return; // Keluar dari loop untuk memulai scan ulang
    }
  }

  // ================================================================
  // TIDAK ADA PIN YANG TERHUBUNG
  // ================================================================
  /*
   * Jika tidak ada pin yang terbaca LOW:
   * 1. Reset lastDetectedPin
   * 2. Clear display
   * 3. Siap untuk deteksi pin berikutnya
   */
  lastDetectedPin = -1;
  tm1637.clear();

  // Small delay untuk mengurangi CPU usage
  delay(SCAN_DELAY);
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lebih lanjut)
// ================================================================

/*
 * getECCode(int pinIndex) - Generate kode EC berdasarkan index
 * Parameter: pinIndex (0 hingga TOTAL_PINS-1)
 * Return: String format "A1", "B2", dst.
 */
/*
String getECCode(int pinIndex) {
  if(pinIndex < 0 || pinIndex >= TOTAL_PINS) return "ERR";

  char suffix = (pinIndex % 2 == 0) ? 'A' : 'B';
  int number = (pinIndex / 2) + 1;

  return String(suffix) + String(number);
}
*/

/*
 * displayECCode(char suffix, int number) - Tampilkan kode EC custom
 * Parameter: suffix ('A' atau 'B'), number (1-18)
 */
/*
void displayECCode(char suffix, int number) {
  uint8_t data[4] = {0, 0, 0, 0};

  // Set karakter A atau b
  data[0] = (suffix == 'A') ? 0b11110111 : 0b11111100;
  data[1] = 0; // Spasi

  // Set angka
  if (number >= 10) {
    data[2] = tm1637.encodeDigit((number / 10) % 10);
    data[3] = tm1637.encodeDigit(number % 10);
  } else {
    data[2] = tm1637.encodeDigit(number % 10);
    data[3] = 0;
  }

  tm1637.setSegments(data);
}
*/

/*
 * playBuzzerPattern(int pattern) - Mainkan pola buzzer berbeda
 * Parameter: pattern (1=single, 2=double, 3=triple)
 */
/*
void playBuzzerPattern(int pattern) {
  for(int i = 0; i < pattern; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    if(i < pattern - 1) delay(100);
  }
}
*/

/*
 * scanAllECPins() - Debug function untuk scan semua pin
 */
/*
void scanAllECPins() {
  Serial.println("=== SCANNING ALL EC PINS ===");
  for(int i = 0; i < TOTAL_PINS; i++) {
    char suffix = (i % 2 == 0) ? 'A' : 'B';
    int number = (i / 2) + 1;
    int state = digitalRead(pins[i]);

    Serial.print("Index ");
    Serial.print(i);
    Serial.print(": Pin ");
    Serial.print(pins[i]);
    Serial.print(" -> ");
    Serial.print(suffix);
    Serial.print(number);
    Serial.print(" = ");
    Serial.println(state ? "HIGH" : "LOW");
    delay(10);
  }
  Serial.println("=== SCAN COMPLETE ===");
}
*/

/*
 * testAllDisplayCodes() - Test semua kode A/B pada display
 */
/*
void testAllDisplayCodes() {
  Serial.println("Testing all EC display codes...");

  for(int i = 0; i < TOTAL_PINS; i++) {
    char suffix = (i % 2 == 0) ? 'A' : 'B';
    int number = (i / 2) + 1;

    displayECCode(suffix, number);

    Serial.print("Displaying: ");
    Serial.print(suffix);
    Serial.println(number);

    delay(800);
  }

  tm1637.clear();
  Serial.println("Display test complete.");
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM EC TESTER:
 * =========================
 *
 * Arduino Mega -> TM1637 Display:
 * 5V    -> VCC
 * GND   -> GND
 * A0    -> CLK (Clock)
 * A1    -> DIO (Data I/O)
 *
 * Arduino Mega -> Buzzer:
 * A2    -> Buzzer + (Positive)
 * GND   -> Buzzer - (Negative)
 *
 * EC Connector Testing Setup:
 * - Connect EC connector pins to Arduino pins sesuai array
 * - Gunakan kabel jumper untuk testing individual pins
 * - Satu ujung ke EC pin, ujung lain ke GND Arduino
 *
 *
 * EC CONNECTOR PIN MAPPING:
 * ========================
 *
 * Physical Pin -> Display Code -> Array Index
 * Pin 22 -> A1  -> Index 0
 * Pin 23 -> B1  -> Index 1
 * Pin 24 -> A2  -> Index 2
 * Pin 25 -> B2  -> Index 3
 * Pin 26 -> A3  -> Index 4
 * Pin 27 -> B3  -> Index 5
 * Pin 28 -> A4  -> Index 6
 * Pin 29 -> B4  -> Index 7
 * Pin 30 -> A5  -> Index 8
 * Pin 31 -> B5  -> Index 9
 * Pin 32 -> A6  -> Index 10
 * Pin 33 -> B6  -> Index 11
 * Pin 34 -> A7  -> Index 12
 * Pin 35 -> B7  -> Index 13
 * Pin 36 -> A8  -> Index 14
 * Pin 37 -> B8  -> Index 15
 * Pin 38 -> A9  -> Index 16
 * Pin 39 -> B9  -> Index 17
 * Pin 40 -> A10 -> Index 18
 * Pin 41 -> B10 -> Index 19
 * Pin 42 -> A11 -> Index 20
 * Pin 43 -> B11 -> Index 21
 * Pin 44 -> A12 -> Index 22
 * Pin 45 -> B12 -> Index 23
 * Pin 46 -> A13 -> Index 24
 * Pin 47 -> B13 -> Index 25
 * Pin 48 -> A14 -> Index 26
 * Pin 49 -> B14 -> Index 27
 * Pin 50 -> A15 -> Index 28
 * Pin 51 -> B15 -> Index 29
 * Pin 52 -> A16 -> Index 30
 * Pin 53 -> B16 -> Index 31
 * Pin 2  -> A17 -> Index 32
 * Pin 3  -> B17 -> Index 33
 * Pin 4  -> A18 -> Index 34
 * Pin 5  -> B18 -> Index 35
 * Pin 6  -> A19 -> Index 36
 *
 *
 * SISTEM PENOMORAN A/B:
 * ====================
 *
 * Konsep: Alternating A/B system untuk identifikasi pasangan
 * - A = Index genap (0,2,4,6,...)
 * - B = Index ganjil (1,3,5,7,...)
 * - Number = (Index ÷ 2) + 1
 *
 * Contoh:
 * Index 0 = A + (0÷2)+1 = A1
 * Index 1 = B + (1÷2)+1 = B1
 * Index 2 = A + (2÷2)+1 = A2
 * Index 3 = B + (3÷2)+1 = B2
 * dst.
 *
 *
 * TROUBLESHOOTING:
 * ===============
 *
 * Problem: Display tidak menampilkan karakter A/B
 * Solution:
 * - Cek bit pattern untuk karakter custom
 * - A = 0b11110111, b = 0b11111100
 * - Pastikan tm1637.setSegments() digunakan bukan showNumber
 * - Test dengan karakter sederhana dulu
 *
 * Problem: Buzzer tidak berbunyi
 * Solution:
 * - Cek koneksi buzzer (A2 ke +, GND ke -)
 * - Pastikan buzzer aktif 5V (bukan passive)
 * - Test dengan digitalWrite HIGH/LOW manual
 * - Cek tegangan output pin A2
 *
 * Problem: Pin tidak terdeteksi
 * Solution:
 * - Pastikan pin ada di array pins[]
 * - Cek pinMode INPUT_PULLUP sudah di-set
 * - Test koneksi dengan multimeter
 * - Verifikasi grounding yang baik
 *
 * Problem: Display berkedip atau tidak stabil
 * Solution:
 * - Tambah capacitor 100µF di VCC TM1637
 * - Cek kualitas koneksi CLK dan DIO
 * - Reduce SCAN_DELAY jika terlalu cepat
 * - Pastikan power supply stabil
 *
 * Problem: False positive detection
 * Solution:
 * - Increase DEBOUNCE_DELAY
 * - Cek floating pins atau interferensi
 * - Pastikan pull-up resistor bekerja
 * - Add hardware debouncing jika perlu
 *
 * Problem: Serial output tidak muncul
 * Solution:
 * - Pastikan Serial.begin(9600) di setup
 * - Cek baud rate di Serial Monitor
 * - Pastikan kabel USB terhubung baik
 * - Restart Arduino IDE jika perlu
 *
 *
 * MODIFIKASI DAN PENGEMBANGAN:
 * ============================
 *
 * 1. Menambah jenis konektor:
 *    - Ubah array pins[] sesuai konektor baru
 *    - Sesuaikan sistem penomoran jika perlu
 *    - Update dokumentasi pin mapping
 *
 * 2. Menambah fitur display:
 *    - LCD 16x2 untuk informasi lebih detail
 *    - LED strip untuk visual indication
 *    - OLED untuk graphics dan info
 *
 * 3. Data logging dan analysis:
 *    - Save hasil test ke SD card
 *    - Timestamp setiap test
 *    - Statistical analysis hasil test
 *    - Export data ke CSV/Excel
 *
 * 4. Advanced testing:
 *    - Resistance measurement
 *    - Voltage level testing
 *    - Continuity dengan threshold
 *    - Automated test sequences
 *
 * 5. User interface:
 *    - Push button untuk mode selection
 *    - Rotary encoder untuk navigation
 *    - Menu system dengan LCD
 *    - Remote control via smartphone
 *
 * 6. Quality control features:
 *    - Pass/Fail statistics
 *    - Test result database
 *    - Batch testing mode
 *    - Production line integration
 *
 *
 * APLIKASI INDUSTRI:
 * ==================
 *
 * 1. PCB Testing:
 *    - Verifikasi routing PCB
 *    - Test solder joints
 *    - Quality control manufacturing
 *
 * 2. Cable Assembly:
 *    - Harness continuity testing
 *    - Multi-core cable verification
 *    - Connector pin mapping
 *
 * 3. Maintenance & Repair:
 *    - Fault diagnosis
 *    - Connection verification
 *    - Field service tool
 *
 * 4. Production Line:
 *    - Automated testing station
 *    - Go/No-go testing
 *    - Integration dengan PLC
 *
 *
 * SPESIFIKASI TEKNIS:
 * ==================
 *
 * Operating Voltage: 5V DC
 * Current Consumption: ~150mA (with buzzer active)
 * Pin Count Support: Up to 54 pins (Arduino Mega)
 * Display: 4-digit 7-segment TM1637
 * Audio Feedback: Active buzzer 5V
 * Response Time: <50ms per pin
 * Scan Rate: ~10Hz (adjustable)
 * Input Impedance: ~20kΩ (internal pull-up)
 *
 * Environmental:
 * Operating Temperature: 0°C to 50°C
 * Storage Temperature: -20°C to 70°C
 * Humidity: <90% non-condensing
 *
 *
 * MAINTENANCE:
 * ===========
 *
 * 1. Regular Cleaning:
 *    - Bersihkan konektor test secara berkala
 *    - Gunakan contact cleaner untuk pin
 *    - Periksa kabel test untuk kerusakan
 *
 * 2. Kalibrasi:
 *    - Test dengan kabel yang diketahui baik
 *    - Verifikasi semua kode A/B tampil benar
 *    - Check timing dan response
 *
 * 3. Component Check:
 *    - Test TM1637 display functionality
 *    - Verify buzzer masih berbunyi
 *    - Check power supply stability
 *
 * 4. Software Update:
 *    - Update firmware jika ada perbaikan
 *    - Backup configuration dan pin mapping
 *    - Test semua fitur setelah update
 *
 */
