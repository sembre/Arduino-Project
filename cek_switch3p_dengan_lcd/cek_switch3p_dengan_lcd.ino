#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>

const int pin2 = 2;  // Pin 2 sebagai output +5V
const int pin3 = 3;  // Pin 3 sebagai pengecekan tahap pertama
const int pin4 = 4;  // Pin 4 sebagai pengecekan tahap kedua
const int buzzer = 5; // Buzzer pada pin 5
const int clk = 6;   // Pin CLK TM1637
const int dio = 7;   // Pin DIO TM1637

bool tahap1 = false;
bool tahap2 = false;
bool errorState = false;
String lastMessage = "";
unsigned long lastUpdate = 0;
const unsigned long lcdUpdateInterval = 500; // Update LCD setiap 500ms
int buzzerCount = 0; // Counter untuk menghitung berapa kali buzzer menyala
bool buzzerTriggered = false; // Status apakah buzzer sudah dihitung

LiquidCrystal_I2C lcd(0x27, 16, 2);
TM1637Display display(clk, dio);

void updateLCD(const String &message, const String &subMessage) {
  if (message != lastMessage && millis() - lastUpdate > lcdUpdateInterval) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
    lcd.setCursor(0, 1);
    lcd.print(subMessage);
    lastMessage = message;
    lastUpdate = millis();
  }
}

void updateDisplay() {
  display.showNumberDec(buzzerCount, false);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(16, 2);
  lcd.backlight();
  display.setBrightness(7);
  updateLCD("Sistem Mulai....", "   by AGUS F");
  delay (5000) ;

  pinMode(pin2, OUTPUT);
  pinMode(pin3, INPUT);
  pinMode(pin4, INPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(pin2, HIGH);
  updateDisplay();
}

void loop() {
  if (!tahap1 && digitalRead(pin4) == HIGH) {
    errorState = true;
    Serial.println("Error: Pin 4 aktif sebelum Pin 3. Proses dihentikan.");
    updateLCD("     ERROR", "Pin 1&3 terbalik");
  }

  if (!errorState && digitalRead(pin3) == HIGH) {
    tahap1 = true;
    Serial.println("Tahap 1 terpenuhi: Pin 3 tersambung");
    updateLCD("Cek pin 2&3 = OK", "  Tekan Switch");
  }

  if (tahap1 && digitalRead(pin4) == HIGH) {
    tahap2 = true;
    Serial.println("Tahap 2 terpenuhi: Pin 4 tersambung");
    updateLCD("Cek pin 1&2 = OK", "Buzzer Siap!");
  }

  if (tahap1 && tahap2) {
    digitalWrite(buzzer, HIGH);
    Serial.println("Buzzer ON");
    updateLCD("  Product OK", "Switch Berfungsi");
    if (!buzzerTriggered) {
      buzzerCount++;
      updateDisplay();
      buzzerTriggered = true;
    }
  } else {
    digitalWrite(buzzer, LOW);
    Serial.println("Buzzer OFF");
  }

  if (digitalRead(pin3) == LOW && digitalRead(pin4) == LOW) {
    tahap1 = false;
    tahap2 = false;
    errorState = false;
    buzzerTriggered = false;
    Serial.println("Reset kondisi");
    updateLCD("Masukan Switch", "Ke Posh HEADER");
  }
}
