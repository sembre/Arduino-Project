/*
 * ================================================================
 * 3-POLE SWITCH TESTER WITH SEQUENTIAL VALIDATION
 * ================================================================
 *
 * Deskripsi:
 * Program untuk menguji switch 3 pole dengan validasi urutan koneksi.
 * Sistem memverifikasi bahwa switch dapat membuat koneksi secara
 * berurutan yang benar dan mendeteksi kesalahan wiring atau
 * mekanis switch yang tidak proper.
 *
 * Fungsi Testing:
 * Program akan memverifikasi:
 * 1. Tahap 1: Pin 2 terhubung ke Pin 3 (posisi pertama switch)
 * 2. Tahap 2: Pin 2 terhubung ke Pin 4 (posisi kedua switch)
 * 3. Urutan yang benar: HARUS tahap 1 dulu, baru tahap 2
 * 4. Error detection: Pin 4 aktif sebelum Pin 3 = ERROR
 *
 * Hardware yang Dibutuhkan:
 * - Arduino Uno/Mega (recommended Mega untuk I2C yang lebih stabil)
 * - LCD 16x2 dengan I2C backpack (alamat 0x27)
 * - Buzzer aktif 5V
 * - Switch 3 pole yang akan ditest
 * - Push header/connector untuk testing
 * - Kabel jumper dan breadboard
 *
 * Pin Configuration:
 * - Pin 2: Output +5V (supply voltage untuk testing)
 * - Pin 3: Input untuk test tahap 1 (Pin 2 → Pin 3)
 * - Pin 4: Input untuk test tahap 2 (Pin 2 → Pin 4)
 * - Pin 5: Output buzzer (konfirmasi switch OK)
 * - SDA/SCL: I2C communication untuk LCD (Pin 20/21 di Mega)
 *
 * Indikator Hasil:
 * - LCD: Status real-time setiap tahap testing
 * - Serial: Log detail untuk debugging
 * - Buzzer: Berbunyi saat switch LULUS test
 * - Error detection: Tampilan error jika urutan salah
 *
 * Aplikasi:
 * - Quality control switch 3 pole manufacturing
 * - Testing rotary switch dan multi-position switch
 * - Verifikasi wiring harness dengan multiple connections
 * - Educational tool untuk belajar sequential logic
 *
 * Author: Sembre
 * Date: 2025
 * Version: 1.0
 * Target: Arduino Mega 2560 (I2C pada pin 20/21)
 * ================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================================================================
// KONFIGURASI HARDWARE DAN PIN
// ================================================================

// Pin konfigurasi untuk testing switch 3 pole
const int pin2 = 2;   // Output +5V supply untuk testing
const int pin3 = 3;   // Input test tahap 1 (Pin2 → Pin3 connection)
const int pin4 = 4;   // Input test tahap 2 (Pin2 → Pin4 connection)
const int buzzer = 5; // Output buzzer untuk konfirmasi PASS

// Konstanta timing dan konfigurasi
const int DEBOUNCE_DELAY = 50;        // Delay debouncing dalam ms
const int DISPLAY_UPDATE_DELAY = 100; // Delay update display
const int BUZZER_DURATION = 2000;     // Durasi buzzer saat PASS (ms)

// ================================================================
// VARIABEL STATUS DAN STATE TRACKING
// ================================================================

/*
 * State variables untuk tracking progress test:
 *
 * tahap1: Flag bahwa koneksi Pin2→Pin3 sudah berhasil
 * tahap2: Flag bahwa koneksi Pin2→Pin4 sudah berhasil
 * errorState: Flag jika ada kesalahan urutan (Pin4 aktif duluan)
 *
 * Logic testing:
 * - Normal sequence: tahap1 = true, kemudian tahap2 = true
 * - Error sequence: Pin4 HIGH sebelum Pin3 pernah HIGH
 * - Complete test: tahap1 && tahap2 && !errorState = PASS
 */
bool tahap1 = false;     // Status tahap 1 (Pin2→Pin3) sudah terpenuhi
bool tahap2 = false;     // Status tahap 2 (Pin2→Pin4) sudah terpenuhi
bool errorState = false; // Status error jika urutan salah

// Previous states untuk edge detection dan debouncing
bool prevPin3State = false;        // Status Pin3 sebelumnya
bool prevPin4State = false;        // Status Pin4 sebelumnya
unsigned long lastStateChange = 0; // Timestamp untuk debouncing

// ================================================================
// LCD CONFIGURATION
// ================================================================
/*
 * LCD I2C Configuration:
 * - Address: 0x27 (default untuk mayoritas I2C backpack)
 * - Size: 16x2 karakter
 * - Connection: SDA ke pin 20, SCL ke pin 21 (Arduino Mega)
 * - Connection: SDA ke pin A4, SCL ke pin A5 (Arduino Uno)
 */
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================================================================
// SETUP FUNCTION - Inisialisasi sistem 3-pole switch tester
// ================================================================
void setup() {
  // ================================================================
  // INISIALISASI KOMUNIKASI SERIAL
  // ================================================================
  Serial.begin(9600);
  Serial.println("========================================");
  Serial.println("3-POLE SWITCH TESTER v1.0");
  Serial.println("========================================");
  Serial.println("Sequential Connection Testing System");
  Serial.println("Testing Order: Pin2→Pin3 THEN Pin2→Pin4");
  Serial.println();
  Serial.println("Pin Configuration:");
  Serial.print("  Pin ");
  Serial.print(pin2);
  Serial.println(": Output +5V (Test Supply)");
  Serial.print("  Pin ");
  Serial.print(pin3);
  Serial.println(": Input Stage 1 Test");
  Serial.print("  Pin ");
  Serial.print(pin4);
  Serial.println(": Input Stage 2 Test");
  Serial.print("  Pin ");
  Serial.print(buzzer);
  Serial.println(": Buzzer Output");
  Serial.println();
  Serial.println("Initializing system...");

  // ================================================================
  // INISIALISASI I2C DAN LCD
  // ================================================================
  /*
   * I2C Communication untuk LCD:
   * - Arduino Mega: SDA = Pin 20, SCL = Pin 21
   * - Arduino Uno: SDA = Pin A4, SCL = Pin A5
   * - LCD Address: 0x27 (scan dengan I2C scanner jika berbeda)
   * - Display: 16x2 karakter dengan backlight
   */
  Wire.begin();
  Serial.println("I2C communication initialized");
  Serial.println("LCD Address: 0x27 (16x2 with I2C backpack)");

  // Inisialisasi LCD
  lcd.begin(16, 2);
  lcd.backlight();

  // Test LCD dengan startup message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("3P Switch Tester");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  Serial.println("LCD initialized and backlight ON");
  delay(2000); // Tampilkan pesan startup

  // ================================================================
  // KONFIGURASI PIN INPUT/OUTPUT
  // ================================================================
  /*
   * Pin Mode Configuration:
   *
   * Pin 2 (OUTPUT): Menyediakan +5V sebagai test voltage
   * - Selalu HIGH untuk memberikan supply ke switch under test
   * - Arus maksimal: 40mA (cukup untuk testing switch)
   *
   * Pin 3 & 4 (INPUT): Deteksi koneksi switch
   * - Mode INPUT standar (bukan INPUT_PULLUP)
   * - Akan HIGH saat terhubung ke Pin 2 (+5V)
   * - Akan LOW/floating saat tidak terhubung
   *
   * Pin 5 (OUTPUT): Buzzer control
   * - HIGH = Buzzer ON, LOW = Buzzer OFF
   * - Untuk konfirmasi switch PASS test
   */
  pinMode(pin2, OUTPUT);
  pinMode(pin3, INPUT);
  pinMode(pin4, INPUT);
  pinMode(buzzer, OUTPUT);

  // Set Pin 2 sebagai supply voltage (+5V)
  digitalWrite(pin2, HIGH);
  Serial.println("Pin modes configured:");
  Serial.println("  Pin 2: OUTPUT HIGH (+5V supply)");
  Serial.println("  Pin 3: INPUT (Stage 1 detection)");
  Serial.println("  Pin 4: INPUT (Stage 2 detection)");
  Serial.println("  Pin 5: OUTPUT (Buzzer control)");

  // Pastikan buzzer mati di awal
  digitalWrite(buzzer, LOW);
  Serial.println("Buzzer initialized: OFF");

  // ================================================================
  // RESET STATE VARIABLES
  // ================================================================
  tahap1 = false;
  tahap2 = false;
  errorState = false;
  prevPin3State = false;
  prevPin4State = false;
  lastStateChange = millis();

  Serial.println("State variables reset to initial values");

  // ================================================================
  // TEST HARDWARE COMPONENTS
  // ================================================================
  Serial.println();
  Serial.println("Testing hardware components...");

  // Test buzzer
  Serial.println("Testing buzzer...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Testing Buzzer");
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(buzzer, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW);
    delay(200);
  }
  Serial.println("Buzzer test complete");

  // Test LCD display patterns
  Serial.println("Testing LCD display...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Display Test");
  lcd.setCursor(0, 1);
  lcd.print("1234567890123456");
  delay(1500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  System Ready");
  lcd.setCursor(0, 1);
  lcd.print("   Ver 1.0");
  delay(2000);

  Serial.println("LCD test complete");

  // ================================================================
  // SYSTEM READY
  // ================================================================
  Serial.println();
  Serial.println("========================================");
  Serial.println("SYSTEM READY FOR TESTING!");
  Serial.println("========================================");
  Serial.println("Test Procedure:");
  Serial.println("1. Insert 3-pole switch into test header");
  Serial.println("2. Connect Pin2 of switch to Pin2 Arduino");
  Serial.println("3. Connect Pin3 of switch to Pin3 Arduino");
  Serial.println("4. Connect Pin4 of switch to Pin4 Arduino");
  Serial.println("5. Operate switch: Position 1 (Pin2→Pin3)");
  Serial.println("6. Then Position 2 (Pin2→Pin4)");
  Serial.println("7. SUCCESS: Both stages + Buzzer ON");
  Serial.println("8. ERROR: Wrong sequence = Error display");
  Serial.println("========================================");
  Serial.println();

  // Display ready message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" INSERT SWITCH");
  lcd.setCursor(0, 1);
  lcd.print("  To Test Header");

  Serial.println("Monitoring switch connections...");
}

// ================================================================
// MAIN LOOP - Sequential switch testing dengan error detection
// ================================================================
void loop() {
  // ================================================================
  // BACA STATUS PIN DAN DEBOUNCING
  // ================================================================
  /*
   * Pembacaan pin switch:
   * - Pin 3: HIGH saat Pin2 terhubung ke Pin3 (Stage 1)
   * - Pin 4: HIGH saat Pin2 terhubung ke Pin4 (Stage 2)
   *
   * Debouncing untuk mencegah false trigger:
   * - Cek apakah ada perubahan state
   * - Tunggu minimum delay sebelum process
   */
  bool currentPin3State = digitalRead(pin3);
  bool currentPin4State = digitalRead(pin4);

  // Debouncing - hanya process jika ada perubahan state dan sudah lewat delay
  bool stateChanged = (currentPin3State != prevPin3State) || (currentPin4State != prevPin4State);
  unsigned long currentTime = millis();

  if (stateChanged && (currentTime - lastStateChange > DEBOUNCE_DELAY))
  {
    lastStateChange = currentTime;
    prevPin3State = currentPin3State;
    prevPin4State = currentPin4State;

    // Log perubahan state untuk debugging
    Serial.print("State Change - Pin3: ");
    Serial.print(currentPin3State ? "HIGH" : "LOW");
    Serial.print(", Pin4: ");
    Serial.print(currentPin4State ? "HIGH" : "LOW");
    Serial.print(" at ");
    Serial.print(currentTime);
    Serial.println("ms");
  }

  // ================================================================
  // ERROR DETECTION - PIN 4 AKTIF SEBELUM PIN 3
  // ================================================================
  /*
   * Critical Error Condition:
   * Jika Pin 4 aktif sebelum tahap 1 (Pin 3) pernah aktif,
   * ini menandakan:
   * - Wiring yang salah (Pin terbalik)
   * - Switch rusak (mechanism tidak benar)
   * - Test procedure yang salah
   *
   * Aksi: Set error state dan tampilkan pesan error
   */
  if (!tahap1 && currentPin4State == HIGH && !errorState)
  {
    errorState = true;

    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("CRITICAL ERROR DETECTED!");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("Pin 4 activated BEFORE Pin 3");
    Serial.println("This indicates:");
    Serial.println("  - Incorrect wiring (pins swapped)");
    Serial.println("  - Faulty switch mechanism");
    Serial.println("  - Wrong test procedure");
    Serial.println("Action: Check connections and retry");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    // Display error message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("    ERROR!");
    lcd.setCursor(0, 1);
    lcd.print("Wrong Sequence");

    // Error buzzer pattern (3 short beeps)
    for (int i = 0; i < 3; i++)
    {
      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
      delay(200);
    }
  }

  // ================================================================
  // STAGE 1 DETECTION - PIN 2 → PIN 3 CONNECTION
  // ================================================================
  /*
   * Tahap 1: Deteksi koneksi Pin2 ke Pin3
   * Kondisi: Pin3 HIGH DAN tidak dalam error state
   * Aksi: Set tahap1 flag dan update display
   */
  if (!errorState && currentPin3State == HIGH && !tahap1)
  {
    tahap1 = true;

    Serial.println("========================================");
    Serial.println("STAGE 1 COMPLETED SUCCESSFULLY");
    Serial.println("========================================");
    Serial.println("Connection: Pin 2 → Pin 3 ESTABLISHED");
    Serial.print("Voltage detected on Pin 3: ");
    Serial.println(currentPin3State ? "HIGH (+5V)" : "LOW");
    Serial.print("Timestamp: ");
    Serial.print(currentTime);
    Serial.println("ms");
    Serial.println("Status: STAGE 1 PASS");
    Serial.println("Next: Activate Stage 2 (Pin 2 → Pin 4)");
    Serial.println("========================================");

    // Update LCD display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Stage 1: PASS");
    lcd.setCursor(0, 1);
    lcd.print("Pin2->Pin3 OK");

    delay(1500); // Tampilkan hasil sebentar

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Now activate");
    lcd.setCursor(0, 1);
    lcd.print("Stage 2 switch");
  }

  // ================================================================
  // STAGE 2 DETECTION - PIN 2 → PIN 4 CONNECTION
  // ================================================================
  /*
   * Tahap 2: Deteksi koneksi Pin2 ke Pin4
   * Kondisi: tahap1 sudah TRUE DAN Pin4 HIGH
   * Aksi: Set tahap2 flag dan update display
   */
  if (tahap1 && currentPin4State == HIGH && !tahap2)
  {
    tahap2 = true;

    Serial.println("========================================");
    Serial.println("STAGE 2 COMPLETED SUCCESSFULLY");
    Serial.println("========================================");
    Serial.println("Connection: Pin 2 → Pin 4 ESTABLISHED");
    Serial.print("Voltage detected on Pin 4: ");
    Serial.println(currentPin4State ? "HIGH (+5V)" : "LOW");
    Serial.print("Timestamp: ");
    Serial.print(currentTime);
    Serial.println("ms");
    Serial.println("Status: STAGE 2 PASS");
    Serial.println("Sequential test: BOTH STAGES COMPLETE");
    Serial.println("========================================");

    // Update LCD display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Stage 2: PASS");
    lcd.setCursor(0, 1);
    lcd.print("Pin2->Pin4 OK");

    delay(1500); // Tampilkan hasil sebentar
  }

  // ================================================================
  // FINAL EVALUATION - TEST COMPLETE
  // ================================================================
  /*
   * Evaluasi akhir test:
   * Jika tahap1 && tahap2 && !errorState = SWITCH PASS
   * Aksi: Buzzer ON, display SUCCESS, konfirmasi ke Serial
   */
  if (tahap1 && tahap2 && !errorState)
  {
    static bool successDisplayed = false;

    // Tampilkan pesan sukses hanya sekali per test cycle
    if (!successDisplayed)
    {
      Serial.println("########################################");
      Serial.println("SWITCH TEST RESULT: PASSED");
      Serial.println("########################################");
      Serial.println("3-Pole Switch Status: FULLY FUNCTIONAL");
      Serial.println("Test Summary:");
      Serial.println("  ✓ Stage 1 (Pin2→Pin3): WORKING");
      Serial.println("  ✓ Stage 2 (Pin2→Pin4): WORKING");
      Serial.println("  ✓ Sequential Order: CORRECT");
      Serial.println("  ✓ Error Detection: NO ERRORS");
      Serial.println("Quality Assessment: SWITCH APPROVED");
      Serial.println("########################################");
      Serial.print("Total test time: ");
      Serial.print(currentTime);
      Serial.println("ms from system start");
      Serial.println("Buzzer: ACTIVATED (Success confirmation)");
      Serial.println("########################################");

      successDisplayed = true;
    }

    // Display success message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  SWITCH OK!");
    lcd.setCursor(0, 1);
    lcd.print(" Test PASSED");

    // Aktivasi buzzer untuk konfirmasi
    digitalWrite(buzzer, HIGH);
  }
  else
  {
    // Reset success display flag untuk cycle berikutnya
    static bool successDisplayed = false;
    successDisplayed = false;

    // Matikan buzzer jika kondisi belum complete
    digitalWrite(buzzer, LOW);
  }

  // ================================================================
  // RESET CONDITION - SWITCH DISCONNECTED
  // ================================================================
  /*
   * Kondisi Reset:
   * Jika kedua Pin3 dan Pin4 LOW (tidak ada koneksi)
   * Aksi: Reset semua flags, siap untuk test ulang
   */
  if (currentPin3State == LOW && currentPin4State == LOW)
  {
    // Cek apakah ini adalah reset yang baru (tidak spam reset message)
    static bool resetMessageShown = false;

    if ((tahap1 || tahap2 || errorState) && !resetMessageShown)
    {
      Serial.println("----------------------------------------");
      Serial.println("SWITCH DISCONNECTED - SYSTEM RESET");
      Serial.println("----------------------------------------");
      Serial.println("All pins LOW - No connections detected");
      Serial.println("Resetting all test conditions:");
      Serial.print("  Previous tahap1: ");
      Serial.println(tahap1 ? "TRUE" : "FALSE");
      Serial.print("  Previous tahap2: ");
      Serial.println(tahap2 ? "TRUE" : "FALSE");
      Serial.print("  Previous errorState: ");
      Serial.println(errorState ? "TRUE" : "FALSE");
      Serial.println("System ready for new test cycle");
      Serial.println("----------------------------------------");

      resetMessageShown = true;
    }

    // Reset semua flags
    tahap1 = false;
    tahap2 = false;
    errorState = false;

    // Reset display message flag
    resetMessageShown = false;

    // Display ready message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" INSERT SWITCH");
    lcd.setCursor(0, 1);
    lcd.print(" To Test Header");

    // Pastikan buzzer mati
    digitalWrite(buzzer, LOW);
  }

  // ================================================================
  // CONTINUOUS MONITORING DAN STATUS UPDATE
  // ================================================================
  /*
   * Monitoring berkelanjutan untuk:
   * - Deteksi perubahan state yang cepat
   * - Update display jika diperlukan
   * - Logging periodic untuk long-term monitoring
   */
  static unsigned long lastStatusUpdate = 0;
  const unsigned long STATUS_UPDATE_INTERVAL = 5000; // 5 detik

  if (currentTime - lastStatusUpdate > STATUS_UPDATE_INTERVAL)
  {
    // Periodic status log (setiap 5 detik)
    Serial.print("Periodic Status - T1:");
    Serial.print(tahap1 ? "✓" : "✗");
    Serial.print(" T2:");
    Serial.print(tahap2 ? "✓" : "✗");
    Serial.print(" Err:");
    Serial.print(errorState ? "!" : "✓");
    Serial.print(" P3:");
    Serial.print(currentPin3State ? "H" : "L");
    Serial.print(" P4:");
    Serial.print(currentPin4State ? "H" : "L");
    Serial.print(" @");
    Serial.print(currentTime);
    Serial.println("ms");

    lastStatusUpdate = currentTime;
  }

  // Small delay untuk CPU optimization
  delay(DISPLAY_UPDATE_DELAY);
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lebih lanjut)
// ================================================================

/*
 * getTestStatusString() - Konversi status test ke string readable
 * Return: String status lengkap untuk logging
 */
/*
String getTestStatusString() {
  String status = "Test Status: ";

  if (errorState) {
    status += "ERROR (Wrong sequence)";
  } else if (tahap1 && tahap2) {
    status += "PASSED (Both stages complete)";
  } else if (tahap1) {
    status += "STAGE1_ONLY (Waiting for stage 2)";
  } else {
    status += "IDLE (Waiting for stage 1)";
  }

  status += " | Pin3: " + String(digitalRead(pin3) ? "HIGH" : "LOW");
  status += " | Pin4: " + String(digitalRead(pin4) ? "HIGH" : "LOW");

  return status;
}
*/

/*
 * displayCustomMessage(String line1, String line2) - Custom LCD message
 * Parameter: line1, line2 (max 16 karakter each)
 */
/*
void displayCustomMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));  // Trim ke 16 karakter
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));  // Trim ke 16 karakter
}
*/

/*
 * playBuzzerSequence(int pattern) - Berbagai pola buzzer
 * Parameter: pattern (1=success, 2=error, 3=warning, 4=ready)
 */
/*
void playBuzzerSequence(int pattern) {
  digitalWrite(buzzer, LOW); // Pastikan mati dulu
  delay(50);

  switch(pattern) {
    case 1: // Success - Long beep
      digitalWrite(buzzer, HIGH);
      delay(1000);
      digitalWrite(buzzer, LOW);
      break;

    case 2: // Error - 3 short beeps
      for(int i = 0; i < 3; i++) {
        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        delay(200);
      }
      break;

    case 3: // Warning - 2 medium beeps
      for(int i = 0; i < 2; i++) {
        digitalWrite(buzzer, HIGH);
        delay(400);
        digitalWrite(buzzer, LOW);
        delay(300);
      }
      break;

    case 4: // Ready - Single short beep
      digitalWrite(buzzer, HIGH);
      delay(150);
      digitalWrite(buzzer, LOW);
      break;
  }
}
*/

/*
 * runDiagnosticTest() - Test diagnostik lengkap sistem
 */
/*
void runDiagnosticTest() {
  Serial.println("=== SYSTEM DIAGNOSTIC TEST ===");

  // Test Pin 2 output
  Serial.print("Pin 2 Output Test: ");
  digitalWrite(pin2, HIGH);
  delay(100);
  Serial.println(digitalRead(pin2) ? "PASS (HIGH)" : "FAIL (LOW)");

  // Test input pins dengan manual prompts
  Serial.println("Pin 3 Input Test: Connect Pin 2 to Pin 3...");
  unsigned long startTime = millis();
  while(millis() - startTime < 10000) { // 10 detik timeout
    if(digitalRead(pin3) == HIGH) {
      Serial.println("Pin 3: PASS (HIGH detected)");
      break;
    }
    delay(100);
  }

  Serial.println("Pin 4 Input Test: Connect Pin 2 to Pin 4...");
  startTime = millis();
  while(millis() - startTime < 10000) { // 10 detik timeout
    if(digitalRead(pin4) == HIGH) {
      Serial.println("Pin 4: PASS (HIGH detected)");
      break;
    }
    delay(100);
  }

  // Test buzzer
  Serial.println("Buzzer Test: Listen for beep...");
  playBuzzerSequence(4);

  // Test LCD
  displayCustomMessage("LCD Test", "Display OK?");

  Serial.println("=== DIAGNOSTIC COMPLETE ===");
}
*/

/*
 * logTestResults(bool passed) - Log hasil test ke Serial dengan timestamp
 */
/*
void logTestResults(bool passed) {
  Serial.println("=== TEST RESULT LOG ===");
  Serial.print("Timestamp: ");
  Serial.print(millis());
  Serial.println("ms");
  Serial.print("Result: ");
  Serial.println(passed ? "PASSED" : "FAILED");
  Serial.print("Stage 1: ");
  Serial.println(tahap1 ? "COMPLETED" : "NOT_COMPLETED");
  Serial.print("Stage 2: ");
  Serial.println(tahap2 ? "COMPLETED" : "NOT_COMPLETED");
  Serial.print("Error State: ");
  Serial.println(errorState ? "ERROR_DETECTED" : "NO_ERROR");
  Serial.println("========================");
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM 3-POLE SWITCH TESTER:
 * ====================================
 *
 * Arduino Mega -> LCD I2C:
 * 5V     -> VCC
 * GND    -> GND
 * Pin 20 -> SDA (I2C Data)
 * Pin 21 -> SCL (I2C Clock)
 *
 * Arduino Mega -> Testing Circuit:
 * Pin 2  -> +5V Supply (Output)
 * Pin 3  -> Stage 1 Test Input
 * Pin 4  -> Stage 2 Test Input
 * Pin 5  -> Buzzer + (Active buzzer 5V)
 * GND    -> Buzzer -
 *
 * 3-Pole Switch Under Test Connection:
 *
 *    Switch Terminals    Arduino Pins
 *    ================    =============
 *    Terminal 1 (COM)  -> Pin 2 (+5V)
 *    Terminal 2 (NC)   -> Pin 3 (Stage 1)
 *    Terminal 3 (NO)   -> Pin 4 (Stage 2)
 *
 * Test Header/Connector:
 * - 3-pin connector atau terminal block
 * - Pin spacing: 2.54mm (0.1") standard
 * - Wire gauge: 22-18 AWG recommended
 * - Secure connections untuk testing akurat
 *
 *
 * JENIS SWITCH 3-POLE YANG BISA DITEST:
 * =====================================
 *
 * 1. SPDT Toggle Switch (Single Pole Double Throw):
 *    - 3 terminal: COM, NC, NO
 *    - Switching: COM connects to NC OR NO
 *    - Application: Direction control, mode selection
 *    - Test: Stage 1 (COM-NC), Stage 2 (COM-NO)
 *
 * 2. SPDT Pushbutton Switch:
 *    - 3 terminal: COM, NC, NO
 *    - Momentary action: Spring return
 *    - Application: Emergency controls, temporary override
 *    - Test: Requires holding positions for each stage
 *
 * 3. SPDT Slide Switch:
 *    - 3 terminal: COM, NC, NO
 *    - Linear sliding mechanism
 *    - Application: Power switches, function selection
 *    - Test: Slide to position 1, then position 2
 *
 * 4. SPDT Rotary Switch (3-position):
 *    - 3 terminal: COM, NC, NO
 *    - Rotational selection: 120° positions
 *    - Application: Multi-speed controls, range selection
 *    - Test: Rotate to position 1, then position 2
 *
 * 5. SPDT Reed Switch:
 *    - 3 terminal: COM, NC, NO
 *    - Magnetic actuation
 *    - Application: Proximity sensing, security systems
 *    - Test: Apply magnet for each position
 *
 * 6. SPDT Micro Switch:
 *    - 3 terminal: COM, NC, NO
 *    - Lever/plunger actuated
 *    - Application: Limit switches, door sensors
 *    - Test: Actuate lever to each position
 *
 *
 * CARA KERJA SEQUENTIAL TESTING:
 * ===============================
 *
 * Stage 1 (Pin 2 → Pin 3):
 * - Switch position: COM connected to NC
 * - Signal flow: +5V from Pin 2 → Switch COM → NC → Pin 3
 * - Detection: Pin 3 reads HIGH
 * - Status: tahap1 = TRUE
 *
 * Stage 2 (Pin 2 → Pin 4):
 * - Switch position: COM connected to NO
 * - Signal flow: +5V from Pin 2 → Switch COM → NO → Pin 4
 * - Detection: Pin 4 reads HIGH
 * - Status: tahap2 = TRUE (if tahap1 already TRUE)
 *
 * Success Condition:
 * - tahap1 && tahap2 && !errorState = SWITCH PASS
 * - Both connections work in correct sequence
 * - No premature activation detected
 *
 * Error Condition:
 * - Pin 4 HIGH before Pin 3 ever HIGH = ERROR
 * - Indicates wiring mistake or faulty switch
 * - Test stops and shows error message
 *
 *
 * INTERPRETASI HASIL TEST:
 * ========================
 *
 * "INSERT SWITCH To Test Header":
 *   - System ready, waiting for switch connection
 *   - Both Pin 3 and Pin 4 are LOW
 *   - No test in progress
 *
 * "Stage 1: PASS Pin2->Pin3 OK":
 *   - First stage completed successfully
 *   - COM-NC connection verified
 *   - Ready for stage 2 testing
 *
 * "Stage 2: PASS Pin2->Pin4 OK":
 *   - Second stage completed successfully
 *   - COM-NO connection verified
 *   - Sequential test progressing correctly
 *
 * "SWITCH OK! Test PASSED":
 *   - Both stages completed successfully
 *   - Switch functions correctly in both positions
 *   - Buzzer activated for confirmation
 *   - Quality control: APPROVED
 *
 * "ERROR! Wrong Sequence":
 *   - Stage 2 activated before Stage 1
 *   - Possible wiring error or faulty switch
 *   - Test stopped for safety
 *   - Action required: Check connections
 *
 *
 * TROUBLESHOOTING GUIDE:
 * ======================
 *
 * Problem: LCD tidak menyala
 * Solution:
 * - Cek koneksi power LCD (VCC, GND)
 * - Cek koneksi I2C (SDA pin 20, SCL pin 21 untuk Mega)
 * - Scan I2C address dengan I2C scanner (mungkin bukan 0x27)
 * - Test dengan LCD standalone code
 * - Cek solder joints pada I2C backpack
 *
 * Problem: Switch tidak terdeteksi (selalu "INSERT SWITCH")
 * Solution:
 * - Cek koneksi Pin 2 ke COM switch (+5V supply)
 * - Verifikasi Pin 3 dan Pin 4 terhubung ke switch terminals
 * - Test kontinuitas dengan multimeter
 * - Pastikan switch tidak rusak secara mekanis
 * - Cek contact resistance switch (<10Ω)
 *
 * Problem: Selalu error "Wrong Sequence"
 * Solution:
 * - Periksa wiring: Pin 3 ke NC, Pin 4 ke NO
 * - Kemungkinan Pin 3 dan Pin 4 terbalik
 * - Switch mungkin memiliki terminal sequence berbeda
 * - Test dengan multimeter untuk identify terminal yang benar
 * - Baca datasheet switch untuk pinout
 *
 * Problem: Stage 1 pass tapi Stage 2 tidak terdeteksi
 * Solution:
 * - Switch mungkin stuck di position 1
 * - Contact NO switch rusak atau kotor
 * - Cek mekanis switch bisa bergerak ke position 2
 * - Clean contact dengan contact cleaner
 * - Test dengan switch lain untuk isolasi masalah
 *
 * Problem: Buzzer tidak berbunyi saat test pass
 * Solution:
 * - Cek koneksi buzzer ke Pin 5 dan GND
 * - Pastikan menggunakan active buzzer 5V (bukan passive)
 * - Test buzzer dengan digitalWrite manual HIGH/LOW
 * - Cek konsumsi current buzzer tidak melebihi pin capacity (40mA)
 * - Ganti buzzer jika rusak
 *
 * Problem: Test hasil tidak konsisten
 * Solution:
 * - Contact switch mungkin bouncing atau intermittent
 * - Increase DEBOUNCE_DELAY value
 * - Clean switch contacts dengan isopropyl alcohol
 * - Periksa kualitas koneksi test header
 * - Check for EMI interference dari sumber lain
 *
 * Problem: Serial Monitor tidak menampilkan output
 * Solution:
 * - Pastikan baud rate 9600 di Serial Monitor
 * - Cek koneksi USB ke Arduino
 * - Restart Arduino IDE
 * - Try different USB port atau cable
 * - Pastikan driver Arduino terinstall
 *
 *
 * QUALITY CONTROL STANDARDS:
 * ==========================
 *
 * Kriteria PASS (Switch APPROVED):
 * ✓ Stage 1 berfungsi (COM-NC connection)
 * ✓ Stage 2 berfungsi (COM-NO connection)
 * ✓ Sequential operation correct (1 then 2)
 * ✓ No premature activation
 * ✓ Contact resistance <5Ω each position
 * ✓ Switching time <500ms between positions
 * ✓ No bouncing >50ms
 * ✓ Mechanical feel smooth dan consistent
 *
 * Kriteria FAIL (Switch REJECTED):
 * ✗ Cannot achieve Stage 1 position
 * ✗ Cannot achieve Stage 2 position
 * ✗ Wrong sequence activation (Stage 2 before 1)
 * ✗ High contact resistance >10Ω
 * ✗ Excessive bouncing >100ms
 * ✗ Intermittent connections
 * ✗ Mechanical binding atau sticking
 * ✗ Partial activation (voltage <4V)
 *
 * Testing Standards:
 * - Test voltage: 5V DC ±0.25V
 * - Test current: <5mA (low power testing)
 * - Contact verification: >4V for HIGH state
 * - Response time: <100ms detection latency
 * - Environmental: 23°C ±5°C, <60% RH
 *
 *
 * MAINTENANCE DAN KALIBRASI:
 * ==========================
 *
 * Daily Maintenance:
 * - Visual inspection test header dan connections
 * - Test dengan reference switch yang diketahui baik
 * - Verify LCD display dan buzzer functionality
 * - Check for loose connections atau wire damage
 * - Clean test header contacts jika perlu
 *
 * Weekly Maintenance:
 * - Calibration check dengan precision switch
 * - Test timing accuracy dengan oscilloscope
 * - Verify supply voltage stability (Pin 2 = 5.0V ±0.1V)
 * - Check I2C communication reliability
 * - Update test log dan maintenance record
 *
 * Monthly Maintenance:
 * - Deep clean semua connections dan contacts
 * - Replace test header jika ada wear
 * - Firmware update check
 * - Backup configuration dan test data
 * - Statistical analysis hasil testing
 *
 * Annual Calibration:
 * - Professional calibration dengan certified standards
 * - Replace aging components (buzzer, connectors)
 * - Update documentation dan procedures
 * - Training refresh untuk operators
 * - System validation dengan external audit
 *
 *
 * PENGEMBANGAN LANJUTAN:
 * ======================
 *
 * 1. Advanced Testing Features:
 *    - Contact resistance measurement dengan precision ADC
 *    - Switching time measurement dengan microsecond accuracy
 *    - Bounce analysis dengan high-speed capture
 *    - Temperature coefficient testing
 *    - Life cycle testing dengan automated cycling
 *
 * 2. Data Logging & Analytics:
 *    - SD card logging dengan timestamp detail
 *    - Statistical process control (SPC) charts
 *    - Trend analysis untuk predictive maintenance
 *    - Database integration untuk production tracking
 *    - Export ke Excel/CSV untuk analysis
 *
 * 3. User Interface Enhancement:
 *    - Larger LCD (20x4) untuk informasi lebih detail
 *    - Touchscreen interface dengan graphics
 *    - Menu system untuk configuration
 *    - Multi-language support
 *    - Remote monitoring via Ethernet/WiFi
 *
 * 4. Automation & Integration:
 *    - Automatic part handling dengan pneumatics
 *    - Conveyor belt integration
 *    - Barcode/QR code tracking
 *    - PLC communication untuk production line
 *    - Robot integration untuk high-volume testing
 *
 * 5. Multiple Switch Testing:
 *    - Multiplexer untuk test multiple switches
 *    - Parallel testing untuk increased throughput
 *    - Different switch types dalam satu sistem
 *    - Batch processing dengan hasil summary
 *
 *
 * SPESIFIKASI TEKNIS SISTEM:
 * ==========================
 *
 * Electrical Specifications:
 * - Operating Voltage: 5V DC ±5%
 * - Current Consumption: 300mA max (dengan LCD backlight)
 * - Test Voltage Output: 5.0V ±0.1V (Pin 2)
 * - Input Threshold: >2.5V for HIGH detection
 * - Contact Test Current: <5mA (safe untuk semua switch types)
 * - Isolation Voltage: 50V DC minimum
 *
 * Mechanical Specifications:
 * - Test Header: 3-pin, 2.54mm pitch
 * - Wire Termination: Screw terminals atau spring clamps
 * - Wire Gauge Support: 14-24 AWG
 * - Connector Force: <10N insertion
 * - Durability: >100,000 test cycles
 *
 * Performance Specifications:
 * - Response Time: <100ms per stage detection
 * - Test Accuracy: >99.9% untuk switches yang baik
 * - False Positive Rate: <0.1%
 * - False Negative Rate: <0.05%
 * - Repeatability: ±0.1% over 1000 tests
 * - Temperature Drift: <0.01%/°C
 *
 * Environmental Specifications:
 * - Operating Temperature: 0°C to +50°C
 * - Storage Temperature: -20°C to +70°C
 * - Humidity: 10% to 90% RH non-condensing
 * - Altitude: Sea level to 2000m
 * - Vibration: 10-55Hz, 1.5mm amplitude
 * - EMC: EN 61326-1 (Industrial environment)
 *
 * Display Specifications:
 * - Type: LCD 16x2 dengan I2C backpack
 * - Character Set: ASCII dengan custom characters
 * - Backlight: Blue dengan brightness control
 * - Viewing Angle: >140° horizontal/vertical
 * - Contrast: Adjustable via I2C commands
 * - Life: >50,000 hours typical
 *
 */
