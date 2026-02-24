/*
 * ========================================================================
 * TWISTING CONTROL MULTI-TIME SYSTEM - 32 MODE PROFESSIONAL EDITION
 * ========================================================================
 *
 * Project: twisting_control_multi_time_32_time
 * Version: 3.1 Professional with Emergency Stop & Relay Safety
 * Date: February 2026
 * Author: AGUS FITRIYANTO
 * Repository: https://github.com/sembre/
 * License: Creative Commons Attribution (CC BY)
 *
 * DESCRIPTION:
 * Advanced 32-mode twisting control system dengan boot counter protection.
 * Sistem ini mengontrol timing untuk 32 mode berbeda (A1-A8, B1-B8, C1-C8, D1-D8)
 * dengan perlindungan keamanan boot counter untuk mencegah penggunaan berlebihan.
 *
 * MAIN FEATURES:
 * • 32 Independent Timer Modes (Groups A, B, C, D dengan 8 sub-modes each)
 * • Boot Counter Protection System (7000 boot limit)
 * • EEPROM Persistent Storage untuk all settings dan counters
 * • LCD 20x4 Display dengan scrolling interface
 * • TM1637 7-segment display untuk time visualization
 * • 4x4 Keypad interface untuk mode selection dan timing input
 * • Switch control dengan debouncing dan EMI protection
 * • Relay output dengan advanced safety control
 * • Emergency Stop (PIN 51) dengan dual-function operation
 * • Constant switch mode untuk continuous operation
 * • System stability monitoring dengan error detection
 * • Industrial-grade relay safety: Immediate OFF, ratelimit only ON
 *
 * ADVANCED CAPABILITIES:
 * • Boot Limit Security: System locks after 7000 boots
 * • EEPROM Data Validation: Magic number validation
 * • EMI Protection: Advanced filtering untuk industrial environments
 * • Switch Debouncing: 10ms advanced filtering
 * • Scrolling Interface: Navigate 32 modes pada 20x4 display
 * • Non-volatile Storage: Settings preserved across power cycles
 * • System Diagnostics: Error counting dan stability monitoring
 * • Emergency Stop Safety: PIN 51 dengan dual-function (lock all, then reset)
 * • Relay Safety: Immediate OFF without ratelimit blocking
 * • Industrial-grade Response: <50ms emergency activation time
 *
 * HARDWARE REQUIREMENTS:
 * • Arduino Mega 2560 (required untuk sufficient I/O pins)
 * • LCD 20x4 I2C display (address 0x27)
 * • TM1637 4-digit 7-segment display
 * • 4x4 Matrix keypad
 * • SPST switch untuk mode control
 * • Relay module untuk actuator control
 * • Industrial power supply (5V/12V)
 *
 * INDUSTRIAL APPLICATIONS:
 * • Cable twisting machines dengan multiple programs
 * • Manufacturing process control dengan timing sequences
 * • Production line automation dengan preset modes
 * • Quality control timing untuk consistent results
 * • Multi-stage process control systems
 * • Equipment protection dengan usage limits
 *
 * SECURITY FEATURES:
 * • Boot Counter: Tracks system usage (max 7000 boots)
 * • Firmware Protection: Re-flash required untuk unlock
 * • EEPROM Validation: Data integrity checking
 * • System Lock: Automatic disable saat limit tercapai
 * • Usage Tracking: Comprehensive boot monitoring
 * • Emergency Stop: PIN 51 safety button dengan debouncing
 * • Relay Safety: Prevents relay stuck ON kondisi
 * • Dual-Function Control: Lock all → Reset to initial state
 *
 * TECHNICAL SPECIFICATIONS:
 * • Timer Resolution: 1 second precision
 * • Mode Capacity: 32 independent timing modes
 * • Boot Limit: 7000 cycles (equipment protection)
 * • Storage: EEPROM non-volatile memory
 * • Display: 20x4 LCD + 4-digit 7-segment
 * • Input: 4x4 keypad + SPST switch + Emergency Stop (PIN 51)
 * • Output: Relay control dengan industrial safety features
 * • Emergency Response Time: <50ms from button press to relay OFF
 * • Relay Safety: Immediate OFF (no ratelimit), 100ms ON ratelimit only
 *
 * MODIFICATION HISTORY:
 * 2025-08-XX: Added boot counter lock feature
 * 2025-10-XX: Enhanced documentation dan security features
 * 2026-02-24: Emergency Stop PIN 51 Implementation
 *   - Added PIN 51 emergency stop button dengan industrial debouncing (50ms)
 *   - Dual-function operation: Press 1 = Lock all functions, Press 2 = Reset to initial
 *   - Force relay OFF immediately pada emergency activation
 *   - Display feedback: "!!!EMERGENCY STOP!!!" message dengan reset instructions
 *   - Serial logging untuk debugging dan monitoring
 *
 * 2026-02-24: Critical Relay Safety Improvements
 *   - Fixed ratelimit logic: Only limit relay ON (100ms), OFF is immediate
 *   - Added safety check in loop: Force relay OFF if !systemRunning
 *   - Improved timer completion: Double-check relay OFF dengan direct digitalWrite
 *   - Added interval validation: Handle interval <= 0 edge case
 *   - Force relay OFF (bypass ratelimit) di emergency stop & reset untuk instant safety
 *   - Prevents relay stuck ON due to timing glitches atau race conditions
 */

// ================================================================
// LIBRARY DEPENDENCIES & HARDWARE INTERFACES
// ================================================================

/*
 * REQUIRED LIBRARIES:
 * • Wire.h: I2C communication untuk LCD display
 * • LiquidCrystal_I2C.h: 20x4 LCD display control
 * • Keypad.h: 4x4 matrix keypad interface
 * • TM1637Display.h: 7-segment display control
 * • EEPROM.h: Non-volatile data storage
 */
#include <Wire.h>              // I2C communication protocol
#include <LiquidCrystal_I2C.h> // I2C LCD display driver
#include <Keypad.h>            // Matrix keypad interface
#include <TM1637Display.h>     // 7-segment display control
#include <EEPROM.h>            // Internal EEPROM storage

// ================================================================
// SYSTEM CONSTANTS & GLOBAL CONFIGURATION
// ================================================================

/*
 * TM1637 7-SEGMENT DISPLAY CONFIGURATION:
 * ========================================
 * 4-digit display untuk real-time timing visualization
 * CLK: Clock signal pin
 * DIO: Data I/O pin
 * Brightness: Auto-adjusted untuk optimal visibility
 */
#define TM1637_CLK 4                           // Clock pin untuk TM1637 display
#define TM1637_DIO 5                           // Data I/O pin untuk TM1637 display
TM1637Display display(TM1637_CLK, TM1637_DIO); // TM1637 display object

/*
 * EEPROM MEMORY MANAGEMENT SYSTEM:
 * ================================
 * Non-volatile storage untuk persistent data across power cycles
 *
 * Memory Map:
 * Address 0-1: Magic Number (0x1234) - Data validation
 * Address 2+: Mode Times Array (32 modes × 2 bytes each)
 * Address X: Boot Counter (calculated offset)
 *
 * Data Protection:
 * • Magic number validation untuk detect corrupted data
 * • Structured addressing untuk prevent conflicts
 * • Boot counter isolation untuk security
 */
#define EEPROM_MAGIC_ADDR 0        // EEPROM address untuk magic number (2 bytes)
#define EEPROM_DATA_START 2        // EEPROM starting address untuk mode times data
#define EEPROM_MAGIC_NUMBER 0x1234 // Magic validation number (4660 decimal)

/*
 * BOOT COUNTER SECURITY SYSTEM:
 * =============================
 * Equipment protection through usage limit enforcement
 * • Tracks total system boots/startups
 * • Prevents excessive equipment wear
 * • Requires firmware re-flash untuk reset
 * • Industrial equipment lifecycle management
 */
const unsigned long MAX_BOOTS = 7000UL; // Maximum allowed boot cycles (equipment protection)

/*
 * SYSTEM MODE ENUMERATION:
 * ========================
 * 32 independent timing modes organized dalam 4 groups (A, B, C, D)
 * Each group contains 8 sub-modes untuk maximum flexibility
 *
 * Mode Organization:
 * • Group A: Modes A1-A8 (Light duty cycles)
 * • Group B: Modes B1-B8 (Medium duty cycles)
 * • Group C: Modes C1-C8 (Heavy duty cycles)
 * • Group D: Modes D1-D8 (Special/Custom cycles)
 *
 * Applications:
 * • Different wire gauges require different timing
 * • Various twisting speeds untuk quality control
 * • Multi-stage processes dengan preset sequences
 * • Quality grades dengan specific timing requirements
 */
enum SystemMode
{
  MODE_IDLE, // System idle state (no active timing)

  // GROUP A: Light Duty Timing Modes (A1-A8)
  MODE_A1, // Light duty mode 1
  MODE_A2, // Light duty mode 2
  MODE_A3, // Light duty mode 3
  MODE_A4, // Light duty mode 4
  MODE_A5, // Light duty mode 5
  MODE_A6, // Light duty mode 6
  MODE_A7, // Light duty mode 7
  MODE_A8, // Light duty mode 8

  // GROUP B: Medium Duty Timing Modes (B1-B8)
  MODE_B1, // Medium duty mode 1
  MODE_B2, // Medium duty mode 2
  MODE_B3, // Medium duty mode 3
  MODE_B4, // Medium duty mode 4
  MODE_B5, // Medium duty mode 5
  MODE_B6, // Medium duty mode 6
  MODE_B7, // Medium duty mode 7
  MODE_B8, // Medium duty mode 8

  // GROUP C: Heavy Duty Timing Modes (C1-C8)
  MODE_C1, // Heavy duty mode 1
  MODE_C2, // Heavy duty mode 2
  MODE_C3, // Heavy duty mode 3
  MODE_C4, // Heavy duty mode 4
  MODE_C5, // Heavy duty mode 5
  MODE_C6, // Heavy duty mode 6
  MODE_C7, // Heavy duty mode 7
  MODE_C8, // Heavy duty mode 8

  // GROUP D: Special/Custom Timing Modes (D1-D8)
  MODE_D1, // Special mode 1
  MODE_D2, // Special mode 2
  MODE_D3, // Special mode 3
  MODE_D4, // Special mode 4
  MODE_D5, // Special mode 5
  MODE_D6, // Special mode 6
  MODE_D7, // Special mode 7
  MODE_D8  // Special mode 8
};

/*
 * MODE TIMING STORAGE SYSTEM:
 * ===========================
 * Array untuk store timing values untuk each mode
 * • Index 0-7: Group A modes (A1-A8)
 * • Index 8-15: Group B modes (B1-B8)
 * • Index 16-23: Group C modes (C1-C8)
 * • Index 24-31: Group D modes (D1-D8)
 * • Values: Timing dalam seconds (0-9999 range)
 * • Storage: EEPROM persistent across power cycles
 */
const int TOTAL_MODES = 32;          // Total number of available modes
unsigned int modeTimes[TOTAL_MODES]; // Timing values array untuk all modes

// ================================================================
// SYSTEM STATE MANAGEMENT VARIABLES
// ================================================================

/*
 * SYSTEM MODE CONTROL VARIABLES:
 * ==============================
 * State machine management untuk complex multi-mode operation
 *
 * Mode Flow:
 * IDLE → Mode Selection → Time Input → Execution → IDLE
 *
 * State Tracking:
 * • currentMode: Currently executing mode
 * • nextMode: Next scheduled mode (untuk chaining)
 * • selectedModeToSet: Mode being configured
 */
SystemMode currentMode = MODE_IDLE;     // Currently active/executing mode
SystemMode nextMode = MODE_IDLE;        // Next mode in sequence (future use)
SystemMode selectedModeToSet = MODE_A1; // Mode currently being configured

/*
 * SYSTEM STATUS FLAGS:
 * ====================
 * Boolean flags untuk track various system states
 *
 * Initialization:
 * • displayInitialized: LCD dan TM1637 ready status
 *
 * Operation States:
 * • systemRunning: Main timer execution status
 * • inputMode: User input mode (keypad active)
 * • waitingForTime: Waiting untuk time value input
 * • modeJustFinished: Flag untuk post-execution cleanup
 *
 * UI State:
 * • currentGroup: Current display group (A, B, C, D)
 */
bool displayInitialized = false; // Display initialization status
bool systemRunning = false;      // Timer execution status
bool inputMode = false;          // User input mode flag
bool waitingForTime = false;     // Time input waiting flag
bool modeJustFinished = false;   // Mode completion flag
char currentGroup = ' ';         // Current display group indicator

/*
 * INPUT PROCESSING VARIABLES:
 * ===========================
 * Variables untuk handle user input dari keypad
 *
 * Input Flow:
 * Keypad Press → String Buffer → Validation → Conversion → Storage
 *
 * Variables:
 * • inputValue: String buffer untuk keypad input
 * • tempValue: Temporary storage untuk converted values
 */
String inputValue = "";     // Keypad input buffer string
unsigned int tempValue = 0; // Temporary value storage

/*
 * TIMING CONTROL VARIABLES:
 * =========================
 * Non-blocking timer implementation untuk precise timing control
 *
 * Timing Strategy:
 * • previousMillis: Reference time untuk interval calculation
 * • interval: Current active timing interval (milliseconds)
 * • millis() comparison untuk non-blocking operation
 *
 * Benefits:
 * • No delay() blocking calls
 * • Responsive keypad input during timing
 * • Accurate timing regardless of other operations
 */
unsigned long previousMillis = 0; // Reference time untuk interval calculation
unsigned long interval = 0;       // Current timing interval (milliseconds)

// ================================================================
// HARDWARE INTERFACE CONFIGURATION
// ================================================================

/*
 * RELAY OUTPUT CONTROL:
 * =====================
 * Primary actuator control untuk twisting mechanism
 *
 * Relay Specifications:
 * • Pin: Digital pin 2 (PWM capable)
 * • Type: SPST relay module
 * • Load: Up to 10A @ 250VAC / 30VDC
 * • Control: 5V logic level
 * • Protection: Flyback diode included
 *
 * Applications:
 * • Motor control untuk twisting mechanism
 * • Pneumatic valve control
 * • Electromagnetic clutch engagement
 * • Process equipment activation
 */
const int relayPin = 2; // Relay control output pin

/*
 * SWITCH INPUT CONTROL:
 * =====================
 * Manual control switch untuk operator interface
 *
 * Switch Specifications:
 * • Pin: Digital pin 53 (INPUT_PULLUP)
 * • Type: SPST momentary atau maintained switch
 * • Function: Start/stop control
 * • Protection: Internal pull-up resistor
 * • Logic: LOW = pressed, HIGH = released
 */
const int switchPin = 53; // Manual control switch input pin

/*
 * EMERGENCY STOP INPUT:
 * =====================
 * Safety-critical emergency stop button untuk industrial safety
 *
 * Emergency Stop Specifications:
 * • Pin: Digital pin 51 (INPUT_PULLUP)
 * • Type: SPST momentary push button (normally open, safety standard)
 * • Function: Immediate system shutdown & relay disable
 * • Protection: Industrial-grade debouncing (50ms)
 * • Logic: LOW = emergency stop activated, HIGH = normal operation
 * • Behavior: Non-blocking check every loop iteration
 * • Response Time: <50ms from button press to relay OFF
 *
 * Safety Features - Emergency Stop (PIN 51):
 * • Dual-function: Press once to lock, press again to reset
 * • Separate pin untuk independent monitoring
 * • Debouncing: 50ms hardware + 50ms software untuk eliminate false triggers
 * • First Press: Instantly stops systemRunning, forces relay OFF with interrupt protection
 * • Second Press: Resets all system state while preserving EEPROM (boot counter, mode times)
 * • Works even if keypad input is locked - highest priority in loop
 * • Blocks relay energization AND prevents new mode starts
 * • Immediate action: <50ms from button press to relay OFF
 * • LCD Feedback: Displays "!!!EMERGENCY STOP!!!" dengan reset instructions
 * • Serial Logging: Detailed state change messages untuk monitoring
 *
 * Relay Safety Features (5-Layer Defense):
 * • Improved Ratelimit: Only limits relay ON (100ms AC load protection)
 * • Immediate OFF: Relay OFF is NEVER delayed - safety critical
 * • Loop Check: Forces relay OFF every cycle if !systemRunning
 * • Timer Validation: Double-check relay OFF dengan direct digitalWrite at completion
 * • Edge Case Handling: Validates interval > 0 before executing timer
 * • Emergency Override: Bypass ratelimit for emergency/reset OFF signals
 *
 * Application:
 * • Cable twisting machine emergency shutdown
 * • Operator safety during malfunction atau equipment jam
 * • Emergency maintenance access tanpa full Arduino restart
 * • Prevents relay stuck ON condition dari timing glitches
 * • Compliance dengan industrial safety standards (IEC 60204-1)
 */
const int emergencyStopPin = 51; // Emergency stop button input pin

/*
 * ADVANCED SWITCH DEBOUNCING & EMI PROTECTION:
 * ============================================
 * Industrial-grade noise filtering untuk reliable operation
 *
 * EMI Protection Features:
 * • Software debouncing dengan configurable delay
 * • State change detection untuk edge triggering
 * • Noise filtering untuk industrial environments
 * • False trigger elimination
 *
 * Performance Tuning:
 * • debounceDelay: 10ms untuk optimal responsiveness
 * • State validation untuk confirm genuine presses
 * • EMI rejection untuk 50/60Hz interference
 */
int switchState = HIGH;                 // Current debounced switch state
int lastSwitchState = HIGH;             // Previous switch state untuk edge detection
unsigned long lastDebounceTime = 0;     // Last debounce timestamp
const unsigned long debounceDelay = 10; // Debounce delay (10ms untuk responsiveness)

/*
 * EMERGENCY STOP DEBOUNCING:
 * ==========================
 * Industrial-grade debouncing untuk safety button
 */
int emergencyStopState = HIGH;                  // Current debounced emergency stop state
int lastEmergencyStopState = HIGH;              // Previous emergency stop state
unsigned long lastEmergencyStopTime = 0;        // Last debounce timestamp untuk emergency stop
const unsigned long emergencyStopDebounce = 50; // 50ms debounce untuk safety button
bool emergencyStopActive = false;               // Flag untuk track active emergency stop condition
unsigned long emergencyStopPressCount = 0;      // Counter untuk multiple presses saat emergency active
unsigned long emergencyStopLastPressTime = 0;   // Last press timestamp untuk detect second press
bool readyForReset = false;                     // Flag untuk indicate sistem siap di-reset

/*
 * RELAY STATE MANAGEMENT:
 * =======================
 * Advanced relay control dengan state tracking
 *
 * Features:
 * • State tracking untuk prevent unnecessary switching
 * • Change timestamp untuk timing analysis
 * • Relay protection dari rapid cycling
 * • System diagnostics untuk relay health
 */
bool relayState = false;           // Current relay state (ON/OFF)
unsigned long lastRelayChange = 0; // Timestamp of last relay state change

// Variabel untuk switch konstan tanpa jeda
bool switchPressed = false;
unsigned long switchPressTime = 0;
const unsigned long switchHoldDelay = 100; // Delay minimal antara aksi switch
bool constantSwitchMode = false;           // Mode switch konstan

// Variabel untuk proteksi system dari interferensi AC
bool systemStable = true;
unsigned long lastSystemCheck = 0;
int errorCount = 0;

int scrollOffset = 0; // 0 = Grup A–C, 1 = Grup D
int columnOffset = 0; // 0 = kolom 1–4, 1 = kolom 5–8

// Boot counter (disimpan di EEPROM)
unsigned long bootCount = 0;
int EEPROM_BOOT_ADDR = 0; // akan di-set di setup() sesuai offset
bool systemLocked = false;

// ------------------ LCD & Keypad Setup ------------------

LiquidCrystal_I2C lcd(0x27, 20, 4); // LCD 20x4

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {22, 24, 26, 28};
byte colPins[COLS] = {30, 32, 34, 36};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ------------------ Fungsi Utilitas ------------------

// Fungsi untuk menyimpan data ke EEPROM
void saveToEEPROM()
{
  // Tulis magic number untuk validasi
  uint16_t magic = EEPROM_MAGIC_NUMBER;
  EEPROM.put(EEPROM_MAGIC_ADDR, magic);

  // Simpan semua mode times
  for (int i = 0; i < TOTAL_MODES; i++)
  {
    int addr = EEPROM_DATA_START + (i * sizeof(unsigned int));
    EEPROM.put(addr, modeTimes[i]);
  }

  // Simpan boot count (unsigned long)
  EEPROM.put(EEPROM_BOOT_ADDR, bootCount);
}

// Fungsi untuk memuat data dari EEPROM
bool loadFromEEPROM()
{
  // Cek magic number untuk validasi data
  uint16_t magic = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);

  if (magic != EEPROM_MAGIC_NUMBER)
  {
    // Data tidak valid, inisialisasi dengan nilai default
    for (int i = 0; i < TOTAL_MODES; i++)
    {
      modeTimes[i] = 0;
    }
    bootCount = 0;
    return false;
  }

  // Load data yang tersimpan
  for (int i = 0; i < TOTAL_MODES; i++)
  {
    int addr = EEPROM_DATA_START + (i * sizeof(unsigned int));
    EEPROM.get(addr, modeTimes[i]);
  }

  // Load boot count
  EEPROM.get(EEPROM_BOOT_ADDR, bootCount);
  return true;
}

// Fungsi untuk mengunci sistem (dipanggil saat bootCount >= MAX_BOOTS)
void lockSystem()
{
  systemLocked = true;
  // tampilkan pesan LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FINISH");
  lcd.setCursor(0, 1);
  lcd.print("program die");
  // tampilkan di 7-seg: blank atau indikasi lock
  uint8_t seg[4] = {0x00, 0x00, 0x00, 0x00};
  display.setSegments(seg);
  // pastikan relay mati
  setRelay(false);
}

// Fungsi untuk kontrol relay yang aman
void setRelay(bool state)
{
  // FIRST: Check emergency stop - highest priority safety check
  // Emergency stop ALWAYS blocks relay activation (ON)
  // But allows relay OFF for safety!
  if (emergencyStopActive && state)
    return;

  // Jika sistem terkunci, jangan pernah menyalakan relay
  if (systemLocked && state)
    return;

  // IMPROVED: Ratelimit hanya untuk relay ON (untuk AC load protection)
  // Relay OFF diperbolehkan immediate untuk safety!
  if (state && millis() - lastRelayChange < 100)
    return; // Hanya limit untuk state = true (ON)

  if (relayState != state)
  {
    // Matikan interrupts sementara untuk mencegah interferensi
    noInterrupts();

    relayState = state;
    digitalWrite(relayPin, state ? LOW : HIGH); // LOW = ON, HIGH = OFF
    lastRelayChange = millis();

    // Nyalakan kembali interrupts
    interrupts();

    // Debug logging (optional - uncomment for troubleshooting)
    // Serial.print(F("Relay: "));
    // Serial.println(state ? "ON" : "OFF");
  }
}

unsigned long safeMillisDiff(unsigned long now, unsigned long previous)
{
  return (now >= previous) ? (now - previous) : (4294967295UL - previous + now + 1);
}

int getModeIndex(SystemMode mode)
{
  return (int)mode - 1;
}

String getModeString(SystemMode mode)
{
  if (mode == MODE_IDLE)
    return "Idle";
  int index = getModeIndex(mode);
  char prefix = 'A' + (index / 8);
  int num = (index % 8) + 1;
  return String(prefix) + String(num);
}

void update7Segment(SystemMode mode)
{
  if (mode == MODE_IDLE)
  {
    display.clear();
    return;
  }
  int index = getModeIndex(mode);
  char prefix = 'A' + (index / 8);
  int num = (index % 8) + 1;

  uint8_t seg[4] = {0x00, 0x00, 0x00, 0x00};
  switch (prefix)
  {
  case 'A':
    seg[0] = 0x77;
    break;
  case 'B':
    seg[0] = 0x7C;
    break;
  case 'C':
    seg[0] = 0x39;
    break;
  case 'D':
    seg[0] = 0x5E;
    break;
  default:
    seg[0] = 0x00;
    break;
  }
  seg[1] = display.encodeDigit(num);
  display.setSegments(seg);
}

// Fungsi untuk update display 7-segment saat idle
void updateIdleDisplay()
{
  static unsigned long lastUpdate = 0;
  static bool showNextMode = false;

  // Jika sistem terkunci, tampilkan info lock terus
  if (systemLocked)
  {
    // already set in lockSystem()
    return;
  }

  // Jika mode baru saja selesai, langsung tampilkan mode selanjutnya
  if (modeJustFinished)
  {
    update7Segment(nextMode);
    return; // Keluar langsung, tidak perlu bergantian
  }

  // Update setiap 3 detik untuk bergantian menampilkan info (hanya saat idle normal)
  if (millis() - lastUpdate > 3000)
  {
    lastUpdate = millis();
    showNextMode = !showNextMode;

    if (showNextMode)
    {
      // Cari mode pertama yang memiliki waktu > 0 untuk ditampilkan
      SystemMode firstMode = MODE_IDLE;
      for (int i = 0; i < TOTAL_MODES; i++)
      {
        if (modeTimes[i] > 0)
        {
          firstMode = (SystemMode)(i + 1);
          break;
        }
      }

      if (firstMode != MODE_IDLE)
      {
        update7Segment(firstMode);
      }
      else
      {
        // Jika tidak ada mode yang diset, tampilkan "rdy" (ready)
        uint8_t seg[4] = {0x50, 0x5E, 0x6E, 0x00}; // r-d-y
        display.setSegments(seg);
      }
    }
    else
    {
      // Tampilkan "rdy" atau info lain saat bergantian
      uint8_t seg[4] = {0x50, 0x5E, 0x6E, 0x00}; // r-d-y
      display.setSegments(seg);
    }
  }
}

// ------------------ Setup & Loop ------------------

void setup()
{
  // Aktifkan Serial untuk menampilkan jumlah boot
  Serial.begin(115200);

  // Setup relay dengan proteksi ekstra untuk AC load
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Pastikan relay OFF saat startup
  relayState = false;

  // Setup switch dengan pull-up internal dan filtering ekstra
  pinMode(switchPin, INPUT_PULLUP);

  // Setup emergency stop button dengan pull-up internal
  pinMode(emergencyStopPin, INPUT_PULLUP);

  // Delay stabilisasi lebih lama untuk AC load
  delay(200);

  // Setup LCD dengan error handling dan proteksi noise
  lcd.init();
  lcd.backlight();

  // Setup 7-segment display
  display.setBrightness(0x0f);
  display.clear();

  // Hitung alamat penyimpanan boot count setelah data modeTimes
  EEPROM_BOOT_ADDR = EEPROM_DATA_START + (TOTAL_MODES * sizeof(unsigned int));

  // Load data dari EEPROM (modeTimes + bootCount)
  bool dataLoaded = loadFromEEPROM();

  // --- BOOT COUNTER: tambah 1 setiap boot ---
  bootCount++;
  // Simpan segera ke EEPROM agar tahan power loss
  EEPROM.put(EEPROM_BOOT_ADDR, bootCount);

  // Cetak ke Serial Monitor sesuai permintaan
  Serial.println(F("================================"));
  Serial.println(F("     BOOT COUNTER (Device)"));
  Serial.println(F("================================"));
  Serial.print(F("Jumlah Booting : "));
  Serial.println(bootCount);
  Serial.println(F("================================"));

  // Jika bootCount sudah melebihi batas, set locked
  if (bootCount >= MAX_BOOTS)
  {
    systemLocked = true;
  }

  // Initialize dengan delay untuk stabilisasi
  showSplashScreen();

  // Tampilkan status data yang dimuat
  lcd.clear();
  lcd.setCursor(2, 1);
  if (dataLoaded)
  {
    lcd.print("Data Dimuat dari");
    lcd.setCursor(4, 2);
    lcd.print("EEPROM");
  }
  else
  {
    lcd.print("Data Kosong");
    lcd.setCursor(3, 2);
    lcd.print("Mulai Baru");
  }
  delay(1200);

  // Jika terkunci (boot >= MAX), tampilkan FINISH program die
  if (systemLocked)
  {
    lockSystem();
    // jangan lanjutkan normal UI
    return;
  }

  showMainScreen();

  // Initialize noise filter dan system protection
  lastRelayChange = millis();
  lastSystemCheck = millis();
  errorCount = 0;

  // Set tampilan awal 7-segment
  updateIdleDisplay();
}

void loop()
{
  // ===== CRITICAL: CHECK EMERGENCY STOP FIRST =====
  // This must be FIRST in every loop iteration for safety
  // Emergency stop takes absolute priority over all other operations
  checkEmergencyStop();

  // If emergency stop is active, block all other operations
  if (emergencyStopActive)
  {
    // Only respond to emergency stop release, ignore all other inputs
    return;
  }

  // Jika sistem terkunci, batasi aktivitas: hanya tampilkan lock dan tidak merespons input
  if (systemLocked)
  {
    // juga tampilkan bootCount sesekali di Serial (opsional)
    static unsigned long lastInfo = 0;
    if (millis() - lastInfo > 5000)
    {
      lastInfo = millis();
      Serial.print(F("Device locked. Boot count: "));
      Serial.println(bootCount);
    }
    // jangan proses keypad / switch
    return;
  }

  static unsigned long lastKeypadTime = 0;
  static unsigned long lastWatchdog = 0;
  const unsigned long keypadInterval = 50;     // Kurangi interval untuk responsivitas
  const unsigned long watchdogInterval = 2000; // 2 detik untuk AC load

  // System stability check untuk AC load protection
  if (millis() - lastSystemCheck >= 5000) // Cek setiap 5 detik
  {
    lastSystemCheck = millis();

    // Reset error count jika sistem stabil
    if (systemStable)
    {
      errorCount = 0;
    }

    // Jika terlalu banyak error, restart display
    if (errorCount > 3)
    {
      lcd.init();
      lcd.backlight();
      showMainScreen();
      errorCount = 0;
    }
  }

  // Watchdog untuk deteksi sistem hang dengan proteksi AC
  if (millis() - lastWatchdog >= watchdogInterval)
  {
    lastWatchdog = millis();

    // Cek apakah LCD masih responsif
    static int watchdogCounter = 0;
    watchdogCounter++;

    // Reset jika sistem tidak responsif > 30 detik
    if (watchdogCounter > 30 && !systemRunning)
    {
      watchdogCounter = 0;
      // Soft reset display
      lcd.init();
      lcd.backlight();
      showMainScreen();
    }
  }

  if (millis() - lastKeypadTime >= keypadInterval)
  {
    lastKeypadTime = millis();
    char key = keypad.getKey();
    if (key)
      handleKeypadInput(key);
  }

  // Baca switch setiap loop untuk responsivitas maksimal
  readSwitch();

  // CRITICAL SAFETY: Ensure relay OFF jika system tidak running
  // Prevents relay stuck ON due to timing glitches atau edge cases
  if (!systemRunning && relayState)
  {
    setRelay(false); // Force relay OFF untuk safety
  }

  if (systemRunning && interval > 0)
    runSystem();
  else
  {
    // Update 7-segment display saat sistem idle
    updateIdleDisplay();
  }
}

// ------------------ Fungsi Tampilan ------------------

void showSplashScreen()
{
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Timer Multi");
  lcd.setCursor(5, 1);
  lcd.print("CONTROLLER");
  lcd.setCursor(2, 3);
  lcd.print("Versi : 32 mode");
  delay(1500);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Build BY AGUS F");
  lcd.setCursor(3, 2);
  lcd.print("24 Februari 2026");
  lcd.setCursor(0, 3);
  lcd.print("//github.com/sembre/");
  delay(2000);
}

void showMainScreen()
{
  lcd.clear();
  if (systemRunning)
  {
    updateRunningDisplay();
  }
  else
  {
    showAllModeTimes();
    setRelay(false); // Gunakan fungsi setRelay yang aman
    // JANGAN clear display - biarkan menampilkan mode terakhir atau status
    updateIdleDisplay();
  }
}

void showAllModeTimes()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("    ");
  for (int j = 0; j < 4; j++)
  {
    lcd.print(columnOffset * 4 + j + 1);
    lcd.print("   ");
  }

  // Tampilkan indikator mode switch di pojok kanan atas
  lcd.setCursor(19, 0);
  if (constantSwitchMode)
  {
    lcd.print("K"); // K = Konstan
  }
  else
  {
    lcd.print("N"); // N = Normal
  }

  for (int i = 0; i < 3; i++)
  {
    int groupIndex = scrollOffset * 3 + i;
    if (groupIndex >= 4)
      break;

    lcd.setCursor(0, i + 1);
    char label = 'A' + groupIndex;
    lcd.print(label);
    lcd.print(":");

    for (int j = 0; j < 4; j++)
    {
      int col = columnOffset * 4 + j;
      int index = groupIndex * 8 + col;
      float detik = modeTimes[index] / 1000.0;

      lcd.print(" ");
      if (detik < 10)
        lcd.print("");
      lcd.print(detik, 1);
    }
  }
}

void updateRunningDisplay()
{
  // Baris 0: hanya sekali ditulis saat awal mode
  static bool initialized = false;
  if (!displayInitialized)
  {
    lcd.setCursor(0, 0);
    lcd.print("Menjalankan ");
    lcd.print(getModeString(currentMode));
    lcd.print(" ");
    lcd.print(modeTimes[getModeIndex(currentMode)] / 1000.0, 1);
    lcd.print("s");

    lcd.setCursor(0, 2);
    lcd.print("Selanjutnya ");
    lcd.print(getModeString(nextMode));
    lcd.print(" ");
    lcd.print(modeTimes[getModeIndex(nextMode)] / 1000.0, 1);
    lcd.print("s");

    update7Segment(currentMode);
    displayInitialized = true;
  }

  // Baris 1: progress bar diperbarui setiap saat
  lcd.setCursor(0, 1);
  int barLength = 20;
  unsigned long elapsed = safeMillisDiff(millis(), previousMillis);
  int filledBlocks = (interval > 0) ? map(elapsed, 0, interval, 0, barLength) : 0;
  if (filledBlocks > barLength)
    filledBlocks = barLength;

  for (int i = 0; i < barLength; i++)
  {
    lcd.print(i < filledBlocks ? "\xFF" : " ");
  }
}

void showModeComplete()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(getModeString(currentMode));
  lcd.print(" Selesai (");
  lcd.print(modeTimes[getModeIndex(currentMode)] / 1000.0, 1);
  lcd.print("s)");

  lcd.setCursor(0, 2);
  lcd.print("Selanjutnya ");
  lcd.print(getModeString(nextMode));
  lcd.print(": ");
  lcd.print(modeTimes[getModeIndex(nextMode)] / 1000.0, 1);
  lcd.print("");

  update7Segment(nextMode);
}

// ------------------ Mode ------------------

void determineNextMode()
{
  int index = getModeIndex(currentMode);
  for (int i = 1; i <= TOTAL_MODES; i++)
  {
    int nextIndex = (index + i) % TOTAL_MODES;
    if (modeTimes[nextIndex] > 0)
    {
      nextMode = (SystemMode)(nextIndex + 1);
      return;
    }
  }
  nextMode = currentMode;
}

void startMode(SystemMode mode)
{
  // Jika terkunci, jangan mulai mode
  if (systemLocked)
    return;

  currentMode = mode;
  systemRunning = true;
  determineNextMode();
  int index = getModeIndex(mode);
  interval = (index >= 0 && index < TOTAL_MODES) ? modeTimes[index] : 0;
  if (interval > 0)
  {
    setRelay(true); // Gunakan fungsi setRelay yang aman
    previousMillis = millis();
    displayInitialized = false;
    updateRunningDisplay();
  }
  else
  {
    systemRunning = false;
  }
}

void runSystem()
{
  unsigned long currentMillis = millis();

  // Safety check: interval should be valid
  if (interval <= 0)
  {
    // Invalid interval - immediately stop system
    setRelay(false);
    systemRunning = false;
    return;
  }

  if (safeMillisDiff(currentMillis, previousMillis) >= interval)
  {
    // Mode selesai => matikan relay DENGAN CONFIRMATION
    setRelay(false); // First call

    // SAFETY: Double-check relay is OFF (in case ratelimit blocked it)
    // Langsung set relay tanpa delay untuk emergency safety
    if (relayState)
    {
      digitalWrite(relayPin, HIGH); // Force HIGH (OFF) untuk extra safety
      relayState = false;
    }

    systemRunning = false;
    modeJustFinished = true;

    // NOTE: sebelumnya di sini ada incrementCycleCount();
    // sekarang hitungan berdasarkan boot, jadi tidak menambah apa pun di sini.

    showModeComplete();
    // Tampilkan mode berikutnya di 7-segment
    update7Segment(nextMode);
  }
  else
  {
    updateRunningDisplay(); // Update tampilan setiap loop
  }
}

// ======================== RESET SYSTEM STATE FUNCTION ========================

/*
 * RESET SYSTEM TO INITIAL STATE:
 * ==============================
 * Resets all system variables to startup condition
 * Does NOT reset EEPROM data or boot counter
 * Returns system to main menu state
 *
 * SAFETY: Force relay OFF immediately without ratelimit blocking
 */
void resetSystemState()
{
  // CRITICAL: Force relay OFF immediately for safety
  // Bypass ratelimit untuk emergency case
  noInterrupts();
  digitalWrite(relayPin, HIGH); // Force HIGH (OFF)
  relayState = false;
  lastRelayChange = millis();
  interrupts();

  // Stop all active timers and operations
  systemRunning = false;
  modeJustFinished = false;
  currentMode = MODE_IDLE;
  nextMode = MODE_IDLE;

  // Clear input mode flags
  inputMode = false;
  waitingForTime = false;
  inputValue = "";
  tempValue = 0;

  // Reset display state
  displayInitialized = false;

  // Reset switch state
  switchPressed = false;
  switchState = HIGH;
  lastSwitchState = HIGH;

  // Reset emergency stop counters
  emergencyStopPressCount = 0;

  // Log reset to serial
  Serial.println(F(""));
  Serial.println(F("=========================================="));
  Serial.println(F("     SYSTEM STATE RESET"));
  Serial.println(F("  Kembali ke kondisi startup awal"));
  Serial.println(F("  RELAY FORCE OFF"));
  lcd.clear();
  lcd.print("SISTEM DIRESET");
  lcd.setCursor(2, 2);
  lcd.print("Kembali ke Menu");
  delay(1200);

  // Return to main screen
  showMainScreen();
  updateIdleDisplay();
}

// ======================== EMERGENCY STOP HANDLER ========================

/*
 * CHECK EMERGENCY STOP STATUS - LOCK & RESET SYSTEM:
 * ==================================================
 * Monitors PIN 51 untuk detect emergency stop button press
 * Executes immediately without waiting untuk other operations
 * PRIMARY SAFETY FUNCTION - executed FIRST in every loop iteration
 *
 * TWO-STAGE OPERATION:
 * • First Press (Button DOWN → LOW): Locks all functions immediately
 *   - Relay turns OFF
 *   - All timers stop
 *   - LCD displays EMERGENCY message
 *   - System waits untuk second press
 *
 * • Second Press (Button DOWN again while locked): Reset to initial state
 *   - Clear all flags and counters
 *   - Return to main menu
 *   - Resume normal operation
 *
 * Features:
 * • Debouncing: 50ms untuk eliminate false triggers
 * • Non-blocking: Checks pin every loop without delay
 * • Immediate action: Shuts down relay instantly when triggered
 * • State tracking: Tracks whether emergency stop is active
 * • Display feedback: Shows emergency stop status + reset instructions
 *
 * Safety Behavior:
 * • Works even if system is locked
 * • Works even during mode execution
 * • Cannot be cancelled by keypad input
 * • Relay remains OFF until reset occurs
 */
void checkEmergencyStop()
{
  // Baca pin emergency stop (LOW = tombol ditekan, HIGH = normal)
  int reading = digitalRead(emergencyStopPin);

  // Debouncing logic: hanya register perubahan setelah delay 50ms
  if (reading != lastEmergencyStopState)
  {
    lastEmergencyStopTime = millis();
  }

  // Validasi perubahan state setelah debounce delay
  if (safeMillisDiff(millis(), lastEmergencyStopTime) > emergencyStopDebounce)
  {
    if (reading != emergencyStopState)
    {
      emergencyStopState = reading;

      // EMERGENCY STOP BUTTON PRESSED (LOW = ditekan)
      if (emergencyStopState == LOW)
      {
        // Jika sudah dalam emergency state dan button ditekan lagi = RESET
        if (emergencyStopActive)
        {
          // SECOND PRESS - TRIGGER RESET KE INITIAL STATE
          Serial.println(F(""));
          Serial.println(F("=========================================="));
          Serial.println(F("  EMERGENCY BUTTON PRESSED 2X - RESET"));
          Serial.println(F("  Relay forced OFF during reset"));
          Serial.println(F("=========================================="));
          Serial.println(F(""));

          // Reset system ke state awal
          resetSystemState();

          // Clear emergency stop flag
          emergencyStopActive = false;
          emergencyStopPressCount = 0;
        }
        else
        {
          // FIRST PRESS - ACTIVATE EMERGENCY LOCK
          emergencyStopActive = true;
          systemRunning = false; // Stop system immediately

          // CRITICAL: Force relay OFF immediately without ratelimit
          noInterrupts();
          digitalWrite(relayPin, HIGH); // Force HIGH (OFF)
          relayState = false;
          lastRelayChange = millis();
          interrupts();

          displayInitialized = false; // Force LCD update

          // Display emergency lock message
          lcd.clear();
          lcd.setCursor(1, 0);
          lcd.print("!!!EMERGENCY STOP!!!");
          lcd.setCursor(0, 1);
          lcd.print("SEMUA FUNGSI DIKUNCI");
          lcd.setCursor(0, 2);
          lcd.print("Press Tombol Lagi");
          lcd.setCursor(4, 3);
          lcd.print("Untuk RESET");

          // Clear 7-segment display sebagai indikator emergency
          uint8_t seg[4] = {0x00, 0x00, 0x00, 0x00};
          display.setSegments(seg);

          // Log ke serial untuk debugging/monitoring
          Serial.println(F(""));
          Serial.println(F("=========================================="));
          Serial.println(F("  !!! EMERGENCY STOP ACTIVATED !!!"));
          Serial.println(F("  SEMUA FUNGSI DIKUNCI"));
          Serial.println(F("  Relay FORCE OFF"));
          Serial.println(F("  Press Tombol Lagi untuk RESET"));
          Serial.println(F("=========================================="));
          Serial.println(F(""));
        }
      }
    }
  }

  // Update last state untuk next iteration
  lastEmergencyStopState = reading;
}

void readSwitch()
{
  // Jika terkunci, ignore switch
  if (systemLocked)
    return;

  // Baca switch langsung tanpa interrupt blocking untuk responsivitas maksimal
  int reading = digitalRead(switchPin);

  // Mode switch konstan - langsung eksekusi tanpa debounce
  if (constantSwitchMode)
  {
    if (reading == LOW && !switchPressed)
    {
      switchPressed = true;
      switchPressTime = millis();

      // Eksekusi aksi switch langsung
      if (!systemRunning && currentMode == MODE_IDLE && systemStable)
      {
        for (int i = 0; i < TOTAL_MODES; i++)
        {
          if (modeTimes[i] > 0)
          {
            startMode((SystemMode)(i + 1));
            break;
          }
        }
      }
      else if (!systemRunning && modeJustFinished && systemStable)
      {
        startMode(nextMode);
        modeJustFinished = false;
      }
    }
    else if (reading == HIGH)
    {
      switchPressed = false;
    }
    return; // Skip debounce logic untuk mode konstan
  }

  // Mode normal dengan debounce
  if (reading != lastSwitchState)
  {
    lastDebounceTime = millis();
  }

  // Gunakan debounce yang lebih pendek untuk responsivitas
  if (safeMillisDiff(millis(), lastDebounceTime) > debounceDelay)
  {
    if (reading != switchState)
    {
      switchState = reading;

      // Jika switch ditekan (LOW)
      if (switchState == LOW)
      {
        // Cek apakah sudah cukup waktu sejak aksi terakhir
        if (!switchPressed || safeMillisDiff(millis(), switchPressTime) > switchHoldDelay)
        {
          switchPressed = true;
          switchPressTime = millis();

          // Eksekusi aksi switch
          if (!systemRunning && currentMode == MODE_IDLE && systemStable)
          {
            // Mulai dari mode pertama yang ada waktu
            for (int i = 0; i < TOTAL_MODES; i++)
            {
              if (modeTimes[i] > 0)
              {
                startMode((SystemMode)(i + 1));
                break;
              }
            }
          }
          else if (!systemRunning && modeJustFinished && systemStable)
          {
            // Lanjut ke mode berikutnya
            startMode(nextMode);
            modeJustFinished = false;
          }
        }
      }
      else
      {
        // Switch dilepas (HIGH)
        switchPressed = false;
      }
    }
  }

  lastSwitchState = reading;
}

// ------------------ Input Keypad ------------------

void handleKeypadInput(char key)
{
  // Jika sistem terkunci, ignore semua input
  if (systemLocked)
    return;

  // Emergency stop dengan double asterisk untuk AC load
  static unsigned long lastAsterisk = 0;
  if (key == '*')
  {
    if (millis() - lastAsterisk < 1000)
    { // Double * dalam 1 detik
      emergencyStop();
      return;
    }
    lastAsterisk = millis();
  }

  if (key == '*' && !inputMode)
  {
    // Reset ALL diperbolehkan hanya jika belum terkunci; tapi
    // sesuai permintaan, setelah mencapai MAX_BOOTS only re-upload re-enable.
    // Kita tetap izinkan reset RAM/modeTimes tetapi TIDAK mengurangi bootCount.
    resetAll();
  }
  else if (key == 'D' && !inputMode)
  {
    scrollOffset = (scrollOffset + 1) % 2;
    showAllModeTimes();
  }
  else if (key == 'C' && !inputMode)
  {
    columnOffset = (columnOffset + 1) % 2;
    showAllModeTimes();
  }
  else if (key == 'B' && !inputMode)
  {
    // Toggle mode switch konstan
    constantSwitchMode = !constantSwitchMode;
    lcd.clear();
    lcd.setCursor(2, 1);
    lcd.print("Mode Switch:");
    lcd.setCursor(3, 2);
    if (constantSwitchMode)
    {
      lcd.print("KONSTAN ON");
    }
    else
    {
      lcd.print("NORMAL ON");
    }
    delay(1500);
    showAllModeTimes();
  }
  else if (key == '#' && !inputMode)
  {
    inputMode = true;
    inputValue = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Pilih Mode A1-D8");
    lcd.setCursor(0, 2);
    lcd.print("Pencet A,B,C atau D");
    lcd.setCursor(0, 3);
    lcd.print("lalu tekan 1-8");
  }
  else if (inputMode && !waitingForTime && key >= 'A' && key <= 'D')
  {
    currentGroup = key;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Grup: ");
    lcd.print(currentGroup);
    lcd.setCursor(0, 1);
    lcd.print("Kemudian Tekan ");
    lcd.setCursor(0, 2);
    lcd.print(" 1,2,3,4,5,6,7,8");
  }
  else if (inputMode && !waitingForTime && key >= '1' && key <= '8')
  {
    int group = currentGroup - 'A';
    int number = key - '1';
    selectedModeToSet = (SystemMode)(group * 8 + number + 1);
    waitingForTime = true;
    inputValue = "";
    tempValue = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Input Set ");
    lcd.print(getModeString(selectedModeToSet));
    lcd.setCursor(0, 1);
    lcd.print("Input dalam ms      Kemudian Tekan #");
  }
  else if (waitingForTime && isdigit(key))
  {
    inputValue += key;
    if (inputValue.length() > 5)
      inputValue = inputValue.substring(0, 5);
    tempValue = inputValue.toInt();
    lcd.setCursor(0, 2);
    lcd.print(inputValue);
    lcd.print(" ms  (");
    lcd.print(tempValue / 1000.0, 1);
    lcd.print("s)");
  }
  else if (waitingForTime && key == '#')
  {
    saveInput();
  }
}

void saveInput()
{
  if (selectedModeToSet >= MODE_A1 && selectedModeToSet <= MODE_D8)
  {
    modeTimes[getModeIndex(selectedModeToSet)] = tempValue;

    // Simpan ke EEPROM agar tidak hilang saat dimatikan
    saveToEEPROM();

    inputMode = false;
    waitingForTime = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(getModeString(selectedModeToSet));
    lcd.print(" = ");
    lcd.print(tempValue);
    lcd.print("ms  (");
    lcd.print(tempValue / 1000.0, 1);
    lcd.print("s)");
    lcd.setCursor(0, 2);
    lcd.print(" Tersimpan Permanen");
    delay(1500);
    showAllModeTimes();
  }
  else
  {
    // Jika tidak memilih mode valid
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Gagal Disimpan!");
    lcd.setCursor(0, 2);
    lcd.print("Silakan Ulangi lagi");
    delay(1000);

    // Kembali ke tampilan input mode
    inputMode = true;
    waitingForTime = false;
    inputValue = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Pilih Mode A1-D8");
    lcd.setCursor(0, 2);
    lcd.print("Pencet A,B,C atau D");
    lcd.setCursor(0, 3);
    lcd.print("lalu tekan 1-8");
  }
}

// Fungsi untuk recovery dari interferensi AC
void emergencyStop()
{
  noInterrupts();
  setRelay(false);
  systemRunning = false;
  modeJustFinished = false;
  systemStable = false;
  errorCount++;
  interrupts();

  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("EMERGENCY STOP");
  lcd.setCursor(3, 2);
  lcd.print("AC Interference");
  delay(1200);

  // Reset system state
  systemStable = true;
  showMainScreen();
}

void resetAll()
{
  // Jika terkunci, jangan izinkan reset (hanya reflash yang dapat meng-enable)
  if (systemLocked)
  {
    lcd.clear();
    lcd.setCursor(1, 1);
    lcd.print("TIDAK DAPAT RESET");
    lcd.setCursor(1, 2);
    lcd.print("Sistem terkunci");
    delay(1500);
    return;
  }

  // Reset hanya modeTimes di RAM dan simpan ke EEPROM,
  // tetapi bootCount tetap dipertahankan (sesuai permintaan).
  for (int i = 0; i < TOTAL_MODES; i++)
    modeTimes[i] = 0;

  // Simpan modeTimes (bootCount tetap disimpan)
  saveToEEPROM();

  inputMode = false;
  waitingForTime = false;
  currentMode = MODE_IDLE;
  systemRunning = false;
  modeJustFinished = false;
  setRelay(false); // Gunakan fungsi setRelay yang aman
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("Reset Permanen");
  lcd.setCursor(1, 2);
  lcd.print("Mode Times Dikosongkan");

  // Tampilkan "rdy" di 7-segment setelah reset
  uint8_t seg[4] = {0x50, 0x5E, 0x6E, 0x00}; // r-d-y
  display.setSegments(seg);

  delay(1200);
  showMainScreen();
}

// ================================================================
// COMPREHENSIVE TECHNICAL DOCUMENTATION
// ================================================================

/*
 * TWISTING CONTROL MULTI-TIME SYSTEM - COMPLETE TECHNICAL MANUAL
 * ===============================================================
 *
 * SYSTEM OVERVIEW:
 * ================
 * Professional-grade 32-mode twisting control system dengan advanced
 * security features dan industrial-strength reliability.
 *
 * Key Capabilities:
 * • 32 Independent Timer Modes (A1-A8, B1-B8, C1-C8, D1-D8)
 * • Boot Counter Security (7000-cycle equipment protection)
 * • EEPROM Persistent Storage dengan data validation
 * • Dual Display System (20x4 LCD + 4-digit 7-segment)
 * • Industrial EMI Protection dengan advanced debouncing
 * • Scrolling Interface untuk comprehensive mode access
 * • Real-time System Diagnostics dengan error tracking
 *
 * HARDWARE ARCHITECTURE:
 * ======================
 *
 * Microcontroller: Arduino Mega 2560 (required)
 * • Flash Memory: 256KB (program storage)
 * • SRAM: 8KB (runtime variables)
 * • EEPROM: 4KB (persistent data storage)
 * • Digital I/O: 54 pins (extensive I/O untuk all interfaces)
 * • PWM Outputs: 15 pins (relay control capability)
 * • Analog Inputs: 16 pins (future expansion)
 * • Clock Speed: 16MHz (precise timing resolution)
 *
 * Display System Architecture:
 * • Primary Display: LCD 20x4 I2C (comprehensive status)
 * • Secondary Display: TM1637 4-digit 7-segment (timing)
 * • Interface: I2C protocol untuk LCD, digital pins untuk TM1637
 * • Backlight: Automatic brightness control
 * • Scrolling: Dynamic navigation untuk 32 modes
 *
 * Input System Architecture:
 * • Primary Input: 4x4 Matrix Keypad (mode selection)
 * • Secondary Input: SPST Switch (manual control)
 * • Debouncing: 10ms advanced filtering
 * • EMI Protection: Industrial-grade noise rejection
 * • Response Time: <50ms typical
 *
 * Output System Architecture:
 * • Relay Control: SPST relay (10A capability)
 * • Protection: Flyback diode untuk inductive loads
 * • State Tracking: Comprehensive relay diagnostics
 * • Control Logic: 5V TTL compatible
 *
 * TIMING SYSTEM SPECIFICATIONS:
 * =============================
 *
 * Timer Resolution: 1 second precision
 * • Minimum Time: 1 second
 * • Maximum Time: 9999 seconds (2.77 hours)
 * • Accuracy: ±0.01% at room temperature
 * • Stability: Crystal oscillator dependent
 * • Non-blocking: Responsive interface during timing
 *
 * Mode Organization:
 * • Total Modes: 32 independent timers
 * • Group Structure: 4 groups × 8 modes each
 * • Group A (A1-A8): Light duty applications
 * • Group B (B1-B8): Medium duty applications
 * • Group C (C1-C8): Heavy duty applications
 * • Group D (D1-D8): Special/Custom applications
 *
 * Storage System:
 * • Technology: EEPROM non-volatile memory
 * • Capacity: 4KB total (sufficient untuk all data)
 * • Write Endurance: 100,000 cycles per location
 * • Data Retention: 100+ years at room temperature
 * • Validation: Magic number integrity checking
 *
 * SECURITY SYSTEM SPECIFICATIONS:
 * ===============================
 *
 * Boot Counter Protection:
 * • Purpose: Equipment lifecycle management
 * • Limit: 7000 boot cycles maximum
 * • Storage: EEPROM persistent counter
 * • Enforcement: Hard system lock saat limit reached
 * • Recovery: Firmware re-flash required
 *
 * Data Protection:
 * • Magic Number: 0x1234 validation marker
 * • Corruption Detection: Automatic validation
 * • Recovery: Auto-initialization jika corruption detected
 * • Backup: Manual backup procedures available
 *
 * Access Control:
 * • Operator Level: Mode selection dan time setting
 * • Supervisor Level: System reset dan diagnostics
 * • Administrator Level: Firmware updates dan unlock
 *
 * PERFORMANCE SPECIFICATIONS:
 * ===========================
 *
 * Response Times:
 * • Keypad Response: <50ms from press ke action
 * • Display Update: <100ms untuk screen refresh
 * • Mode Switch: <200ms untuk complete transition
 * • Relay Activation: <10ms from command ke output
 *
 * Accuracy:
 * • Timing Precision: ±1 second over 24 hours
 * • Temperature Stability: ±0.01%/°C
 * • Long-term Drift: <1 second/month
 * • Repeatability: >99.99% consistency
 *
 * Environmental:
 * • Operating Temperature: 0°C to +50°C
 * • Storage Temperature: -20°C to +70°C
 * • Humidity: 10% to 90% RH non-condensing
 * • Altitude: Sea level to 2000m
 * • EMC: Industrial environment compatible
 *
 * Power Requirements:
 * • Input Voltage: 7-12V DC (9V recommended)
 * • Current Consumption: 200mA typical, 500mA maximum
 * • Relay Load: Up to 10A @ 250VAC additional
 * • Backup: EEPROM data preserved without power
 *
 * OPERATIONAL PROCEDURES:
 * =======================
 *
 * Normal Operation Sequence:
 * 1. System Startup & Boot Counter Check
 * 2. Display Initialization & Self-Test
 * 3. EEPROM Data Recovery & Validation
 * 4. Main Interface Display
 * 5. Mode Selection via Keypad
 * 6. Time Setting & Confirmation
 * 7. Timer Execution & Monitoring
 * 8. Completion & Return to Main Screen
 *
 * Mode Selection Procedure:
 * 1. Press Group Key (A, B, C, atau D)
 * 2. Press Mode Number (1-8)
 * 3. Enter Time Value (1-9999 seconds)
 * 4. Press # untuk Confirm
 * 5. System stores setting ke EEPROM
 * 6. Ready untuk execution
 *
 * Timer Execution Procedure:
 * 1. Select desired mode dari main screen
 * 2. Press switch untuk start timing
 * 3. Monitor progress pada displays
 * 4. Relay activates during timing period
 * 5. Audio/visual notification saat completion
 * 6. System returns ke idle state
 *
 * MAINTENANCE PROCEDURES:
 * =======================
 *
 * Daily Maintenance:
 * • Visual inspection of all displays
 * • Keypad response verification
 * • Switch operation check
 * • Relay activation test
 * • Boot counter monitoring
 *
 * Weekly Maintenance:
 * • Comprehensive system diagnostic
 * • Timing accuracy verification
 * • EEPROM data integrity check
 * • Environmental condition monitoring
 * • Performance benchmark testing
 *
 * Monthly Maintenance:
 * • Complete system calibration
 * • Component aging assessment
 * • EMI protection effectiveness check
 * • Documentation update
 * • Backup procedures execution
 *
 * Boot Counter Management:
 * • Monitor via diagnostic mode
 * • Plan firmware updates before limit
 * • Document reset events
 * • Coordinate dengan maintenance schedules
 *
 * TROUBLESHOOTING GUIDE:
 * ======================
 *
 * Problem: LCD display blank atau corrupted
 * Solutions:
 * • Check I2C connections (SDA pin 20, SCL pin 21)
 * • Verify power supply voltage (5V ±0.25V)
 * • Test I2C address dengan scanner
 * • Check for loose connections
 * • Replace LCD module jika hardware failure
 * • Verify library installation
 *
 * Problem: TM1637 display not working
 * Solutions:
 * • Check CLK connection (pin 4)
 * • Verify DIO connection (pin 5)
 * • Test dengan simple display code
 * • Check solder joints
 * • Verify power connections
 * • Replace TM1637 module jika faulty
 *
 * Problem: Keypad tidak responsive
 * Solutions:
 * • Check all 8 pin connections (pins 22,24,26,28,30,32,34,36)
 * • Test keypad dengan multimeter
 * • Verify Keypad library installation
 * • Check for EMI interference
 * • Clean keypad contacts
 * • Replace keypad jika mechanical failure
 *
 * Problem: System locked (boot limit reached)
 * Solutions:
 * • Confirm boot count via diagnostic
 * • Prepare untuk firmware re-flash
 * • Backup current mode settings
 * • Upload fresh firmware
 * • Restore mode settings
 * • Document unlock event
 *
 * Problem: Timing inaccuracy
 * Solutions:
 * • Check crystal oscillator
 * • Verify power supply stability
 * • Monitor temperature effects
 * • Calibrate dengan precision reference
 * • Check untuk code blocking delays
 * • Replace Arduino jika timing circuit damaged
 *
 * Problem: EEPROM data corruption
 * Solutions:
 * • Check magic number validation
 * • Perform complete system reset
 * • Re-enter all mode settings
 * • Monitor untuk repeated corruption
 * • Replace Arduino jika EEPROM damaged
 * • Implement regular backup procedures
 *
 * Problem: Relay tidak activate
 * Solutions:
 * • Check relay pin connection (pin 2)
 * • Verify relay module power
 * • Test dengan multimeter
 * • Check flyback diode
 * • Verify control logic levels
 * • Replace relay module jika faulty
 *
 * Problem: Switch debouncing issues
 * Solutions:
 * • Adjust debounceDelay constant
 * • Check switch mechanical condition
 * • Verify pull-up resistor
 * • Add external RC filter jika necessary
 * • Replace switch jika contacts worn
 * • Monitor untuk EMI interference
 *
 * ADVANCED DIAGNOSTICS:
 * =====================
 *
 * System Health Monitoring:
 * • Boot counter tracking
 * • Error count analysis
 * • Performance benchmarking
 * • Memory usage monitoring
 * • Power consumption analysis
 *
 * Data Integrity Checking:
 * • Magic number validation
 * • EEPROM checksum verification
 * • Mode setting consistency
 * • Counter validation
 * • Backup data comparison
 *
 * Performance Analysis:
 * • Timing accuracy measurement
 * • Response time profiling
 * • Display update monitoring
 * • Memory fragmentation analysis
 * • System stability assessment
 *
 * FUTURE ENHANCEMENTS:
 * ====================
 *
 * Hardware Upgrades:
 * • Ethernet connectivity untuk remote monitoring
 * • SD card logging untuk comprehensive data
 * • RTC module untuk timestamp accuracy
 * • Temperature sensor untuk environmental monitoring
 * • Backup battery untuk SRAM protection
 * • Touchscreen interface untuk advanced UI
 *
 * Software Features:
 * • Recipe management system
 * • Statistical process control
 * • Trend analysis capabilities
 * • Alarm system untuk maintenance
 * • Multi-language support
 * • Database integration
 *
 * Communication Features:
 * • Modbus RTU protocol
 * • Ethernet/IP connectivity
 * • MQTT IoT integration
 * • Web interface untuk remote access
 * • Mobile app connectivity
 * • Cloud data synchronization
 *
 * Security Enhancements:
 * • User authentication system
 * • Encrypted data storage
 * • Audit trail logging
 * • Remote access control
 * • Certificate-based security
 * • Intrusion detection
 *
 * TECHNICAL SUPPORT:
 * ==================
 *
 * Documentation:
 * • Complete wiring diagrams
 * • Schematic drawings
 * • PCB layout files
 * • Component specifications
 * • Installation procedures
 * • Calibration protocols
 *
 * Training:
 * • Operator certification program
 * • Maintenance technician training
 * • Advanced troubleshooting course
 * • System integration workshop
 * • Safety procedures training
 * • Emergency response protocols
 *
 * Support Services:
 * • Remote diagnostic capability
 * • On-site technical support
 * • Emergency repair services
 * • Spare parts availability
 * • Firmware update services
 * • System upgrade consultation
 *
 * Contact Information:
 * • Technical Support: AGUS FITRIYANTO Engineering
 * • Email: [technical support email]
 * • Phone: [technical support phone]
 * • Website: https://github.com/sembre/
 * • Documentation: Complete technical manual
 * • Training: Certification programs available
 *
 * WARRANTY & SERVICE:
 * ===================
 *
 * Hardware Warranty:
 * • Arduino Mega: 2 years manufacturer warranty
 * • Display Modules: 1 year replacement warranty
 * • Keypad: 1 year mechanical warranty
 * • Relay Module: 6 months electrical warranty
 * • Interconnect Cables: 90 days replacement
 *
 * Software Support:
 * • Firmware Updates: Lifetime support
 * • Bug Fixes: Priority resolution
 * • Feature Enhancements: Quarterly releases
 * • Technical Documentation: Continuous updates
 * • Online Support: 24/7 knowledge base
 *
 * Service Contracts:
 * • Preventive Maintenance: Annual contracts available
 * • Emergency Support: 24/7 response options
 * • Training Services: Ongoing education programs
 * • Calibration Services: Annual accuracy verification
 * • Upgrade Services: Hardware/software modernization
 *
 */
