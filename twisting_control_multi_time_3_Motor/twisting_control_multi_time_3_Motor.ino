#include <Keypad.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin definisi untuk motor (aktif-LOW)
#define MOTOR_A 22
#define MOTOR_B 24
#define MOTOR_C 26

// Konstanta logika motor aktif-LOW
#define MOTOR_ACTIVE LOW    // Keluaran LOW menyalakan motor
#define MOTOR_INACTIVE HIGH // Keluaran HIGH mematikan motor

// Pin untuk TM1637
#define CLK 7
#define DIO 6
TM1637Display display(CLK, DIO);

// LCD I2C 20x4
#define LCD_ADDR 0x27 // Ganti ke 0x3F jika modul Anda memakai alamat itu
LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

// Pin untuk start button
#define START_BUTTON 2

// Keypad 4x4 configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {47, 49, 51, 53};
byte colPins[COLS] = {39, 41, 43, 45};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variabel untuk menyimpan waktu motor (dalam ms)
unsigned long motorTimeA = 0;
unsigned long motorTimeB = 0;
unsigned long motorTimeC = 0;

// Variabel input waktu sementara
unsigned long inputTime = 0;
int selectedMotor = 0; // 0: none, 1: A, 2: B, 3: C

// Variabel siklus
unsigned int cycleCount = 0;
unsigned int displayCount = 0; // Counter yang ditampilkan di TM1637 (bisa di-reset)
bool systemRunning = false;

// Tracking untuk mengurangi update LCD berlebihan (opsional)
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 500; // ms

// Animasi LCD saat motor berjalan
const char ANIM_FRAMES[] = {'|', '/', '-', '\\'}; // gunakan escape untuk backslash
uint8_t animIndex = 0;

// Tracking untuk kombinasi tombol *+D reset counter
unsigned long lastStarPress = 0;

// UI Message temporary
String uiMessage = "";
unsigned long uiMessageUntil = 0; // millis deadline
void showMessage(const String &msg, unsigned long durationMs = 1500)
{
  uiMessage = msg;
  uiMessageUntil = millis() + durationMs;
}

// Splash Screen saat boot
void showSplashScreen()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Twisting 2197969-00 ");
  lcd.setCursor(5, 1);
  lcd.print("CONTROLLER");
  lcd.setCursor(2, 3);
  lcd.print("Versi : 3 Motor");
  delay(2000);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Build BY AGUS F");
  lcd.setCursor(1, 2);
  lcd.print("29 September 2025");
  lcd.setCursor(0, 3);
  lcd.print("//github.com/sembre/");
  delay(3000);
  lcd.clear();
}

// Alamat EEPROM
#define EEPROM_CYCLE 0
#define EEPROM_TIME_A 10
#define EEPROM_TIME_B 20
#define EEPROM_TIME_C 30

void setup()
{
  Serial.begin(9600);

  // Inisialisasi pin motor sebagai OUTPUT dan LANGSUNG set INACTIVE (HIGH)
  // untuk mencegah gerakan relay saat power-on
  pinMode(MOTOR_A, OUTPUT);
  digitalWrite(MOTOR_A, MOTOR_INACTIVE);
  pinMode(MOTOR_B, OUTPUT);
  digitalWrite(MOTOR_B, MOTOR_INACTIVE);
  pinMode(MOTOR_C, OUTPUT);
  digitalWrite(MOTOR_C, MOTOR_INACTIVE);

  // Inisialisasi start button dengan INPUT_PULLUP
  pinMode(START_BUTTON, INPUT_PULLUP);

  // Inisialisasi TM1637
  display.setBrightness(7);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  showSplashScreen();

  // Baca data dari EEPROM
  loadFromEEPROM();

  // Pastikan semua motor dalam keadaan OFF (aktif-LOW berarti set HIGH)
  allMotorsOff();

  // Tampilkan siklus saat startup
  updateDisplay();

  Serial.println("System Ready");
  printStatus();
}

void loop()
{
  // Handle keypad input
  handleKeypad();

  // Handle start button
  handleStartButton();

  // Update display setiap 100ms
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 100)
  {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Update LCD terpisah agar tidak terlalu sering
  if (millis() - lastLCDUpdate > LCD_UPDATE_INTERVAL)
  {
    updateLCD();
    lastLCDUpdate = millis();
  }
}

void handleKeypad()
{
  char key = keypad.getKey();

  if (key)
  {
    Serial.print("Key pressed: ");
    Serial.println(key);

    switch (key)
    {
    case 'A':
      selectedMotor = 1;
      inputTime = 0;
      Serial.println("Motor A selected");
      break;

    case 'B':
      selectedMotor = 2;
      inputTime = 0;
      Serial.println("Motor B selected");
      break;

    case 'C':
      selectedMotor = 3;
      inputTime = 0;
      Serial.println("Motor C selected");
      break;

    case '0' ... '9':
      if (selectedMotor != 0)
      {
        inputTime = inputTime * 10 + (key - '0');
        Serial.print("Input time: ");
        Serial.print(inputTime / 1000.0, 1);
        Serial.println(" s");
      }
      break;

    case '#': // Simpan waktu
      if (selectedMotor != 0 && inputTime > 0)
      {
        saveMotorTime();
        inputTime = 0;
        // Kembali ke mode standby langsung
        selectedMotor = 0;
        printStatus();
      }
      break;

    case '*':                   // Hapus semua waktu motor + siapkan reset counter
      lastStarPress = millis(); // Catat waktu tekan *
      clearAllTimes();
      printStatus();
      showMessage("Motor clear. D=total", 3000); // Pesan lebih lama untuk beri waktu tekan D
      break;

    case 'D': // Reset display counter atau total counter jika * ditekan sebelumnya
      // Cek apakah * ditekan dalam 2 detik terakhir
      if (millis() - lastStarPress < 2000)
      {
        // Reset total counter (sudah termasuk displayCount di dalam fungsi)
        resetCycleCounter();
        showMessage("Total count reset!");
      }
      else
      {
        // Reset display counter saja
        displayCount = 0;
        updateDisplay();
        showMessage("Display reset");
      }
      break;
    }
  }
}

void handleStartButton()
{
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(START_BUTTON);

  // Deteksi falling edge (tombol ditekan)
  if (lastButtonState == HIGH && currentButtonState == LOW)
  {
    // Debouncing delay
    delay(50);
    if (digitalRead(START_BUTTON) == LOW)
    {
      startSystem();
    }
  }

  lastButtonState = currentButtonState;
}

void saveMotorTime()
{
  switch (selectedMotor)
  {
  case 1:
    motorTimeA = inputTime;
    EEPROM.put(EEPROM_TIME_A, motorTimeA);
    Serial.println("Motor A time saved");
    break;
  case 2:
    motorTimeB = inputTime;
    EEPROM.put(EEPROM_TIME_B, motorTimeB);
    Serial.println("Motor B time saved");
    break;
  case 3:
    motorTimeC = inputTime;
    EEPROM.put(EEPROM_TIME_C, motorTimeC);
    Serial.println("Motor C time saved");
    break;
  }
}

void clearAllTimes()
{
  motorTimeA = 0;
  motorTimeB = 0;
  motorTimeC = 0;

  EEPROM.put(EEPROM_TIME_A, motorTimeA);
  EEPROM.put(EEPROM_TIME_B, motorTimeB);
  EEPROM.put(EEPROM_TIME_C, motorTimeC);

  Serial.println("All motor times cleared");
}

void resetCycleCounter()
{
  cycleCount = 0;
  displayCount = 0;
  EEPROM.put(EEPROM_CYCLE, cycleCount);
  updateDisplay();
  Serial.println("Cycle counter reset");
}

void startSystem()
{
  if (systemRunning)
    return;

  // Cek apakah ada waktu yang di-set
  if (motorTimeA == 0 && motorTimeB == 0 && motorTimeC == 0)
  {
    Serial.println("No motor times set!");
    return;
  }

  systemRunning = true;
  Serial.println("System STARTED");

  // Cari waktu terlama
  unsigned long maxTime = max(motorTimeA, max(motorTimeB, motorTimeC));

  // Hitung delay untuk masing-masing motor
  unsigned long delayA = maxTime - motorTimeA;
  unsigned long delayB = maxTime - motorTimeB;
  unsigned long delayC = maxTime - motorTimeC;

  Serial.print("Max time: ");
  Serial.print(maxTime / 1000.0, 1);
  Serial.println(" s");

  // Motor dengan waktu terlama menyala pertama (t=0)
  if (motorTimeA == maxTime)
  {
    digitalWrite(MOTOR_A, MOTOR_ACTIVE);
    Serial.println("Motor A ON (t=0)");
  }
  if (motorTimeB == maxTime)
  {
    digitalWrite(MOTOR_B, MOTOR_ACTIVE);
    Serial.println("Motor B ON (t=0)");
  }
  if (motorTimeC == maxTime)
  {
    digitalWrite(MOTOR_C, MOTOR_ACTIVE);
    Serial.println("Motor C ON (t=0)");
  }

  // Motor A dengan delay
  if (motorTimeA > 0 && motorTimeA != maxTime)
  {
    delay(delayA);
    digitalWrite(MOTOR_A, MOTOR_ACTIVE);
    Serial.print("Motor A ON after delay: ");
    Serial.print(delayA / 1000.0, 1);
    Serial.println(" s");
  }

  // Motor B dengan delay
  if (motorTimeB > 0 && motorTimeB != maxTime)
  {
    delay(delayB);
    digitalWrite(MOTOR_B, MOTOR_ACTIVE);
    Serial.print("Motor B ON after delay: ");
    Serial.print(delayB / 1000.0, 1);
    Serial.println(" s");
  }

  // Motor C dengan delay
  if (motorTimeC > 0 && motorTimeC != maxTime)
  {
    delay(delayC);
    digitalWrite(MOTOR_C, MOTOR_ACTIVE);
    Serial.print("Motor C ON after delay: ");
    Serial.print(delayC / 1000.0, 1);
    Serial.println(" s");
  }

  // Tunggu sampai waktu terlama selesai
  delay(maxTime);

  // Matikan semua motor bersamaan
  allMotorsOff();

  // Increment both counters
  cycleCount++;
  if (cycleCount > 9999)
    cycleCount = 9999;
  displayCount++;
  if (displayCount > 9999)
    displayCount = 9999;
  EEPROM.put(EEPROM_CYCLE, cycleCount);

  Serial.println("All motors OFF");
  Serial.print("Cycle completed: ");
  Serial.println(cycleCount);

  systemRunning = false;
}

void allMotorsOff()
{
  digitalWrite(MOTOR_A, MOTOR_INACTIVE);
  digitalWrite(MOTOR_B, MOTOR_INACTIVE);
  digitalWrite(MOTOR_C, MOTOR_INACTIVE);
}

void loadFromEEPROM()
{
  EEPROM.get(EEPROM_CYCLE, cycleCount);
  EEPROM.get(EEPROM_TIME_A, motorTimeA);
  EEPROM.get(EEPROM_TIME_B, motorTimeB);
  EEPROM.get(EEPROM_TIME_C, motorTimeC);

  // Validasi data
  if (cycleCount > 9999)
    cycleCount = 0;
  if (motorTimeA > 600000)
    motorTimeA = 0; // Max 10 menit
  if (motorTimeB > 600000)
    motorTimeB = 0;
  if (motorTimeC > 600000)
    motorTimeC = 0;

  // Initialize display counter sama dengan total counter saat boot
  displayCount = cycleCount;
}

void updateDisplay()
{
  display.showNumberDec(displayCount, false);
}

// Fungsi helper untuk cetak 1 baris dan bersihkan sisa karakter
void lcdPrintLine(uint8_t row, const String &text)
{
  lcd.setCursor(0, row);
  String t = text;
  if (t.length() > 20)
  {
    t = t.substring(0, 20);
  }
  else
  {
    while (t.length() < 20)
      t += ' ';
  }
  lcd.print(t);
}

void updateLCD()
{
  // Helper format waktu
  auto fmtTime = [](unsigned long ms) -> String
  { return String(ms / 1000.0, 1) + "s"; };

  // Prioritas 1: sistem running
  if (systemRunning)
  {
    String l0 = "RUN A:" + fmtTime(motorTimeA) + " B:" + fmtTime(motorTimeB);
    if (l0.length() > 20)
      l0 = l0.substring(0, 20);
    lcdPrintLine(0, l0);

    String l1 = "C:" + fmtTime(motorTimeC) + " Cyc:" + String(cycleCount);
    if (l1.length() > 20)
      l1 = l1.substring(0, 20);
    lcdPrintLine(1, l1);

    String l2 = "Motor aktif...";
    if (l2.length() > 20)
      l2 = l2.substring(0, 20);
    lcdPrintLine(2, l2);

    String l3 = String("Working ") + ANIM_FRAMES[animIndex];
    animIndex = (animIndex + 1) % (sizeof(ANIM_FRAMES));
    lcdPrintLine(3, l3);
    return;
  }

  // Prioritas 2: pesan sementara
  if (uiMessage.length() && millis() < uiMessageUntil)
  {
    lcdPrintLine(0, uiMessage);

    // Cek apakah sedang dalam window * + D untuk reset total
    if (millis() - lastStarPress < 2000)
    {
      unsigned long sisaWaktu = 2000 - (millis() - lastStarPress);
      String l1 = "Sisa:" + String(sisaWaktu / 1000 + 1) + "s utk tekan D";
      if (l1.length() > 20)
        l1 = l1.substring(0, 20);
      lcdPrintLine(1, l1);
      lcdPrintLine(2, "D = Reset Total Count");
      lcdPrintLine(3, "");
    }
    else
    {
      lcdPrintLine(1, "");
      lcdPrintLine(2, "Tekan A/B/C edit");
      lcdPrintLine(3, "#=Simpan *=Hapus");
    }
    return;
  }

  // Prioritas 3: mode editing (selectedMotor != 0) sebelum save
  if (selectedMotor != 0)
  {
    char selChar = (selectedMotor == 1 ? 'A' : (selectedMotor == 2 ? 'B' : 'C'));
    String l0 = String("Edit Motor ") + selChar;
    lcdPrintLine(0, l0);

    // Baris 1: tampilkan ms dan detik dalam kurung: 3200 (3.2s)
    String msStr = String(inputTime); // dalam ms
    String secStr = String(inputTime / 1000.0, 1);
    String l1 = "In:" + msStr + " (" + secStr + "s)"; // contoh: In:3200 (3.2s)
    if (l1.length() > 20)
      l1 = l1.substring(0, 20); // jaga lebar
    lcdPrintLine(1, l1);

    lcdPrintLine(2, "#=Simpan *=Hapus");
    lcdPrintLine(3, "Digit utk detik");
    return;
  }

  // Standby: tampilkan waktu setiap motor dalam format penuh
  String sA = "Timer Motor A:" + fmtTime(motorTimeA);
  String sB = "Timer Motor B:" + fmtTime(motorTimeB);
  String sC = "Timer Motor C:" + fmtTime(motorTimeC);
  if (sA.length() > 20)
    sA = sA.substring(0, 20);
  if (sB.length() > 20)
    sB = sB.substring(0, 20);
  if (sC.length() > 20)
    sC = sC.substring(0, 20);
  lcdPrintLine(0, sA);
  lcdPrintLine(1, sB);
  lcdPrintLine(2, sC);
  String l3 = String("Total Count:") + cycleCount;
  if (l3.length() > 20)
    l3 = l3.substring(0, 20);
  lcdPrintLine(3, l3);
}

void printStatus()
{
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Cycle Count: ");
  Serial.println(cycleCount);
  Serial.print("Motor A Time: ");
  Serial.print(motorTimeA / 1000.0, 1);
  Serial.println(" s");
  Serial.print("Motor B Time: ");
  Serial.print(motorTimeB / 1000.0, 1);
  Serial.println(" s");
  Serial.print("Motor C Time: ");
  Serial.print(motorTimeC / 1000.0, 1);
  Serial.println(" s");
  Serial.println("====================\n");
}
