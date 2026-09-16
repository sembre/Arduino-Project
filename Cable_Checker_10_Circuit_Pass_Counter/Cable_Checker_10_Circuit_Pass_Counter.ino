#include <LiquidCrystal.h>
#include <EEPROM.h>

/*
  ==========================================================
  CABLE CHECKER 10 CIRCUIT + PASS COUNTER EEPROM
  ==========================================================

  PASS COUNTER:
  - Bertambah satu kali setelah hasil stabil PASSED.
  - Tidak bertambah terus selama kabel masih terpasang.
  - Kabel harus dilepas atau hasil menjadi FAIL sebelum
    kabel berikutnya dapat dihitung.
  - Nilai disimpan di EEPROM.
  - Nilai tidak hilang ketika Arduino dimatikan/restart.

  CARA RESET PASS COUNTER:
  1. Ubah:
       const bool RESET_PASS_COUNTER = false;
     menjadi:
       const bool RESET_PASS_COUNTER = true;

  2. Upload kode ke Arduino.

  3. Kembalikan menjadi:
       const bool RESET_PASS_COUNTER = false;

  4. Upload kembali.

  PENTING:
  Jangan biarkan RESET_PASS_COUNTER = true karena counter
  akan kembali ke 0 setiap Arduino dinyalakan.
*/

// ==========================================================
// PENGATURAN PASS COUNTER
// ==========================================================

const long MAX_PASS_COUNT = 90000L;

// Ubah menjadi true hanya saat ingin reset counter.
const bool RESET_PASS_COUNTER = false;

// Waktu hasil PASS harus stabil sebelum dihitung.
const unsigned long PASS_STABLE_TIME = 300UL;

// Alamat penyimpanan EEPROM.
const int EEPROM_PASS_ADDR  = 0;
const int EEPROM_MAGIC_ADDR = 8;

// Penanda bahwa EEPROM sudah pernah diinisialisasi.
const long MAGIC_KEY = 0x12345678L;

// Nilai counter.
long passCount = 0L;

// Status penguncian setelah mencapai 90000.
bool systemLocked = false;

// Mencegah satu kabel dihitung berulang-ulang.
bool passAlreadyCounted = false;

// Timer kestabilan hasil PASS.
bool passStableTiming = false;
unsigned long passStableStart = 0;

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

// End A menjadi OUTPUT.
const byte endA[10] = {
  22, 23, 24, 25, 26,
  27, 28, 29, 30, 31
};

// End B menjadi INPUT_PULLUP.
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

// Pin dihubungkan ke GND untuk memilih jumlah circuit.
const byte buttonPins[10] = {
  32, 33, 34, 35, 36,
  37, 38, 39, 40, 41
};

// Default pemeriksaan 10 circuit.
int Nu = 10;

// ==========================================================
// BUZZER DAN OUTPUT PASS
// ==========================================================

// Speaker suara.
const byte SPEAKER_PIN = 4;

// Output relay/buzzer PASS.
const byte BUZZER_PIN = 13;

// Output tambahan PASS.
const byte TRIGGER_PIN = 3;

// Potensiometer pengatur suara.
// Pin 64 pada Arduino Mega adalah A10.
const byte POT_PIN = A10;

int frequency = 500;

// ==========================================================
// VARIABEL HASIL PEMERIKSAAN
// ==========================================================

int result[10] = {
  -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1
};

int test[10] = {
  -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1
};

int counter[10] = {
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0
};

bool fail = false;

// ==========================================================
// FUNGSI EEPROM
// ==========================================================

long readPassCountFromEEPROM()
{
  long count = 0;

  EEPROM.get(EEPROM_PASS_ADDR, count);

  // EEPROM kosong atau nilai rusak.
  if (count < 0 || count > MAX_PASS_COUNT)
  {
    count = 0;
  }

  return count;
}

void writePassCountToEEPROM(long count)
{
  /*
    EEPROM.put menggunakan mekanisme update sehingga byte
    yang nilainya sama tidak ditulis ulang.
  */
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
// FUNGSI TAMPILAN LCD
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

  // Membersihkan sisa karakter pada baris.
  while (position < 16)
  {
    lcd.print(' ');
    position++;
  }
}

void displayPassCounter(const char *resultText, const char *statusText)
{
  char line1[17];
  char line2[17];

  snprintf(
    line1,
    sizeof(line1),
    "PASS:%ld/%ld",
    passCount,
    MAX_PASS_COUNT
  );

  snprintf(
    line2,
    sizeof(line2),
    "%s %s",
    resultText,
    statusText
  );

  printLcdLine(0, line1);
  printLcdLine(1, line2);
}

void displayReady()
{
  char line1[17];

  snprintf(
    line1,
    sizeof(line1),
    "PASS:%ld/%ld",
    passCount,
    MAX_PASS_COUNT
  );

  printLcdLine(0, line1);
  printLcdLine(1, "READY");
}

void displayLocked()
{
  printLcdLine(0, "JOB finish");
  printLcdLine(1, "Thanks Bye Bye");
}

// ==========================================================
// FUNGSI MEMATIKAN OUTPUT PASS
// ==========================================================

void stopPassOutput()
{
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(TRIGGER_PIN, LOW);

  noTone(SPEAKER_PIN);
}

// ==========================================================
// FUNGSI PENGUNCIAN SISTEM
// ==========================================================

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
  /*
    Jika tidak ada pin pemilih yang terhubung ke GND,
    jumlah circuit kembali menjadi 10.
  */
  int selectedCircuit = 10;

  for (int i = 0; i < 10; i++)
  {
    int buttonState = digitalRead(buttonPins[i]);

    if (buttonState == LOW)
    {
      digitalWrite(ledPins[i], HIGH);

      // Index 0 berarti 1 circuit.
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
// PEMERIKSAAN KABEL
// ==========================================================

bool runCableTest(
  int circuitCount,
  char *resultText,
  bool &openFound,
  bool &crossFound
)
{
  bool testFailed = false;

  openFound  = false;
  crossFound = false;

  // Semua output dimatikan terlebih dahulu.
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(endA[i], LOW);
  }

  delayMicroseconds(300);

  for (int i = 0; i < circuitCount; i++)
  {
    bool expectedPinHigh = false;
    bool baselineOpen = false;

    int wrongHighCount = 0;

    result[i]  = -1;
    counter[i] = 0;
    test[i]    = -1;

    /*
      TES OPEN:

      Ketika semua End A LOW:
      - Kabel tersambung: End B ikut LOW.
      - Kabel OPEN: End B tetap HIGH karena INPUT_PULLUP.
    */

    int baselineState = digitalRead(endB[i]);

    if (baselineState == HIGH)
    {
      baselineOpen = true;
    }

    /*
      TES GOOD DAN CROSS:

      Satu per satu End A dibuat HIGH.

      Untuk core i:
      - Ketika End A[i] HIGH, End B[i] harus HIGH.
      - Ketika End A selain i HIGH, End B[i] harus LOW.
    */

    for (int j = 0; j < circuitCount; j++)
    {
      digitalWrite(endA[j], HIGH);

      delayMicroseconds(250);

      int inputState = digitalRead(endB[i]);

      test[i] = inputState;

      if (j == i)
      {
        expectedPinHigh = (inputState == HIGH);
      }
      else
      {
        if (inputState == HIGH)
        {
          wrongHighCount++;
        }
      }

      digitalWrite(endA[j], LOW);

      delayMicroseconds(100);
    }

    counter[i] = wrongHighCount;

    // Prioritas pertama: OPEN.
    if (baselineOpen)
    {
      result[i] = -2;
      resultText[i] = 'O';

      openFound = true;
      testFailed = true;
    }
    // GOOD: pin yang benar HIGH dan tidak ada pin salah HIGH.
    else if (expectedPinHigh && wrongHighCount == 0)
    {
      result[i] = 0;
      resultText[i] = '1';
    }
    // Selain itu dianggap CROSS/SHORT.
    else
    {
      result[i] = 20 + wrongHighCount;
      resultText[i] = 'X';

      crossFound = true;
      testFailed = true;
    }
  }

  // Akhir teks.
  resultText[circuitCount] = '\0';

  // Pastikan output kembali LOW setelah tes.
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(endA[i], LOW);
  }

  return testFailed;
}

// ==========================================================
// SETUP
// ==========================================================

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("CABLE CHECKER STARTING"));
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

  // LED dan tombol pemilih circuit.
  for (int i = 0; i < 10; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);

    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // End A dan End B.
  for (int i = 0; i < 10; i++)
  {
    pinMode(endA[i], OUTPUT);
    digitalWrite(endA[i], LOW);

    pinMode(endB[i], INPUT_PULLUP);
  }

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
    Serial.println(F("CHANGE IT BACK TO FALSE AND UPLOAD AGAIN"));
  }
  else if (storedMagicKey != MAGIC_KEY)
  {
    /*
      Upload pertama atau EEPROM belum memiliki magic key.
    */
    passCount = 0L;

    writePassCountToEEPROM(passCount);
    writeMagicKeyToEEPROM(MAGIC_KEY);

    Serial.println(F("EEPROM INITIALIZED"));
  }
  else
  {
    /*
      Startup normal: baca count sebelumnya.
    */
    passCount = readPassCountFromEEPROM();

    Serial.println(F("PASS COUNTER LOADED FROM EEPROM"));
  }

  Serial.print(F("CURRENT PASS COUNT: "));
  Serial.print(passCount);
  Serial.print('/');
  Serial.println(MAX_PASS_COUNT);

  // Cek apakah target sudah tercapai.
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
  // Jika sudah mencapai maksimal, sistem berhenti.
  if (systemLocked)
  {
    stopPassOutput();
    displayLocked();

    delay(1000);
    return;
  }

  // Membaca jumlah circuit yang dipilih.
  updateCircuitSelection();

  // Buffer hasil maksimal 10 karakter + null terminator.
  char resultText[11];

  bool openFound  = false;
  bool crossFound = false;

  // Jalankan pemeriksaan.
  fail = runCableTest(
    Nu,
    resultText,
    openFound,
    crossFound
  );

  // ========================================================
  // HASIL FAIL
  // ========================================================

  if (fail)
  {
    stopPassOutput();

    /*
      Ketika kabel dilepas atau FAIL, latch dibuka kembali.
      Kabel PASS berikutnya dapat dihitung.
    */
    passAlreadyCounted = false;
    passStableTiming   = false;
    passStableStart    = 0;

    if (openFound && crossFound)
    {
      displayPassCounter(resultText, "MIXED");
    }
    else if (openFound)
    {
      displayPassCounter(resultText, "OPEN");
    }
    else
    {
      displayPassCounter(resultText, "CROSS");
    }
  }

  // ========================================================
  // HASIL PASS
  // ========================================================

  else
  {
    // Baca pengaturan frekuensi speaker.
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

    /*
      Counter hanya boleh bertambah jika kabel ini belum
      pernah dihitung.
    */
    if (!passAlreadyCounted)
    {
      /*
        Mulai timer kestabilan PASS.
      */
      if (!passStableTiming)
      {
        passStableStart  = millis();
        passStableTiming = true;

        Serial.println(F("PASS DETECTED - STABILIZING"));
      }

      /*
        Hasil harus PASS selama PASS_STABLE_TIME.
      */
      if (millis() - passStableStart >= PASS_STABLE_TIME)
      {
        passCount++;

        if (passCount > MAX_PASS_COUNT)
        {
          passCount = MAX_PASS_COUNT;
        }

        // Simpan count ke EEPROM.
        writePassCountToEEPROM(passCount);

        // Aktifkan latch agar tidak menghitung berulang.
        passAlreadyCounted = true;
        passStableTiming   = false;
        passStableStart    = 0;

        Serial.println(F("================================"));
        Serial.print(F("NEW PASS COUNT: "));
        Serial.print(passCount);
        Serial.print('/');
        Serial.println(MAX_PASS_COUNT);
        Serial.println(F("================================"));

        displayPassCounter(resultText, "GOOD");

        /*
          Jika mencapai batas maksimum, sistem langsung
          dikunci.
        */
        if (passCount >= MAX_PASS_COUNT)
        {
          delay(500);
          lockSystem();
          return;
        }
      }
    }

    /*
      Jika kabel sudah dihitung, hanya tampilkan hasil.
      Counter tidak bertambah lagi sampai terjadi FAIL.
    */
    displayPassCounter(resultText, "GOOD");
  }

  /*
    Tidak ada resetFunc() di sini.

    Arduino terus berjalan dan nilai latch tetap tersimpan
    selama kabel masih terpasang.
  */
  delay(20);
}
