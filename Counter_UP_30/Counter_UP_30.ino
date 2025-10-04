#include <TM1637Display.h> // Mengimpor library untuk mengendalikan display 7-segmen TM1637

#define CLK 2 // Mendefinisikan pin CLK (jam) TM1637 ke pin digital 2 Arduino
#define DIO 3 // Mendefinisikan pin DIO (data) TM1637 ke pin digital 3 Arduino
TM1637Display display(CLK, DIO); // Membuat objek 'display' untuk mengontrol TM1637

#define BUTTON_UP 4     // Pin tombol untuk menambah waktu
#define BUTTON_DOWN 5   // Pin tombol untuk mengurangi waktu
#define RELAY_PIN 6     // Pin untuk mengontrol relay

#define TRIG_PIN 7      // Pin trigger sensor ultrasonik
#define ECHO_PIN 8      // Pin echo sensor ultrasonik

int countdownTime = 30;        // Waktu awal countdown (detik)
int initialTime = 30;          // Menyimpan waktu awal untuk reset
unsigned long previousMillis = 0; // Waktu terakhir countdown dikurangi
bool countingDown = false;     // Menandai apakah sedang menghitung mundur
bool started = false;          // Menandai apakah sudah dimulai

void setup() {
  pinMode(BUTTON_UP, INPUT_PULLUP);   // Atur tombol tambah sebagai input dengan resistor pull-up internal
  pinMode(BUTTON_DOWN, INPUT_PULLUP); // Atur tombol kurang sebagai input dengan pull-up
  pinMode(RELAY_PIN, OUTPUT);         // Atur pin relay sebagai output
  pinMode(TRIG_PIN, OUTPUT);          // Atur pin trigger sensor sebagai output
  pinMode(ECHO_PIN, INPUT);           // Atur pin echo sensor sebagai input

  digitalWrite(RELAY_PIN, HIGH); // Matikan relay pada awalnya (HIGH = off untuk relay aktif-low)

  display.setBrightness(5); // Atur kecerahan display TM1637
  display.showNumberDec(countdownTime, false); // Tampilkan waktu awal di display

  Serial.begin(9600); // Mulai komunikasi serial
  Serial.println("Sistem siap. Menunggu objek...");
}

void loop() {
  // Jika tombol tambah ditekan
  if (digitalRead(BUTTON_UP) == LOW) {
    delay(200); // Debounce sederhana
    countdownTime++; // Tambah waktu countdown
    if (countdownTime > 9999) countdownTime = 9999; // Batas maksimum display 4 digit
    initialTime = countdownTime; // Simpan waktu awal baru
    display.showNumberDec(countdownTime, false); // Tampilkan ke display
    countingDown = false; // Reset status countdown
    started = false;
  }

  // Jika tombol kurang ditekan
  if (digitalRead(BUTTON_DOWN) == LOW) {
    delay(200); // Debounce
    if (countdownTime > 0) countdownTime--; // Kurangi waktu jika masih > 0
    initialTime = countdownTime; // Simpan nilai awal baru
    display.showNumberDec(countdownTime, false); // Tampilkan ke display
    countingDown = false;
    started = false;
  }

  // Mengukur jarak menggunakan sensor ultrasonik
  long duration;
  float distanceCm;

  digitalWrite(TRIG_PIN, LOW);          // Set trigger LOW dulu
  delayMicroseconds(2);                 // Tunggu 2 mikrodetik
  digitalWrite(TRIG_PIN, HIGH);         // Trigger HIGH selama 10 mikrodetik
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);          // Kembali LOW

  duration = pulseIn(ECHO_PIN, HIGH, 30000); // Hitung durasi pantulan, timeout 30ms
  distanceCm = duration * 0.034 / 2;         // Konversi durasi ke jarak dalam cm

  // Tampilkan jarak ke Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  // Jika objek terdeteksi cukup dekat (< 10 cm) dan belum mulai
  if (distanceCm > 0 && distanceCm < 10 && !started && countdownTime > 0) {
    started = true;              // Tandai sudah mulai
    countingDown = true;         // Aktifkan countdown
    previousMillis = millis();   // Catat waktu mulai
    Serial.println("Objek terdeteksi! Memulai countdown...");
  }

  // Proses pengurangan waktu per detik jika countdown aktif
  if (countingDown && millis() - previousMillis >= 1000 && countdownTime > 0) {
    previousMillis = millis();   // Update waktu terakhir
    countdownTime--;             // Kurangi 1 detik

    digitalWrite(RELAY_PIN, LOW);  // Nyalakan relay sebentar
    delay(100);
    digitalWrite(RELAY_PIN, HIGH); // Matikan lagi

    display.showNumberDec(countdownTime, false); // Update display
  }

  // Jika countdown selesai
  if (countdownTime == 0 && countingDown) {
    countingDown = false;             // Matikan status countdown
    started = false;                  // Reset status mulai
    countdownTime = initialTime;      // Kembalikan waktu ke nilai awal
    display.showNumberDec(countdownTime, false); // Tampilkan kembali
    Serial.println("Countdown selesai. Reset ke nilai awal.");
  }

  delay(100); // Delay kecil agar loop tidak terlalu cepat
}
