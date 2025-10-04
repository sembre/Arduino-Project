#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <TM1637Display.h>

// ------------------ Konstanta dan Variabel ------------------

#define TM1637_CLK 4
#define TM1637_DIO 5
TM1637Display display(TM1637_CLK, TM1637_DIO);

enum SystemMode {
  MODE_IDLE,
  MODE_A1, MODE_A2, MODE_A3, MODE_A4, MODE_A5, MODE_A6, MODE_A7, MODE_A8, MODE_A9,
  MODE_B1, MODE_B2, MODE_B3, MODE_B4, MODE_B5, MODE_B6, MODE_B7, MODE_B8, MODE_B9,
  MODE_C1, MODE_C2, MODE_C3, MODE_C4, MODE_C5, MODE_C6, MODE_C7, MODE_C8, MODE_C9,
  MODE_D1, MODE_D2, MODE_D3, MODE_D4, MODE_D5, MODE_D6, MODE_D7, MODE_D8, MODE_D9
};

const int TOTAL_MODES = 36;
unsigned int modeTimes[TOTAL_MODES];

SystemMode currentMode = MODE_IDLE;
SystemMode nextMode = MODE_IDLE;
SystemMode selectedModeToSet = MODE_A1;

bool systemRunning = false;
bool inputMode = false;
bool waitingForTime = false;
bool modeJustFinished = false;
char currentGroup = ' ';

String inputValue = "";
unsigned int tempValue = 0;

unsigned long previousMillis = 0;
unsigned long interval = 0;

const int relayPin = 2;
const int switchPin = 3;

int switchState = HIGH;
int lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ------------------ LCD & Keypad Setup ------------------

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {22, 24, 26, 28};
byte colPins[COLS] = {30, 32, 34, 36};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ------------------ Fungsi Utilitas ------------------

unsigned long safeMillisDiff(unsigned long now, unsigned long previous) {
  return (now >= previous) ? (now - previous) : (4294967295UL - previous + now + 1);
}

int getModeIndex(SystemMode mode) {
  return (int)mode - 1;
}

String getModeString(SystemMode mode) {
  if (mode == MODE_IDLE) return "Idle";
  int index = getModeIndex(mode);
  char prefix = 'A' + (index / 9);
  int num = (index % 9) + 1;
  return String(prefix) + String(num);
}

void update7Segment(SystemMode mode) {
  if (mode == MODE_IDLE) {
    display.clear();
    return;
  }
  int index = getModeIndex(mode);
  char prefix = 'A' + (index / 9);
  int num = (index % 9) + 1;

  uint8_t seg[4] = { 0x00, 0x00, 0x00, 0x00 };
  switch (prefix) {
    case 'A': seg[0] = 0x77; break; // A
    case 'B': seg[0] = 0x7C; break; // b
    case 'C': seg[0] = 0x39; break; // C
    case 'D': seg[0] = 0x5E; break; // d
    default:  seg[0] = 0x00; break;
  }
  seg[1] = display.encodeDigit(num);
  display.setSegments(seg);
}

// ------------------ Setup & Loop ------------------

void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  pinMode(switchPin, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  display.setBrightness(0x0f);
  display.clear();
  showSplashScreen();
  delay(500);
  showMainScreen();
}

void loop() {
  static unsigned long lastKeypadTime = 0;
  const unsigned long keypadInterval = 50;

  if (millis() - lastKeypadTime >= keypadInterval) {
    lastKeypadTime = millis();
    char key = keypad.getKey();
    if (key) handleKeypadInput(key);
  }

  readSwitch();

  if (systemRunning && interval > 0) runSystem();
}

// ------------------ Fungsi Tampilan ------------------

void showSplashScreen() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  Timer Multi");
  lcd.setCursor(0, 1); lcd.print("   CONTROLLER");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("    BY AGUS F");
  lcd.setCursor(0, 1); lcd.print("  15 JULI 2025");
  delay(2000);
}

void showMainScreen() {
  lcd.clear();
  if (systemRunning) {
    updateRunningDisplay();
  } else {
    lcd.setCursor(0, 0); lcd.print("Sistem Ready");
    lcd.setCursor(0, 1); lcd.print("Tekan Saklar");
    digitalWrite(relayPin, HIGH);
    display.clear();
  }
}

void updateRunningDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Running ");
  lcd.print(getModeString(currentMode));
  lcd.print(" (");
  lcd.print(modeTimes[getModeIndex(currentMode)] / 1000.0, 1);
  lcd.print("s)");

  lcd.setCursor(0, 1);
  lcd.print("NEXT ");
  lcd.print(getModeString(nextMode));
  lcd.print(" (");
  lcd.print(modeTimes[getModeIndex(nextMode)] / 1000.0, 1);
  lcd.print("s)");

  update7Segment(currentMode);
}

void showModeComplete() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(getModeString(currentMode));
  lcd.print(" Selesai (");
  int timeSec = modeTimes[getModeIndex(currentMode)] / 1000;
  lcd.print(timeSec);
  lcd.print("s)");

  lcd.setCursor(0, 1);
  lcd.print("NEXT ");
  lcd.print(getModeString(nextMode));
  lcd.print(" (");
  lcd.print(modeTimes[getModeIndex(nextMode)] / 1000);
  lcd.print("s)");

  update7Segment(nextMode);
} 

// ------------------ Mode ------------------

void determineNextMode() {
  int index = getModeIndex(currentMode);
  for (int i = 1; i <= TOTAL_MODES; i++) {
    int nextIndex = (index + i) % TOTAL_MODES;
    if (modeTimes[nextIndex] > 0) {
      nextMode = (SystemMode)(nextIndex + 1);
      return;
    }
  }
  nextMode = currentMode;
}

void startMode(SystemMode mode) {
  currentMode = mode;
  systemRunning = true;
  determineNextMode();
  int index = getModeIndex(mode);
  interval = (index >= 0 && index < TOTAL_MODES) ? modeTimes[index] : 0;
  if (interval > 0) {
    digitalWrite(relayPin, LOW);
    previousMillis = millis();
    updateRunningDisplay();
  } else {
    systemRunning = false;
  }
}

void runSystem() {
  unsigned long currentMillis = millis();
  if (safeMillisDiff(currentMillis, previousMillis) >= interval) {
    digitalWrite(relayPin, HIGH);
    systemRunning = false;
    modeJustFinished = true;
    showModeComplete();
  }
}

void readSwitch() {
  int reading = digitalRead(switchPin);
  if (reading != lastSwitchState) lastDebounceTime = millis();
  if (safeMillisDiff(millis(), lastDebounceTime) > debounceDelay) {
    if (reading != switchState) {
      switchState = reading;
      if (switchState == LOW) {
        if (!systemRunning && currentMode == MODE_IDLE) {
          for (int i = 0; i < TOTAL_MODES; i++) {
            if (modeTimes[i] > 0) {
              startMode((SystemMode)(i + 1));
              break;
            }
          }
        } else if (!systemRunning && modeJustFinished) {
          startMode(nextMode);
          modeJustFinished = false;
        }
      }
    }
  }
  lastSwitchState = reading;
}

// ------------------ Input Keypad ------------------

void handleKeypadInput(char key) {
  if (key == '*' && !inputMode) {
    resetAll();
  } else if (key == '#' && !inputMode) {
    inputMode = true;
    inputValue = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pilih Mode A1-D9");
    lcd.setCursor(0, 1);
    lcd.print("Gunakan A..D lalu 1..9");
  } else if (inputMode && !waitingForTime && key >= 'A' && key <= 'D') {
    currentGroup = key;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Grup: "); lcd.print(currentGroup);
    lcd.setCursor(0, 1);
    lcd.print("Tekan 1-9");
  } else if (inputMode && !waitingForTime && key >= '1' && key <= '9') {
    int group = currentGroup - 'A';
    int number = key - '1';
    selectedModeToSet = (SystemMode)(group * 9 + number + 1);
    waitingForTime = true;
    inputValue = "";
    tempValue = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set "); lcd.print(getModeString(selectedModeToSet));
    lcd.setCursor(0, 1); lcd.print("Input ms + #");
  } else if (waitingForTime && isdigit(key)) {
    inputValue += key;
    if (inputValue.length() > 5) inputValue = inputValue.substring(0, 5);
    tempValue = inputValue.toInt();
    lcd.setCursor(0, 1);
    lcd.print(inputValue); lcd.print(" ms    ");
  } else if (waitingForTime && key == '#') {
    saveInput();
  }
}

void saveInput() {
  if (selectedModeToSet >= MODE_A1 && selectedModeToSet <= MODE_D9) {
    modeTimes[getModeIndex(selectedModeToSet)] = tempValue;
  }
  inputMode = false;
  waitingForTime = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(getModeString(selectedModeToSet));
  lcd.print(" = ");
  lcd.print(tempValue); lcd.print("ms");
  lcd.setCursor(0, 1);
  lcd.print("Disimpan");
  delay(1000);
  showMainScreen();
}

void resetAll() {
  for (int i = 0; i < TOTAL_MODES; i++) modeTimes[i] = 0;
  inputMode = false;
  waitingForTime = false;
  currentMode = MODE_IDLE;
  systemRunning = false;
  modeJustFinished = false;
  digitalWrite(relayPin, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Reset Berhasil");
  delay(1000);
  showMainScreen();
}
