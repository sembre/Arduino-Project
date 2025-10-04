#include <TM1637Display.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN 21
#define DIO_PIN 20

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

// Deklarasi pin untuk pengecekan koneksi
const int connectionPairs[][2] = {
  {2, 3},  // Pasangan pin 2 dan 3
  {4, 5}   // Pasangan pin 4 dan 5
};

void setup() {
  // Inisialisasi TM1637
  tm1637.setBrightness(7); // Set kecerahan (brightness), nilainya bisa diatur antara 0 hingga 7
}

void loop() {
  for (int i = 0; i < sizeof(connectionPairs) / sizeof(connectionPairs[0]); i++) {
    int pin1 = connectionPairs[i][0];
    int pin2 = connectionPairs[i][1];

    pinMode(pin1, INPUT_PULLUP);
    pinMode(pin2, INPUT_PULLUP);

    if (digitalRead(pin1) == LOW && digitalRead(pin2) == LOW) {
      // Jika koneksi sesuai, tampilkan ---- pada TM1637 display
      tm1637.showNumberDecEx(0, 0b01110011); // Menampilkan "----"
    } else {
      // Jika koneksi tidak sesuai, tampilkan nomor pin pada digit 1 dan 4
      tm1637.showNumberDecEx(pin1, (1 << 3)); // Menampilkan nomor pin pada digit 1
      tm1637.showNumberDecEx(pin2, 3 << 12); // Menampilkan nomor pin pada digit 4
    }

    delay(2000); // Tampilkan selama 2 detik
    tm1637.clear(); // Hapus tampilan setelah selesai
  }
}
