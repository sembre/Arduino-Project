#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>

// ------------------ Pin Setup ------------------ 
//perbaikan 17 juli 2025
//✅ PERBAIKAN YANG DILAKUKAN:
//Pastikan semua kondisi logis terpenuhi sebelum startMotor().
//Tambahkan pengecekan switch emergency (switchPin) sebelum aktivasi.
//Tambahkan validasi targetTurnsA dan targetTurnsB harus valid.
//Tambahkan flag systemReady agar tidak langsung running saat baru nyala.
//Hindari penggunaan digitalWrite HIGH jika tombol belum dipastikan ditekan.

const int motorPin = A1;
const int startButtonPin = 2;
const int irSensorPin = 3;
const int outputPin = A3;
const int switchPin = A2;
const int clk = 4;
const int dio = 5;

volatile int count = 0;
int targetTurnsA = 0;
int targetTurnsB = 0;
bool motorRunning = false;
bool keypadLocked = false;
bool inputModeA = false;
bool inputModeB = false;
bool phaseACompleted = false;
bool systemReady = false;

int counter = 0;
unsigned long pressStartTime = 0;
bool isKeyDPressed = false;

// LCD & Display
LiquidCrystal_I2C lcd(0x27, 16, 2);
TM1637Display display(clk, dio);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {A12, A13, A14, A15};
byte colPins[COLS] = {A8, A9, A10, A11};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ------------------ Setup ------------------
void setup() {
  pinMode(motorPin, OUTPUT);       digitalWrite(motorPin, LOW);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(irSensorPin, INPUT_PULLUP);
  pinMode(outputPin, OUTPUT);     digitalWrite(outputPin, LOW);
  pinMode(switchPin, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  display.setBrightness(7);
  display.showNumberDec(0, true);

  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(irSensorPin), countTurns, FALLING);

  lcd.setCursor(0, 0); lcd.print("Motor Controller");
  lcd.setCursor(0, 1); lcd.print("Starting......");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Koding Juli");
  lcd.setCursor(0, 1); lcd.print("2025 Ver.1.2");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Created & Design");
  lcd.setCursor(0, 1); lcd.print("by AGUS F");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Mesin Siap");
  lcd.setCursor(0, 1); lcd.print("Masukan Putaran");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Set A:    B:  ");
  systemReady = true; // sistem baru aktif setelah setup selesai
}

// ------------------ Interrupt ------------------
void countTurns() {
  if (digitalRead(irSensorPin) == LOW) {
    delayMicroseconds(3000);
    if (digitalRead(irSensorPin) == LOW) {
      count++;
    }
  }
}

// ------------------ Debounce ------------------
bool debounceButton(int pin) {
  static unsigned long lastPress = 0;
  const unsigned long debounceTime = 100;
  if (digitalRead(pin) == LOW) {
    if (millis() - lastPress > debounceTime) {
      lastPress = millis();
      return true;
    }
  }
  return false;
}

// ------------------ Main Loop ------------------
void loop() {
  if (!systemReady) return;

  if (digitalRead(switchPin) == LOW) {
    stopMotor();
    return;
  }

  // Tombol Start ditekan
  if (debounceButton(startButtonPin) && !motorRunning) {
    if (keypadLocked && targetTurnsA > 0 && targetTurnsB > 0) {
      startMotor();
      counter++;
      display.showNumberDec(counter);
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Lock dulu (#)   ");
    }
  }

  // Update putaran
  static int lastCount = -1;
  if (count != lastCount) {
    lastCount = count;
    lcd.setCursor(0, 1);
    lcd.print("BERPUTAR: ");
    lcd.print(count);
    lcd.print("    ");

    if (!phaseACompleted && count >= targetTurnsA) {
      digitalWrite(outputPin, LOW); // masuk fase B
      phaseACompleted = true;
      count = 0;
    } else if (phaseACompleted && count >= targetTurnsB) {
      stopMotor();
    }
  }

  // Keypad Input
  char key = keypad.getKey();
  if (key != NO_KEY) {
    if (key == 'A') {
      inputModeA = true; inputModeB = false;
      targetTurnsA = 0;
      lcd.setCursor(0, 1); lcd.print("Input A Mode: ON ");
    } else if (key == 'B') {
      inputModeB = true; inputModeA = false;
      targetTurnsB = 0;
      lcd.setCursor(0, 1); lcd.print("Input B Mode: ON ");
    } else if (key >= '0' && key <= '9') {
      if (inputModeA) {
        targetTurnsA = targetTurnsA * 10 + (key - '0');
        lcd.setCursor(6, 0); lcd.print(targetTurnsA);
      } else if (inputModeB) {
        targetTurnsB = targetTurnsB * 10 + (key - '0');
        lcd.setCursor(12, 0); lcd.print(targetTurnsB);
      }
    } else if (key == '#') {
      if (targetTurnsA > 0 && targetTurnsB > 0) {
        keypadLocked = true;
        lcd.setCursor(0, 1); lcd.print("Locked          ");
      }
    } else if (key == '*') {
      targetTurnsA = 0; targetTurnsB = 0;
      keypadLocked = false;
      lcd.setCursor(0, 0); lcd.print("Set A:    B:    ");
    } else if (key == 'C') {
      counter++;
      display.showNumberDec(counter);
    } else if (key == 'D') {
      if (!isKeyDPressed) {
        pressStartTime = millis();
        isKeyDPressed = true;
      }
    }
  }

  // Deteksi tombol D ditekan lama untuk reset counter
  if (isKeyDPressed) {
    if (millis() - pressStartTime >= 3000) {
      counter = 0;
      display.showNumberDec(0, true);
      lcd.setCursor(0, 1); lcd.print("Counter Reset   ");
      isKeyDPressed = false;
    }
  } else {
    isKeyDPressed = false;
  }
}

// ------------------ Motor Control ------------------
void startMotor() {
  if (digitalRead(switchPin) == LOW) {
    stopMotor();
    return;
  }
  motorRunning = true;
  digitalWrite(motorPin, HIGH);
  digitalWrite(outputPin, HIGH);
  count = 0;
  phaseACompleted = false;
  lcd.setCursor(3, 1);
  lcd.print("Turns: 0    ");
}

void stopMotor() {
  digitalWrite(motorPin, LOW);
  digitalWrite(outputPin, LOW);
  motorRunning = false;
  lcd.setCursor(0, 1);
  lcd.print("Motor Stopped   ");
}
