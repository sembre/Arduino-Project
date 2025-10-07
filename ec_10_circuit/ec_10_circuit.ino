#include <LiquidCrystal.h>
#include <EEPROM.h>

/*
  CARA RESET PASS COUNTER:
  1. Ubah baris: const long MAGIC_KEY = 0x12345678L;
     Menjadi:    const long MAGIC_KEY = 0x87654321L;
  2. Upload kode yang sudah diubah
  3. Ubah kembali ke: const long MAGIC_KEY = 0x12345678L;
  4. Upload lagi
  5. Pass counter akan direset ke 0
*/

// Sambungan pin pins LCD
// LCD 16 X 2 -----> ARDUINO
// 1 -----> GND          11 -----> 9(D4)
// 2 -----> +5V          12 -----> 10(D5)
// 3 -----> 6(CONTRAS)   13 -----> 11(D6)
// 4 -----> 7(RS)        14 -----> 12(D7)
// 5 -----> GND          15 -----> 5(BACKLIGHT)
// 6 -----> 8(E)         16 -----> GND
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); // RS,E,D4,D5,D6,D7

// Pass counter variables - menggunakan EEPROM untuk persistensi
const long MAX_PASS_COUNT = 90000L; // Maksimal pass count (90000 untuk production)
long passCount = 0L;                // Counter pass yang disimpan di EEPROM
bool systemLocked = false;

// Variabel untuk pin 13 trigger counting
bool pin13WasLow = true;                      // Flag untuk deteksi perubahan dari LOW ke HIGH pada pin 13
const unsigned long PASS_TIMER_INTERVAL = 30; // Interval timer (tidak digunakan lagi)

const int EEPROM_PASS_ADDR = 0;           // Alamat EEPROM untuk pass counter (4 bytes)
const int EEPROM_MAGIC_ADDR = 8;          // Alamat EEPROM untuk magic key (4 bytes)
const long MAGIC_KEY = 0x12345678L;       // Magic key untuk deteksi upload ulang
const long RESET_MAGIC_KEY = 0x87654321L; // Magic key khusus untuk reset
// pins SIDE A , SIDE B
int endA[10] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31}; // pins end A 15CZ-6Y No 15,... ,
int endB[10] = {63, 62, 61, 60, 59, 58, 57, 56, 55, 54}; // pins end B 15CZ-6H No 1,... ,

// Membuat variabel untuk LED
int pinLed1 = 42; // KELUARAN BISA DI SAMBUNG DENGAN LED No.1
int pinLed2 = 43;
int pinLed3 = 44;
int pinLed4 = 45;
int pinLed5 = 46;
int pinLed6 = 47;
int pinLed7 = 48;
int pinLed8 = 49;
int pinLed9 = 50;
int pinLed10 = 51; // KELUARAN BISA DI SAMBUNG DENGAN LED No.10

// Membuat variabel untuk Button atau baris S
int pinBtn1 = 32; // STOP NO 1
int pinBtn2 = 33;
int pinBtn3 = 34;
int pinBtn4 = 35;
int pinBtn5 = 36;
int pinBtn6 = 37;
int pinBtn7 = 38;
int pinBtn8 = 39;
int pinBtn9 = 40;
int pinBtn10 = 41;

#define NUMBER (Nu) // hasil hitungan untuk S dari sambungan s ke GND
int Nu = 10;        // tanpa di sambung ke GND hitungan menjadi 10
int result[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int test[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int counter[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
bool fail = false;
int Contrast = 60;
#define LCD_Backlight 5
const int speakerPin = 4;
int frequency;
int POT = 64;
int potVal = analogRead(POT);
const int BUZZER_PIN = 13; // Arduino pin connected to Buzzer's pin

// Fungsi untuk membaca pass count dari EEPROM (menggunakan EEPROM.get)
long readPassCountFromEEPROM()
{
  long count = 0;
  EEPROM.get(EEPROM_PASS_ADDR, count);

  // Jika EEPROM kosong (0xFFFFFFFF) atau nilai negatif, set ke 0
  if (count == 0xFFFFFFFF || count < 0)
  {
    count = 0;
  }

  // Jika nilai terlalu besar (corrupted), reset ke 0
  if (count > MAX_PASS_COUNT * 2)
  {
    count = 0;
  }

  return count;
}

// Fungsi untuk menyimpan pass count ke EEPROM (menggunakan EEPROM.put)
void writePassCountToEEPROM(long count)
{
  EEPROM.put(EEPROM_PASS_ADDR, count);
}

// Fungsi untuk membaca magic key dari EEPROM
long readMagicKeyFromEEPROM()
{
  long key = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, key);
  return key;
}

// Fungsi untuk menyimpan magic key ke EEPROM
void writeMagicKeyToEEPROM(long key)
{
  EEPROM.put(EEPROM_MAGIC_ADDR, key);
}

// Fungsi untuk reset pass counter (hanya bisa dengan upload ulang)
void resetPassCounter()
{
  passCount = 0;
  writePassCountToEEPROM(passCount);
  writeMagicKeyToEEPROM(MAGIC_KEY); // Set magic key normal
  Serial.println("Pass counter has been reset to 0 by code upload");
}

void setup()
{
  // Inisialisasi Serial terlebih dahulu untuk debugging
  Serial.begin(115200);

  // Debug: tampilkan status awal
  Serial.println("=== ARDUINO STARTUP ===");

  // Setup pin button untuk pengecekan reset
  pinMode(pinBtn1, INPUT_PULLUP);

  // Baca magic key dari EEPROM
  long currentMagicKey = readMagicKeyFromEEPROM();
  Serial.print("Magic key from EEPROM: 0x");
  Serial.println(currentMagicKey, HEX);

  // Cek apakah ini adalah upload ulang kode (reset request)
  if (currentMagicKey == RESET_MAGIC_KEY)
  {
    Serial.println("RESET MAGIC KEY DETECTED - Code uploaded with reset request");
    passCount = 0;
    writePassCountToEEPROM(passCount);
    writeMagicKeyToEEPROM(MAGIC_KEY); // Set ke magic key normal
    Serial.println("Pass counter has been RESET to 0 by code upload");
  }
  else if (currentMagicKey != MAGIC_KEY)
  {
    Serial.println("First upload or corrupted EEPROM detected");
    // Ini adalah upload pertama atau EEPROM corrupt
    passCount = 0;
    writePassCountToEEPROM(passCount);
    writeMagicKeyToEEPROM(MAGIC_KEY);
    Serial.println("Pass counter initialized to 0");
  }
  else
  {
    // Normal startup - baca pass count dari EEPROM
    passCount = readPassCountFromEEPROM();
    Serial.println("Normal startup - reading existing pass count");
  }

  Serial.print("Pass count from EEPROM: ");
  Serial.println(passCount);

  // CATATAN: Pass counter hanya bisa direset dengan upload ulang kode
  // Tidak ada tombol atau cara lain untuk reset

  // Inisialisasi pin 13 trigger variables
  pin13WasLow = true;
  Serial.println("Pin 13 trigger initialized for pass counting");

  // Cek apakah sistem sudah terkunci
  if (passCount >= MAX_PASS_COUNT)
  {
    systemLocked = true;
  }

  frequency = map(potVal, 0, 1023, 0, 255); // scalarizing the value of the potentiometer into PWM values
  pinMode(speakerPin, OUTPUT);              // OUT Speaker headphone
  analogWrite(6, Contrast);                 // Pin for LCD_Backlight
  pinMode(LCD_Backlight, OUTPUT);
  analogWrite(LCD_Backlight, 400); // Adjust for LCD_Backlight

  // variabel pinLed menjadi output digunakan untuk hitungan S
  pinMode(pinLed1, OUTPUT); // KELUARAN LED 1-10
  pinMode(pinLed2, OUTPUT);
  pinMode(pinLed3, OUTPUT);
  pinMode(pinLed4, OUTPUT);
  pinMode(pinLed5, OUTPUT);
  pinMode(pinLed6, OUTPUT);
  pinMode(pinLed7, OUTPUT);
  pinMode(pinLed8, OUTPUT);
  pinMode(pinLed9, OUTPUT);
  pinMode(pinLed10, OUTPUT);

  // variabel pinBtn menjadi input
  pinMode(pinBtn1, INPUT_PULLUP); // BARIS S 1-10 satu satu DISAMBUNG DENGAN GND untuk hasilkan angka
  pinMode(pinBtn2, INPUT_PULLUP);
  pinMode(pinBtn3, INPUT_PULLUP);
  pinMode(pinBtn4, INPUT_PULLUP);
  pinMode(pinBtn5, INPUT_PULLUP);
  pinMode(pinBtn6, INPUT_PULLUP);
  pinMode(pinBtn7, INPUT_PULLUP);
  pinMode(pinBtn8, INPUT_PULLUP);
  pinMode(pinBtn9, INPUT_PULLUP);
  pinMode(pinBtn10, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT); // DISAMBUNG DENGAN TRANSISTOR SWITCH KEMUDIAN KE RELAY
  lcd.begin(16, 2);

  // Tampilkan pass count di Serial Monitor
  Serial.println("========================");
  Serial.print("CURRENT Pass Count: ");
  Serial.print(passCount);
  Serial.print("/");
  Serial.println(MAX_PASS_COUNT);
  Serial.println("========================");
  Serial.println("TO RESET PASS COUNTER:");
  Serial.println("1. Change MAGIC_KEY to RESET_MAGIC_KEY in code");
  Serial.println("2. Upload the modified code");
  Serial.println("3. Change back to MAGIC_KEY and upload again");
  Serial.println("========================");

  // Jika sistem terkunci, tampilkan pesan dan masuk ke infinite loop
  if (systemLocked)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("JOB finish");
    lcd.setCursor(0, 1);
    lcd.print("Thanks Bye Bye");
    Serial.println("SYSTEM LOCKED - JOB FINISHED - Pass count reached 90000");

    // Infinite loop - sistem terkunci permanent, hanya bisa direset dengan upload ulang
    while (1)
    {
      delay(1000);
    }
  }
  // setup pins
  for (int i = 0; i < NUMBER; i++)
  {
    pinMode(endA[i], OUTPUT);       // set output pins (end A)
    pinMode(endB[i], INPUT_PULLUP); // set input pins (end B)
  }
}
void loop()
{
  // Reset flag fail untuk loop baru
  fail = false;

  // Jika sistem terkunci, sistem tidak akan masuk ke sini karena infinite loop di setup()
  // Tapi tetap ada pengecekan untuk safety
  if (systemLocked)
  {
    return;
  }

  // Tampilkan pass count di Serial Monitor - tidak mempengaruhi LCD display
  Serial.print("Pass Count: ");
  Serial.print(passCount);
  Serial.print("/");
  Serial.println(MAX_PASS_COUNT);

  int statusBtn1 = digitalRead(pinBtn1); // kondisi S di sambung ke GND
  int statusBtn2 = digitalRead(pinBtn2);
  int statusBtn3 = digitalRead(pinBtn3);
  int statusBtn4 = digitalRead(pinBtn4);
  int statusBtn5 = digitalRead(pinBtn5);
  int statusBtn6 = digitalRead(pinBtn6);
  int statusBtn7 = digitalRead(pinBtn7);
  int statusBtn8 = digitalRead(pinBtn8);
  int statusBtn9 = digitalRead(pinBtn9);
  int statusBtn10 = digitalRead(pinBtn10);

  // Ketika push button tidak ditekan nilainya HIGH/1
  if (statusBtn1 == HIGH)
  {
    digitalWrite(pinLed1, LOW);
  }
  else
  {
    digitalWrite(pinLed1, HIGH);
    Nu = 1;
  }
  if (statusBtn2 == HIGH)
  {
    digitalWrite(pinLed2, LOW);
  }
  else
  {
    digitalWrite(pinLed2, HIGH);
    Nu = 2;
  }
  if (statusBtn3 == HIGH)
  {
    digitalWrite(pinLed3, LOW);
  }
  else
  {
    digitalWrite(pinLed3, HIGH);
    Nu = 3;
  }
  if (statusBtn4 == HIGH)
  {
    digitalWrite(pinLed4, LOW);
  }
  else
  {
    digitalWrite(pinLed4, HIGH);
    Nu = 4;
  }
  if (statusBtn5 == HIGH)
  {
    digitalWrite(pinLed5, LOW);
  }
  else
  {
    digitalWrite(pinLed5, HIGH);
    Nu = 5;
  }
  if (statusBtn6 == HIGH)
  {
    digitalWrite(pinLed6, LOW);
  }
  else
  {
    digitalWrite(pinLed6, HIGH);
    Nu = 6;
  }
  if (statusBtn7 == HIGH)
  {
    digitalWrite(pinLed7, LOW);
  }
  else
  {
    digitalWrite(pinLed7, HIGH);
    Nu = 7;
  }
  if (statusBtn8 == HIGH)
  {
    digitalWrite(pinLed8, LOW);
  }
  else
  {
    digitalWrite(pinLed8, HIGH);
    Nu = 8;
  }
  if (statusBtn9 == HIGH)
  {
    digitalWrite(pinLed9, LOW);
  }
  else
  {
    digitalWrite(pinLed9, HIGH);
    Nu = 9;
  }
  if (statusBtn10 == HIGH)
  {
    digitalWrite(pinLed10, LOW);
  }
  else
  {
    digitalWrite(pinLed10, HIGH);
    Nu = 10;
  }

  String resultS = "";
  // user interface
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cek:");
  lcd.setCursor(6, 0);
  lcd.print(NUMBER);
  lcd.print(" Circuit");
  Serial.print("  Check :  ");
  Serial.print(NUMBER);
  Serial.println(" Circuit");
  lcd.setCursor(0, 1);
  for (int i = 0; i < NUMBER; i++)
  {
    counter[i] = 0;
    for (int j = 0; j < NUMBER; j++)
    {
      digitalWrite(endA[i], LOW); // set all outputs to LOW
    }
    for (int j = 0; j < NUMBER; j++)
    {                                 // check for crossed / open circuits vs closed, good, circuits
      digitalWrite(endA[j], HIGH);    // cek input satu persatu
      test[i] = digitalRead(endB[i]); // read the output
      if (test[i] == 1 && j != i)
      { // crossed or open circuit
        counter[i]++;
        result[i] = 2 * NUMBER + j;
      }
      else if (test[i] == 1 && j == i && result[i] < 2 * NUMBER)
      { // Good, closed circuit
        result[i] = 0;
      }
      digitalWrite(endA[j], LOW);
      // debugging
      /*
        Serial.print("test1 input core  ");
        Serial.print(i);
        Serial.print(" with output core  ");
        Serial.print(j);
        Serial.print(" test =");
        Serial.print(test[i]);
        Serial.print(" counter =");
        Serial.println(counter[i]);*/
    }
    Serial.print("Core ");
    Serial.print(i);
    Serial.print(" Ke ");
    Serial.print(i);
    Serial.print(" result = ");
    if (result[i] == 0)
    {
      Serial.println(" Good");
      resultS += "1";
    }
    else if (counter[i] == NUMBER - 1)
    {
      Serial.println(" Open");
      resultS += "O";
      lcd.setCursor(11, 1);
      lcd.print("=OPEN");
      fail = true;
    }
    else
    {
      Serial.println(" Cross");
      resultS += "X";
      lcd.setCursor(10, 1);
      lcd.print("=CROSS");
      fail = true;
    }
  }
  // Pass counter logic berdasarkan pin 13 trigger (dari LOW ke HIGH)
  if (fail)
  {
    digitalWrite(13, LOW); // Pin 13 MATI BUZZER MATI
    Serial.println("FAILED");
    lcd.setCursor(0, 1);
    lcd.print(resultS);
    noTone(speakerPin);

    // HANYA set pin13WasLow = true saat FAILED (LOW state)
    if (!pin13WasLow)
    {
      pin13WasLow = true; // Reset flag - siap untuk counting berikutnya
      Serial.println("PIN 13 NOW LOW - Ready for next HIGH transition");
    }
  }
  else
  {
    digitalWrite(13, HIGH); // Pin 13 Hidup , BUZZER NYALA
    Serial.println("PASSED");
    lcd.print("     PASSED");
    tone(speakerPin, frequency); // using tone function to generate the tone of the frequency given by POT

    // Pin 13 berubah dari LOW ke HIGH - increment pass counter HANYA SEKALI
    if (pin13WasLow)
    {
      // Ini adalah transisi dari LOW ke HIGH - increment pass counter
      passCount++;
      pin13WasLow = false; // Set flag bahwa pin 13 sekarang HIGH (prevent multiple count)

      Serial.print("PIN 13 LOW->HIGH TRANSITION - Incrementing pass count to: ");
      Serial.println(passCount);

      // Simpan ke EEPROM
      writePassCountToEEPROM(passCount);

      // Cek apakah sudah mencapai batas maksimum
      if (passCount >= MAX_PASS_COUNT)
      {
        Serial.println("MAXIMUM PASS COUNT REACHED - SYSTEM WILL LOCK ON NEXT RESTART");
      }

      // Delay 2 detik setelah increment untuk mencegah multiple counting
      Serial.println("DELAY 2 seconds after counting...");
      delay(1000);
      Serial.println("Delay finished - Ready for next test cycle");
    }
    else
    {
      // Pin 13 masih HIGH dari loop sebelumnya - tidak increment
      Serial.println("PIN 13 STILL HIGH - No increment (already counted this cycle)");
    }
  }

  // Fungsi reset seperti semula
  void (*resetFunc)(void) = 0; // declare reset function @ address 0
  Serial.println("Performing reset");
  delay(600);
  resetFunc(); // call reset
}
