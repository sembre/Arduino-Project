/*
 * ========================================================================
 * TWISTING CONTROL MULTI-TIME SYSTEM - 32 MODE PROFESSIONAL EDITION
 * ========================================================================
 *
 * Version: 5.0 (EMI / Relay Fail-Safe Edition) - ARDUINO UNO VERSION
 * ========================================================================
 * Perbaikan stabilitas v5.0:
 * - Relay aktif-LOW dibuat fail-safe dan OFF dipaksa sebagai kondisi default
 * - Hardware watchdog 2 detik
 * - Timeout dan recovery bus I2C
 * - LCD diinisialisasi ulang setelah transien relay mereda
 * - Debounce switch diperkuat dan jalur bypass debounce dihapus
 * - EEPROM 32-bit dengan migrasi otomatis dari versi 4.7
 *
 * Fitur Serial Monitor:
 * - Boot Counter
 * - Status sistem lengkap (mode aktif, relay, emergency stop, lock)
 * - Log start/stop mode
 * - Log input setting mode
 * - Log error dan reset
 * - Daftar mode aktif saat boot dan setiap perubahan
 * ========================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <TM1637Display.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// ========== PIN & KONSTAN ==========
#define TM1637_CLK 4
#define TM1637_DIO 5
TM1637Display display(TM1637_CLK, TM1637_DIO);

#define EEPROM_MAGIC_ADDR 0
#define EEPROM_DATA_START 2
#define EEPROM_MAGIC_NUMBER        0xA507
#define EEPROM_LEGACY_MAGIC_NUMBER 0x1234
const uint32_t MAX_BOOTS = 2500UL;
const uint32_t MAX_MODE_TIME_MS = 99999UL;

enum SystemMode {
  MODE_IDLE,
  MODE_A1, MODE_A2, MODE_A3, MODE_A4, MODE_A5, MODE_A6, MODE_A7, MODE_A8,
  MODE_B1, MODE_B2, MODE_B3, MODE_B4, MODE_B5, MODE_B6, MODE_B7, MODE_B8,
  MODE_C1, MODE_C2, MODE_C3, MODE_C4, MODE_C5, MODE_C6, MODE_C7, MODE_C8,
  MODE_D1, MODE_D2, MODE_D3, MODE_D4, MODE_D5, MODE_D6, MODE_D7, MODE_D8
};
const int TOTAL_MODES = 32;
uint32_t modeTimes[TOTAL_MODES];

// ========== STATE VARIABLES ==========
SystemMode currentMode = MODE_IDLE;
SystemMode nextMode = MODE_IDLE;
SystemMode selectedNextMode = MODE_IDLE;
SystemMode selectedModeToSet = MODE_A1;

bool systemRunning = false;
bool inputMode = false;
bool waitingForTime = false;
char currentGroup = ' ';

char inputValue[6] = {0};
byte inputLength = 0;
uint32_t tempValue = 0;

unsigned long previousMillis = 0;
unsigned long interval = 0;

unsigned long relayOnStartTime = 0;
const unsigned long RELAY_SAFETY_MARGIN = 500;
const uint8_t RELAY_TIMEOUT_PERCENT = 120;
bool relayTimeoutTriggered = false;

// === PENYESUAIAN PIN UNTUK ARDUINO UNO ===
const byte relayPin = 2;
const byte switchPin = 3;         // PIN BARU (Uno)
const byte emergencyStopPin = 6;  // PIN BARU (Uno)
const byte RELAY_ON_LEVEL = LOW;
const byte RELAY_OFF_LEVEL = HIGH;

int switchState = HIGH;
int lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 40;

int emergencyStopState = HIGH;
int lastEmergencyStopState = HIGH;
unsigned long lastEmergencyStopTime = 0;
const unsigned long emergencyStopDebounce = 80;
bool emergencyStopActive = false;

bool relayState = false;
unsigned long lastRelayChange = 0;

bool switchPressed = false;
unsigned long switchPressTime = 0;
const unsigned long switchHoldDelay = 100;

bool systemStable = true;
unsigned long lastSystemCheck = 0;
int errorCount = 0;

int activeModeIndices[TOTAL_MODES];
int activeModeCount = 0;
int activeModePage = 0;
const int MODES_PER_PAGE = 6;

unsigned long bootCount = 0;
int EEPROM_BOOT_ADDR = 0;
bool systemLocked = false;

// Penyegaran LCD ditunda setelah relay berpindah agar EMI sudah mereda.
bool lcdRefreshPending = false;
bool lcdFullReinitPending = false;
unsigned long lcdRefreshDue = 0;
const unsigned long LCD_RELAY_SETTLE_MS = 250;

// Menyimpan penyebab reset MCU. Dijalankan sangat awal sebelum setup().
uint8_t mcusrMirror __attribute__((section(".noinit")));
void captureResetCause(void) __attribute__((naked)) __attribute__((section(".init3")));
void captureResetCause(void) {
  mcusrMirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

LiquidCrystal_I2C lcd(0x27, 20, 4);

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// === PENYESUAIAN PIN KEYPAD UNTUK ARDUINO UNO ===
byte rowPins[ROWS] = {7, 8, 9, 10}; 
byte colPins[COLS] = {11, 12, A0, A1}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ========== SERIAL LOG FUNCTIONS ==========
void serialPrintHeader() {
  Serial.println(F("========================================"));
  Serial.println(F(" TWISTING CONTROL SYSTEM v5.0 FAIL-SAFE"));
  Serial.println(F("========================================"));
}

void serialPrintBootInfo() {
  Serial.print(F("Boot Counter: "));
  Serial.println(bootCount);
  Serial.print(F("Max Boots Allowed: "));
  Serial.println(MAX_BOOTS);
  if (bootCount >= MAX_BOOTS) {
    Serial.println(F("!!! SISTEM TERKUNCI !!! (Boot limit exceeded)"));
  } else {
    Serial.println(F("Sistem siap."));
  }
}

void serialPrintActiveModes() {
  Serial.println(F("--- Daftar Mode Aktif (Waktu > 0) ---"));
  if (activeModeCount == 0) {
    Serial.println(F("Tidak ada mode aktif. Tekan # untuk setting."));
  } else {
    for (int i = 0; i < activeModeCount; i++) {
      int idx = activeModeIndices[i];
      char prefix = 'A' + (idx/8);
      int num = (idx%8)+1;
      uint32_t timeMs = modeTimes[idx];
      float seconds = timeMs / 1000.0;
      Serial.print(prefix); Serial.print(num);
      Serial.print(F(" : "));
      Serial.print(timeMs); Serial.print(F(" ms ("));
      Serial.print(seconds, 1); Serial.println(F(" s)"));
    }
  }
  Serial.println(F("--------------------------------------"));
}

void serialPrintSystemStatus() {
  Serial.println(F("--- STATUS SISTEM ---"));
  Serial.print(F("Sistem Running   : ")); Serial.println(systemRunning ? F("YES") : F("NO"));
  Serial.print(F("Emergency Stop   : ")); Serial.println(emergencyStopActive ? F("AKTIF") : F("TIDAK"));
  Serial.print(F("System Locked    : ")); Serial.println(systemLocked ? F("YA") : F("TIDAK"));
  Serial.print(F("Relay State      : ")); Serial.println(relayState ? F("ON") : F("OFF"));
  Serial.print(F("Current Mode     : "));
  if (currentMode == MODE_IDLE) Serial.println(F("IDLE"));
  else Serial.println(getModeString(currentMode));
  Serial.print(F("Next Mode        : "));
  if (nextMode == MODE_IDLE) Serial.println(F("IDLE"));
  else Serial.println(getModeString(nextMode));
  Serial.print(F("Selected Next    : "));
  if (selectedNextMode == MODE_IDLE) Serial.println(F("IDLE"));
  else Serial.println(getModeString(selectedNextMode));
  Serial.print(F("Active Modes     : ")); Serial.println(activeModeCount);
  Serial.print(F("LCD Page         : ")); Serial.print(activeModePage+1);
  int totalPages = (activeModeCount + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
  Serial.print(F(" / ")); Serial.println(totalPages);
  Serial.println(F("-----------------------"));
}

void serialLogModeStart(SystemMode mode) {
  int idx = getModeIndex(mode);
  uint32_t t = modeTimes[idx];
  float sec = t/1000.0;
  Serial.print(F("[START] Mode "));
  Serial.print(getModeString(mode));
  Serial.print(F(" -> "));
  Serial.print(t); Serial.print(F(" ms ("));
  Serial.print(sec, 1); Serial.println(F(" s)"));
  Serial.print(F("Next mode after finish: "));
  Serial.println(getModeString(findNextActiveMode(mode)));
}

void serialLogModeFinish(SystemMode mode) {
  Serial.print(F("[FINISH] Mode "));
  Serial.print(getModeString(mode));
  Serial.println(F(" selesai."));
}

void serialLogRelayOn() {
  Serial.println(F("[RELAY] ON"));
}

void serialLogRelayOff() {
  Serial.println(F("[RELAY] OFF"));
}

void serialLogEmergencyStop(bool activated) {
  if (activated) {
    Serial.println(F("!!! EMERGENCY STOP AKTIF !!! Semua fungsi dikunci."));
  } else {
    Serial.println(F("Emergency STOP di-reset. Sistem normal."));
  }
}

void serialLogSystemReset() {
  Serial.println(F("!!! SYSTEM RESET oleh pengguna (tombol * dua kali) !!!"));
}

void serialLogEepromSave(SystemMode mode, uint32_t value) {
  Serial.print(F("[EEPROM] Mode "));
  Serial.print(getModeString(mode));
  Serial.print(F(" disimpan dengan nilai "));
  Serial.print(value); Serial.print(F(" ms ("));
  Serial.print(value/1000.0, 1); Serial.println(F(" s)"));
}

void serialLogError(const char* msg) {
  Serial.print(F("[ERROR] "));
  Serial.println(msg);
}

void serialLogInputStart() {
  Serial.println(F("Mode input aktif. Pengguna akan memilih mode dan memasukkan waktu."));
}

void serialLogInputModeSelected(SystemMode mode) {
  Serial.print(F("Mode dipilih: "));
  Serial.println(getModeString(mode));
}

// ========== UTILITY ==========
void saveToEEPROM() {
  uint16_t invalidMagic = 0;
  EEPROM.put(EEPROM_MAGIC_ADDR, invalidMagic);

  for (int i = 0; i < TOTAL_MODES; i++) {
    int addr = EEPROM_DATA_START + i * sizeof(uint32_t);
    EEPROM.put(addr, modeTimes[i]);
  }

  EEPROM.put(EEPROM_BOOT_ADDR, bootCount);

  uint16_t magic = EEPROM_MAGIC_NUMBER;
  EEPROM.put(EEPROM_MAGIC_ADDR, magic);
}

bool loadFromEEPROM() {
  uint16_t magic = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);

  if (magic == EEPROM_MAGIC_NUMBER) {
    for (int i = 0; i < TOTAL_MODES; i++) {
      int addr = EEPROM_DATA_START + i * sizeof(uint32_t);
      EEPROM.get(addr, modeTimes[i]);
      if (modeTimes[i] > MAX_MODE_TIME_MS) modeTimes[i] = 0;
    }
    EEPROM.get(EEPROM_BOOT_ADDR, bootCount);
    return true;
  }

  if (magic == EEPROM_LEGACY_MAGIC_NUMBER) {
    for (int i = 0; i < TOTAL_MODES; i++) {
      uint16_t legacyValue = 0;
      int oldAddr = EEPROM_DATA_START + i * sizeof(uint16_t);
      EEPROM.get(oldAddr, legacyValue);
      modeTimes[i] = legacyValue;
    }

    const int legacyBootAddr = EEPROM_DATA_START + TOTAL_MODES * sizeof(uint16_t);
    EEPROM.get(legacyBootAddr, bootCount);
    saveToEEPROM();
    Serial.println(F("[EEPROM] Data versi lama berhasil dimigrasikan ke v5.0."));
    return true;
  }

  for (int i = 0; i < TOTAL_MODES; i++) modeTimes[i] = 0;
  bootCount = 0;
  return false;
}


void updateActiveModesList() {
  activeModeCount = 0;
  for (int i=0; i<TOTAL_MODES; i++) {
    if (modeTimes[i] > 0) activeModeIndices[activeModeCount++] = i;
  }
  int maxPage = (activeModeCount > 0) ? (activeModeCount-1)/MODES_PER_PAGE : 0;
  if (activeModePage > maxPage) activeModePage = maxPage;
  if (activeModePage < 0) activeModePage = 0;
}

void updateSelectedNextMode() {
  if (activeModeCount == 0) {
    selectedNextMode = MODE_IDLE;
    return;
  }
  int idx = getModeIndex(selectedNextMode);
  bool found = false;
  for (int i=0; i<activeModeCount; i++) {
    if (activeModeIndices[i] == idx) { found = true; break; }
  }
  if (!found || selectedNextMode == MODE_IDLE) {
    selectedNextMode = (SystemMode)(activeModeIndices[0] + 1);
  }
}

int findPageForMode(SystemMode mode) {
  if (activeModeCount == 0) return 0;
  int modeIdx = getModeIndex(mode);
  for (int i=0; i<activeModeCount; i++) {
    if (activeModeIndices[i] == modeIdx) return i / MODES_PER_PAGE;
  }
  return 0;
}

void autoPageToMarkedMode() {
  if (activeModeCount == 0) return;
  SystemMode markedMode;
  if (systemRunning) {
    markedMode = currentMode;
  } else {
    markedMode = selectedNextMode;
  }
  int targetPage = findPageForMode(markedMode);
  if (targetPage != activeModePage) {
    activeModePage = targetPage;
    showAllModeTimes();
  }
}

SystemMode findNextActiveMode(SystemMode mode) {
  if (activeModeCount == 0) return MODE_IDLE;
  int currentIdx = getModeIndex(mode);
  int pos = -1;
  for (int i=0; i<activeModeCount; i++) {
    if (activeModeIndices[i] == currentIdx) { pos = i; break; }
  }
  if (pos == -1) return (SystemMode)(activeModeIndices[0] + 1);
  int nextPos = (pos + 1) % activeModeCount;
  return (SystemMode)(activeModeIndices[nextPos] + 1);
}

void selectPreviousMode() {
  if (activeModeCount == 0) return;
  int currentPos = -1;
  for (int i=0; i<activeModeCount; i++) {
    if (activeModeIndices[i] == getModeIndex(selectedNextMode)) {
      currentPos = i;
      break;
    }
  }
  if (currentPos == -1) currentPos = 0;
  int newPos = (currentPos - 1 + activeModeCount) % activeModeCount;
  selectedNextMode = (SystemMode)(activeModeIndices[newPos] + 1);
  showAllModeTimes();
  autoPageToMarkedMode();
  update7Segment(selectedNextMode);
  Serial.print(F("[NAVIGASI] Previous mode: "));
  Serial.println(getModeString(selectedNextMode));
}

void selectNextMode() {
  if (activeModeCount == 0) return;
  int currentPos = -1;
  for (int i=0; i<activeModeCount; i++) {
    if (activeModeIndices[i] == getModeIndex(selectedNextMode)) {
      currentPos = i;
      break;
    }
  }
  if (currentPos == -1) currentPos = -1;
  int newPos = (currentPos + 1) % activeModeCount;
  selectedNextMode = (SystemMode)(activeModeIndices[newPos] + 1);
  showAllModeTimes();
  autoPageToMarkedMode();
  update7Segment(selectedNextMode);
  Serial.print(F("[NAVIGASI] Next mode: "));
  Serial.println(getModeString(selectedNextMode));
}

void lockSystem() {
  systemLocked = true;
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(F("FINISH"));
  lcd.setCursor(0,1); lcd.print(F("program die"));
  uint8_t blank[4] = {0,0,0,0};
  display.setSegments(blank);
  setRelay(false);
  Serial.println(F("[LOCK] Sistem terkunci karena mencapai batas boot maksimum."));
}

void setRelay(bool state) {
  if (!state) {
    bool wasOn = relayState;
    digitalWrite(relayPin, RELAY_OFF_LEVEL);
    relayState = false;
    relayOnStartTime = 0;
    relayTimeoutTriggered = false;
    lastRelayChange = millis();
    if (wasOn) serialLogRelayOff();
    return;
  }

  if (emergencyStopActive || systemLocked || !systemStable || interval == 0) {
    digitalWrite(relayPin, RELAY_OFF_LEVEL);
    relayState = false;
    return;
  }

  digitalWrite(relayPin, RELAY_ON_LEVEL);
  relayState = true;
  relayOnStartTime = millis();
  relayTimeoutTriggered = false;
  lastRelayChange = millis();
  serialLogRelayOn();
}

unsigned long safeMillisDiff(unsigned long now, unsigned long prev) {
  return now - prev;
}

void checkRelayTimeout() {
  if (relayState && relayOnStartTime > 0 && interval > 0) {
    uint32_t maxAllowed = (interval * RELAY_TIMEOUT_PERCENT) / 100UL;
    maxAllowed += RELAY_SAFETY_MARGIN;

    if (safeMillisDiff(millis(), relayOnStartTime) > maxAllowed && !relayTimeoutTriggered) {
      relayTimeoutTriggered = true;
      errorCount++;
      setRelay(false);
      systemRunning = false;
      serialLogError("Relay timeout! Sistem dihentikan.");
      requestLCDRefresh(100, true);
    }
  } else if (!relayState) {
    relayTimeoutTriggered = false;
  }
}


void initializeLCDHardware() {
  lcd.init();
  lcd.backlight();
  byte arrowRight[8] = {0x00,0x04,0x06,0x1F,0x06,0x04,0x00,0x00};
  lcd.createChar(0, arrowRight);
}

void configureI2C() {
  Wire.begin();
  Wire.setClock(50000UL);  
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000UL, true);
  Wire.clearWireTimeoutFlag();
#endif
}

void recoverI2CBus() {
  Wire.end();
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, OUTPUT);
  digitalWrite(SCL, HIGH);
  delayMicroseconds(5);

  for (byte i = 0; i < 9; i++) {
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
  }

  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(SDA, HIGH);
  delayMicroseconds(5);

  pinMode(SDA, INPUT_PULLUP);
  configureI2C();
}

void requestLCDRefresh(unsigned long delayMs, bool fullReinit) {
  lcdRefreshPending = true;
  if (fullReinit) lcdFullReinitPending = true;
  lcdRefreshDue = millis() + delayMs;
}

void serviceLCDRefresh() {
  if (!lcdRefreshPending) return;
  if ((long)(millis() - lcdRefreshDue) < 0) return;

  lcdRefreshPending = false;
  bool doFullReinit = lcdFullReinitPending;
  lcdFullReinitPending = false;

  if (doFullReinit) initializeLCDHardware();
  showAllModeTimes();
  autoPageToMarkedMode();
}

void checkI2CTimeout() {
#if defined(WIRE_HAS_TIMEOUT)
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    errorCount++;
    serialLogError("Timeout I2C terdeteksi. Relay dimatikan dan LCD dipulihkan.");
    resetLCDModule();
  }
#endif
}

void checkLCDHealth() {
  checkI2CTimeout();
  serviceLCDRefresh();
}

void resetLCDModule() {
  setRelay(false);
  systemRunning = false;
  recoverI2CBus();
  initializeLCDHardware();
  showAllModeTimes();
  Serial.println(F("[LCD] LCD dan bus I2C berhasil diinisialisasi ulang."));
}

int getModeIndex(SystemMode mode) { return (int)mode - 1; }

const char* getModeString(SystemMode mode) {
  static char modeText[5];
  if (mode == MODE_IDLE) return "Idle";

  int idx = getModeIndex(mode);
  modeText[0] = 'A' + (idx / 8);
  modeText[1] = '1' + (idx % 8);
  modeText[2] = '\0';
  return modeText;
}

void update7Segment(SystemMode mode) {
  if (mode == MODE_IDLE) { display.clear(); return; }
  int idx = getModeIndex(mode);
  char prefix = 'A' + (idx/8);
  int num = (idx%8)+1;
  uint8_t seg[4] = {0,0,0,0};
  switch(prefix) {
    case 'A': seg[0]=0x77; break;
    case 'B': seg[0]=0x7C; break;
    case 'C': seg[0]=0x39; break;
    case 'D': seg[0]=0x5E; break;
  }
  seg[1] = display.encodeDigit(num);
  display.setSegments(seg);
}

void updateIdleDisplay() {
  static unsigned long lastUpd=0;
  const unsigned long INTERVAL = 3000;
  if (systemLocked) return;
  if (millis()-lastUpd > INTERVAL) {
    lastUpd = millis();
    SystemMode modeToShow = (systemRunning ? currentMode : selectedNextMode);
    if (modeToShow != MODE_IDLE) {
      update7Segment(modeToShow);
    } else {
      uint8_t rdy[4] = {0x50,0x5E,0x6E,0x00};
      display.setSegments(rdy);
    }
  }
}

void printModeValue(int modeIdx, int col, int row, bool isSelected) {
  lcd.setCursor(col, row);
  char prefix = 'A' + (modeIdx/8);
  int num = (modeIdx%8)+1;
  uint32_t timeMs = modeTimes[modeIdx];
  uint32_t secondsInt = timeMs / 1000UL;
  uint8_t decimals = (timeMs % 1000UL) / 100UL;
  lcd.print(prefix);
  lcd.print(num);
  lcd.print(' ');
  lcd.print(secondsInt);
  lcd.print('.');
  lcd.print(decimals);
  lcd.print('s');
  if (isSelected) lcd.write(0xFF); else lcd.print(' ');
  int printed = 7 + (secondsInt >= 10 ? 2 : 1);
  for (int i = printed; i < 9; i++) lcd.print(' ');
}

void showAllModeTimes() {
  lcd.clear();
  updateActiveModesList();
  updateSelectedNextMode();

  if (activeModeCount == 0) {
    lcd.setCursor(4,1); lcd.print(F("Tidak ada"));
    lcd.setCursor(3,2); lcd.print(F("mode aktif"));
    lcd.setCursor(2,3); lcd.print(F("Tekan # untuk set"));
    return;
  }

  int totalPages = (activeModeCount + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
  
  lcd.setCursor(0,0);
  if (systemRunning) lcd.write(byte(0)); else lcd.print(" ");
  lcd.setCursor(1,0);
  lcd.print(activeModeCount);
  lcd.print(F(" Mode "));
  lcd.print(F("Page:"));
  lcd.print(activeModePage+1);
  lcd.print(F(" of "));
  lcd.print(totalPages);

  int start = activeModePage * MODES_PER_PAGE;
  int end = start + MODES_PER_PAGE;
  if (end > activeModeCount) end = activeModeCount;
  
  int markedModeIdx = -1;
  if (systemRunning) {
    markedModeIdx = getModeIndex(currentMode);
  } else {
    markedModeIdx = getModeIndex(selectedNextMode);
  }

  for (int row=1; row<=3; row++) {
    int leftIdx = start + (row-1);
    int rightIdx = leftIdx + 3;
    if (leftIdx < end) {
      bool isSel = (activeModeIndices[leftIdx] == markedModeIdx);
      printModeValue(activeModeIndices[leftIdx], 0, row, isSel);
    } else {
      lcd.setCursor(0, row); lcd.print(F("         "));
    }
    if (rightIdx < end) {
      bool isSel = (activeModeIndices[rightIdx] == markedModeIdx);
      printModeValue(activeModeIndices[rightIdx], 10, row, isSel);
    } else {
      lcd.setCursor(10, row); lcd.print(F("         "));
    }
  }
}

// ========== SETUP ==========
void setup() {
  digitalWrite(relayPin, RELAY_OFF_LEVEL);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, RELAY_OFF_LEVEL);

  pinMode(switchPin, INPUT_PULLUP);
  pinMode(emergencyStopPin, INPUT_PULLUP);

  switchState = lastSwitchState = digitalRead(switchPin);
  emergencyStopState = lastEmergencyStopState = digitalRead(emergencyStopPin);

  keypad.setDebounceTime(50);
  keypad.setHoldTime(800);

  Serial.begin(115200);
  serialPrintHeader();

  Serial.print(F("Reset cause: "));
  if (mcusrMirror & _BV(WDRF)) Serial.print(F("WATCHDOG "));
  if (mcusrMirror & _BV(BORF)) Serial.print(F("BROWN-OUT "));
  if (mcusrMirror & _BV(EXTRF)) Serial.print(F("EXTERNAL "));
  if (mcusrMirror & _BV(PORF)) Serial.print(F("POWER-ON "));
  Serial.println();

  delay(200);
  configureI2C();
  initializeLCDHardware();

  display.setBrightness(0x0f);
  display.clear();

  EEPROM_BOOT_ADDR = EEPROM_DATA_START + TOTAL_MODES * sizeof(uint32_t);
  bool dataLoaded = loadFromEEPROM();
  updateActiveModesList();
  updateSelectedNextMode();

  bootCount++;
  if (dataLoaded) {
    EEPROM.put(EEPROM_BOOT_ADDR, bootCount);
  } else {
    saveToEEPROM();
  }
  serialPrintBootInfo();

  if (bootCount >= MAX_BOOTS) {
    systemLocked = true;
    Serial.println(F("[WARNING] Batas boot tercapai. Sistem dikunci."));
  }

  lcd.clear();
  lcd.setCursor(5,0); lcd.print(F("Timer Multi"));
  lcd.setCursor(5,1); lcd.print(F("CONTROLLER"));
  lcd.setCursor(2,3); lcd.print(F("Versi : 32 mode"));
  delay(1500);

  lcd.clear();
  lcd.setCursor(3,0); lcd.print(F("Build BY AGUS F"));
  lcd.setCursor(3,2); lcd.print(F("  29 Mei 2026"));
  lcd.setCursor(0,3); lcd.print(F("//github.com/sembre/"));
  delay(2000);

  lcd.clear();
  lcd.setCursor(2,1); lcd.print(dataLoaded ? F("Data Dimuat") : F("Data Kosong"));
  lcd.setCursor(4,2); lcd.print(dataLoaded ? F("dari EEPROM") : F("Mulai Baru"));
  delay(1200);

  if (systemLocked) {
    lockSystem();
    Serial.println(F("Sistem terkunci karena boot limit."));
    return;
  }

  showAllModeTimes();
  lastRelayChange = millis();
  lastSystemCheck = millis();
  errorCount = 0;
  relayOnStartTime = 0;
  relayTimeoutTriggered = false;
  updateIdleDisplay();

  serialPrintActiveModes();
  serialPrintSystemStatus();
  Serial.println(F("Siap menerima perintah."));

  wdt_enable(WDTO_2S);
  wdt_reset();
}

void loop() {
  wdt_reset();

  checkEmergencyStop();

  if (emergencyStopActive) {
    setRelay(false);
    return;
  }

  if (systemLocked) {
    setRelay(false);
    return;
  }

  checkRelayTimeout();
  checkLCDHealth();

  static unsigned long lastKey = 0;
  static unsigned long lastDog = 0;
  const unsigned long KEY_INT = 35;
  const unsigned long DOG_INT = 500;

  if (millis() - lastSystemCheck >= 5000) {
    lastSystemCheck = millis();

    if (systemStable && errorCount > 0) errorCount--;

    if (errorCount > 3) {
      setRelay(false);
      systemRunning = false;
      errorCount = 0;
      systemStable = true;
      serialLogError("Terlalu banyak error, sistem distabilkan.");
      requestLCDRefresh(100, true);
    }
  }

  if (millis() - lastDog >= DOG_INT) {
    lastDog = millis();
    if (!systemRunning && relayState) {
      setRelay(false);
      Serial.println(F("[WATCHDOG] Relay dimatikan karena sistem tidak running."));
    }
  }

  if (millis() - lastKey >= KEY_INT) {
    lastKey = millis();
    char k = keypad.getKey();
    if (k) handleKeypadInput(k);
  }

  readSwitch();

  if (!systemRunning) {
    setRelay(false);
  }

  if (systemRunning && interval > 0) {
    runSystem();
  } else {
    updateIdleDisplay();
  }
}

void startMode(SystemMode mode) {
  if (systemLocked || emergencyStopActive || !systemStable) return;

  int idx = getModeIndex(mode);
  if (idx < 0 || idx >= TOTAL_MODES || modeTimes[idx] == 0) {
    if (activeModeCount > 0) {
      mode = (SystemMode)(activeModeIndices[0] + 1);
      idx = activeModeIndices[0];
    } else {
      return;
    }
  }

  currentMode = mode;
  nextMode = findNextActiveMode(currentMode);
  if (selectedNextMode == MODE_IDLE) selectedNextMode = nextMode;

  interval = modeTimes[idx];
  if (interval == 0) return;

  relayTimeoutTriggered = false;
  relayOnStartTime = 0;
  previousMillis = millis();
  systemRunning = true;

  setRelay(true);

  if (!relayState) {
    systemRunning = false;
    serialLogError("Relay gagal ON karena kondisi fail-safe.");
    return;
  }

  requestLCDRefresh(LCD_RELAY_SETTLE_MS, true);
  serialLogModeStart(currentMode);
}

void runSystem() {
  unsigned long now = millis();

  if (interval == 0) {
    setRelay(false);
    systemRunning = false;
    selectedNextMode = nextMode;
    requestLCDRefresh(LCD_RELAY_SETTLE_MS, true);
    update7Segment(selectedNextMode);
    serialLogModeFinish(currentMode);
    return;
  }

  if (safeMillisDiff(now, previousMillis) >= interval) {
    SystemMode finishedMode = currentMode;
    setRelay(false);
    systemRunning = false;
    selectedNextMode = nextMode;
    currentMode = MODE_IDLE;
    requestLCDRefresh(LCD_RELAY_SETTLE_MS, true);
    update7Segment(selectedNextMode);
    serialLogModeFinish(finishedMode);
  }
}

void resetSystemState() {
  setRelay(false);
  systemRunning = false;
  currentMode = MODE_IDLE;
  nextMode = MODE_IDLE;
  interval = 0;

  inputMode = false;
  waitingForTime = false;
  inputLength = 0; inputValue[0] = '\0';
  tempValue = 0;

  switchPressed = false;
  switchState = lastSwitchState = digitalRead(switchPin);
  updateSelectedNextMode();

  delay(20);
  initializeLCDHardware();
  lcd.clear();
  lcd.setCursor(0,1); lcd.print(F("  SISTEM DIRESET"));
  lcd.setCursor(1,2); lcd.print(F("Kembali ke Menu"));
  delay(300);

  showAllModeTimes();
  autoPageToMarkedMode();
  updateIdleDisplay();
  Serial.println(F("[RESET] Sistem state di-reset (bukan reset EEPROM)."));
}

void checkEmergencyStop() {
  int reading = digitalRead(emergencyStopPin);

  if (emergencyStopActive) setRelay(false);

  if (reading != lastEmergencyStopState) {
    lastEmergencyStopTime = millis();
  }

  if (safeMillisDiff(millis(), lastEmergencyStopTime) >= emergencyStopDebounce) {
    if (reading != emergencyStopState) {
      emergencyStopState = reading;

      if (emergencyStopState == LOW) {
        if (emergencyStopActive) {
          emergencyStopActive = false;
          systemStable = true;
          resetSystemState();
          serialLogEmergencyStop(false);
        } else {
          emergencyStopActive = true;
          systemStable = false;
          systemRunning = false;
          currentMode = MODE_IDLE;
          interval = 0;
          setRelay(false);

          initializeLCDHardware();
          lcd.clear();
          lcd.setCursor(1,0); lcd.print(F("!!!EMERGENCY STOP!!!"));
          lcd.setCursor(0,1); lcd.print(F("SEMUA FUNGSI DIKUNCI"));
          lcd.setCursor(0,2); lcd.print(F("Press Tombol B Lagi"));
          lcd.setCursor(4,3); lcd.print(F("Untuk RESET"));

          uint8_t blank[4] = {0,0,0,0};
          display.setSegments(blank);
          serialLogEmergencyStop(true);
        }
      }
    }
  }
  lastEmergencyStopState = reading;
}


void readSwitch() {
  if (systemLocked || emergencyStopActive || inputMode) return;

  int reading = digitalRead(switchPin);

  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }

  if (safeMillisDiff(millis(), lastDebounceTime) >= debounceDelay) {
    if (reading != switchState) {
      switchState = reading;

      if (switchState == LOW && !systemRunning && systemStable) {
        if (selectedNextMode != MODE_IDLE) {
          startMode(selectedNextMode);
        } else if (activeModeCount > 0) {
          startMode((SystemMode)(activeModeIndices[0] + 1));
        }
      }
    }
  }

  lastSwitchState = reading;
}


void handleKeypadInput(char key) {
  if (systemLocked) return;

  if (key == '*' && systemRunning) {
    emergencyStop();
    return;
  }

  if (key == '*' && inputMode) {
    inputMode = false;
    waitingForTime = false;
    currentGroup = ' ';
    inputLength = 0;
    inputValue[0] = '\0';
    tempValue = 0;
    showAllModeTimes();
    Serial.println(F("[INPUT] Setting dibatalkan."));
    return;
  }

  static unsigned long firstStarTime = 0;
  if (key == '*' && !inputMode && !systemRunning) {
    unsigned long now = millis();
    if (firstStarTime != 0 && safeMillisDiff(now, firstStarTime) <= 2000) {
      firstStarTime = 0;
      resetAll();
      return;
    }

    firstStarTime = now;
    lcd.clear();
    lcd.setCursor(1,0); lcd.print(F("RESET SEMUA MODE?"));
    lcd.setCursor(0,1); lcd.print(F("Tekan * sekali lagi"));
    lcd.setCursor(0,2); lcd.print(F("maksimal 2 detik"));
    lcd.setCursor(0,3); lcd.print(F("Tombol lain: BATAL"));
    return;
  }

  if (firstStarTime != 0) {
    firstStarTime = 0;
    showAllModeTimes();
  }

  if (!inputMode && !systemRunning) {
    if (key == 'A') { selectPreviousMode(); return; }
    if (key == 'B') { selectNextMode(); return; }
    if (key == 'C') {
      if (activeModeCount>0) {
        int total = (activeModeCount+MODES_PER_PAGE-1)/MODES_PER_PAGE;
        activeModePage = (activeModePage==0) ? total-1 : activeModePage-1;
        showAllModeTimes();
      }
      return;
    }
    if (key == 'D') {
      if (activeModeCount>0) {
        int total = (activeModeCount+MODES_PER_PAGE-1)/MODES_PER_PAGE;
        activeModePage = (activeModePage+1) % total;
        showAllModeTimes();
      }
      return;
    }
  }
  
  if (key=='#' && !inputMode && !systemRunning) {
    inputMode = true;
    currentGroup = ' ';
    inputLength = 0; inputValue[0] = '\0';
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("  Pilih Mode A1-D8"));
    lcd.setCursor(0,2); lcd.print(F("Pencet A,B,C atau D"));
    lcd.setCursor(0,3); lcd.print(F("lalu tekan 1-8"));
    serialLogInputStart();
    return;
  }
  if (inputMode && !waitingForTime && key>='A' && key<='D') {
    currentGroup = key;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("Grup: ")); lcd.print(currentGroup);
    lcd.setCursor(0,1); lcd.print(F("Kemudian Tekan "));
    lcd.setCursor(0,2); lcd.print(F(" 1,2,3,4,5,6,7,8"));
    return;
  }
  if (inputMode && !waitingForTime && key>='1' && key<='8') {
    if (currentGroup < 'A' || currentGroup > 'D') {
      lcd.clear();
      lcd.setCursor(0,1); lcd.print(F("Pilih Grup A-D dulu"));
      delay(300);
      lcd.clear();
      lcd.setCursor(0,0); lcd.print(F("  Pilih Mode A1-D8"));
      lcd.setCursor(0,2); lcd.print(F("Pencet A,B,C atau D"));
      lcd.setCursor(0,3); lcd.print(F("lalu tekan 1-8"));
      return;
    }
    int group = currentGroup - 'A';
    int num = key - '1';
    selectedModeToSet = (SystemMode)(group*8 + num + 1);
    waitingForTime = true;
    inputLength = 0; inputValue[0] = '\0'; tempValue=0;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("Input Set ")); lcd.print(getModeString(selectedModeToSet));
    lcd.setCursor(0,1); lcd.print(F("Input dalam ms"));
    lcd.setCursor(0,3); lcd.print(F("Simpan: tekan #"));
    serialLogInputModeSelected(selectedModeToSet);
    return;
  }
  if (waitingForTime && isdigit(key)) {
    if (inputLength < 5) {
      inputValue[inputLength++] = key;
      inputValue[inputLength] = '\0';
    }

    tempValue = strtoul(inputValue, nullptr, 10);
    if (tempValue > MAX_MODE_TIME_MS) tempValue = MAX_MODE_TIME_MS;

    lcd.setCursor(0,2);
    lcd.print(F("                    "));
    lcd.setCursor(0,2);
    lcd.print(inputValue);
    lcd.print(F(" ms ("));
    lcd.print(tempValue / 1000.0, 1);
    lcd.print(F("s)"));
    return;
  }
  if (waitingForTime && key=='#') {
    saveInput();
  }
}

void saveInput() {
  if (selectedModeToSet >= MODE_A1 && selectedModeToSet <= MODE_D8 && tempValue <= MAX_MODE_TIME_MS) {
    modeTimes[getModeIndex(selectedModeToSet)] = tempValue;
    saveToEEPROM();
    updateActiveModesList();
    updateSelectedNextMode();

    inputMode = false;
    waitingForTime = false;
    currentGroup = ' ';

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(getModeString(selectedModeToSet));
    lcd.print(F(" = "));
    lcd.print(tempValue);
    lcd.print(F("ms"));
    lcd.setCursor(0,1);
    lcd.print(F("Waktu: "));
    lcd.print(tempValue / 1000.0, 1);
    lcd.print(F(" detik"));
    lcd.setCursor(0,2); lcd.print(F("Tersimpan Permanen"));
    delay(300);

    showAllModeTimes();
    autoPageToMarkedMode();
    serialLogEepromSave(selectedModeToSet, tempValue);
    serialPrintActiveModes();
  } else {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("  Gagal Disimpan!"));
    lcd.setCursor(0,2); lcd.print(F("Silakan Ulangi lagi"));
    delay(300);

    inputMode = true;
    waitingForTime = false;
    inputLength = 0; inputValue[0] = '\0';
    tempValue = 0;
    currentGroup = ' ';

    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("  Pilih Mode A1-D8"));
    lcd.setCursor(0,2); lcd.print(F("Pencet A,B,C atau D"));
    lcd.setCursor(0,3); lcd.print(F("lalu tekan 1-8"));
    Serial.println(F("[ERROR] Gagal menyimpan mode tidak valid."));
  }
}

void emergencyStop() {
  setRelay(false);
  systemRunning = false;
  systemStable = false;
  currentMode = MODE_IDLE;
  interval = 0;
  errorCount++;

  delay(20);
  initializeLCDHardware();
  lcd.clear();
  lcd.setCursor(2,1); lcd.print(F("EMERGENCY STOP"));
  lcd.setCursor(2,2); lcd.print(F("Recovery Mode"));
  delay(300);

  systemStable = true;
  showAllModeTimes();
  autoPageToMarkedMode();
  Serial.println(F("[EMERGENCY] Stop darurat dipicu dari keypad."));
}

void resetAll() {
  if (systemLocked) {
    lcd.clear();
    lcd.setCursor(1,1); lcd.print(F("TIDAK DAPAT RESET"));
    lcd.setCursor(1,2); lcd.print(F("Sistem terkunci"));
    delay(1500);
    Serial.println(F("[ERROR] Reset ditolak karena sistem terkunci."));
    return;
  }

  setRelay(false);
  systemRunning = false;
  currentMode = MODE_IDLE;
  nextMode = MODE_IDLE;
  interval = 0;

  for (int i = 0; i < TOTAL_MODES; i++) modeTimes[i] = 0;
  saveToEEPROM();
  updateActiveModesList();
  updateSelectedNextMode();

  inputMode = false;
  waitingForTime = false;
  inputLength = 0; inputValue[0] = '\0';
  tempValue = 0;
  currentGroup = ' ';

  lcd.clear();
  lcd.setCursor(2,1); lcd.print(F("Reset Permanen"));
  lcd.setCursor(1,2); lcd.print(F("Mode Dikosongkan"));

  uint8_t rdy[4] = {0x50,0x5E,0x6E,0x00};
  display.setSegments(rdy);
  delay(300);

  showAllModeTimes();
  serialLogSystemReset();
  serialPrintActiveModes();
}
