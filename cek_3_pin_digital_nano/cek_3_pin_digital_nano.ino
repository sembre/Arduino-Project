#include <TM1637Display.h>
#include <EEPROM.h>

// =====================================================
// TESTER KONEKTOR 3 PIN + TM1637 + EEPROM BOOT COUNTER
// Arduino Uno R3
//
// Fitur:
// - Scan 2 arah: A -> B dan B -> A
// - OK: buzzer ON, counter OK naik 1x, display GOOd
// - OPEN total: display counter OK
// - OPEN sebagian / CROSS / SHORT: display angka + garis posisi pin
// - Booting tampil AgUS
// - Boot counter tersimpan EEPROM
// - Maksimal booting 1000
// - Jika mencapai 1000 tampil Err8 dan alat terkunci
// - Reset Err8 cukup upload ulang sketch utama ini
// =====================================================

const byte NUM_PINS = 3;

// Sisi A konektor
const byte pinsA[NUM_PINS] = {2, 3, 4};

// Sisi B konektor
const byte pinsB[NUM_PINS] = {5, 6, 7};

// Buzzer
const byte buzzerPin = 8;

// TM1637
const byte TM_CLK = 9;
const byte TM_DIO = 10;
TM1637Display display(TM_CLK, TM_DIO);

// Kecerahan TM1637
const byte DISPLAY_BRIGHTNESS = 4;

// =====================================================
// EEPROM SETTING
// =====================================================

const int EEPROM_MAGIC_ADDR = 0;
const int EEPROM_BOOT_ADDR  = 4;
const int EEPROM_BUILD_ADDR = 20;

const unsigned long EEPROM_MAGIC = 0xA6A5A6A5UL;
const int MAX_BOOT_COUNT = 1000;

// Build stamp otomatis berubah setiap compile/upload ulang
const char BUILD_STAMP[] = __DATE__ " " __TIME__;
const byte BUILD_STAMP_SIZE = 21;

int bootCounter = 0;
bool systemLocked = false;

// Waktu scan
const int scanDelay = 5;

// Serial print
unsigned long lastPrint = 0;
const unsigned long printInterval = 500;

// Counter OK
int okCounter = 0;

// Status sebelumnya
bool previousOK = false;
bool lastOK = false;

// Untuk ganti tampilan error bergantian
unsigned long lastErrorDisplayChange = 0;
const unsigned long errorDisplayInterval = 800;
byte errorDisplayIndex = 0;

// Data error
const byte MAX_ERRORS = 12;
byte errorA[MAX_ERRORS];
byte errorB[MAX_ERRORS];
byte errorCount = 0;

// Segment kosong
const uint8_t SEG_BLANK = 0x00;

// =====================================================
// SEGMENT DISPLAY
// =====================================================

// AgUS
const uint8_t SEG_AGUS[] = {
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,          // A
  SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          // g / G
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,                  // U
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G                   // S
};

// GOOd
const uint8_t SEG_GOOD[] = {
  SEG_A | SEG_C | SEG_D | SEG_B | SEG_F | SEG_G,          // g
  SEG_C | SEG_D | SEG_E | SEG_G,                          // O
  SEG_C | SEG_D | SEG_E | SEG_G,                          // O
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G                   // d
};

// Err8
const uint8_t SEG_ERR8[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                  // E
  SEG_E | SEG_G,                                          // r
  SEG_E | SEG_G,                                          // r
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G   // 8
};

void showAGUS() {
  display.setSegments(SEG_AGUS);
}

void showGOOD() {
  display.setSegments(SEG_GOOD);
}

void showERR8() {
  display.setSegments(SEG_ERR8);
}

// Counter OK tanpa nol depan
void showCounter() {
  display.showNumberDec(okCounter, false);
}

// Boot counter tanpa nol depan
void showBootCounter() {
  display.showNumberDec(bootCounter, false);
}

// Garis posisi pin
uint8_t pinLineSegment(byte pinNumber) {
  if (pinNumber == 1) {
    return SEG_A;     // garis atas
  } 
  else if (pinNumber == 2) {
    return SEG_G;     // garis tengah
  } 
  else if (pinNumber == 3) {
    return SEG_D;     // garis bawah
  }

  return SEG_BLANK;
}

// Tampilkan pasangan pin error dengan posisi dibalik
// Digit 1 = angka B, digit 2 = garis B,
// digit 3 = garis A, digit 4 = angka A.
void showErrorPair(byte aPin, byte bPin) {
  uint8_t segs[4];

  segs[0] = display.encodeDigit(bPin);   // digit 1 angka B
  segs[1] = pinLineSegment(bPin);        // digit 2 garis posisi B
  segs[2] = pinLineSegment(aPin);        // digit 3 garis posisi A
  segs[3] = display.encodeDigit(aPin);   // digit 4 angka A / indeks error

  display.setSegments(segs);
}

// Tambahkan pasangan error, cegah dobel
void addErrorPair(byte aPin, byte bPin) {
  if (aPin < 1 || aPin > 3) return;
  if (bPin < 1 || bPin > 3) return;

  for (byte i = 0; i < errorCount; i++) {
    if (errorA[i] == aPin && errorB[i] == bPin) {
      return;
    }
  }

  if (errorCount < MAX_ERRORS) {
    errorA[errorCount] = aPin;
    errorB[errorCount] = bPin;
    errorCount++;
  }
}

// =====================================================
// BUILD STAMP EEPROM
// Agar setelah upload ulang sketch utama,
// boot counter otomatis reset.
// =====================================================

bool isBuildStampDifferent() {
  for (byte i = 0; i < BUILD_STAMP_SIZE; i++) {
    char savedChar = EEPROM.read(EEPROM_BUILD_ADDR + i);
    char currentChar = BUILD_STAMP[i];

    if (savedChar != currentChar) {
      return true;
    }

    if (currentChar == '\0') {
      break;
    }
  }

  return false;
}

void saveBuildStamp() {
  for (byte i = 0; i < BUILD_STAMP_SIZE; i++) {
    EEPROM.update(EEPROM_BUILD_ADDR + i, BUILD_STAMP[i]);

    if (BUILD_STAMP[i] == '\0') {
      break;
    }
  }
}

void resetBootCounterBecauseNewUpload() {
  bootCounter = 0;
  EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_BOOT_ADDR, bootCounter);
  saveBuildStamp();

  Serial.println("BUILD BARU TERDETEKSI");
  Serial.println("BOOT COUNTER DIRESET OTOMATIS");
}

// =====================================================
// EEPROM BOOT COUNTER
// =====================================================

void loadAndUpdateBootCounter() {
  unsigned long magicValue = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, magicValue);

  bool needReset = false;

  if (magicValue != EEPROM_MAGIC) {
    needReset = true;
  } 
  else if (isBuildStampDifferent()) {
    needReset = true;
  }

  if (needReset) {
    resetBootCounterBecauseNewUpload();
  } 
  else {
    EEPROM.get(EEPROM_BOOT_ADDR, bootCounter);

    if (bootCounter < 0 || bootCounter > 9999) {
      bootCounter = 0;
      EEPROM.put(EEPROM_BOOT_ADDR, bootCounter);
    }
  }

  // Jika sudah mencapai batas dan tidak ada build baru,
  // alat dikunci.
  if (bootCounter >= MAX_BOOT_COUNT) {
    bootCounter = MAX_BOOT_COUNT;
    systemLocked = true;
    return;
  }

  // Setiap dinyalakan / reset, hitung 1
  bootCounter++;
  EEPROM.put(EEPROM_BOOT_ADDR, bootCounter);

  if (bootCounter >= MAX_BOOT_COUNT) {
    systemLocked = true;
  }
}

void bootingMode() {
  showAGUS();
  delay(1500);

  loadAndUpdateBootCounter();

  showBootCounter();
  delay(1500);

  if (systemLocked) {
    showERR8();
    digitalWrite(buzzerPin, LOW);

    Serial.println("=================================");
    Serial.println(" SYSTEM LOCKED");
    Serial.println(" BOOT COUNTER SUDAH MENCAPAI 300");
    Serial.println(" DISPLAY: Err8");
    Serial.println("Upload ulang sketch utama ini untuk reset.");
    Serial.println("=================================");
  }
}

// =====================================================
// SCAN SATU ARAH
// =====================================================

void scanDirection(const byte drivePins[], const byte sensePins[], bool matrix[NUM_PINS][NUM_PINS]) {
  for (byte r = 0; r < NUM_PINS; r++) {
    for (byte c = 0; c < NUM_PINS; c++) {
      matrix[r][c] = false;
    }
  }

  // Semua drive jadi OUTPUT HIGH
  for (byte i = 0; i < NUM_PINS; i++) {
    pinMode(drivePins[i], OUTPUT);
    digitalWrite(drivePins[i], HIGH);
  }

  // Semua sense jadi INPUT_PULLUP
  for (byte i = 0; i < NUM_PINS; i++) {
    pinMode(sensePins[i], INPUT_PULLUP);
  }

  delay(2);

  for (byte r = 0; r < NUM_PINS; r++) {
    for (byte x = 0; x < NUM_PINS; x++) {
      digitalWrite(drivePins[x], HIGH);
    }

    digitalWrite(drivePins[r], LOW);
    delay(scanDelay);

    for (byte c = 0; c < NUM_PINS; c++) {
      if (digitalRead(sensePins[c]) == LOW) {
        matrix[r][c] = true;
      }
    }

    digitalWrite(drivePins[r], HIGH);
  }
}

byte countTotalConnections(bool matrix[NUM_PINS][NUM_PINS]) {
  byte total = 0;

  for (byte r = 0; r < NUM_PINS; r++) {
    for (byte c = 0; c < NUM_PINS; c++) {
      if (matrix[r][c]) {
        total++;
      }
    }
  }

  return total;
}

void setup() {
  Serial.begin(115200);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  display.setBrightness(DISPLAY_BRIGHTNESS);
  display.clear();

  bootingMode();

  if (!systemLocked) {
    showCounter();

    Serial.println("=================================");
    Serial.println(" TESTER KONEKTOR 3 PIN + 2 ARAH");
    Serial.print(" BUILD: ");
    Serial.println(BUILD_STAMP);
    Serial.print(" BOOT COUNTER: ");
    Serial.println(bootCounter);
    Serial.println("=================================");
  }
}

void loop() {
  if (systemLocked) {
    showERR8();
    digitalWrite(buzzerPin, LOW);
    delay(500);
    return;
  }

  bool matrixAB[NUM_PINS][NUM_PINS]; // A -> B
  bool matrixBA[NUM_PINS][NUM_PINS]; // B -> A

  // Scan pertama: A output, B input
  scanDirection(pinsA, pinsB, matrixAB);

  // Scan kedua: B output, A input
  scanDirection(pinsB, pinsA, matrixBA);

  bool isOK = true;
  bool hasOpen = false;
  bool hasCross = false;
  bool hasShort = false;

  errorCount = 0;

  byte totalAB = countTotalConnections(matrixAB);
  byte totalBA = countTotalConnections(matrixBA);
  byte totalConnections = totalAB + totalBA;

  // =====================================================
  // CEK HASIL SCAN A -> B
  // =====================================================

  for (byte a = 0; a < NUM_PINS; a++) {
    byte connectedCount = 0;
    byte connectedB = 255;

    for (byte b = 0; b < NUM_PINS; b++) {
      if (matrixAB[a][b]) {
        connectedCount++;
        connectedB = b;
      }
    }

    if (connectedCount == 0) {
      hasOpen = true;
      isOK = false;

      if (totalConnections > 0) {
        addErrorPair(a + 1, a + 1);
      }
    }

    if (connectedCount > 1) {
      hasShort = true;
      isOK = false;

      for (byte b = 0; b < NUM_PINS; b++) {
        if (matrixAB[a][b]) {
          addErrorPair(a + 1, b + 1);
        }
      }
    }

    if (connectedCount == 1 && connectedB != a) {
      hasCross = true;
      isOK = false;
      addErrorPair(a + 1, connectedB + 1);
    }
  }

  // Cek B menerima lebih dari satu A
  for (byte b = 0; b < NUM_PINS; b++) {
    byte inputCount = 0;

    for (byte a = 0; a < NUM_PINS; a++) {
      if (matrixAB[a][b]) {
        inputCount++;
      }
    }

    if (inputCount > 1) {
      hasShort = true;
      isOK = false;

      for (byte a = 0; a < NUM_PINS; a++) {
        if (matrixAB[a][b]) {
          addErrorPair(a + 1, b + 1);
        }
      }
    }
  }

  // =====================================================
  // CEK HASIL SCAN B -> A
  // matrixBA[b][a]
  // =====================================================

  for (byte b = 0; b < NUM_PINS; b++) {
    byte connectedCount = 0;
    byte connectedA = 255;

    for (byte a = 0; a < NUM_PINS; a++) {
      if (matrixBA[b][a]) {
        connectedCount++;
        connectedA = a;
      }
    }

    if (connectedCount == 0) {
      hasOpen = true;
      isOK = false;

      if (totalConnections > 0) {
        addErrorPair(b + 1, b + 1);
      }
    }

    if (connectedCount > 1) {
      hasShort = true;
      isOK = false;

      for (byte a = 0; a < NUM_PINS; a++) {
        if (matrixBA[b][a]) {
          addErrorPair(a + 1, b + 1);
        }
      }
    }

    if (connectedCount == 1 && connectedA != b) {
      hasCross = true;
      isOK = false;
      addErrorPair(connectedA + 1, b + 1);
    }
  }

  // Cek A menerima lebih dari satu B
  for (byte a = 0; a < NUM_PINS; a++) {
    byte inputCount = 0;

    for (byte b = 0; b < NUM_PINS; b++) {
      if (matrixBA[b][a]) {
        inputCount++;
      }
    }

    if (inputCount > 1) {
      hasShort = true;
      isOK = false;

      for (byte b = 0; b < NUM_PINS; b++) {
        if (matrixBA[b][a]) {
          addErrorPair(a + 1, b + 1);
        }
      }
    }
  }

  // =====================================================
  // VALIDASI OK
  // Harus benar pada dua arah:
  // A1 -> B1 dan B1 -> A1
  // A2 -> B2 dan B2 -> A2
  // A3 -> B3 dan B3 -> A3
  // =====================================================

  if (isOK) {
    for (byte i = 0; i < NUM_PINS; i++) {
      if (matrixAB[i][i] == false || matrixBA[i][i] == false) {
        isOK = false;
        hasOpen = true;

        if (totalConnections > 0) {
          addErrorPair(i + 1, i + 1);
        }
      }
    }

    if (totalAB != 3 || totalBA != 3) {
      isOK = false;
      hasShort = true;
    }
  }

  // =====================================================
  // BUZZER, COUNTER, DISPLAY
  // =====================================================

  if (isOK) {
    digitalWrite(buzzerPin, HIGH);

    showGOOD();

    if (previousOK == false) {
      okCounter++;

      if (okCounter > 9999) {
        okCounter = 0;
      }

      Serial.print("COUNTER OK: ");
      Serial.println(okCounter);
    }

  } else {
    digitalWrite(buzzerPin, LOW);

    if (errorCount > 0) {
      if (millis() - lastErrorDisplayChange >= errorDisplayInterval) {
        lastErrorDisplayChange = millis();
        errorDisplayIndex++;

        if (errorDisplayIndex >= errorCount) {
          errorDisplayIndex = 0;
        }
      }

      showErrorPair(errorA[errorDisplayIndex], errorB[errorDisplayIndex]);

    } else {
      showCounter();
    }
  }

  previousOK = isOK;

  // =====================================================
  // SERIAL MONITOR
  // =====================================================

  if (millis() - lastPrint >= printInterval || isOK != lastOK) {
    lastPrint = millis();
    lastOK = isOK;

    Serial.println();
    Serial.println("Hasil scan A -> B:");

    for (byte a = 0; a < NUM_PINS; a++) {
      Serial.print("A");
      Serial.print(a + 1);
      Serial.print(" -> ");

      bool found = false;

      for (byte b = 0; b < NUM_PINS; b++) {
        if (matrixAB[a][b]) {
          Serial.print("B");
          Serial.print(b + 1);
          Serial.print(" ");
          found = true;
        }
      }

      if (!found) {
        Serial.print("OPEN");
      }

      Serial.println();
    }

    Serial.println("Hasil scan B -> A:");

    for (byte b = 0; b < NUM_PINS; b++) {
      Serial.print("B");
      Serial.print(b + 1);
      Serial.print(" -> ");

      bool found = false;

      for (byte a = 0; a < NUM_PINS; a++) {
        if (matrixBA[b][a]) {
          Serial.print("A");
          Serial.print(a + 1);
          Serial.print(" ");
          found = true;
        }
      }

      if (!found) {
        Serial.print("OPEN");
      }

      Serial.println();
    }

    Serial.print("Status: ");

    if (isOK) {
      Serial.println("OK - GOOD");
    } else {
      if (hasOpen) {
        Serial.print("OPEN ");
      }
      if (hasCross) {
        Serial.print("CROSS/TERBALIK ");
      }
      if (hasShort) {
        Serial.print("SHORT ");
      }
      Serial.println();

      if (errorCount > 0) {
        Serial.println("Pasangan pin bermasalah:");
        for (byte e = 0; e < errorCount; e++) {
          Serial.print("A");
          Serial.print(errorA[e]);
          Serial.print(" -> B");
          Serial.println(errorB[e]);
        }
      }
    }

    Serial.print("Counter OK: ");
    Serial.println(okCounter);
    Serial.print("Boot Counter: ");
    Serial.println(bootCounter);
  }

  delay(100);
}
