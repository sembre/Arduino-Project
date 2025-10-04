#include <TM1637Display.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

const int comToNCPin = 2;  // Pin untuk COM ke NC
const int comToNOPin = 3;  // Pin untuk COM ke NO
const int buzzerPin = 6;   // Pin untuk buzzer
const int ledPin = 7;      // Pin untuk LED

int storedConditions = 0;  // Menyimpan status kondisi
const uint8_t SEG_GOOD[] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_G | SEG_F,   // g
  SEG_C | SEG_D | SEG_E | SEG_G,                   // O
  SEG_C | SEG_D | SEG_E | SEG_G,                   // O
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
};
const uint8_t SEG_NC[] = {
  SEG_G,                                           // -
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_G | SEG_D | SEG_E,                           // c
  SEG_G,                                           // -
};
const uint8_t SEG_NO[] = {
  SEG_G,                                           // -
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,           // n
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // o
  SEG_G,                                           // -
};
void setup() {
  pinMode(comToNCPin, INPUT_PULLUP); // Mengatur pin COM ke NC sebagai input dengan pull-up resistor
  pinMode(comToNOPin, INPUT_PULLUP); // Mengatur pin COM ke NO sebagai input dengan pull-up resistor
  pinMode(buzzerPin, OUTPUT);        // Mengatur pin buzzer sebagai output
  pinMode(ledPin, OUTPUT);           // Mengatur pin LED sebagai output
  tm1637.setBrightness(0x0a); // Atur kecerahan tampilan

  // Matikan buzzer dan LED saat awalnya
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, LOW);

  // Mulai komunikasi dengan Serial Monitor
  Serial.begin(9600);
}

void loop() {
  int comToNCState = digitalRead(comToNCPin); // Membaca kondisi COM ke NC
  int comToNOState = digitalRead(comToNOPin); // Membaca kondisi COM ke NO

  // Kondisi pertama: Semua belum tersambung
  if (comToNCState == HIGH && comToNOState == HIGH) {
    if (storedConditions != 0) {
      Serial.println("Kembali ke Kondisi Pertama. Mereset kondisi yang tersimpan.");
      storedConditions = 0;
      tm1637.clear(); // Bersihkan tampilan TM1637
    }
  }

  // Kondisi kedua: Saklar dalam posisi COM dan NC terhubung
  if (comToNCState == LOW && comToNOState == HIGH) {
    storedConditions |= 1;  // Set bit pertama untuk kondisi kedua
    Serial.println("Kondisi Kedua terpenuhi: COM dan NC terhubung.");
    tm1637.setSegments(SEG_NC); // Menampilkan "NC" pada tampilan TM1637
  } else {
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
    digitalWrite(ledPin, LOW);    // Matikan LED
  }

  // Kondisi ketiga: Saklar dalam posisi COM dan NO terhubung
  if (comToNCState == HIGH && comToNOState == LOW) {
    storedConditions |= 2;  // Set bit kedua untuk kondisi ketiga
    Serial.println("Kondisi Ketiga terpenuhi: COM dan NO terhubung.");
    tm1637.clear(); // Bersihkan tampilan TM1637
    tm1637.setSegments(SEG_NC); // Menampilkan "NO" pada tampilan TM1637
  } else {
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
    digitalWrite(ledPin, LOW);    // Matikan LED
  }
  // Memeriksa apakah semua tiga kondisi terpenuhi
  if (storedConditions == 3) {
    Serial.println("Semua kondisi terpenuhi. Menyalakan buzzer.");
    tm1637.clear(); // Bersihkan tampilan TM1637
    tm1637.setSegments(SEG_GOOD); // Tampilkan "GOOD" pada tampilan TM1637
    digitalWrite(buzzerPin, HIGH); // Nyalakan buzzer
    Serial.println("Menyalakan buzzer.");
  } else {
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
    digitalWrite(ledPin, LOW);    // Matikan LED
  }

  delay(200); // Tunggu sebentar sebelum membaca ulang
}
