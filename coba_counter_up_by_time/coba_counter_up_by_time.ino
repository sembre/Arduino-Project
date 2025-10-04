#include <Keypad.h>

const unsigned long minuteToMillis = 60000; // Konversi menit ke milidetik
unsigned long interval = 180000; // Interval waktu awal dalam milidetik (3 menit)
unsigned long previousMillis = 0; // Variabel untuk menyimpan waktu sebelumnya
int counter = 0; // Counter awal
String inputInterval = ""; // Variabel untuk menyimpan input interval

const byte ROWS = 4; // Jumlah baris keypad
const byte COLS = 4; // Jumlah kolom keypad

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {A7, A6, A5, A4}; // Pin baris keypad
byte colPins[COLS] = {A3, A2, A1, A0}; // Pin kolom keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(9600);
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == '*') {
      // Mengatur ulang counter ke 0
      counter = 0;
      Serial.println("Counter diatur ulang ke 0.");
    } else if (key == '#') {
      // Menyimpan dan mengatur interval waktu berdasarkan input pengguna
      if (inputInterval.length() > 0) {
        interval = inputInterval.toInt() * minuteToMillis;
        Serial.print("Interval waktu diatur ke ");
        Serial.print(inputInterval);
        Serial.println(" menit.");
        inputInterval = "";
      }
    } else if (isdigit(key)) {
      // Menerima input interval waktu dari pengguna
      inputInterval += key;
    }

    Serial.print("Tombol ditekan: ");
    Serial.println(key);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    counter++;
    Serial.print("Counter saat ini: ");
    Serial.println(counter);
  }
}
