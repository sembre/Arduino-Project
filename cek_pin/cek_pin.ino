#include <TM1637Display.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Deklarasi pin secara individu
const int pins[] = {
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
  14, 15, 
  A2, A3, A4, A5, A6, A7, A8, A9,
  A10, A11, A12, A13, A14, A15,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53
};

void setup() {
  // Inisialisasi TM1637
  tm1637.setBrightness(7); // Set kecerahan (brightness), nilainya bisa diatur antara 0 hingga 7

  // Deklarasi pin sebagai INPUT_PULLUP agar pin terbaca HIGH saat tidak tersambung ke GND
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }
}

void loop() {
  // Cek setiap pin
  for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if (digitalRead(pins[i]) == LOW) {
      // Jika pin terhubung ke GND, tampilkan angka pada TM1637 display
      int valueToShow = i + 1; // Mulai angka dari 1
      tm1637.showNumberDecEx(valueToShow);
      delay(1000); // Tampilkan selama 1 detik
      tm1637.clear(); // Hapus tampilan setelah selesai
      break; // Keluar dari loop setelah menemukan pin yang terhubung
    }
  }
}
