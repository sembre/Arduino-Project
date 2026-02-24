#include <TM1637Display.h>
#include <EEPROM.h>

#define CLK 2
#define DIO 3
#define BTN_UP 6 //ASLINYA 4
#define BTN_DOWN 7 //ASLINYA 5

TM1637Display display(CLK, DIO);

int counter = 0;
const int EEPROM_ADDR = 0;

const unsigned long DEBOUNCE = 200;
const unsigned long LONG_PRESS = 2000;

// Variabel UP
bool upLast = HIGH;
unsigned long upPressedTime = 0;

// Variabel DOWN
bool downLast = HIGH;
unsigned long downPressedTime = 0;
bool downLongExecuted = false;

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  display.setBrightness(7);

  EEPROM.get(EEPROM_ADDR, counter);
  updateDisplay();

  Serial.begin(9600);
  Serial.println("System Ready. Counter = " + String(counter));
}

void loop() {
  handleButtonUp();
  handleButtonDown();
}

void handleButtonUp() {
  bool state = digitalRead(BTN_UP);
  unsigned long now = millis();

  // Tekan (HIGH→LOW)
  if (upLast == HIGH && state == LOW) {
    upPressedTime = now;
  }

  // Lepas (LOW→HIGH)
  if (upLast == LOW && state == HIGH) {
    if (now - upPressedTime > DEBOUNCE) {
      counter++;
      updateDisplay();
      saveEEPROM();
      Serial.println("UP: " + String(counter));
    }
  }

  upLast = state;
}

void handleButtonDown() {
  bool state = digitalRead(BTN_DOWN);
  unsigned long now = millis();

  // Tekan (HIGH→LOW)
  if (downLast == HIGH && state == LOW) {
    downPressedTime = now;
    downLongExecuted = false;
  }

  // Tahan tombol
  if (downLast == LOW && state == LOW) {
    if (!downLongExecuted && (now - downPressedTime >= LONG_PRESS)) {
      counter = 0;
      updateDisplay();
      saveEEPROM();
      Serial.println("RESET via long press");
      downLongExecuted = true;
    }
  }

  // Lepas (LOW→HIGH)
  if (downLast == LOW && state == HIGH) {
    if (!downLongExecuted && (now - downPressedTime > DEBOUNCE)) {
      if (counter > 0) counter--;
      updateDisplay();
      saveEEPROM();
      Serial.println("DOWN: " + String(counter));
    }
  }

  downLast = state;
}

void updateDisplay() {
  display.showNumberDec(counter, false);
}

void saveEEPROM() {
  static int lastSaved = -9999;

  // Simpan EEPROM hanya jika beda → lebih awet
  if (counter != lastSaved) {
    EEPROM.put(EEPROM_ADDR, counter);
    lastSaved = counter;
  }
}
