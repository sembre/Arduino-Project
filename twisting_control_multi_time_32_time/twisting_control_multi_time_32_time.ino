/*
 * ========================================================================
 * TWISTING CONTROL MULTI-TIME SYSTEM - 32 MODE PROFESSIONAL EDITION
 * ========================================================================
 *
 * Version: 4.7 (Serial Monitor Enhanced)
 * ========================================================================
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

// ========== PIN & KONSTAN ==========
#define TM1637_CLK 4
#define TM1637_DIO 5
TM1637Display display(TM1637_CLK, TM1637_DIO);

#define EEPROM_MAGIC_ADDR 0
#define EEPROM_DATA_START 2
#define EEPROM_MAGIC_NUMBER 0x1234
const unsigned long MAX_BOOTS = 2500UL;

enum SystemMode {
  MODE_IDLE,
  MODE_A1, MODE_A2, MODE_A3, MODE_A4, MODE_A5, MODE_A6, MODE_A7, MODE_A8,
  MODE_B1, MODE_B2, MODE_B3, MODE_B4, MODE_B5, MODE_B6, MODE_B7, MODE_B8,
  MODE_C1, MODE_C2, MODE_C3, MODE_C4, MODE_C5, MODE_C6, MODE_C7, MODE_C8,
  MODE_D1, MODE_D2, MODE_D3, MODE_D4, MODE_D5, MODE_D6, MODE_D7, MODE_D8
};
const int TOTAL_MODES = 32;
unsigned int modeTimes[TOTAL_MODES];

// ========== STATE VARIABLES ==========
SystemMode currentMode = MODE_IDLE;
SystemMode nextMode = MODE_IDLE;
SystemMode selectedNextMode = MODE_IDLE;
SystemMode selectedModeToSet = MODE_A1;

bool systemRunning = false;
bool inputMode = false;
bool waitingForTime = false;
char currentGroup = ' ';

String inputValue = "";
unsigned int tempValue = 0;

unsigned long previousMillis = 0;
unsigned long interval = 0;

unsigned long relayOnStartTime = 0;
const unsigned long RELAY_SAFETY_MARGIN = 500;
#define RELAY_TIMEOUT_MULTIPLIER 1.2
bool relayTimeoutTriggered = false;

const int relayPin = 2;
const int switchPin = 53;
const int emergencyStopPin = 51;

int switchState = HIGH;
int lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 10;

int emergencyStopState = HIGH;
int lastEmergencyStopState = HIGH;
unsigned long lastEmergencyStopTime = 0;
const unsigned long emergencyStopDebounce = 50;
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

LiquidCrystal_I2C lcd(0x27, 20, 4);

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// byte rowPins[ROWS] = {22,24,26,28}; // LAMA
// byte colPins[COLS] = {30,32,34,36}; // LAMA

byte colPins[COLS] = {22,24,26,28}; // BARU
byte rowPins[ROWS] = {30,32,34,36}; // MARU
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ========== SERIAL LOG FUNCTIONS ==========
void serialPrintHeader() {
  Serial.println("========================================");
  Serial.println(" TWISTING CONTROL SYSTEM v4.7");
  Serial.println("========================================");
}

void serialPrintBootInfo() {
  Serial.print("Boot Counter: ");
  Serial.println(bootCount);
  Serial.print("Max Boots Allowed: ");
  Serial.println(MAX_BOOTS);
  if (bootCount >= MAX_BOOTS) {
    Serial.println("!!! SISTEM TERKUNCI !!! (Boot limit exceeded)");
  } else {
    Serial.println("Sistem siap.");
  }
}

void serialPrintActiveModes() {
  Serial.println("--- Daftar Mode Aktif (Waktu > 0) ---");
  if (activeModeCount == 0) {
    Serial.println("Tidak ada mode aktif. Tekan # untuk setting.");
  } else {
    for (int i = 0; i < activeModeCount; i++) {
      int idx = activeModeIndices[i];
      char prefix = 'A' + (idx/8);
      int num = (idx%8)+1;
      unsigned int timeMs = modeTimes[idx];
      float seconds = timeMs / 1000.0;
      Serial.print(prefix); Serial.print(num);
      Serial.print(" : ");
      Serial.print(timeMs); Serial.print(" ms (");
      Serial.print(seconds, 1); Serial.println(" s)");
    }
  }
  Serial.println("--------------------------------------");
}

void serialPrintSystemStatus() {
  Serial.println("--- STATUS SISTEM ---");
  Serial.print("Sistem Running   : "); Serial.println(systemRunning ? "YES" : "NO");
  Serial.print("Emergency Stop   : "); Serial.println(emergencyStopActive ? "AKTIF" : "TIDAK");
  Serial.print("System Locked    : "); Serial.println(systemLocked ? "YA" : "TIDAK");
  Serial.print("Relay State      : "); Serial.println(relayState ? "ON" : "OFF");
  Serial.print("Current Mode     : ");
  if (currentMode == MODE_IDLE) Serial.println("IDLE");
  else Serial.println(getModeString(currentMode));
  Serial.print("Next Mode        : ");
  if (nextMode == MODE_IDLE) Serial.println("IDLE");
  else Serial.println(getModeString(nextMode));
  Serial.print("Selected Next    : ");
  if (selectedNextMode == MODE_IDLE) Serial.println("IDLE");
  else Serial.println(getModeString(selectedNextMode));
  Serial.print("Active Modes     : "); Serial.println(activeModeCount);
  Serial.print("LCD Page         : "); Serial.print(activeModePage+1);
  int totalPages = (activeModeCount + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
  Serial.print(" / "); Serial.println(totalPages);
  Serial.println("-----------------------");
}

void serialLogModeStart(SystemMode mode) {
  int idx = getModeIndex(mode);
  unsigned int t = modeTimes[idx];
  float sec = t/1000.0;
  Serial.print("[START] Mode ");
  Serial.print(getModeString(mode));
  Serial.print(" -> ");
  Serial.print(t); Serial.print(" ms (");
  Serial.print(sec, 1); Serial.println(" s)");
  Serial.print("Next mode after finish: ");
  Serial.println(getModeString(findNextActiveMode(mode)));
}

void serialLogModeFinish(SystemMode mode) {
  Serial.print("[FINISH] Mode ");
  Serial.print(getModeString(mode));
  Serial.println(" selesai.");
}

void serialLogRelayOn() {
  Serial.println("[RELAY] ON");
}

void serialLogRelayOff() {
  Serial.println("[RELAY] OFF");
}

void serialLogEmergencyStop(bool activated) {
  if (activated) {
    Serial.println("!!! EMERGENCY STOP AKTIF !!! Semua fungsi dikunci.");
  } else {
    Serial.println("Emergency STOP di-reset. Sistem normal.");
  }
}

void serialLogSystemReset() {
  Serial.println("!!! SYSTEM RESET oleh pengguna (tombol * tahan) !!!");
}

void serialLogEepromSave(SystemMode mode, unsigned int value) {
  Serial.print("[EEPROM] Mode ");
  Serial.print(getModeString(mode));
  Serial.print(" disimpan dengan nilai ");
  Serial.print(value); Serial.print(" ms (");
  Serial.print(value/1000.0, 1); Serial.println(" s)");
}

void serialLogError(const char* msg) {
  Serial.print("[ERROR] ");
  Serial.println(msg);
}

void serialLogInputStart() {
  Serial.println("Mode input aktif. Pengguna akan memilih mode dan memasukkan waktu.");
}

void serialLogInputModeSelected(SystemMode mode) {
  Serial.print("Mode dipilih: ");
  Serial.println(getModeString(mode));
}

// ========== UTILITY ==========
void saveToEEPROM() {
  uint16_t magic = EEPROM_MAGIC_NUMBER;
  EEPROM.put(EEPROM_MAGIC_ADDR, magic);
  for (int i=0; i<TOTAL_MODES; i++) {
    int addr = EEPROM_DATA_START + i*sizeof(unsigned int);
    EEPROM.put(addr, modeTimes[i]);
  }
  EEPROM.put(EEPROM_BOOT_ADDR, bootCount);
}

bool loadFromEEPROM() {
  uint16_t magic;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);
  if (magic != EEPROM_MAGIC_NUMBER) {
    for (int i=0; i<TOTAL_MODES; i++) modeTimes[i] = 0;
    bootCount = 0;
    return false;
  }
  for (int i=0; i<TOTAL_MODES; i++) {
    int addr = EEPROM_DATA_START + i*sizeof(unsigned int);
    EEPROM.get(addr, modeTimes[i]);
  }
  EEPROM.get(EEPROM_BOOT_ADDR, bootCount);
  return true;
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
  // Serial log
  Serial.print("[NAVIGASI] Previous mode: ");
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
  Serial.print("[NAVIGASI] Next mode: ");
  Serial.println(getModeString(selectedNextMode));
}

void lockSystem() {
  systemLocked = true;
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("FINISH");
  lcd.setCursor(0,1); lcd.print("program die");
  uint8_t blank[4] = {0,0,0,0};
  display.setSegments(blank);
  setRelay(false);
  Serial.println("[LOCK] Sistem terkunci karena mencapai batas boot maksimum.");
}

void setRelay(bool state) {
  if ((emergencyStopActive || systemLocked) && state) return;
  if (state && interval <= 0) return;
  if (state && millis() - lastRelayChange < 100) return;
  if (relayState != state) {
    noInterrupts();
    relayState = state;
    digitalWrite(relayPin, state ? LOW : HIGH);
    lastRelayChange = millis();
    if (state) {
      relayOnStartTime = millis();
      relayTimeoutTriggered = false;
      serialLogRelayOn();
    } else {
      relayOnStartTime = 0;
      relayTimeoutTriggered = false;
      serialLogRelayOff();
    }
    interrupts();
  }
}

unsigned long safeMillisDiff(unsigned long now, unsigned long prev) {
  return (now >= prev) ? (now - prev) : (4294967295UL - prev + now + 1);
}

void checkRelayTimeout() {
  if (relayState && relayOnStartTime>0 && interval>0) {
    unsigned long maxAllowed = (unsigned long)(interval * RELAY_TIMEOUT_MULTIPLIER) + RELAY_SAFETY_MARGIN;
    if (safeMillisDiff(millis(), relayOnStartTime) > maxAllowed && !relayTimeoutTriggered) {
      relayTimeoutTriggered = true;
      errorCount++;
      noInterrupts();
      digitalWrite(relayPin, HIGH);
      relayState = false;
      relayOnStartTime = 0;
      lastRelayChange = millis();
      interrupts();
      systemRunning = false;
      serialLogError("Relay timeout! Sistem dihentikan.");
    }
  } else if (!relayState) relayTimeoutTriggered = false;
}

void checkLCDHealth() {
  if (systemLocked) return;
  static unsigned long lastCheck = 0;
  const unsigned long INTERVAL = 10000;
  static int cnt = 0;
  if (millis() - lastCheck < INTERVAL) return;
  lastCheck = millis();
  lcd.setCursor(19,3); lcd.print(" ");
  delay(2);
  cnt++;
  if (cnt > 30) { cnt=0; resetLCDModule(); }
}

void resetLCDModule() {
  bool wasRunning = systemRunning;
  if (systemRunning) {
    systemRunning = false;
    noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; interrupts();
  }
  delay(100);
  lcd.noBacklight(); delay(50); lcd.backlight(); delay(50);
  lcd.init(); delay(100); lcd.backlight(); lcd.clear(); delay(50);
  byte arrowRight[8] = {0x00,0x04,0x06,0x1F,0x06,0x04,0x00,0x00};
  lcd.createChar(0, arrowRight);
  if (wasRunning && currentMode != MODE_IDLE) {
    showAllModeTimes();
  } else showAllModeTimes();
}

int getModeIndex(SystemMode mode) { return (int)mode - 1; }

String getModeString(SystemMode mode) {
  if (mode == MODE_IDLE) return "Idle";
  int idx = getModeIndex(mode);
  char prefix = 'A' + (idx/8);
  int num = (idx%8)+1;
  return String(prefix) + String(num);
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
  unsigned int timeMs = modeTimes[modeIdx];
  int secondsInt = timeMs / 1000;
  int decimals = (timeMs % 1000) / 100;
  lcd.print(prefix);
  lcd.print(num);
  lcd.print(' ');
  lcd.print(secondsInt);
  lcd.print('.');
  lcd.print(decimals);
  lcd.print('s');
  if (isSelected) lcd.write(0xFF); else lcd.print(' ');
  int printed = 7 + (secondsInt>=10?1:0) + (isSelected?1:0);
  for (int i=printed; i<9; i++) lcd.print(' ');
}

void showAllModeTimes() {
  lcd.clear();
  updateActiveModesList();
  updateSelectedNextMode();

  if (activeModeCount == 0) {
    lcd.setCursor(4,1); lcd.print("Tidak ada");
    lcd.setCursor(3,2); lcd.print("mode aktif");
    lcd.setCursor(2,3); lcd.print("Tekan # untuk set");
    return;
  }

  int totalPages = (activeModeCount + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
  
  lcd.setCursor(0,0);
  if (systemRunning) lcd.write(byte(0)); else lcd.print(" ");
  lcd.setCursor(1,0);
  lcd.print(activeModeCount);
  lcd.print(" Mode ");
  lcd.print("Page:");
  lcd.print(activeModePage+1);
  lcd.print(" of ");
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
      lcd.setCursor(0, row); lcd.print("         ");
    }
    if (rightIdx < end) {
      bool isSel = (activeModeIndices[rightIdx] == markedModeIdx);
      printModeValue(activeModeIndices[rightIdx], 10, row, isSel);
    } else {
      lcd.setCursor(10, row); lcd.print("         ");
    }
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  serialPrintHeader();
  
  pinMode(relayPin, OUTPUT); digitalWrite(relayPin, HIGH);
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(emergencyStopPin, INPUT_PULLUP);
  delay(200);
  lcd.init(); lcd.backlight();
  byte arrowRight[8] = {0x00,0x04,0x06,0x1F,0x06,0x04,0x00,0x00};
  lcd.createChar(0, arrowRight);
  display.setBrightness(0x0f); display.clear();

  EEPROM_BOOT_ADDR = EEPROM_DATA_START + TOTAL_MODES*sizeof(unsigned int);
  bool dataLoaded = loadFromEEPROM();
  updateActiveModesList();
  updateSelectedNextMode();

  bootCount++;
  EEPROM.put(EEPROM_BOOT_ADDR, bootCount);
  serialPrintBootInfo();
  
  if (bootCount >= MAX_BOOTS) {
    systemLocked = true;
    Serial.println("[WARNING] Batas boot tercapai. Sistem dikunci.");
  }

  // Splash screen
  lcd.clear();
  lcd.setCursor(5,0); lcd.print("Timer Multi");
  lcd.setCursor(5,1); lcd.print("CONTROLLER");
  lcd.setCursor(2,3); lcd.print("Versi : 32 mode");
  delay(1500);
  lcd.clear();
  lcd.setCursor(3,0); lcd.print("Build BY AGUS F");
  lcd.setCursor(3,2); lcd.print("  29 Mei 2026");
  lcd.setCursor(0,3); lcd.print("//github.com/sembre/");
  delay(2000);

  lcd.clear();
  lcd.setCursor(2,1); lcd.print(dataLoaded ? "Data Dimuat" : "Data Kosong");
  lcd.setCursor(4,2); lcd.print(dataLoaded ? "dari EEPROM" : "Mulai Baru");
  delay(1200);

  if (systemLocked) { 
    lockSystem(); 
    Serial.println("Sistem terkunci karena boot limit.");
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
  Serial.println("Siap menerima perintah.");
}

void loop() {
  checkEmergencyStop();
  if (emergencyStopActive) {
    if (relayState) { noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts(); }
    delayMicroseconds(100);
    return;
  }
  if (systemLocked) return;
  checkRelayTimeout();
  checkLCDHealth();

  static unsigned long lastKey=0, lastDog=0;
  const unsigned long KEY_INT=50, DOG_INT=2000;

  if (millis() - lastSystemCheck >= 5000) {
    lastSystemCheck = millis();
    if (systemStable) errorCount=0;
    if (errorCount>3) {
      noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts();
      errorCount=0; systemStable=true;
      serialLogError("Terlalu banyak error, sistem distabilkan.");
    }
  }
  if (millis() - lastDog >= DOG_INT) {
    lastDog = millis();
    if (!systemRunning && relayState) {
      noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts();
      Serial.println("[WATCHDOG] Relay dimatikan karena tidak dalam mode running.");
    }
  }
  if (millis() - lastKey >= KEY_INT) {
    lastKey = millis();
    char k = keypad.getKey();
    if (k) handleKeypadInput(k);
  }
  readSwitch();
  if (!systemRunning && relayState) {
    noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts();
  }
  if (systemRunning && interval>0) {
    runSystem();
  } else {
    updateIdleDisplay();
  }
}

void startMode(SystemMode mode) {
  if (systemLocked) return;
  int idx = getModeIndex(mode);
  if (idx < 0 || idx >= TOTAL_MODES || modeTimes[idx] == 0) {
    if (activeModeCount > 0) {
      mode = (SystemMode)(activeModeIndices[0] + 1);
      idx = activeModeIndices[0];
    } else return;
  }
  currentMode = mode;
  systemRunning = true;
  nextMode = findNextActiveMode(currentMode);
  if (selectedNextMode == MODE_IDLE) selectedNextMode = nextMode;
  interval = modeTimes[idx];
  if (interval > 0) {
    relayTimeoutTriggered = false;
    relayOnStartTime = 0;
    setRelay(true);
    previousMillis = millis();
    showAllModeTimes();
    autoPageToMarkedMode();
    serialLogModeStart(currentMode);
  } else {
    systemRunning = false;
  }
}

void runSystem() {
  unsigned long now = millis();
  if (interval <= 0) {
    noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts();
    systemRunning = false;
    selectedNextMode = nextMode;
    showAllModeTimes();
    autoPageToMarkedMode();
    update7Segment(selectedNextMode);
    serialLogModeFinish(currentMode);
    return;
  }
  if (safeMillisDiff(now, previousMillis) >= interval) {
    noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; relayTimeoutTriggered=false; lastRelayChange=millis(); interrupts();
    setRelay(false);
    systemRunning = false;
    selectedNextMode = nextMode;
    showAllModeTimes();
    autoPageToMarkedMode();
    update7Segment(selectedNextMode);
    serialLogModeFinish(currentMode);
  }
}

void resetSystemState() {
  noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; relayTimeoutTriggered=false; lastRelayChange=millis(); interrupts();
  systemRunning=false; currentMode=MODE_IDLE; nextMode=MODE_IDLE;
  inputMode=false; waitingForTime=false; inputValue=""; tempValue=0;
  switchPressed=false; switchState=HIGH; lastSwitchState=HIGH;
  updateSelectedNextMode();
  delay(20); lcd.clear(); delay(10);
  lcd.setCursor(0,1); lcd.print("  SISTEM DIRESET");
  lcd.setCursor(1,2); lcd.print("Kembali ke Menu");
  delay(300);
  showAllModeTimes();
  autoPageToMarkedMode();
  updateIdleDisplay();
  Serial.println("[RESET] Sistem state di-reset (bukan reset EEPROM).");
}

void checkEmergencyStop() {
  int reading = digitalRead(emergencyStopPin);
  if (emergencyStopActive && reading==LOW) {
    if (relayState) { noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; interrupts(); }
  }
  if (reading != lastEmergencyStopState) lastEmergencyStopTime = millis();
  if (safeMillisDiff(millis(), lastEmergencyStopTime) > emergencyStopDebounce) {
    if (reading != emergencyStopState) {
      emergencyStopState = reading;
      if (emergencyStopState == LOW) {
        if (emergencyStopActive) {
          resetSystemState();
          emergencyStopActive = false;
          serialLogEmergencyStop(false);
        } else {
          emergencyStopActive = true;
          systemRunning = false;
          noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; relayTimeoutTriggered=false; interrupts();
          lcd.clear();
          lcd.setCursor(1,0); lcd.print("!!!EMERGENCY STOP!!!");
          lcd.setCursor(0,1); lcd.print("SEMUA FUNGSI DIKUNCI");
          lcd.setCursor(0,2); lcd.print("Press Tombol B Lagi");
          lcd.setCursor(4,3); lcd.print("Untuk RESET");
          uint8_t blank[4]={0,0,0,0}; display.setSegments(blank);
          serialLogEmergencyStop(true);
        }
      }
    }
  }
  lastEmergencyStopState = reading;
}

void readSwitch() {
  if (systemLocked) return;
  int reading = digitalRead(switchPin);
  if (reading == LOW && !systemRunning && systemStable && !switchPressed) {
    switchPressed = true; switchPressTime = millis();
    if (selectedNextMode != MODE_IDLE) startMode(selectedNextMode);
    else if (activeModeCount > 0) startMode((SystemMode)(activeModeIndices[0]+1));
    return;
  }
  if (reading != lastSwitchState) lastDebounceTime = millis();
  if (safeMillisDiff(millis(), lastDebounceTime) > debounceDelay) {
    if (reading != switchState) {
      switchState = reading;
      if (switchState == LOW) {
        if (!switchPressed || safeMillisDiff(millis(), switchPressTime) > switchHoldDelay) {
          switchPressed = true; switchPressTime = millis();
          if (!systemRunning && systemStable) {
            if (selectedNextMode != MODE_IDLE) startMode(selectedNextMode);
            else if (activeModeCount > 0) startMode((SystemMode)(activeModeIndices[0]+1));
          }
        }
      } else switchPressed = false;
    }
  }
  lastSwitchState = reading;
}

void handleKeypadInput(char key) {
  if (systemLocked) return;
  static unsigned long lastStar=0;
  if (key=='*') {
    if (millis()-lastStar < 1000) { emergencyStop(); return; }
    lastStar = millis();
  }
  if (key=='*' && !inputMode) { resetAll(); return; }
  
  if (!inputMode && !systemRunning) {
    if (key == 'A') {
      selectPreviousMode();
      return;
    }
    if (key == 'B') {
      selectNextMode();
      return;
    }
    if (key == 'C') {
      if (activeModeCount>0) {
        int total = (activeModeCount+MODES_PER_PAGE-1)/MODES_PER_PAGE;
        activeModePage = (activeModePage==0) ? total-1 : activeModePage-1;
        showAllModeTimes();
        Serial.print("[LCD] Halaman sebelumnya: ");
        Serial.println(activeModePage+1);
      }
      return;
    }
    if (key == 'D') {
      if (activeModeCount>0) {
        int total = (activeModeCount+MODES_PER_PAGE-1)/MODES_PER_PAGE;
        activeModePage = (activeModePage+1) % total;
        showAllModeTimes();
        Serial.print("[LCD] Halaman berikutnya: ");
        Serial.println(activeModePage+1);
      }
      return;
    }
  }
  
  if (key=='#' && !inputMode) {
    inputMode=true; inputValue="";
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("  Pilih Mode A1-D8");
    lcd.setCursor(0,2); lcd.print("Pencet A,B,C atau D");
    lcd.setCursor(0,3); lcd.print("lalu tekan 1-8");
    serialLogInputStart();
    return;
  }
  if (inputMode && !waitingForTime && key>='A' && key<='D') {
    currentGroup = key;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Grup: "); lcd.print(currentGroup);
    lcd.setCursor(0,1); lcd.print("Kemudian Tekan ");
    lcd.setCursor(0,2); lcd.print(" 1,2,3,4,5,6,7,8");
    return;
  }
  if (inputMode && !waitingForTime && key>='1' && key<='8') {
    int group = currentGroup - 'A';
    int num = key - '1';
    selectedModeToSet = (SystemMode)(group*8 + num + 1);
    waitingForTime = true;
    inputValue=""; tempValue=0;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Input Set "); lcd.print(getModeString(selectedModeToSet));
    lcd.setCursor(0,1); lcd.print("Input dalam ms      Kemudian Tekan #");
    serialLogInputModeSelected(selectedModeToSet);
    return;
  }
  if (waitingForTime && isdigit(key)) {
    inputValue += key;
    if (inputValue.length()>5) inputValue = inputValue.substring(0,5);
    tempValue = inputValue.toInt();
    lcd.setCursor(0,2);
    lcd.print(inputValue); lcd.print(" ms  (");
    lcd.print(tempValue/1000.0,1); lcd.print("s)");
    return;
  }
  if (waitingForTime && key=='#') {
    saveInput();
  }
}

void saveInput() {
  if (selectedModeToSet >= MODE_A1 && selectedModeToSet <= MODE_D8) {
    modeTimes[getModeIndex(selectedModeToSet)] = tempValue;
    saveToEEPROM();
    updateActiveModesList();
    updateSelectedNextMode();
    inputMode=false; waitingForTime=false;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(getModeString(selectedModeToSet));
    lcd.print(" = "); lcd.print(tempValue); lcd.print("ms  (");
    lcd.print(tempValue/1000.0,1); lcd.print("s)");
    lcd.setCursor(0,2); lcd.print(" Tersimpan Permanen");
    delay(200);
    showAllModeTimes();
    autoPageToMarkedMode();
    serialLogEepromSave(selectedModeToSet, tempValue);
    serialPrintActiveModes();  // Cetak ulang daftar mode aktif
  } else {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("  Gagal Disimpan!");
    lcd.setCursor(0,2); lcd.print("Silakan Ulangi lagi");
    delay(200);
    inputMode=true; waitingForTime=false; inputValue="";
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("  Pilih Mode A1-D8");
    lcd.setCursor(0,2); lcd.print("Pencet A,B,C atau D");
    lcd.setCursor(0,3); lcd.print("lalu tekan 1-8");
    Serial.println("[ERROR] Gagal menyimpan mode tidak valid.");
  }
}

void emergencyStop() {
  noInterrupts();
  digitalWrite(relayPin, HIGH);
  relayState=false; relayOnStartTime=0; relayTimeoutTriggered=false;
  systemRunning=false; systemStable=false; errorCount++;
  interrupts();
  delay(20); lcd.clear(); delay(10);
  lcd.setCursor(2,1); lcd.print("EMERGENCY STOP");
  lcd.setCursor(2,2); lcd.print("Recovery Mode");
  delay(300);
  systemStable=true;
  showAllModeTimes();
  autoPageToMarkedMode();
  Serial.println("[EMERGENCY] Stop darurat dipicu oleh tombol * tahan.");
}

void resetAll() {
  if (systemLocked) {
    lcd.clear(); lcd.setCursor(1,1); lcd.print("TIDAK DAPAT RESET");
    lcd.setCursor(1,2); lcd.print("Sistem terkunci");
    delay(1500);
    Serial.println("[ERROR] Reset ditolak karena sistem terkunci.");
    return;
  }
  for (int i=0; i<TOTAL_MODES; i++) modeTimes[i]=0;
  saveToEEPROM();
  updateActiveModesList();
  updateSelectedNextMode();
  inputMode=false; waitingForTime=false;
  currentMode=MODE_IDLE; systemRunning=false;
  noInterrupts(); digitalWrite(relayPin, HIGH); relayState=false; relayOnStartTime=0; relayTimeoutTriggered=false; interrupts();
  setRelay(false);
  lcd.clear();
  lcd.setCursor(2,1); lcd.print("Reset Permanen");
  lcd.setCursor(1,2); lcd.print("Mode Times Dikosongkan");
  uint8_t rdy[4] = {0x50,0x5E,0x6E,0x00};
  display.setSegments(rdy);
  delay(300);
  showAllModeTimes();
  serialLogSystemReset();
  serialPrintActiveModes();  // Tampilkan bahwa semua mode kosong
}
