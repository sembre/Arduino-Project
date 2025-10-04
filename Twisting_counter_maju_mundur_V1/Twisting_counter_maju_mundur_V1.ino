#include <Keypad.h> 
#include <LiquidCrystal.h>
#include <limits.h>
#define LCD_Backlight 4

// Pin konfigurasi
const int motorPin = A1;
const int startButtonPin = 2;
const int irSensorPin = 3;
const int outputPin = A3;
const int switchPin = A2;

volatile int count = 0;
int targetTurnsA = 0;
int targetTurnsB = 0;
bool motorRunning = false;
bool keypadLocked = false;
bool inputModeA = false;
bool inputModeB = false;

enum Phase { PHASE_IDLE, PHASE_A, PHASE_B, DONE };
Phase currentPhase = PHASE_IDLE;

bool processCompleted = false;

LiquidCrystal lcd(14, 15, 16, 17, 18, 19);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {12, 11, 10, 9};
byte colPins[COLS] = {8, 7, 6, 5};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(motorPin, OUTPUT); digitalWrite(motorPin, LOW);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(irSensorPin, INPUT_PULLUP);
  pinMode(outputPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);

  analogWrite(LCD_Backlight, 100);
  lcd.begin(16, 2);
  Serial.begin(9600);

  lcd.setCursor(0,0); lcd.print("Motor Controller");
  lcd.setCursor(0,1); lcd.print("Starting......");
  delay(2000);

  lcd.clear(); lcd.setCursor(0,0); lcd.print("Koding Februari");
  lcd.setCursor(0,1); lcd.print("2025 Ver.1");
  delay(2000);

  lcd.clear(); lcd.setCursor(0,0); lcd.print("Created by AGUSF");
  delay(2000);

  lcd.clear(); lcd.setCursor(0,0); lcd.print("Mesin Siap");
  lcd.setCursor(0,1); lcd.print("Masukan Putaran");
  delay(2000);

  lcd.clear(); lcd.setCursor(0,0); lcd.print("Set A:   B:  ");

  attachInterrupt(digitalPinToInterrupt(irSensorPin), countTurns, FALLING);
}

void countTurns() {
  delayMicroseconds(3000);
  if (digitalRead(irSensorPin) == LOW) count++;
}

bool debounceButton(int pin) {
  static unsigned long lastPress = 0;
  if (digitalRead(pin) == LOW && millis() - lastPress > 200) {
    lastPress = millis();
    return true;
  }
  return false;
}

void loop() {
  if (digitalRead(switchPin) == LOW) {
    stopMotor();
    return;
  }

  // Tombol start ditekan
  if (debounceButton(startButtonPin) && !motorRunning && keypadLocked) {
    currentPhase = PHASE_A;
    count = 0;
    startMotor();
    processCompleted = false;
  }

  // Proses counting saat motor aktif
  if (motorRunning) {
    lcd.setCursor(0, 1);
    lcd.print("Turns: ");
    lcd.print(count);
    lcd.print("    ");

    if (currentPhase == PHASE_A && count >= targetTurnsA) {
      stopMotor();
      delay(300);
      count = 0;
      currentPhase = PHASE_B;
      startMotor();
    } else if (currentPhase == PHASE_B && count >= targetTurnsB) {
      stopMotor();
      currentPhase = DONE;
      processCompleted = true;
    }
  }

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
    }

    if (inputModeA && isDigit(key)) {
      targetTurnsA = targetTurnsA * 10 + (key - '0');
      lcd.setCursor(6, 0); lcd.print(targetTurnsA);
    } else if (inputModeB && isDigit(key)) {
      targetTurnsB = targetTurnsB * 10 + (key - '0');
      lcd.setCursor(12, 0); lcd.print(targetTurnsB);
    }

    if (key == '#') {
      if (targetTurnsA > 0 && targetTurnsB > 0) {
        keypadLocked = true;
        inputModeA = false;
        inputModeB = false;
        lcd.setCursor(0, 1); lcd.print("Locked            ");
      } else {
        lcd.setCursor(0, 1); lcd.print("Set turns first   ");
      }
    }

    if (key == '*') {
      targetTurnsA = 0; targetTurnsB = 0;
      keypadLocked = false;
      inputModeA = false; inputModeB = false;
      currentPhase = PHASE_IDLE;
      processCompleted = false;
      lcd.setCursor(6, 0); lcd.print("0    ");
      lcd.setCursor(12, 0); lcd.print("0    ");
      lcd.setCursor(0, 1); lcd.print("Set A:    B:      ");
    }
  }
}

void startMotor() {
  digitalWrite(motorPin, HIGH);

  if (currentPhase == PHASE_A) {
    digitalWrite(outputPin, HIGH); // ON hanya di Mode A
  } else {
    digitalWrite(outputPin, LOW);  // OFF di Mode B
  }

  motorRunning = true;
  lcd.setCursor(0, 1);
  lcd.print("Turns: 0            ");
  count = 0;
}


void stopMotor() {
  digitalWrite(motorPin, LOW);
  digitalWrite(outputPin, LOW);
  motorRunning = false;
  lcd.setCursor(0, 1); lcd.print("Motor Stopped ");
}
