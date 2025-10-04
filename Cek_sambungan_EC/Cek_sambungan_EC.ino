#include <TM1637Display.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1
#define BUZZER_PIN A2 // Tambahkan buzzer pada pin A2

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Deklarasi pin secara individu
const int pins[] = {
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53 ,
  2,  3,  4,  5,  6
};

void setup() {
  tm1637.setBrightness(7); // Set kecerahan
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }
}

void loop() {
  static int lastPin = -1;
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if (digitalRead(pins[i]) == LOW) {
      if (pins[i] != lastPin) {
        lastPin = pins[i];
        char suffix = (i % 2 == 0) ? 'A' : 'B';
        int number = (i / 2) + 1;

        uint8_t data[4] = {0, 0, 0, 0};
        data[0] = (suffix == 'A') ? 0b11110111 : 0b11111100; // A atau b
        data[1] = 0; // Spasi kosong
        
        if (number >= 10) {
          data[2] = tm1637.encodeDigit((number / 10) % 10); // Puluhan
          data[3] = tm1637.encodeDigit(number % 10); // Satuan
        } else {
          data[2] = tm1637.encodeDigit(number % 10); // Hanya satuan
          data[1] = 0; // Pastikan hanya satu angka ditampilkan
        }
        
        tm1637.setSegments(data);
        
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        
        Serial.print("Pin ");
        Serial.print(pins[i]);
        Serial.print(" terhubung, menampilkan: ");
        Serial.print(suffix);
        Serial.print(number);
        Serial.println();
      }
      delay(500);
      return;
    }
  }
  lastPin = -1;
  tm1637.clear();
}
