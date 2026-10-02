#include <Keypad.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define LCD_Backlight 4

// EEPROM (menyimpan target terakhir yang dikunci dengan #)
#define EEPROM_ADDR_TARGET_TURNS 0
#define EEPROM_ADDR_VALID_FLAG   2
#define EEPROM_VALID_VALUE       170

// Target putaran default: dipakai jika belum ada nilai tersimpan & setelah reset
#define DEFAULT_TURNS 20

// Pin
const int motorPin = A1;
const int startButtonPin = 2;
const int irSensorPin = 3;   // Sensor infrared (OUT modul IR) - pin interrupt INT1

// Sensor Infrared
// Modul IR umum (FC-51 / TCRT5000) output LOW saat mendeteksi objek.
// Jika sensor Anda output HIGH saat mendeteksi, ganti menjadi HIGH.
#define IR_ACTIVE_LEVEL     LOW
// Jeda minimum antar hitungan (ms) untuk menyaring kedip/noise sensor.
// Harus lebih kecil dari waktu 1 putaran motor.
#define IR_MIN_INTERVAL_MS  30

// Variabel
volatile int count = 0;
volatile int targetTurns = 0;
volatile bool motorRunning = false;
bool keypadLocked = false;
volatile unsigned long lastIrPulse = 0;
int lastShownCount = -1;
bool showingDefault = false;   // true = angka di LCD masih nilai default, digit pertama menggantinya

unsigned long lastButtonPress = 0;
const unsigned long buttonDebounceDelay = 23;

// LCD
LiquidCrystal lcd(14, 15, 16, 17, 18, 19);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {12,11,10,9};
byte colPins[COLS] = {8,7,6,5};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ========================== SETUP ==========================
void setup()
{
  pinMode(motorPin, OUTPUT);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(irSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(irSensorPin), irSensorISR,
                  IR_ACTIVE_LEVEL == LOW ? FALLING : RISING);

  analogWrite(LCD_Backlight, 100);

  lcd.begin(16,2);

  lcd.print("Turns Controller");
  lcd.setCursor(0,1);
  lcd.print("TWISTING MACHINE");
  delay(3000);
  lcd.clear();

  lcd.print("Created & Design");
  lcd.setCursor(4,1);
  lcd.print("By Agus F");
  delay(3000);
  lcd.clear();

  // Setiap dinyalakan langsung pakai target terakhir (atau default) & terkunci
  loadFromEEPROM();
  keypadLocked = true; // 🔒 AUTO LOCK

  lcd.setCursor(0,0);
  lcd.print("Set Turns:");
  lcd.setCursor(11,0);
  lcd.print(targetTurns);
  lcd.setCursor(0,1);
  lcd.print("Ready - Press ▶ ");

  Serial.begin(9600);
}

// ========================== LOOP ==========================
void loop()
{
  // START BUTTON
  if (digitalRead(startButtonPin) == LOW && !motorRunning)
  {
    unsigned long now = millis();
    if (now - lastButtonPress > buttonDebounceDelay)
    {
      lastButtonPress = now;

      if (keypadLocked && targetTurns > 0)
      {
        startMotor();
      }
      else
      {
        lcd.setCursor(0,1);
        lcd.print("Lock with #     ");
      }
    }
  }

  // IR SENSOR COUNTER (hitungan dilakukan di interrupt irSensorISR)
  noInterrupts();
  int currentCount = count;
  bool running = motorRunning;
  interrupts();

  if (currentCount != lastShownCount && lastShownCount >= 0)
  {
    lastShownCount = currentCount;
    lcd.setCursor(0,1);
    lcd.print("Turns: ");
    lcd.print(currentCount);
    lcd.print("      ");

    if (!running)   // target tercapai, motor sudah dimatikan di ISR
    {
      stopMotor();
    }
  }

  // KEYPAD
  char key = keypad.getKey();

  if (key != NO_KEY)
  {
    // RESET TOTAL
    if (key == '*')
    {
      motorRunning = false;
      digitalWrite(motorPin, LOW);
      lastShownCount = -1;
      targetTurns = DEFAULT_TURNS;
      showingDefault = true;
      keypadLocked = false;

      lcd.clear();
      lcd.print("Reset Complete ");
      delay(1500);
      lcd.clear();
      lcd.print("Set Turns:");
      lcd.setCursor(11,0);
      lcd.print(targetTurns);
      lcd.setCursor(0,1);
      lcd.print("Input & Lock # ");
      return;
    }

    if (!keypadLocked)
    {
      if (key >= '0' && key <= '9')
      {
        if (showingDefault)   // digit pertama menimpa nilai default
        {
          targetTurns = 0;
          showingDefault = false;
        }

        if (targetTurns <= 999)   // maksimal 4 digit (9999)
        {
          targetTurns = targetTurns * 10 + (key - '0');
          lcd.setCursor(11,0);
          lcd.print("    ");
          lcd.setCursor(11,0);
          lcd.print(targetTurns);
        }
      }

      else if (key == '#')
      {
        if (targetTurns > 0)
        {
          keypadLocked = true;
          saveToEEPROM(targetTurns);
          lcd.setCursor(0,1);
          lcd.print("Ready - Press ▶ ");
        }
      }

      else if (key == 'D')
      {
        showingDefault = false;
        targetTurns /= 10;
        lcd.setCursor(11,0);
        lcd.print("    ");
        if (targetTurns > 0)
        {
          lcd.setCursor(11,0);
          lcd.print(targetTurns);
        }
      }
    }
  }
}

// ========================== MOTOR ==========================
void startMotor()
{
  noInterrupts();
  count = 0;
  lastIrPulse = millis();
  motorRunning = true;
  interrupts();

  lastShownCount = 0;
  digitalWrite(motorPin, HIGH);

  lcd.setCursor(0,1);
  lcd.print("Turns: 0        ");
}

void stopMotor()
{
  digitalWrite(motorPin, LOW);
  motorRunning = false;
  lastShownCount = -1;

  // 🔒 TETAP LOCK → PAKAI TARGET LAMA
  keypadLocked = true;

  lcd.setCursor(0,1);
  lcd.print("Completed       ");
  delay(2000);
  lcd.setCursor(0,1);
  lcd.print("Ready - Press ▶ ");
}

// ========================== SENSOR IR ==========================
// Dipanggil otomatis setiap sensor IR mendeteksi objek (tepi sinyal),
// sehingga tidak ada putaran yang terlewat walau LCD/keypad sedang sibuk.
void irSensorISR()
{
  if (!motorRunning) return;

  unsigned long now = millis();
  if (now - lastIrPulse < IR_MIN_INTERVAL_MS) return;   // filter noise
  lastIrPulse = now;

  count++;
  if (count >= targetTurns)
  {
    digitalWrite(motorPin, LOW);   // matikan motor secepatnya
    motorRunning = false;
  }
}

// ========================== EEPROM ==========================
// EEPROM.update hanya menulis jika nilainya berubah (memperpanjang umur EEPROM)
void saveToEEPROM(int value)
{
  EEPROM.update(EEPROM_ADDR_TARGET_TURNS, value & 0xFF);
  EEPROM.update(EEPROM_ADDR_TARGET_TURNS + 1, (value >> 8) & 0xFF);
  EEPROM.update(EEPROM_ADDR_VALID_FLAG, EEPROM_VALID_VALUE);
}

void loadFromEEPROM()
{
  targetTurns = DEFAULT_TURNS;   // belum ada data → pakai default

  if (EEPROM.read(EEPROM_ADDR_VALID_FLAG) == EEPROM_VALID_VALUE)
  {
    int low = EEPROM.read(EEPROM_ADDR_TARGET_TURNS);
    int high = EEPROM.read(EEPROM_ADDR_TARGET_TURNS + 1);
    int saved = (high << 8) | low;
    if (saved > 0 && saved <= 9999)
      targetTurns = saved;
  }
}
