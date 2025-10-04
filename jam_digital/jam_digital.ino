#include <TM1637Display.h>

// Konfigurasi pin CLK dan DIO pada modul TM1637
#define CLK_PIN A0
#define DIO_PIN A1
#define BUZZER_PIN 13 // Pin untuk buzzer

// Inisialisasi objek TM1637 dengan pin CLK dan DIO
TM1637Display tm1637(CLK_PIN, DIO_PIN);

bool blinkColon = false;
int setHours = 0;
int setMinutes = 0;
int setSeconds = 0;
unsigned long lastUpdateMillis = 0;
int currentBrightness = 7;  // Menyimpan nilai kecerahan saat ini

// Pin untuk switch A, C, dan brightness
const int switchAPin = A2;
const int switchCPin = A3;
const int brightnessPin = A4;

// Array untuk menyimpan waktu-waktu yang akan memicu bunyi buzzer
const int buzzerTimes[][2] = {
  {6, 29},   // Contoh: Bunyi buzzer pada jam 08:00
  {7, 29}, // Contoh: Bunyi buzzer pada jam 12:30
  {8, 29},
  {9, 29},
  {10, 59},
  {11, 59},
  {13, 9},
  {14, 29},
  {15, 49},
  {16, 49},
  // Tambahkan waktu-waktu lain yang diinginkan di sini
};

bool buzzerState = false; // Status buzzer (nyala/mati)
unsigned long buzzerStartTime = 0;
const unsigned long buzzerDuration = 10; // Durasi bunyi buzzer dalam milidetik (5 detik)

void setup() {
  Serial.begin(9600); // Inisialisasi komunikasi serial
  pinMode(BUZZER_PIN, OUTPUT); // Inisialisasi pin buzzer
  // Inisialisasi TM1637
  tm1637.setBrightness(currentBrightness); // Set kecerahan (brightness) awal ke 7

  // Mengatur pin switch dan brightness sebagai input dengan pull-up resistor
  pinMode(switchAPin, INPUT_PULLUP);
  pinMode(switchCPin, INPUT_PULLUP);
  pinMode(brightnessPin, INPUT_PULLUP);
}

void loop() {
  unsigned long currentMillis = millis();

  // Membaca status switch A dan mengatur waktu sesuai dengan tombol yang ditekan
  if (digitalRead(switchAPin) == LOW) {
    setHours = (setHours + 1) % 24;
    delay(200); // Memberikan delay untuk menghindari bouncing pada switch
  }

  // Membaca status switch C dan mengatur waktu sesuai dengan tombol yang ditekan
  if (digitalRead(switchCPin) == LOW) {
    setMinutes = (setMinutes + 1) % 60;
    delay(200); // Memberikan delay untuk menghindari bouncing pada switch
  }

  // Membaca status switch brightness dan mengatur brightness sesuai dengan tombol yang ditekan
  if (digitalRead(brightnessPin) == HIGH) {
    if (currentBrightness != 7) {
      currentBrightness = 7;
      tm1637.setBrightness(currentBrightness);
    }
  } else {
    if (currentBrightness != 1) {
      currentBrightness = 1;
      tm1637.setBrightness(currentBrightness);
    }
  }

  // Menghitung waktu aktual setiap detik
  if (currentMillis - lastUpdateMillis >= 1000) {
    lastUpdateMillis = currentMillis;

    setSeconds += 1;
    if (setSeconds >= 60) {
      setSeconds = 0;
      setMinutes = (setMinutes + 1) % 60;
      if (setMinutes == 0) {
        setHours = (setHours + 1) % 24;
        
        // Memeriksa apakah saat ini jam dan menit sama dengan yang di dalam array
        for (int i = 0; i < sizeof(buzzerTimes) / sizeof(buzzerTimes[0]); i++) {
          if (setHours == buzzerTimes[i][0] && setMinutes == buzzerTimes[i][1]) {
            // Menghidupkan buzzer dan mengatur waktu mulai bunyi buzzer
            buzzerState = true;
            buzzerStartTime = currentMillis;
            digitalWrite(BUZZER_PIN, HIGH);
            break; // Keluar dari loop setelah menghidupkan buzzer
          }
        }
      }
    }
    


    // Menampilkan waktu yang diatur pada TM1637 display
    int displayValue = setHours * 100 + setMinutes;
    if (blinkColon) {
      tm1637.showNumberDecEx(displayValue); // Menampilkan waktu tanpa titik dua tengah
    } else {
      tm1637.showNumberDecEx(displayValue, 0b01000000); // Menampilkan waktu dengan titik dua tengah berkedip
    }
    blinkColon = !blinkColon;

    // Menampilkan waktu di Serial Monitor
    Serial.print("Time: ");
    Serial.print(setHours < 10 ? "0" : "");
    Serial.print(setHours);
    Serial.print(":");
    Serial.print(setMinutes < 10 ? "0" : "");
    Serial.print(setMinutes);
    Serial.print(":");
    Serial.println(setSeconds < 10 ? "0" : "");
    
    // Menampilkan perbandingan waktu saat ini dengan waktu dalam array di Serial Monitor
    for (int i = 0; i < sizeof(buzzerTimes) / sizeof(buzzerTimes[0]); i++) {
      int arrayHours = buzzerTimes[i][0];
      int arrayMinutes = buzzerTimes[i][1];
      if (setHours == arrayHours && setMinutes == arrayMinutes) {
        Serial.println("Buzzer: ON (Matched Time)");
        // Menghidupkan buzzer selama 5 detik saat cocok dengan waktu dalam array
        buzzerState = true;
        buzzerStartTime = currentMillis;
        
        digitalWrite(BUZZER_PIN, HIGH);
        delay(buzzerDuration);
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        Serial.println("Buzzer: OFF (No Match)");
      }
    }
  }

  delay(10); // Delay kecil untuk menghindari bouncing pada switch
}
