// Version: Januari 2025

#include <Keypad.h>
#include <LiquidCrystal.h>
#define LCD_Backlight 4

// Pin konfigurasi
const int motorPin = A1;        // Pin untuk motor
const int startButtonPin = 2;   // Pin untuk tombol start
const int irSensorPin = 3;      // Pin untuk sensor infrared
const int outputPin = A3;       // Pin output tambahan
const int switchPin = A2;       // Pin untuk switch tambahan

int count = 0;                  // Variabel untuk menyimpan jumlah putaran saat ini
int targetTurnsA = 0;           // Jumlah putaran dari tombol A
int targetTurnsB = 0;           // Jumlah putaran dari tombol B
bool motorRunning = false;      // Status motor
bool keypadLocked = false;      // Status penguncian keypad
bool inputModeA = false;        // Mode input targetTurns dengan tombol A
bool inputModeB = false;        // Mode input targetTurns dengan tombol B
bool phaseACompleted = false;   // Status untuk mengetahui apakah fase A sudah selesai

// Setup LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(14, 15, 16, 17, 18, 19);

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
  digitalWrite(motorPin, LOW);  // Memastikan motor dalam keadaan mati saat pertama kali dinyalakan
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(irSensorPin, INPUT_PULLUP);
  pinMode(outputPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);  // Konfigurasi pin switch

  analogWrite(LCD_Backlight, 100);
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Set A:   B:  ");
  Serial.begin(9600);
}

void loop() {

  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; // 50ms debounce
  
  // Jika switch ON, motor harus mati
  if (digitalRead(switchPin) == LOW) {
    stopMotor();
  }

  if (digitalRead(startButtonPin) == LOW && !motorRunning) {
    if (millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      if (keypadLocked) {
        startMotor();
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Lock first (#)     ");
      }
    }
  }

  if (digitalRead(irSensorPin) == LOW && motorRunning) { // Mengubah logika sensor IR
    delay(5);
    if (digitalRead(irSensorPin) == LOW) {
      count++;
      lcd.setCursor(0, 1);
      lcd.print("Turns: ");
      lcd.print(count);
      lcd.print("    ");

      if (!phaseACompleted && count >= targetTurnsA) {
        digitalWrite(outputPin, LOW);
        phaseACompleted = true;
        count = 0;
        lcd.setCursor(0, 1);
        lcd.print("Turns: 0            ");
      } else if (phaseACompleted && count >= targetTurnsB) {
        stopMotor();
      }
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
    
    if (inputModeA) {
      if (key >= '0' && key <= '9') {
        targetTurnsA = targetTurnsA * 10 + (key - '0');
        lcd.setCursor(6, 0);
        lcd.print(targetTurnsA);
      }
    } else if (inputModeB) {
      if (key >= '0' && key <= '9') {
        targetTurnsB = targetTurnsB * 10 + (key - '0');
        lcd.setCursor(12, 0);
        lcd.print(targetTurnsB);
      }
    }

    if (key == '#') {
      if (targetTurnsA > 0 && targetTurnsB > 0) {
        keypadLocked = true;
        inputModeA = false;
        inputModeB = false;
        lcd.setCursor(0, 1);
        lcd.print("Locked            ");
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Set turns first   ");
      }
    }

    if (key == '*') {
      targetTurnsA = 0;
      targetTurnsB = 0;
      keypadLocked = false;
      inputModeA = false;
      inputModeB = false;
      lcd.setCursor(6, 0);
      lcd.print("0    ");
      lcd.setCursor(12, 0);
      lcd.print("0    ");
      lcd.setCursor(0, 1);
      lcd.print("Set A:    B:      ");
    }
  }
}

void startMotor() {
  // Jika switch ON, motor tidak boleh menyala
  if (digitalRead(switchPin) == LOW) {
    stopMotor();
    return;
  }

  digitalWrite(motorPin, HIGH);
  digitalWrite(outputPin, HIGH);
  motorRunning = true;
  count = 0;
  phaseACompleted = false;
  lcd.setCursor(0, 1);
  lcd.print("Turns: 0            ");
}

void stopMotor() {
  digitalWrite(motorPin, LOW);
  digitalWrite(outputPin, LOW);
  motorRunning = false;
  lcd.setCursor(0, 1);
  lcd.print("Motor Stopped ");
}
