#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>



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
bool phaseACompleted = false;
int brightness = 100;

// Setup LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Setup keypad (4x4)
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {12, 11, 10, 9};
byte colPins[COLS] = {8, 7, 6, 5};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(irSensorPin, INPUT_PULLUP);
  pinMode(outputPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP); 

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Motor Controller");
  lcd.setCursor(0, 1);
  lcd.print("Starting......");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Koding Februari");
  lcd.setCursor(0, 1);
  lcd.print("2025 Ver.1");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Created & Design");
  lcd.setCursor(0, 1);
  lcd.print("by AGUS F");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mesin Siap");
  lcd.setCursor(0, 1);
  lcd.print("Masukan Putaran");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set A:    B:  ");

  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(irSensorPin), countTurns, FALLING);
}

void countTurns() {
  if (digitalRead(irSensorPin) == LOW) {
    delayMicroseconds(5000);
    if (digitalRead(irSensorPin) == LOW) {
      count++;
    }
  }
}

bool debounceButton(int pin) {
  static unsigned long lastPress = 0;
  const unsigned long debounceTime = 50;
  if (digitalRead(pin) == LOW) {
    if (millis() - lastPress > debounceTime) {
      lastPress = millis();
      return true;
    }
  }
  return false;
}

void loop() {
  if (digitalRead(switchPin) == LOW) {
    stopMotor();
  }

  if (debounceButton(startButtonPin) && !motorRunning) {
    if (keypadLocked) {
      startMotor();
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Lock first (#) ");
    }
  }

  static int lastCount = -1;
  if (count != lastCount) {
    lastCount = count;
    lcd.setCursor(0, 1);
    lcd.print("Turns: ");
    lcd.print(count);
    lcd.print("    ");

    if (!phaseACompleted && count >= targetTurnsA) {
      digitalWrite(outputPin, LOW);
      phaseACompleted = true;
      count = 0;
      lcd.setCursor(0, 1);
      lcd.print("Turns: 0     ");
    } else if (phaseACompleted && count >= targetTurnsB) {
      stopMotor();
    }
  }

  char key = keypad.getKey();
  if (key != NO_KEY) {
    if (key == 'A') {
      inputModeA = true;
      inputModeB = false;
      targetTurnsA = 0;
      lcd.setCursor(0, 1);
      lcd.print("Input A Mode: ON ");
    } else if (key == 'B') {
      inputModeB = true;
      inputModeA = false;
      targetTurnsB = 0;
      lcd.setCursor(0, 1);
      lcd.print("Input B Mode: ON ");
    }
    if (inputModeA && key >= '0' && key <= '9') {
      targetTurnsA = targetTurnsA * 10 + (key - '0');
      lcd.setCursor(6, 0);
      lcd.print(targetTurnsA);
    } else if (inputModeB && key >= '0' && key <= '9') {
      targetTurnsB = targetTurnsB * 10 + (key - '0');
      lcd.setCursor(12, 0);
      lcd.print(targetTurnsB);
    }
    if (key == 'C') {
      brightness = min(brightness + 10, 255);
      analogWrite(6, brightness);
    }
    if (key == 'D') {
      brightness = max(brightness - 10, 0);
      analogWrite(6, brightness);
    }
    if (key == '#') {
      if (targetTurnsA > 0 && targetTurnsB > 0) {
        keypadLocked = true;
        lcd.setCursor(0, 1);
        lcd.print("Locked       ");
      }
    }
    if (key == '*') {
      targetTurnsA = 0;
      targetTurnsB = 0;
      keypadLocked = false;
      lcd.setCursor(0, 0);
      lcd.print("Set A:    B:      ");
    }
  }
}

void startMotor() {
  digitalWrite(motorPin, HIGH);
  motorRunning = true;
  count = 0;
  phaseACompleted = false;
  lcd.setCursor(0, 1);
  lcd.print("Turns: 0    ");
}

void stopMotor() {
  digitalWrite(motorPin, LOW);
  motorRunning = false;
  lcd.setCursor(0, 1);
  lcd.print("Motor Stopped");
}
