#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int pin2 = 2;  // Pin 2 sebagai output +5V
const int pin3 = 3;  // Pin 3 sebagai pengecekan tahap pertama
const int pin4 = 4;  // Pin 4 sebagai pengecekan tahap kedua
const int buzzer = 5; // Buzzer pada pin 5

bool tahap1 = false; // Menyimpan status apakah tahap pertama telah terjadi
bool tahap2 = false; // Menyimpan status apakah tahap kedua telah terjadi
bool errorState = false; // Menyimpan status jika terjadi kesalahan (Pin 4 aktif duluan)

LiquidCrystal_I2C lcd(0x27, 16, 2); // Inisialisasi LCD dengan alamat 0x27

void setup() {
  Serial.begin(9600); // Inisialisasi Serial Monitor
  Wire.begin(); // Inisialisasi komunikasi I2C pada Arduino Mega (SDA = 20, SCL = 21)
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Mulai");
  
  pinMode(pin2, OUTPUT);
  pinMode(pin3, INPUT);
  pinMode(pin4, INPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(pin2, HIGH); // Pin 2 selalu mengeluarkan +5V
}

void loop() {
 
  // Jika Pin 4 aktif sebelum tahap pertama terjadi, hentikan proses
  if (!tahap1 && digitalRead(pin4) == HIGH) {
    errorState = true;
    Serial.println("Error: Pin 4 aktif sebelum Pin 3. Proses dihentikan.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("     ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Pin 1&4 terbalik");
  }
  
  if (!errorState && digitalRead(pin3) == HIGH) {
    tahap1 = true; // Menyimpan status tahap pertama
    Serial.println("Tahap 1 terpenuhi: Pin 3 tersambung");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Cek pin 2&3 = OK");
    lcd.setCursor(0, 1);
    lcd.print("  Tekan Switch");
  }
 
  if (tahap1 && digitalRead(pin4) == HIGH) {
    tahap2 = true; // Menyimpan status tahap kedua
    Serial.println("Tahap 2 terpenuhi: Pin 4 tersambung");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Cek pin 1&2 = OK");
  }
   
  if (tahap1 && tahap2) {
    digitalWrite(buzzer, HIGH); // Buzzer menyala jika kedua tahap terpenuhi
    Serial.println("Buzzer ON");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Product OK");    
    lcd.setCursor(0, 1);
    lcd.print("Switch Berfungsi");
  } else {
    digitalWrite(buzzer, LOW); // Buzzer mati jika kondisi tidak terpenuhi
    Serial.println("Buzzer OFF");

  }
  
  
  if (digitalRead(pin3) == LOW && digitalRead(pin4) == LOW) {
    tahap1 = false; // Reset jika tidak ada koneksi
    tahap2 = false;
    errorState = false;
    Serial.println("Reset kondisi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" MASUKAN SWITCH");
    lcd.setCursor(0, 1);
    lcd.print(" Ke posh Header");
  }
}
