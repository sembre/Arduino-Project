// =====================================
// BTL AUDIO DETECTOR
// RELAY ACTIVE LOW
// =====================================

const int spkPlus  = A0;
const int spkMinus = A1;

const int relayPin = 8;

// sensitivitas
const int threshold = 700;

// relay aktif 2 detik
const unsigned long relayTime = 1000;

unsigned long lastDetect = 0;

void setup() {

  pinMode(relayPin, OUTPUT);

  // relay OFF
  digitalWrite(relayPin, HIGH);

  Serial.begin(9600);
}

void loop() {

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

    lastDetect = millis();
  }

  // OFF setelah 2 detik tanpa suara
  if (millis() - lastDetect > relayTime) {

    digitalWrite(relayPin, HIGH);
  }
}
