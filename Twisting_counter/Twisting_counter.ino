// Version: Agustus 2025

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#define LCD_Backlight 4

// EEPROM Addresses
#define EEPROM_ADDR_TARGET_TURNS 0 // Alamat untuk menyimpan target turns (2 bytes untuk int)
#define EEPROM_ADDR_VALID_FLAG 2   // Flag untuk validasi data (1 byte)
#define EEPROM_VALID_VALUE 170     // Nilai magic untuk validasi (0xAA)
// Pin konfigurasi
const int motorPin = A1;      // Pin untuk motor
const int startButtonPin = 2; // Pin untuk tombol start
const int switchPin = 3;      // Pin untuk saklar penghitung putaran

int count = 0;                                 // Variabel untuk menyimpan jumlah putaran saat ini
int targetTurns = 0;                           // Jumlah putaran yang diinput dari keypad
bool motorRunning = false;                     // Status motor
bool keypadLocked = false;                     // Status penguncian keypad
bool switchState = HIGH;                       // Status terakhir dari saklar
bool switchPressed = false;                    // Flag untuk mencegah multiple counting
unsigned long lastButtonPress = 0;             // Untuk debouncing tombol start
const unsigned long buttonDebounceDelay = 25; // 25ms debounce delay

// Setup LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(14, 15, 16, 17, 18, 19); // Sesuaikan pin LCD dengan rangkaian Anda

// Setup keypad (4x4)
const byte ROWS = 4; // 4 baris
const byte COLS = 4; // 4 kolom
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {12, 11, 10, 9}; // Pin untuk baris keypad
byte colPins[COLS] = {8, 7, 6, 5};    // Pin untuk kolom keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup()
{
  pinMode(motorPin, OUTPUT);             // Set pin motor sebagai output
  pinMode(startButtonPin, INPUT_PULLUP); // Set pin tombol sebagai input
  pinMode(switchPin, INPUT_PULLUP);      // Set pin saklar sebagai input
  analogWrite(LCD_Backlight, 100);       // Adjust for LCD_Backlight

  // Tampilkan pesan "Created & Design By Agus F" selama 5 detik

  lcd.begin(16, 2); // Inisialisasi LCD 16x2

  lcd.setCursor(0, 0);
  lcd.print("Turns Controller");
  lcd.setCursor(0, 1);
  lcd.print("TWISTING MACHINE");
  delay(5000); // Tunda selama 5 detik
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Created & Design");
  lcd.setCursor(4, 1);
  lcd.print("By Agus F");
  delay(5000); // Tunda selama 5 detik
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Agustus 2025 ");
  lcd.setCursor(0, 1);
  lcd.print("-----------Rev:3");
  delay(3000); // Tunda selama 2 detik
  lcd.clear();

  // Load data terakhir dari EEPROM
  loadFromEEPROM();

  lcd.print("Set Turns: "); // Pesan awal pada LCD

  // Tampilkan nilai terakhir jika ada
  if (targetTurns > 0)
  {
    lcd.setCursor(11, 0);
    lcd.print(targetTurns);
    lcd.setCursor(0, 1);
    lcd.print("Last: ");
    lcd.print(targetTurns);
    delay(3000); // Tampilkan selama 3 detik
    lcd.setCursor(0, 1);
    lcd.print("D=Del *=Clear   ");
    Serial.print("Nilai terakhir: ");
    Serial.println(targetTurns);
    Serial.println("Gunakan: Angka=input, D=hapus digit, #=lock, *=reset");
  }
  else
  {
    lcd.setCursor(0, 1);
    lcd.print("D=Del *=Clear   ");
    Serial.println("Gunakan: Angka=input, D=hapus digit, #=lock, *=reset");
  }

  Serial.begin(9600);
  Serial.println("Masukkan jumlah putaran menggunakan keypad:");
}

void loop()
{

  // Baca tombol start dengan debouncing
  if (digitalRead(startButtonPin) == LOW && !motorRunning)
  {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress > buttonDebounceDelay)
    {
      lastButtonPress = currentTime;

      // Pastikan motor hanya bisa mulai jika keypad sudah dikunci (dengan tombol '#')
      if (keypadLocked && targetTurns > 0)
      {
        startMotor();
      }
      else if (!keypadLocked)
      {
        Serial.println("Keypad belum dikunci. Tekan '#' untuk memulai motor.");
        lcd.setCursor(0, 1);
        lcd.print("Lock first (#)  ");
      }
      else if (targetTurns == 0)
      {
        Serial.println("Set target turns terlebih dahulu.");
        lcd.setCursor(0, 1);
        lcd.print("Set turns first ");
      }
    }
  }

  // Baca saklar untuk menghitung putaran dengan debouncing yang lebih baik
  bool currentSwitchState = digitalRead(switchPin);

  if (motorRunning)
  {
    // Deteksi transisi dari HIGH ke LOW (falling edge)
    if (switchState == HIGH && currentSwitchState == LOW && !switchPressed)
    {
      delay(25); // Debounce delay yang lebih panjang

      // Konfirmasi bahwa saklar masih dalam keadaan LOW setelah delay
      if (digitalRead(switchPin) == LOW)
      {
        switchPressed = true; // Set flag untuk mencegah counting berulang
        count++;
        Serial.print("Putaran saat ini: ");
        Serial.println(count);

        // Tampilkan putaran saat ini di LCD
        lcd.setCursor(0, 1); // Pindah ke baris kedua
        lcd.print("Turns: ");
        lcd.print(count);
        lcd.print("        "); // Clear sisa karakter

        // Matikan motor jika jumlah putaran sudah mencapai target
        if (count >= targetTurns)
        {
          stopMotor();
        }
      }
    }

    // Reset flag ketika saklar kembali ke HIGH
    if (currentSwitchState == HIGH && switchPressed)
    {
      switchPressed = false;
    }
  }

  // Update state saklar
  switchState = currentSwitchState;

  // Input dari keypad
  char key = keypad.getKey();

  if (key != NO_KEY)
  {
    if (key == '*')
    {
      // Reset semua variabel dan status
      clearEEPROM(); // Hapus data EEPROM
      targetTurns = 0;
      count = 0;
      keypadLocked = false;
      motorRunning = false;
      switchPressed = false;
      digitalWrite(motorPin, LOW); // Pastikan motor mati

      Serial.println("Sistem direset dan EEPROM dihapus. Masukkan jumlah putaran baru.");
      lcd.setCursor(11, 0);
      lcd.print("     "); // Bersihkan tampilan target turns
      lcd.setCursor(0, 1);
      lcd.print("Reset & Cleared ");
      delay(1000);
      lcd.setCursor(0, 1);
      lcd.print("Set Turns:      "); // Pesan untuk memasukkan kembali
    }

    // Input jumlah putaran hanya jika keypad belum terkunci
    if (!keypadLocked)
    {
      if (key >= '0' && key <= '9')
      {
        // Batasi input maksimum 4 digit (9999)
        if (targetTurns <= 999)
        {
          targetTurns = targetTurns * 10 + (key - '0');
          Serial.print("Jumlah putaran yang diinginkan: ");
          Serial.println(targetTurns);

          // Tampilkan jumlah putaran diinginkan di LCD
          lcd.setCursor(11, 0); // Pindah ke posisi di baris pertama
          lcd.print("    ");    // Clear previous value
          lcd.setCursor(11, 0);
          lcd.print(targetTurns);

          // Auto-save setiap input angka (untuk backup)
          saveToEEPROM(targetTurns);
        }
        else
        {
          Serial.println("Maksimum 4 digit (9999)");
          lcd.setCursor(0, 1);
          lcd.print("Max 4 digits    ");
          delay(1000);
          lcd.setCursor(0, 1);
          lcd.print("                ");
        }
      }
      else if (key == '#')
      {
        // Kunci input keypad setelah tombol '#' ditekan
        if (targetTurns > 0)
        {
          keypadLocked = true;
          saveToEEPROM(targetTurns); // Simpan ke EEPROM saat lock
          Serial.println("Keypad dikunci dan data disimpan.");
          lcd.setCursor(0, 1);
          lcd.print("Locked & Saved   ");
        }
        else
        {
          Serial.println("Masukkan jumlah putaran sebelum mengunci.");
          lcd.setCursor(0, 1);
          lcd.print("Set turns first   ");
        }
      }
      else if (key == 'D')
      {
        // Tombol 'D' untuk backspace (hapus digit terakhir)
        if (targetTurns > 0)
        {
          targetTurns = targetTurns / 10;
          Serial.print("Digit terakhir dihapus. Nilai saat ini: ");
          Serial.println(targetTurns);

          // Update tampilan LCD
          lcd.setCursor(11, 0);
          lcd.print("     "); // Clear area
          if (targetTurns > 0)
          {
            lcd.setCursor(11, 0);
            lcd.print(targetTurns);
          }

          // Auto-save perubahan
          saveToEEPROM(targetTurns);
        }
      }
    }
  }
}

void startMotor()
{
  digitalWrite(motorPin, HIGH); // Nyalakan motor
  motorRunning = true;
  count = 0;                            // Reset hitungan putaran
  switchPressed = false;                // Reset flag saklar
  switchState = digitalRead(switchPin); // Initialize switch state
  Serial.println("Motor mulai berjalan...");
  lcd.setCursor(0, 1);           // Tampilkan pesan di baris kedua
  lcd.print("Turns: 0        "); // Kosongkan jumlah putaran awal
}

void stopMotor()
{
  digitalWrite(motorPin, LOW); // Matikan motor
  motorRunning = false;
  switchPressed = false; // Reset flag saklar
  keypadLocked = false;  // Unlock keypad untuk setting baru
  Serial.println("Motor berhenti.");
  Serial.print("Total putaran: ");
  Serial.println(count);
  lcd.setCursor(0, 1);
  lcd.print("Completed       ");
  delay(2000); // Tampilkan pesan selesai selama 2 detik
  lcd.setCursor(0, 1);
  lcd.print("Set new turns   ");
}

// Fungsi untuk menyimpan target turns ke EEPROM
void saveToEEPROM(int value)
{
  // Simpan nilai target turns (2 bytes)
  EEPROM.write(EEPROM_ADDR_TARGET_TURNS, value & 0xFF);            // Low byte
  EEPROM.write(EEPROM_ADDR_TARGET_TURNS + 1, (value >> 8) & 0xFF); // High byte

  // Simpan flag validasi
  EEPROM.write(EEPROM_ADDR_VALID_FLAG, EEPROM_VALID_VALUE);

  Serial.print("Data disimpan ke EEPROM: ");
  Serial.println(value);
}

// Fungsi untuk membaca target turns dari EEPROM
void loadFromEEPROM()
{
  // Cek flag validasi terlebih dahulu
  if (EEPROM.read(EEPROM_ADDR_VALID_FLAG) == EEPROM_VALID_VALUE)
  {
    // Baca nilai target turns (2 bytes)
    int lowByte = EEPROM.read(EEPROM_ADDR_TARGET_TURNS);
    int highByte = EEPROM.read(EEPROM_ADDR_TARGET_TURNS + 1);
    targetTurns = (highByte << 8) | lowByte;

    // Validasi range nilai (0-9999)
    if (targetTurns < 0 || targetTurns > 9999)
    {
      targetTurns = 0;
    }
    else
    {
      Serial.print("Data dibaca dari EEPROM: ");
      Serial.println(targetTurns);
    }
  }
  else
  {
    targetTurns = 0;
    Serial.println("Tidak ada data valid di EEPROM");
  }
}

// Fungsi untuk menghapus data EEPROM
void clearEEPROM()
{
  EEPROM.write(EEPROM_ADDR_VALID_FLAG, 0);
  targetTurns = 0;
  Serial.println("Data EEPROM dihapus");
}
