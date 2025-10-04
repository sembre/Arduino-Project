#include <TM1637Display.h>

// Pin solenoid
int solenoidPin = 8;
// Pin potensiometer
int potPin = A0;

// Pin CLK dan DIO untuk TM1637
#define CLK 2
#define DIO 3
TM1637Display display(CLK, DIO);

void setup() {
  // Atur pin solenoid sebagai output
  pinMode(solenoidPin, OUTPUT);

  // Inisialisasi TM1637
  display.setBrightness(0x0f);  // Atur kecerahan maksimum
}

void loop() {
  // Baca nilai potensiometer (0 - 1023)
  int potValue = analogRead(potPin);
  
  // Ubah nilai potensiometer menjadi waktu delay (semakin kecil, semakin cepat getaran)
  int delayTime = map(potValue, 0, 1023, 50, 1000); // Ubah range delay sesuai kebutuhan
  
  // Tampilkan nilai potensiometer pada TM1637
  display.showNumberDec(potValue, false, 4, 0);

  // Hidupkan solenoid
  digitalWrite(solenoidPin, HIGH);
  delay(delayTime);

  // Matikan solenoid
  digitalWrite(solenoidPin, LOW);
  delay(delayTime);
}
