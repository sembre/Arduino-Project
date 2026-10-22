// =====================================
// BTL AUDIO DETECTOR
// RELAY ACTIVE LOW
// =====================================

#include <EEPROM.h>

const int spkPlus  = A0;
const int spkMinus = A1;

const int relayPin  = 11;
const int relayPin2 = 8;

// LED indikator batas booting
const int ledPin = 2;

// input saklar (dipakai saat alat terkunci)
const int switchPin = 10;

// sensitivitas
const int threshold = 700;

// relay aktif 2 detik
const unsigned long relayTime = 1000;

unsigned long lastDetect = 0;

// batas jumlah booting
const unsigned int maxBoot = 3000;

// alamat EEPROM
const int addrMagic = 0;
const int addrBoot  = 1;
const byte magicValue = 0xA5;

unsigned int bootCount = 0;
bool locked = false;

void setup() {

  pinMode(relayPin, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // relay OFF
  digitalWrite(relayPin, HIGH);
  digitalWrite(relayPin2, HIGH);

  // LED OFF
  digitalWrite(ledPin, LOW);

  pinMode(switchPin, INPUT_PULLUP);

  Serial.begin(115200);

  // EEPROM baru (belum pernah dipakai) -> mulai dari 0
  if (EEPROM.read(addrMagic) != magicValue) {
    EEPROM.write(addrMagic, magicValue);
    EEPROM.put(addrBoot, (unsigned int)0);
  }

  EEPROM.get(addrBoot, bootCount);

  // tambah hitungan booting (berhenti menulis setelah melewati batas)
  if (bootCount <= maxBoot) {
    bootCount++;
    EEPROM.put(addrBoot, bootCount);
  }

  Serial.print("Boot ke: ");
  Serial.println(bootCount);

  if (bootCount > maxBoot) {
    locked = true;
    digitalWrite(ledPin, HIGH);
    Serial.println("Batas booting terlampaui, mode saklar pin 10 -> pin 11");
  }
}

void loop() {

  // alat terkunci: hanya pin 11 mengikuti pin 10
  // pin 10 LOW -> pin 11 LOW, pin 10 HIGH -> pin 11 HIGH
  if (locked) {

    if (digitalRead(switchPin) == LOW) {
      digitalWrite(relayPin, LOW);
    } else {
      digitalWrite(relayPin, HIGH);
    }

    return;
  }

  int maxDiff = 0;

  // ambil banyak sample
  for (int i = 0; i < 100; i++) {

    int A = analogRead(spkPlus);
    int B = analogRead(spkMinus);

    int diff = abs(A - B);

    if (diff > maxDiff) {
      maxDiff = diff;
    }

    delayMicroseconds(200);
  }

  Serial.println(maxDiff);

  // ada suara
  if (maxDiff > threshold) {

    digitalWrite(relayPin, LOW);
    digitalWrite(relayPin2, LOW);

    lastDetect = millis();
  }

  // OFF setelah 2 detik tanpa suara
  if (millis() - lastDetect > relayTime) {

    digitalWrite(relayPin, HIGH);
    digitalWrite(relayPin2, HIGH);
  }
}
