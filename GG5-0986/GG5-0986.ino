#include <LiquidCrystal.h>

// Inisialisasi LCD: RS = 8, Enable = 12, D4 = 9, D5 = 10, D6 = 11, D7 = 13
LiquidCrystal lcd(8, 12, 9, 10, 11, 13);

const int pins[] = {2, 3, 4, 5, 6, 7};  // D2 sampai D6
const int backlightPin = A0;          // Pin untuk mengontrol backlight LCD
const char* messages[] = {
  "MS-VS-J1", "MS-VS-J2", "MS-VS-J3", "MS-VS-J4", "MS-VS-J5", "MS-VS-J6"
};

int lastActivePin = -1; // Menyimpan pin aktif terakhir

void setup() {
  lcd.begin(16, 2);
  pinMode(backlightPin, OUTPUT);
  digitalWrite(backlightPin, HIGH);  // Nyalakan backlight

  // Tampilkan pesan "Created & Design By Agus F" selama 5 detik
  lcd.setCursor(0, 0);
  lcd.print("Created & Design");
  lcd.setCursor(4, 1);
  lcd.print("By Agus F");
  delay(5000);  // Tunda selama 5 detik

  // Tampilkan teks tetap pada baris pertama
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pasang Flag Tie");
  
  // Konfigurasi pin input dengan internal pull-up
  for (int i = 0; i < 6; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }
}

void loop() {
  int currentActivePin = -1;

  // Cek apakah ada pin yang tersambung ke GND
  for (int i = 0; i < 6; i++) {
    if (digitalRead(pins[i]) == LOW) { // Jika pin disambungkan ke GND
      currentActivePin = i;
      break;
    }
  }

  // Perbarui LCD dan kontrol backlight hanya jika pin aktif berubah
  if (currentActivePin != lastActivePin) {
    if (currentActivePin != -1) {
      digitalWrite(backlightPin, HIGH); // Nyalakan backlight
      lcd.setCursor(4, 1);              // Pindahkan kursor ke baris kedua
      lcd.print("                ");     // Bersihkan baris kedua
      lcd.setCursor(4, 1);              // Kembali ke baris kedua
      lcd.print(messages[currentActivePin]); // Tampilkan pesan baru
    } else {
      lcd.setCursor(4, 1);
      lcd.print("                ");     // Bersihkan baris kedua jika tidak ada pin aktif
      digitalWrite(backlightPin, LOW);   // Matikan backlight
    }
    lastActivePin = currentActivePin; // Perbarui status pin aktif terakhir
  }
}
