#include <TM1637Display.h>  // Library untuk TM1637 4-digit 7-segment

// === PIN SETUP ===
const int triggerPin = 2;
const int relayPin = 8;
const int CLK = 3;
const int DIO = 4;

TM1637Display display(CLK, DIO);

// === CUSTOM KARAKTER ===
const uint8_t OFF_chars[] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,       // 'O'
  SEG_A | SEG_E | SEG_F | SEG_G,                       // 'F'
  SEG_A | SEG_E | SEG_F | SEG_G,                       // 'F'
  0x00                                                 // spasi
};

const uint8_t ON_prefix[] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,       // 'O'
  SEG_C | SEG_E | SEG_G                                // 'N'
};

// === WAKTU & DEBOUNCE ===
const unsigned long debounceDelay = 50;
bool timerActive = false;
unsigned long lastDebounceTime = 0;
bool lastTriggerState = HIGH;

void setup() {
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  Serial.begin(9600);
  Serial.println("Sistem Timer ON/OFF dengan Tampilan 7-Segment");

  display.setBrightness(7);
  display.clear();
}

void loop() {
  int reading = digitalRead(triggerPin);

  if (reading != lastTriggerState) {
    lastDebounceTime = millis();
    lastTriggerState = reading;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && !timerActive) {
      timerActive = true;
      digitalWrite(relayPin, HIGH);
      Serial.println("Trigger aktif. Mulai countdown dan ON relay.");

      // Mulai countdown dari 10 ke 0
      for (int i = 10; i >= 0; i--) {
        uint8_t segs[4] = {0};

        // Salin karakter "ON"
        segs[0] = ON_prefix[0];  // 'O'
        segs[1] = ON_prefix[1];  // 'N'

        if (i >= 10) {
          segs[2] = display.encodeDigit(i / 10);
          segs[3] = display.encodeDigit(i % 10);
        } else {
          segs[2] = 0x00;  // Spasi
          segs[3] = display.encodeDigit(i);
        }

        display.setSegments(segs);
        delay(1000);
      }

      // Selesai countdown
      digitalWrite(relayPin, LOW);
      timerActive = false;
      Serial.println("Countdown selesai. Relay OFF.");
      display.setSegments(OFF_chars);  // Tampilkan OFF
    }
  }
}
