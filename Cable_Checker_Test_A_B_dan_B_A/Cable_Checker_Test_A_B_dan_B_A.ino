#include <LiquidCrystal.h>
#include <EEPROM.h>

/*
  ==========================================================
  CABLE CHECKER 10 CIRCUIT
  TEST DUA ARAH: A -> B DAN B -> A
  PASS COUNTER TERSIMPAN DI EEPROM
  ==========================================================

  HASIL DINYATAKAN PASSED HANYA JIKA:
  1. Pemeriksaan A -> B semuanya GOOD.
  2. Pemeriksaan B -> A semuanya GOOD.

  KODE HASIL:
  1 = GOOD
  O = OPEN
  X = CROSS / SHORT / STACK WIRE

  PASS COUNTER:
  - Bertambah satu kali setelah hasil dua arah stabil PASSED.
  - Tidak bertambah berulang selama kabel masih terpasang.
  - Kabel harus dilepas atau menjadi FAIL sebelum produk
    berikutnya dapat dihitung.
  - Nilai disimpan di EEPROM.
  - Pass counter tidak ditampilkan pada LCD.
  - Pass counter dapat dilihat melalui Serial Monitor 115200.

  CARA RESET PASS COUNTER:
  1. Ubah:
       const bool RESET_PASS_COUNTER = false;
     menjadi:
       const bool RESET_PASS_COUNTER = true;

  2. Upload kode.

  3. Kembalikan lagi menjadi:
       const bool RESET_PASS_COUNTER = false;

  4. Upload kembali.

  PENTING:
  Jangan membiarkan RESET_PASS_COUNTER = true.
*/

// ==========================================================
// PENGATURAN PASS COUNTER
// ==========================================================

const long MAX_PASS_COUNT = 90000L;

// Ubah menjadi true hanya saat ingin mereset pass counter.
const bool RESET_PASS_COUNTER = false;

// Hasil dua arah harus stabil selama waktu ini sebelum dihitung.
const unsigned long PASS_STABLE_TIME = 300UL;

// Interval informasi hasil pada Serial Monitor.
const unsigned long SERIAL_STATUS_INTERVAL = 500UL;

// Alamat EEPROM.
const int EEPROM_PASS_ADDR  = 0;
const int EEPROM_MAGIC_ADDR = 8;

// Penanda EEPROM sudah diinisialisasi.
const long MAGIC_KEY = 0x0L;

// Nilai pass counter.
long passCount = 0L;

// Status sistem.
bool systemLocked = false;
bool passAlreadyCounted = false;

// Timer kestabilan PASS.
bool passStableTiming = false;
unsigned long passStableStart = 0;

// Timer Serial Monitor.
unsigned long lastSerialStatus = 0;

// ==========================================================
// LCD 16 x 2
// ==========================================================

// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

const byte LCD_BACKLIGHT_PIN = 5;
const byte LCD_CONTRAST_PIN  = 6;
const byte LCD_CONTRAST      = 60;

// ==========================================================
// PIN CABLE CHECKER
// ==========================================================

// Sisi A.
const byte endA[10] = {
  22, 23, 24, 25, 26,
  27, 28, 29, 30, 31
};

// Sisi B.
const byte endB[10] = {
  63, 62, 61, 60, 59,
  58, 57, 56, 55, 54
};

// ==========================================================
// PIN LED PEMILIH JUMLAH CIRCUIT
// ==========================================================

const byte ledPins[10] = {
  42, 43, 44, 45, 46,
  47, 48, 49, 50, 51
};

// ==========================================================
// PIN PEMILIH JUMLAH CIRCUIT
// ==========================================================

// Hubungkan pin pilihan ke GND.
const byte buttonPins[10] = {
  32, 33, 34, 35, 36,
  37, 38, 39, 40, 41
};

// Default 10 circuit.
int Nu = 10;

// ==========================================================
// BUZZER DAN OUTPUT PASS
// ==========================================================

const byte SPEAKER_PIN = 4;
const byte BUZZER_PIN  = 13;
const byte TRIGGER_PIN = 3;

// Arduino Mega pin 64 = A10.
const byte POT_PIN = A10;

int frequency = 500;

// ==========================================================
// VARIABEL HASIL PEMERIKSAAN
// ==========================================================

char resultAB[11];
char resultBA[11];
char resultCombined[11];

bool fail = false;

// ==========================================================
// FUNGSI EEPROM
// ==========================================================

long readPassCountFromEEPROM()
{
  long count = 0;

  EEPROM.get(EEPROM_PASS_ADDR, count);

  if (count < 0 || count > MAX_PASS_COUNT)
  {
    count = 0;
  }

  return count;
}

void writePassCountToEEPROM(long count)
{
  EEPROM.put(EEPROM_PASS_ADDR, count);
}

long readMagicKeyFromEEPROM()
{
  long key = 0;

  EEPROM.get(EEPROM_MAGIC_ADDR, key);

  return key;
}

void writeMagicKeyToEEPROM(long key)
{
  EEPROM.put(EEPROM_MAGIC_ADDR, key);
}

void resetPassCounter()
{
  passCount = 0L;

  writePassCountToEEPROM(passCount);
  writeMagicKeyToEEPROM(MAGIC_KEY);

  Serial.println(F("PASS COUNTER RESET TO 0"));
}

// ==========================================================
// FUNGSI LCD
// ==========================================================

void printLcdLine(byte row, const char *text)
{
  lcd.setCursor(0, row);

  byte position = 0;

  while (text[position] != '\0' && position < 16)
  {
    lcd.print(text[position]);
    position++;
  }

  while (position < 16)
  {
    lcd.print(' ');
    position++;
  }
}

void displayTestHeader()
{
  lcd.setCursor(0, 0);
  lcd.print("                ");

  lcd.setCursor(0, 0);
  lcd.print("Cek:");

  lcd.setCursor(6, 0);
  lcd.print(Nu);
  lcd.print(" Circuit");
}

void displayReady()
{
  displayTestHeader();
  printLcdLine(1, "READY");
}

void displayPassResult()
{
  displayTestHeader();
  printLcdLine(1, "     PASSED");
}

void displayFailResult(
  const char *resultText,
  bool openFound,
  bool crossFound
)
{
  displayTestHeader();

  printLcdLine(1, "");

  lcd.setCursor(0, 1);
  lcd.print(resultText);

  if (openFound && crossFound)
  {
    lcd.setCursor(10, 1);
    lcd.print("=MIXED");
  }
  else if (openFound)
  {
    lcd.setCursor(11, 1);
    lcd.print("=OPEN");
  }
  else
  {
    lcd.setCursor(10, 1);
    lcd.print("=CROSS");
  }
}

void displayLocked()
{
  printLcdLine(0, "JOB finish");
  printLcdLine(1, "Thanks Bye Bye");
}

// ==========================================================
// OUTPUT PASS
// ==========================================================

void stopPassOutput()
{
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(TRIGGER_PIN, LOW);

  noTone(SPEAKER_PIN);
}

void lockSystem()
{
  systemLocked = true;

  stopPassOutput();
  displayLocked();

  Serial.println(F("================================"));
  Serial.println(F("MAXIMUM PASS COUNT REACHED"));
  Serial.println(F("SYSTEM LOCKED"));
  Serial.println(F("================================"));
}

// ==========================================================
// PEMILIH JUMLAH CIRCUIT
// ==========================================================

void updateCircuitSelection()
{
  int selectedCircuit = 10;

  for (int i = 0; i < 10; i++)
  {
    int buttonState = digitalRead(buttonPins[i]);

    if (buttonState == LOW)
    {
      digitalWrite(ledPins[i], HIGH);
      selectedCircuit = i + 1;
    }
    else
    {
      digitalWrite(ledPins[i], LOW);
    }
  }

  Nu = selectedCircuit;

  if (Nu < 1)
  {
    Nu = 1;
  }

  if (Nu > 10)
  {
    Nu = 10;
  }
}

// ==========================================================
// PENGATURAN PIN PEMERIKSAAN
// ==========================================================

void makeAllPinsHighImpedance()
{
  for (int i = 0; i < 10; i++)
  {
    // Matikan pull-up sebelum menjadi INPUT biasa.
    digitalWrite(endA[i], LOW);
    digitalWrite(endB[i], LOW);

    pinMode(endA[i], INPUT);
    pinMode(endB[i], INPUT);
  }
}

void prepareDirection(
  const byte sourcePins[],
  const byte sensePins[]
)
{
  /*
    SOURCE:
    - Semua pin dibuat INPUT/high-impedance.
    - Hanya satu pin yang akan menjadi OUTPUT LOW saat dites.

    SENSE:
    - Semua pin menjadi INPUT_PULLUP.
    - Jalur yang tersambung ke source aktif akan terbaca LOW.

    Cara ini lebih aman saat terjadi short karena tidak ada
    dua output dengan level berlawanan yang saling bertabrakan.
  */

  for (int i = 0; i < 10; i++)
  {
    digitalWrite(sourcePins[i], LOW);
    pinMode(sourcePins[i], INPUT);

    pinMode(sensePins[i], INPUT_PULLUP);
  }

  delayMicroseconds(200);
}

// ==========================================================
// TEST SATU ARAH
// ==========================================================

bool runDirectionalTest(
  const byte sourcePins[],
  const byte sensePins[],
  int circuitCount,
  char directionResult[]
)
{
  bool directionFailed = false;

  prepareDirection(sourcePins, sensePins);

  for (int sourceIndex = 0; sourceIndex < circuitCount; sourceIndex++)
  {
    /*
      Pastikan seluruh source kembali high-impedance sebelum
      mengaktifkan source berikutnya.
    */
    for (int i = 0; i < 10; i++)
    {
      digitalWrite(sourcePins[i], LOW);
      pinMode(sourcePins[i], INPUT);
    }

    /*
      Aktifkan satu source dengan OUTPUT LOW.
    */
    digitalWrite(sourcePins[sourceIndex], LOW);
    pinMode(sourcePins[sourceIndex], OUTPUT);

    delayMicroseconds(250);

    int lowCount = 0;
    int wrongLowCount = 0;
    bool expectedDestinationLow = false;

    /*
      Baca seluruh destination.
    */
    for (int destinationIndex = 0;
         destinationIndex < circuitCount;
         destinationIndex++)
    {
      int inputState = digitalRead(sensePins[destinationIndex]);

      if (inputState == LOW)
      {
        lowCount++;

        if (destinationIndex == sourceIndex)
        {
          expectedDestinationLow = true;
        }
        else
        {
          wrongLowCount++;
        }
      }
    }

    /*
      KLASIFIKASI:

      GOOD:
      - Pin tujuan yang benar LOW.
      - Tidak ada pin tujuan lain yang LOW.
      - Hanya satu tujuan yang tersambung.

      OPEN:
      - Tidak ada satu pun pin tujuan yang LOW.

      CROSS / SHORT / STACK:
      - Tujuan yang salah menjadi LOW.
      - Lebih dari satu tujuan menjadi LOW.
      - Tujuan yang benar tidak LOW tetapi ada tujuan lain LOW.
    */

    if (
      expectedDestinationLow &&
      wrongLowCount == 0 &&
      lowCount == 1
    )
    {
      directionResult[sourceIndex] = '1';
    }
    else if (lowCount == 0)
    {
      directionResult[sourceIndex] = 'O';
      directionFailed = true;
    }
    else
    {
      directionResult[sourceIndex] = 'X';
      directionFailed = true;
    }

    // Source aktif dikembalikan menjadi high-impedance.
    digitalWrite(sourcePins[sourceIndex], LOW);
    pinMode(sourcePins[sourceIndex], INPUT);

    delayMicroseconds(100);
  }

  directionResult[circuitCount] = '\0';

  makeAllPinsHighImpedance();

  return directionFailed;
}

// ==========================================================
// GABUNGKAN HASIL A->B DAN B->A
// ==========================================================

bool combineBidirectionalResults(
  int circuitCount,
  bool &openFound,
  bool &crossFound
)
{
  bool combinedFailed = false;

  openFound = false;
  crossFound = false;

  for (int i = 0; i < circuitCount; i++)
  {
    /*
      CROSS mempunyai prioritas lebih tinggi.
      Jika salah satu arah CROSS/SHORT/STACK, hasil X.

      Jika tidak CROSS tetapi salah satu arah OPEN, hasil O.

      Hanya jika kedua arah GOOD, hasil 1.
    */

    if (resultAB[i] == 'X' || resultBA[i] == 'X')
    {
      resultCombined[i] = 'X';

      crossFound = true;
      combinedFailed = true;
    }
    else if (resultAB[i] == 'O' || resultBA[i] == 'O')
    {
      resultCombined[i] = 'O';

      openFound = true;
      combinedFailed = true;
    }
    else
    {
      resultCombined[i] = '1';
    }
  }

  resultCombined[circuitCount] = '\0';

  return combinedFailed;
}

// ==========================================================
// TEST DUA ARAH
// ==========================================================

bool runBidirectionalCableTest(
  int circuitCount,
  bool &openFound,
  bool &crossFound
)
{
  /*
    Tahap 1:
    Source A, sense B.
  */
  bool failAB = runDirectionalTest(
    endA,
    endB,
    circuitCount,
    resultAB
  );

  delayMicroseconds(300);

  /*
    Tahap 2:
    Source B, sense A.
  */
  bool failBA = runDirectionalTest(
    endB,
    endA,
    circuitCount,
    resultBA
  );

  delayMicroseconds(300);

  bool combinedFail = combineBidirectionalResults(
    circuitCount,
    openFound,
    crossFound
  );

  /*
    combinedFail sebenarnya sudah mewakili kedua hasil.
    failAB/failBA tetap digunakan agar hasil arah mana pun
    yang gagal pasti dianggap gagal.
  */
  return combinedFail || failAB || failBA;
}

// ==========================================================
// SERIAL MONITOR
// ==========================================================

void printTestStatusToSerial()
{
  if (millis() - lastSerialStatus < SERIAL_STATUS_INTERVAL)
  {
    return;
  }

  lastSerialStatus = millis();

  Serial.println(F("--------------------------------"));
  Serial.print(F("Circuit : "));
  Serial.println(Nu);

  Serial.print(F("A -> B  : "));
  Serial.println(resultAB);

  Serial.print(F("B -> A  : "));
  Serial.println(resultBA);

  Serial.print(F("Gabungan: "));
  Serial.println(resultCombined);

  Serial.print(F("Pass Count: "));
  Serial.print(passCount);
  Serial.print('/');
  Serial.println(MAX_PASS_COUNT);
}

// ==========================================================
// SETUP
// ==========================================================

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("CABLE CHECKER BIDIRECTIONAL"));
  Serial.println(F("TEST A->B AND B->A"));
  Serial.println(F("================================"));

  // LCD.
  lcd.begin(16, 2);

  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  pinMode(LCD_CONTRAST_PIN, OUTPUT);

  analogWrite(LCD_BACKLIGHT_PIN, 255);
  analogWrite(LCD_CONTRAST_PIN, LCD_CONTRAST);

  printLcdLine(0, "SYSTEM STARTING");
  printLcdLine(1, "PLEASE WAIT");

  // Speaker dan output PASS.
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);

  stopPassOutput();

  // LED dan tombol pilihan circuit.
  for (int i = 0; i < 10; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);

    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Kondisi aman awal untuk seluruh pin pemeriksaan.
  makeAllPinsHighImpedance();

  // ========================================================
  // INISIALISASI EEPROM
  // ========================================================

  long storedMagicKey = readMagicKeyFromEEPROM();

  Serial.print(F("EEPROM MAGIC KEY: 0x"));
  Serial.println(storedMagicKey, HEX);

  if (RESET_PASS_COUNTER)
  {
    resetPassCounter();

    Serial.println(F("RESET_PASS_COUNTER = TRUE"));
    Serial.println(F("CHANGE IT BACK TO FALSE"));
    Serial.println(F("THEN UPLOAD AGAIN"));
  }
  else if (storedMagicKey != MAGIC_KEY)
  {
    passCount = 0L;

    writePassCountToEEPROM(passCount);
    writeMagicKeyToEEPROM(MAGIC_KEY);

    Serial.println(F("EEPROM INITIALIZED"));
  }
  else
  {
    passCount = readPassCountFromEEPROM();

    Serial.println(F("PASS COUNTER LOADED FROM EEPROM"));
  }

  Serial.print(F("CURRENT PASS COUNT: "));
  Serial.print(passCount);
  Serial.print('/');
  Serial.println(MAX_PASS_COUNT);

  if (passCount >= MAX_PASS_COUNT)
  {
    lockSystem();
  }
  else
  {
    displayReady();
  }

  delay(1000);
}

// ==========================================================
// LOOP
// ==========================================================

void loop()
{
  if (systemLocked)
  {
    stopPassOutput();
    makeAllPinsHighImpedance();
    displayLocked();

    delay(1000);
    return;
  }

  // Baca pilihan jumlah circuit.
  updateCircuitSelection();

  bool openFound = false;
  bool crossFound = false;

  /*
    Jalankan test A->B lalu B->A.
  */
  fail = runBidirectionalCableTest(
    Nu,
    openFound,
    crossFound
  );

  printTestStatusToSerial();

  // ========================================================
  // HASIL FAIL
  // ========================================================

  if (fail)
  {
    stopPassOutput();

    /*
      Setelah OPEN/CROSS/FAIL, sistem siap menghitung produk
      PASS berikutnya.
    */
    passAlreadyCounted = false;
    passStableTiming = false;
    passStableStart = 0;

    displayFailResult(
      resultCombined,
      openFound,
      crossFound
    );
  }

  // ========================================================
  // HASIL PASS DUA ARAH
  // ========================================================

  else
  {
    int potValue = analogRead(POT_PIN);

    frequency = map(
      potValue,
      0,
      1023,
      100,
      2000
    );

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(TRIGGER_PIN, HIGH);

    tone(SPEAKER_PIN, frequency);

    if (!passAlreadyCounted)
    {
      if (!passStableTiming)
      {
        passStableStart = millis();
        passStableTiming = true;

        Serial.println(F("PASS 2 ARAH - STABILIZING"));
      }

      if (millis() - passStableStart >= PASS_STABLE_TIME)
      {
        passCount++;

        if (passCount > MAX_PASS_COUNT)
        {
          passCount = MAX_PASS_COUNT;
        }

        writePassCountToEEPROM(passCount);

        passAlreadyCounted = true;
        passStableTiming = false;
        passStableStart = 0;

        Serial.println(F("================================"));
        Serial.println(F("BIDIRECTIONAL TEST PASSED"));
        Serial.print(F("NEW PASS COUNT: "));
        Serial.print(passCount);
        Serial.print('/');
        Serial.println(MAX_PASS_COUNT);
        Serial.println(F("================================"));

        displayPassResult();

        if (passCount >= MAX_PASS_COUNT)
        {
          delay(500);
          lockSystem();
          return;
        }
      }
    }

    /*
      LCD tetap seperti sebelumnya dan tidak menampilkan
      pass counter.
    */
    displayPassResult();
  }

  delay(20);
}
