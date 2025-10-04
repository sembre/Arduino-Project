/*
 * ================================================================
 * 5-CIRCUIT CONTINUITY TESTER
 * ================================================================
 *
 * Deskripsi:
 * Program untuk menguji kontinuitas 5 rangkaian secara otomatis.
 * Dapat mendeteksi kondisi:
 * - GOOD: Rangkaian tertutup dan tidak ada crossing
 * - OPEN: Rangkaian terputus/terbuka
 * - CROSS: Rangkaian yang saling terhubung (short circuit)
 *
 * Hardware yang Dibutuhkan:
 * - Arduino Uno/Nano
 * - LCD 16x2 (pin RS=7, E=8, D4-D7=9-12)
 * - LED indikator (pin 13, built-in)
 * - 5 set kabel uji dengan konektor
 * - Resistor pull-up internal digunakan
 *
 * Cara Kerja:
 * 1. Program mengirim sinyal HIGH ke satu output (endA)
 * 2. Membaca semua input (endB) untuk mendeteksi sinyal
 * 3. Jika sinyal hanya diterima di input yang sesuai = GOOD
 * 4. Jika tidak ada sinyal yang diterima = OPEN
 * 5. Jika sinyal diterima di input lain = CROSS
 *
 * Hasil Test:
 * - LCD menampilkan status: "PASSED" atau "Terputus/OPEN"
 * - LED pin 13: ON = PASSED, OFF = FAILED
 * - Serial Monitor: Detail hasil setiap circuit
 *
 * Author: Sembre
 * Date: 2025
 * Version: 1.0
 * ================================================================
 */

#include <LiquidCrystal.h>

// ================================================================
// KONFIGURASI PIN DAN VARIABEL
// ================================================================

// Konfigurasi LCD 16x2
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); // RS, E, D4, D5, D6, D7

// Pin Definitions
const int LED_INDICATOR = 13; // LED indikator hasil test (built-in)

// Array pin untuk end A (output pins) - mengirim sinyal test
int endA[5] = {2, 3, 4, 5, 6}; // Digital pins 2-6 sebagai OUTPUT

// Array pin untuk end B (input pins) - menerima sinyal test
int endB[5] = {A4, A3, A2, A1, A0}; // Analog pins A0-A4 sebagai INPUT_PULLUP

// Array untuk menyimpan hasil test
int result[5] = {-1, -1, -1, -1, -1};  // Hasil akhir: 0=GOOD, >5=CROSS, open jika counter=5
int test[5] = {-1, -1, -1, -1, -1};    // Nilai pembacaan sementara
int counter[5] = {-1, -1, -1, -1, -1}; // Counter untuk menghitung kondisi error

// Flag status test
bool fail = false; // Flag jika ada circuit yang gagal

// Konstanta hasil test
const int RESULT_GOOD = 0;           // Circuit baik
const int RESULT_OPEN_THRESHOLD = 5; // Threshold untuk mendeteksi circuit terbuka
const int TOTAL_CIRCUITS = 5;        // Jumlah total circuit yang ditest
// ================================================================
// SETUP FUNCTION - Inisialisasi sistem
// ================================================================
void setup() {
  // Inisialisasi LED indikator
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW); // LED mati di awal

  // Inisialisasi komunikasi serial untuk debugging
  Serial.begin(115200);
  Serial.println("=================================");
  Serial.println("5-CIRCUIT CONTINUITY TESTER v1.0");
  Serial.println("=================================");
  Serial.println("Initializing system...");

  // Inisialisasi LCD 16x2
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Circuit Tester");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Setup pin modes untuk semua circuit
  Serial.println("Setting up pins:");
  for (int i = 0; i < TOTAL_CIRCUITS; i++)
  {
    // Set pin endA sebagai OUTPUT (untuk mengirim sinyal test)
    pinMode(endA[i], OUTPUT);
    digitalWrite(endA[i], LOW); // Set ke LOW di awal

    // Set pin endB sebagai INPUT_PULLUP (untuk menerima sinyal test)
    pinMode(endB[i], INPUT_PULLUP);

    // Print konfigurasi pin untuk debugging
    Serial.print("Circuit ");
    Serial.print(i);
    Serial.print(": endA=");
    Serial.print(endA[i]);
    Serial.print(" (OUTPUT), endB=");
    Serial.print(endB[i]);
    Serial.println(" (INPUT_PULLUP)");
  }

  delay(2000); // Delay untuk membaca pesan inisialisasi
  Serial.println("System ready. Starting test...");
  Serial.println("---------------------------------");
}
// ================================================================
// MAIN LOOP - Program utama yang berjalan terus menerus
// ================================================================
void loop() {
  // Jalankan test kontinuitas untuk 5 circuit
  // Fungsi ini akan:
  // 1. Melakukan test kontinuitas semua circuit
  // 2. Menampilkan hasil di LCD dan Serial Monitor
  // 3. Mengontrol LED indikator
  // 4. Reset system otomatis setelah test selesai
  runTest_5x2();
}
// ================================================================
// FUNGSI UTAMA TEST - Menjalankan test kontinuitas 5 circuit
// ================================================================
/*
 * Fungsi runTest_5x2()
 *
 * Cara Kerja:
 * 1. Untuk setiap circuit i (0-4):
 *    - Reset counter dan set semua output ke LOW
 *    - Test setiap kemungkinan koneksi dengan circuit j (0-4)
 *    - Set output j ke HIGH, baca input i
 *    - Analisis hasil:
 *      * Jika i==j dan sinyal HIGH = GOOD (koneksi benar)
 *      * Jika i!=j dan sinyal HIGH = CROSS (koneksi silang)
 *      * Jika tidak ada sinyal HIGH sama sekali = OPEN
 *
 * Hasil Kode:
 * - result[i] = 0 : Circuit GOOD
 * - result[i] > 5 : Circuit CROSS dengan circuit lain
 * - counter[i] = 5 : Circuit OPEN (tidak ada koneksi)
 *
 * Output:
 * - LCD: Status keseluruhan (PASSED/FAILED)
 * - Serial: Detail hasil setiap circuit
 * - LED: ON=PASSED, OFF=FAILED
 */
void runTest_5x2(){
  String resultS = ""; // String untuk menyimpan hasil ringkas

  // Reset flag dan inisialisasi display
  fail = false;

  // Debounce dan setup display
  delay(500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Check: 5 Circuit");
  lcd.setCursor(0, 1);

  Serial.println("Starting continuity test...");
  Serial.println("Testing each circuit:");

  // ================================================================
  // TEST LOOP UTAMA - Test setiap circuit satu per satu
  // ================================================================
  for (int i = 0; i < TOTAL_CIRCUITS; i++)
  {
    // Reset counter untuk circuit ini
    counter[i] = 0;
    result[i] = -1; // Reset result

    // Pastikan semua output dalam kondisi LOW
    for (int j = 0; j < TOTAL_CIRCUITS; j++)
    {
      digitalWrite(endA[j], LOW);
    }

    Serial.print("Testing circuit ");
    Serial.print(i);
    Serial.print(": ");

    // ================================================================
    // TEST KONEKSI - Test circuit i dengan semua kemungkinan output
    // ================================================================
    for (int j = 0; j < TOTAL_CIRCUITS; j++)
    {
      // Aktifkan output j dan baca input i
      digitalWrite(endA[j], HIGH);
      delay(10); // Small delay untuk stabilitas
      test[i] = digitalRead(endB[i]);

      // Analisis hasil pembacaan
      if (test[i] == HIGH && j != i)
      {
        // Sinyal diterima dari output yang salah = CROSS CIRCUIT
        counter[i]++;
        result[i] = RESULT_OPEN_THRESHOLD + j; // Simpan info circuit mana yang cross

#ifdef DEBUG_VERBOSE
        Serial.print("[CROSS with ");
        Serial.print(j);
        Serial.print("] ");
#endif
      }
      else if (test[i] == HIGH && j == i && result[i] < RESULT_OPEN_THRESHOLD)
      {
        // Sinyal diterima dari output yang benar = GOOD CIRCUIT
        result[i] = RESULT_GOOD;

#ifdef DEBUG_VERBOSE
        Serial.print("[GOOD] ");
#endif
      }

      // Matikan output untuk test selanjutnya
      digitalWrite(endA[j], LOW);
    }

    // ================================================================
    // EVALUASI HASIL - Tentukan status circuit berdasarkan test
    // ================================================================
    Serial.print("Circuit ");
    Serial.print(i);
    Serial.print(" -> ");

    if (result[i] == RESULT_GOOD)
    {
      // Circuit dalam kondisi baik
      Serial.println("GOOD");
      resultS += "1"; // 1 = Good
    }
    else if (counter[i] == TOTAL_CIRCUITS)
    {
      // Tidak ada sinyal yang diterima = OPEN CIRCUIT
      Serial.println("OPEN (No connection)");
      resultS += "O"; // O = Open
      fail = true;
    }
    else
    {
      // Ada koneksi silang = CROSS CIRCUIT
      Serial.print("CROSS (Connected to circuit ");
      Serial.print(result[i] - RESULT_OPEN_THRESHOLD);
      Serial.println(")");
      resultS += "X"; // X = Cross
      fail = true;
    }
  }

  // ================================================================
  // TAMPILKAN HASIL AKHIR
  // ================================================================
  Serial.println("---------------------------------");
  Serial.print("Test Result Summary: ");
  Serial.println(resultS);
  Serial.println("---------------------------------");

  // Update LED indikator dan LCD
  if (fail)
  {
    digitalWrite(LED_INDICATOR, LOW); // LED mati = FAILED
    Serial.println("OVERALL RESULT: FAILED");
    Serial.println("Action: Check connections for OPEN/CROSS circuits");
    lcd.print(" Terputus/CROSS");
  }
  else
  {
    digitalWrite(LED_INDICATOR, HIGH); // LED hidup = PASSED
    Serial.println("OVERALL RESULT: PASSED");
    Serial.println("Action: All circuits are good");
    lcd.print("     PASSED");
  }

  // ================================================================
  // AUTO RESET SYSTEM
  // ================================================================
  Serial.println("Auto-reset in 1 second...");
  delay(500);
  Serial.println("Resetting system...");
  delay(500);

  // Deklarasi fungsi reset ke alamat 0 (restart Arduino)
  void (*resetFunc)(void) = 0;
  resetFunc(); // Panggil reset function
}

// ================================================================
// UTILITY FUNCTIONS (Opsional - untuk pengembangan lebih lanjut)
// ================================================================

/*
 * printCircuitStatus() - Debug function untuk print status semua circuit
 * Uncomment untuk debugging detail
 */
/*
void printCircuitStatus() {
  Serial.println("=== CIRCUIT STATUS DEBUG ===");
  for(int i = 0; i < TOTAL_CIRCUITS; i++) {
    Serial.print("Circuit ");
    Serial.print(i);
    Serial.print(": result=");
    Serial.print(result[i]);
    Serial.print(", counter=");
    Serial.print(counter[i]);
    Serial.print(", test=");
    Serial.println(test[i]);
  }
  Serial.println("===========================");
}
*/

/*
 * testSingleCircuit(int circuitNum) - Test circuit tertentu saja
 * Parameter: circuitNum (0-4)
 * Return: 0=GOOD, 1=OPEN, 2=CROSS
 */
/*
int testSingleCircuit(int circuitNum) {
  if(circuitNum < 0 || circuitNum >= TOTAL_CIRCUITS) return -1;

  // Reset semua output
  for(int j = 0; j < TOTAL_CIRCUITS; j++) {
    digitalWrite(endA[j], LOW);
  }

  int localCounter = 0;
  int localResult = -1;

  // Test circuit
  for(int j = 0; j < TOTAL_CIRCUITS; j++) {
    digitalWrite(endA[j], HIGH);
    delay(10);
    int reading = digitalRead(endB[circuitNum]);

    if(reading == HIGH && j != circuitNum) {
      localCounter++;
      localResult = 2; // CROSS
    }
    else if(reading == HIGH && j == circuitNum) {
      localResult = 0; // GOOD
    }

    digitalWrite(endA[j], LOW);
  }

  if(localCounter == TOTAL_CIRCUITS) {
    localResult = 1; // OPEN
  }

  return localResult;
}
*/

// ================================================================
// DOKUMENTASI TEKNIS DAN TROUBLESHOOTING
// ================================================================

/*
 * WIRING DIAGRAM:
 * ===============
 *
 * Arduino Uno -> LCD 16x2:
 * GND     -> VSS, RW, K (Backlight -)
 * 5V      -> VDD, A (Backlight +)
 * Pin 7   -> RS
 * Pin 8   -> E
 * Pin 9   -> D4
 * Pin 10  -> D5
 * Pin 11  -> D6
 * Pin 12  -> D7
 *
 * Arduino Uno -> Test Circuits:
 * Pin 2   -> End A Circuit 0
 * Pin 3   -> End A Circuit 1
 * Pin 4   -> End A Circuit 2
 * Pin 5   -> End A Circuit 3
 * Pin 6   -> End A Circuit 4
 *
 * Pin A0  -> End B Circuit 4
 * Pin A1  -> End B Circuit 3
 * Pin A2  -> End B Circuit 2
 * Pin A3  -> End B Circuit 1
 * Pin A4  -> End B Circuit 0
 *
 * Pin 13  -> LED Indikator (Built-in)
 *
 *
 * TROUBLESHOOTING:
 * ===============
 *
 * Problem: LCD tidak menampilkan apa-apa
 * Solution:
 * - Cek koneksi power LCD (5V, GND)
 * - Cek koneksi data LCD (pins 7-12)
 * - Adjust kontras LCD dengan potensiometer
 *
 * Problem: Semua circuit terdeteksi OPEN
 * Solution:
 * - Cek koneksi pull-up pada input pins
 * - Pastikan kabel test terhubung dengan baik
 * - Cek kabel jumper Arduino ke breadboard
 *
 * Problem: Semua circuit terdeteksi CROSS
 * Solution:
 * - Ada short circuit pada breadboard
 * - Cek isolasi antar jalur kabel test
 * - Periksa solder joints pada konektor
 *
 * Problem: Result tidak konsisten
 * Solution:
 * - Tambah delay pada pembacaan (sudah ada 10ms)
 * - Cek kualitas kontak pada konektor test
 * - Pastikan tidak ada interferensi EMI
 *
 *
 * MODIFIKASI DAN PENGEMBANGAN:
 * ============================
 *
 * 1. Menambah jumlah circuit:
 *    - Ubah TOTAL_CIRCUITS
 *    - Tambah pin di array endA[] dan endB[]
 *    - Sesuaikan LCD display jika perlu
 *
 * 2. Menambah mode test:
 *    - Manual test per circuit
 *    - Continuous monitoring mode
 *    - Data logging ke SD card
 *
 * 3. Menambah interface:
 *    - Push button untuk kontrol
 *    - Buzzer untuk audio feedback
 *    - Ethernet/WiFi untuk remote monitoring
 *
 * 4. Menambah fitur:
 *    - Resistance measurement
 *    - Voltage level testing
 *    - Test result history
 *
 *
 * KALIBRASI:
 * ==========
 *
 * 1. Test dengan kabel pendek yang diketahui baik
 * 2. Adjust delay jika diperlukan untuk stabilitas
 * 3. Test dengan berbagai jenis konektor
 * 4. Verifikasi dengan multimeter untuk akurasi
 *
 *
 * MAINTENANCE:
 * ============
 *
 * 1. Bersihkan konektor test secara berkala
 * 2. Cek kabel test untuk kerusakan fisik
 * 3. Kalibrasi ulang jika hasil tidak akurat
 * 4. Update firmware jika ada perbaikan bug
 *
 */
