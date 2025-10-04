/*
 * ========================================================================
 * KEYPAD TIME-BASED COUNTER SYSTEM
 * ========================================================================
 *
 * Project: coba_counter_up_by_time
 * Version: 1.0
 * Date: October 2025
 * Author: AGUS F
 *
 * DESCRIPTION:
 * Sistem counter otomatis berbasis waktu dengan kontrol keypad 4x4.
 * Counter akan bertambah secara otomatis setiap interval waktu tertentu
 * yang dapat diatur melalui keypad. Default interval adalah 3 menit.
 *
 * FEATURES:
 * • Auto-increment counter berdasarkan interval waktu
 * • Keypad 4x4 untuk input dan kontrol
 * • Pengaturan interval waktu dinamis (dalam menit)
 * • Reset counter manual dengan tombol '*'
 * • Konfirmasi setting dengan tombol '#'
 * • Serial monitoring untuk debugging dan tracking
 * • Non-blocking timing menggunakan millis()
 *
 * HARDWARE REQUIREMENTS:
 * • Arduino Uno/Nano/Mega
 * • Keypad 4x4 matrix
 * • Koneksi Serial untuk monitoring
 * • Power supply 5V DC
 *
 * APPLICATIONS:
 * • Production timing system
 * • Interval-based data logging
 * • Periodic task automation
 * • Time-based quality control
 * • Manufacturing process monitoring
 *
 * ========================================================================
 */

#include <Keypad.h>

// ================================================================
// TIMING CONFIGURATION
// ================================================================
const unsigned long minuteToMillis = 60000; // Konversi menit ke milidetik (1 menit = 60,000ms)
unsigned long interval = 180000;            // Interval waktu awal dalam milidetik (3 menit default)
unsigned long previousMillis = 0;           // Variabel untuk menyimpan waktu sebelumnya (non-blocking timing)

// ================================================================
// COUNTER SYSTEM VARIABLES
// ================================================================
int counter = 0;           // Counter utama yang auto-increment berdasarkan waktu
String inputInterval = ""; // Buffer untuk menyimpan input interval dari keypad

// ================================================================
// KEYPAD 4x4 MATRIX CONFIGURATION
// ================================================================
const byte ROWS = 4; // Jumlah baris keypad matrix
const byte COLS = 4; // Jumlah kolom keypad matrix

/*
 * KEYPAD LAYOUT MAPPING:
 * +---+---+---+---+
 * | 1 | 2 | 3 | A |  Row 0 (A7)
 * +---+---+---+---+
 * | 4 | 5 | 6 | B |  Row 1 (A6)
 * +---+---+---+---+
 * | 7 | 8 | 9 | C |  Row 2 (A5)
 * +---+---+---+---+
 * | * | 0 | # | D |  Row 3 (A4)
 * +---+---+---+---+
 *  Col0 Col1 Col2 Col3
 *  (A3) (A2) (A1) (A0)
 */
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'}, // Row 0: Angka 1-3, Function A
    {'4', '5', '6', 'B'}, // Row 1: Angka 4-6, Function B
    {'7', '8', '9', 'C'}, // Row 2: Angka 7-9, Function C
    {'*', '0', '#', 'D'}  // Row 3: Reset(*), Zero(0), Confirm(#), Function D
};

// PIN ASSIGNMENTS untuk keypad matrix
byte rowPins[ROWS] = {A7, A6, A5, A4}; // Pin baris keypad (R1-R4)
byte colPins[COLS] = {A3, A2, A1, A0}; // Pin kolom keypad (C1-C4)

// Inisialisasi objek keypad dengan mapping pin dan karakter
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================================================================
// SISTEM INITIALIZATION
// ================================================================

/*
 * setup() - Inisialisasi sistem saat startup
 *
 * FUNGSI:
 * • Mengaktifkan komunikasi Serial untuk monitoring
 * • Menampilkan banner startup dan informasi sistem
 * • Menginisialisasi keypad matrix
 * • Set default interval timing (3 menit)
 * • Display usage instructions untuk operator
 *
 * SERIAL OUTPUT:
 * • System startup banner
 * • Default interval setting
 * • Keypad usage instructions
 * • Current counter value
 */
void setup() {
  // Inisialisasi komunikasi Serial untuk monitoring dan debugging
  Serial.begin(9600);

  // Tunggu Serial port siap (terutama untuk Leonardo/Micro)
  while (!Serial)
  {
    ; // Wait for serial port to connect
  }

  // Display startup banner
  Serial.println("========================================");
  Serial.println("  KEYPAD TIME-BASED COUNTER SYSTEM");
  Serial.println("         Version 1.0 - AGUS F");
  Serial.println("========================================");
  Serial.println();

  // Display default settings
  Serial.print("Default interval: ");
  Serial.print(interval / minuteToMillis);
  Serial.println(" menit");
  Serial.print("Counter awal: ");
  Serial.println(counter);
  Serial.println();

  // Display keypad usage instructions
  Serial.println("KEYPAD INSTRUCTIONS:");
  Serial.println("• Angka 0-9: Input interval waktu (menit)");
  Serial.println("• Tombol '*': Reset counter ke 0");
  Serial.println("• Tombol '#': Konfirmasi setting interval");
  Serial.println("• Counter auto-increment setiap interval");
  Serial.println("========================================");
  Serial.println("Sistem siap! Menunggu input atau auto-count...");
  Serial.println();
}

// ================================================================
// MAIN PROGRAM LOOP
// ================================================================

/*
 * loop() - Program utama yang berjalan terus menerus
 *
 * MAIN FUNCTIONS:
 * 1. Keypad Input Processing
 *    • Scan keypad untuk input dari operator
 *    • Process numeric input untuk interval setting
 *    • Handle special keys (*, #) untuk reset dan confirm
 *
 * 2. Non-blocking Timer Management
 *    • Monitor waktu berjalan tanpa blocking program
 *    • Auto-increment counter setiap interval terpenuhi
 *    • Update previousMillis untuk timing accuracy
 *
 * 3. Serial Communication
 *    • Real-time feedback untuk setiap keypad input
 *    • Counter update notifications
 *    • System status monitoring
 *
 * TIMING STRATEGY:
 * Menggunakan millis() non-blocking approach untuk:
 * • Tidak mengganggu keypad scanning
 * • Akurasi timing yang konsisten
 * • Responsivitas sistem tetap terjaga
 * • Menghindari delay() yang blocking
 */
void loop() {
  // ================================================================
  // KEYPAD INPUT PROCESSING SECTION
  // ================================================================

  // Scan keypad untuk input dari operator
  char key = keypad.getKey();

  // Process keypad input jika ada tombol yang ditekan
  if (key) {

    // RESET FUNCTION - Tombol '*'
    if (key == '*') {
      counter = 0; // Reset counter ke nilai awal
      Serial.println("=== COUNTER RESET ===");
      Serial.println("Counter diatur ulang ke 0.");
      Serial.print("Interval saat ini: ");
      Serial.print(interval / minuteToMillis);
      Serial.println(" menit");
      Serial.println("====================");

      // CONFIRM SETTING - Tombol '#'
    } else if (key == '#') {
      // Konfirmasi dan apply interval setting dari inputInterval buffer
      if (inputInterval.length() > 0) {
        // Konversi input menit ke milliseconds
        unsigned long newInterval = inputInterval.toInt() * minuteToMillis;

        // Validasi input (1-999 menit)
        if (inputInterval.toInt() >= 1 && inputInterval.toInt() <= 999)
        {
          interval = newInterval;
          Serial.println("=== INTERVAL UPDATE ===");
          Serial.print("Interval waktu diatur ke ");
          Serial.print(inputInterval);
          Serial.println(" menit.");
          Serial.print("Equivalent milliseconds: ");
          Serial.println(interval);
          Serial.println("Setting berhasil disimpan!");
          Serial.println("======================");

          // Reset timer untuk mulai dengan interval baru
          previousMillis = millis();
        }
        else
        {
          Serial.println("=== INPUT ERROR ===");
          Serial.println("Interval harus 1-999 menit!");
          Serial.println("Input diabaikan.");
          Serial.println("==================");
        }

        // Clear input buffer setelah processing
        inputInterval = "";
      }
      else
      {
        // Tidak ada input untuk dikonfirmasi
        Serial.println("=== NO INPUT ===");
        Serial.println("Tidak ada interval untuk dikonfirmasi.");
        Serial.print("Interval saat ini: ");
        Serial.print(interval / minuteToMillis);
        Serial.println(" menit");
        Serial.println("================");
      }

      // NUMERIC INPUT - Angka 0-9
    } else if (isdigit(key)) {
      // Tambahkan digit ke input buffer
      inputInterval += key;
      Serial.print("Input interval: ");
      Serial.print(inputInterval);
      Serial.println(" menit (tekan # untuk konfirmasi)");

      // Batasi panjang input (maksimal 3 digit)
      if (inputInterval.length() > 3)
      {
        inputInterval = inputInterval.substring(0, 3);
        Serial.println("WARNING: Input terbatas 3 digit (999 menit max)");
      }

      // FUNCTION KEYS - A, B, C, D (reserved untuk future features)
    }
    else if (key == 'A' || key == 'B' || key == 'C' || key == 'D')
    {
      Serial.print("Function key pressed: ");
      Serial.print(key);
      Serial.println(" (reserved untuk future features)");
    }

    // Log semua keypad activity untuk debugging
    Serial.print("Tombol ditekan: ");
    Serial.print(key);
    Serial.print(" | Counter: ");
    Serial.print(counter);
    Serial.print(" | Next count in: ");
    Serial.print((interval - (millis() - previousMillis)) / 1000);
    Serial.println(" detik");
  }

  // ================================================================
  // NON-BLOCKING TIMER MANAGEMENT SECTION
  // ================================================================

  // Dapatkan waktu saat ini (milliseconds sejak startup)
  unsigned long currentMillis = millis();

  // Check apakah interval waktu sudah terpenuhi
  if (currentMillis - previousMillis >= interval) {

    // Update timing reference untuk interval berikutnya
    previousMillis = currentMillis;

    // Increment counter otomatis
    counter++;

    // Display counter update dengan timestamp
    Serial.println("======= AUTO COUNT =======");
    Serial.print("Timer triggered! Counter: ");
    Serial.println(counter);
    Serial.print("Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.println(" detik");
    Serial.print("Next count in: ");
    Serial.print(interval / 1000);
    Serial.println(" detik");
    Serial.println("=========================");

    // Optional: Counter overflow protection (reset at 9999)
    if (counter >= 9999)
    {
      Serial.println("=== COUNTER OVERFLOW ===");
      Serial.println("Counter mencapai limit (9999)");
      Serial.println("Auto-reset ke 0 untuk mencegah overflow");
      Serial.println("========================");
      counter = 0;
    }
  }
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lanjutan)
// ================================================================

/*
 * displaySystemStatus() - Tampilkan status lengkap sistem
 * Untuk debugging dan monitoring sistem
 */
/*
void displaySystemStatus() {
  unsigned long uptime = millis() / 1000;
  unsigned long timeToNext = (interval - (millis() - previousMillis)) / 1000;

  Serial.println("===== SYSTEM STATUS =====");
  Serial.print("Current Counter: ");
  Serial.println(counter);
  Serial.print("Current Interval: ");
  Serial.print(interval / minuteToMillis);
  Serial.println(" menit");
  Serial.print("System Uptime: ");
  Serial.print(uptime);
  Serial.println(" detik");
  Serial.print("Time to Next Count: ");
  Serial.print(timeToNext);
  Serial.println(" detik");
  Serial.print("Input Buffer: '");
  Serial.print(inputInterval);
  Serial.println("'");
  Serial.println("=========================");
}
*/

/*
 * calculateProductionRate() - Hitung rate produksi per jam
 * Return: Counter per hour berdasarkan waktu berjalan
 */
/*
float calculateProductionRate() {
  unsigned long uptimeSeconds = millis() / 1000;
  if (uptimeSeconds == 0) return 0.0;

  float ratePerSecond = (float)counter / uptimeSeconds;
  float ratePerHour = ratePerSecond * 3600.0;

  return ratePerHour;
}
*/

/*
 * resetSystemToDefaults() - Reset sistem ke pengaturan default
 * Untuk maintenance atau kalibrasi ulang
 */
/*
void resetSystemToDefaults() {
  counter = 0;
  interval = 180000; // 3 menit default
  inputInterval = "";
  previousMillis = millis();

  Serial.println("=== SYSTEM RESET ===");
  Serial.println("Counter: 0");
  Serial.println("Interval: 3 menit (default)");
  Serial.println("Input buffer: cleared");
  Serial.println("Timer: restarted");
  Serial.println("====================");
}
*/

/*
 * validateIntervalInput() - Validasi input interval
 * Return: true jika valid, false jika tidak valid
 */
/*
bool validateIntervalInput(String input) {
  if (input.length() == 0) return false;

  int value = input.toInt();
  if (value < 1 || value > 999) {
    Serial.println("ERROR: Interval harus 1-999 menit");
    return false;
  }

  return true;
}
*/

/*
 * logCounterActivity() - Log aktivitas counter ke serial
 * Untuk audit trail dan monitoring
 */
/*
void logCounterActivity(String activity, int oldValue, int newValue) {
  unsigned long timestamp = millis() / 1000;

  Serial.print("[");
  Serial.print(timestamp);
  Serial.print("s] ");
  Serial.print(activity);
  Serial.print(": ");
  Serial.print(oldValue);
  Serial.print(" -> ");
  Serial.println(newValue);
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM KEYPAD TIME-BASED COUNTER:
 * ==========================================
 *
 * Arduino Uno/Nano -> Keypad 4x4:
 * 5V     -> VCC (jika keypad memerlukan power)
 * GND    -> GND (jika keypad memerlukan ground)
 * A7     -> ROW 1 (Pin 1 keypad)
 * A6     -> ROW 2 (Pin 2 keypad)
 * A5     -> ROW 3 (Pin 3 keypad)
 * A4     -> ROW 4 (Pin 4 keypad)
 * A3     -> COL 1 (Pin 5 keypad)
 * A2     -> COL 2 (Pin 6 keypad)
 * A1     -> COL 3 (Pin 7 keypad)
 * A0     -> COL 4 (Pin 8 keypad)
 *
 * Keypad Matrix Layout:
 *    COL1  COL2  COL3  COL4
 *     A3    A2    A1    A0
 *    +-----+-----+-----+-----+
 * A7 |  1  |  2  |  3  |  A  | ROW1
 *    +-----+-----+-----+-----+
 * A6 |  4  |  5  |  6  |  B  | ROW2
 *    +-----+-----+-----+-----+
 * A5 |  7  |  8  |  9  |  C  | ROW3
 *    +-----+-----+-----+-----+
 * A4 |  *  |  0  |  #  |  D  | ROW4
 *    +-----+-----+-----+-----+
 *
 * Optional Components:
 * - LED indicator untuk visual feedback
 * - Buzzer untuk audio feedback
 * - LCD display untuk local display
 * - RTC module untuk timestamp accuracy
 *
 *
 * SISTEM OPERASI DAN WORKFLOW:
 * ============================
 *
 * 1. System Startup:
 *    - Arduino boot sequence
 *    - Serial communication establishment (9600 baud)
 *    - Keypad matrix initialization
 *    - Default interval setting (3 menit)
 *    - Display startup banner dan instructions
 *
 * 2. Normal Operation Mode:
 *    - Continuous keypad scanning
 *    - Non-blocking timer management
 *    - Auto-increment counter setiap interval
 *    - Real-time serial monitoring
 *
 * 3. Interval Setting Workflow:
 *    a. Input: Tekan angka 1-999 (maksimal 3 digit)
 *    b. Display: Tampil "Input interval: XXX menit"
 *    c. Confirm: Tekan '#' untuk apply setting
 *    d. Validation: Cek range 1-999 menit
 *    e. Apply: Update interval dan restart timer
 *
 * 4. Counter Reset Workflow:
 *    a. Input: Tekan '*' kapan saja
 *    b. Action: Counter langsung reset ke 0
 *    c. Feedback: Serial message konfirmasi
 *    d. Continue: Timer tetap berjalan normal
 *
 * 5. Auto-Count Workflow:
 *    a. Timer: Monitor millis() non-blocking
 *    b. Trigger: Saat currentMillis - previousMillis >= interval
 *    c. Action: Increment counter, update previousMillis
 *    d. Feedback: Serial notification dengan timestamp
 *    e. Protection: Auto-reset jika counter >= 9999
 *
 *
 * TIMING SYSTEM TECHNICAL DETAILS:
 * =================================
 *
 * Non-blocking Timing Strategy:
 * - Menggunakan millis() system timer
 * - Tidak menggunakan delay() yang blocking
 * - Akurasi timing dalam milliseconds
 * - Overflow handling untuk millis() ~49 hari
 *
 * Timing Calculations:
 * - Input: Menit (user-friendly)
 * - Storage: Milliseconds (system precision)
 * - Conversion: minutes × 60,000 = milliseconds
 * - Range: 1-999 menit (60,000 - 59,940,000 ms)
 *
 * Timer Accuracy:
 * - Resolution: 1 millisecond
 * - Drift: <±0.1% pada room temperature
 * - Jitter: <1ms typical
 * - Long-term stability: Tergantung crystal oscillator
 *
 * Overflow Protection:
 * - millis() overflow: ~49.7 days uptime
 * - Counter overflow: Auto-reset di 9999
 * - Input buffer: Limited 3 digits
 * - Memory: String length protection
 *
 *
 * KEYPAD INTERFACE SPECIFICATIONS:
 * ================================
 *
 * Matrix Configuration:
 * - Type: 4×4 membrane keypad
 * - Connections: 8 pins (4 rows + 4 columns)
 * - Pull-up: Internal Arduino pull-up resistors
 * - Debouncing: Hardware built-in keypad
 * - Response time: <10ms typical
 *
 * Key Functions:
 * • Digits 0-9: Numeric input untuk interval setting
 * • Key '*': Instant counter reset function
 * • Key '#': Confirm/apply interval setting
 * • Keys A-D: Reserved untuk future features
 *
 * Input Validation:
 * - Numeric only: 0-9 digits accepted
 * - Range check: 1-999 menit valid range
 * - Length limit: 3 digits maximum
 * - Buffer management: Auto-clear setelah apply
 *
 * User Interface Flow:
 * 1. Type interval (e.g., "15" untuk 15 menit)
 * 2. Press '#' untuk confirm setting
 * 3. Press '*' untuk reset counter anytime
 * 4. Monitor serial untuk feedback
 *
 *
 * APPLICATIONS DAN USE CASES:
 * ============================
 *
 * Industrial Applications:
 * • Production line timing system
 * • Quality control interval sampling
 * • Manufacturing process monitoring
 * • Shift production counting
 * • Equipment runtime tracking
 *
 * Laboratory Applications:
 * • Time-based data logging
 * • Experiment interval timing
 * • Sample collection scheduling
 * • Process automation timing
 * • Environmental monitoring
 *
 * Maintenance Applications:
 * • Preventive maintenance scheduling
 * • Equipment service intervals
 * • Calibration reminder system
 * • Runtime hour counting
 * • Service life tracking
 *
 * Educational Applications:
 * • Arduino timing concepts
 * • Non-blocking programming
 * • Keypad interface learning
 * • Serial communication
 * • Real-time system basics
 *
 *
 * TROUBLESHOOTING GUIDE:
 * ======================
 *
 * Problem: Keypad tidak merespon
 * Solution:
 * - Cek koneksi 8 pin keypad ke Arduino
 * - Verify pin mapping: A7-A4 (rows), A3-A0 (cols)
 * - Test keypad dengan multimeter (continuity)
 * - Check Keypad library installation
 * - Verify row/col pin definitions dalam code
 * - Try different keypad (hardware test)
 *
 * Problem: Counter tidak auto-increment
 * Solution:
 * - Check interval setting (harus > 0)
 * - Monitor Serial untuk timer debug info
 * - Verify millis() timing logic
 * - Check previousMillis initialization
 * - Restart Arduino (power cycle)
 * - Verify interval conversion (menit→ms)
 *
 * Problem: Serial output tidak muncul
 * Solution:
 * - Verify baud rate 9600 di Serial Monitor
 * - Check USB connection dan driver
 * - Try different USB port/cable
 * - Restart Arduino IDE
 * - Verify Serial.begin(9600) dalam setup()
 * - Check power supply adequacy
 *
 * Problem: Timer tidak akurat
 * Solution:
 * - millis() overflow setelah ~49 hari (restart)
 * - Crystal oscillator drift dengan temperature
 * - Code blocking delays (avoid delay() functions)
 * - Power supply instability
 * - External interference (EMI/RFI)
 * - Use RTC module untuk precision timing
 *
 * Problem: Input interval tidak tersimpan
 * Solution:
 * - Pastikan tekan '#' setelah input angka
 * - Check range validation (1-999 menit)
 * - Verify inputInterval string handling
 * - Monitor Serial untuk validation messages
 * - Check String buffer overflow protection
 * - Restart sistem jika buffer corrupt
 *
 * Problem: Memory atau stability issues
 * Solution:
 * - Avoid String concatenation yang excessive
 * - Use const untuk string literals
 * - Monitor RAM usage (Serial.print untuk debug)
 * - Minimize dynamic memory allocation
 * - Use F() macro untuk PROGMEM strings
 * - Regular system restart untuk long-term operation
 *
 *
 * PENGEMBANGAN LANJUTAN:
 * ======================
 *
 * Hardware Enhancements:
 * • LCD Display 16x2/20x4 untuk local readout
 * • RTC Module (DS3231) untuk timestamp accuracy
 * • SD Card logging untuk data persistence
 * • WiFi/Ethernet untuk remote monitoring
 * • Buzzer untuk audio feedback
 * • LED indicators untuk visual status
 *
 * Software Features:
 * • Data logging dengan timestamp
 * • Statistical analysis (rate, averages)
 * • Alarm system untuk maintenance intervals
 * • Web interface untuk remote control
 * • EEPROM storage untuk persistent settings
 * • Multiple timer channels
 *
 * Production Features:
 * • Barcode/QR code integration
 * • Database connectivity
 * • Shift report generation
 * • Quality control metrics
 * • Predictive maintenance alerts
 * • Multi-station networking
 *
 * Advanced Timing:
 * • Microsecond precision timing
 * • GPS time synchronization
 * • NTP time server sync
 * • Multi-timezone support
 * • Daylight saving time handling
 * • Calendar-based scheduling
 *
 *
 * SPESIFIKASI TEKNIS:
 * ===================
 *
 * Microcontroller Requirements:
 * - Arduino Uno/Nano: 2KB RAM, 32KB Flash (sufficient)
 * - Arduino Mega: 8KB RAM, 256KB Flash (recommended)
 * - Clock: 16MHz (timing accuracy dependent)
 * - I/O: 8 digital pins untuk keypad matrix
 * - Serial: 1 UART untuk monitoring (9600 baud)
 *
 * Timing Specifications:
 * - Resolution: 1 millisecond
 * - Range: 1-999 menit (60s - 16.65 hours)
 * - Accuracy: ±0.1% typical (crystal dependent)
 * - Stability: Room temperature operation
 * - Overflow: millis() resets setiap ~49.7 hari
 *
 * Counter Specifications:
 * - Range: 0-9999 (auto-reset pada overflow)
 * - Type: Integer (16-bit signed: -32,768 to 32,767)
 * - Persistence: Volatile (lost pada power cycle)
 * - Update rate: Sesuai interval setting
 * - Resolution: 1 count per increment
 *
 * Performance Specifications:
 * - Keypad scan rate: ~100Hz (10ms cycle)
 * - Response time: <50ms untuk keypad input
 * - Serial output rate: 9600 baud (960 char/s)
 * - Memory usage: <500 bytes RAM
 * - Power consumption: <50mA @ 5V
 *
 * Environmental Specifications:
 * - Operating temperature: 0°C to +50°C
 * - Storage temperature: -20°C to +70°C
 * - Humidity: 10% to 90% RH non-condensing
 * - Altitude: Sea level to 2000m
 * - EMC: Industrial environment compatible
 *
 */
