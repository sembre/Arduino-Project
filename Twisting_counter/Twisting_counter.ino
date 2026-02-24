#include <Keypad.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define LCD_Backlight 4

// EEPROM
#define EEPROM_ADDR_TARGET_TURNS 0
#define EEPROM_ADDR_VALID_FLAG   2
#define EEPROM_VALID_VALUE       170

// Pin
const int motorPin = A1;
const int startButtonPin = 2;
const int switchPin = 3;

// Variabel
int count = 0;
int targetTurns = 0;
bool motorRunning = false;
bool keypadLocked = false;
bool switchState = HIGH;
bool switchPressed = false;

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
  pinMode(switchPin, INPUT_PULLUP);

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

  // Load EEPROM
  loadFromEEPROM();

  lcd.setCursor(0,0);
  lcd.print("Set Turns:");

  if (targetTurns > 0)
  {
    keypadLocked = true; // 🔒 AUTO LOCK
    lcd.setCursor(11,0);
    lcd.print(targetTurns);
    lcd.setCursor(0,1);
    lcd.print("Ready - Press ▶ ");
  }
  else
  {
    lcd.setCursor(0,1);
    lcd.print("Input & Lock # ");
  }

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

  // SWITCH COUNTER
  bool currentSwitchState = digitalRead(switchPin);

  if (motorRunning)
  {
    if (switchState == HIGH && currentSwitchState == LOW && !switchPressed)
    {
      delay(25);
      if (digitalRead(switchPin) == LOW)
      {
        switchPressed = true;
        count++;

        lcd.setCursor(0,1);
        lcd.print("Turns: ");
        lcd.print(count);
        lcd.print("      ");

        if (count >= targetTurns)
        {
          stopMotor();
        }
      }
    }

    if (currentSwitchState == HIGH)
    {
      switchPressed = false;
    }
  }

  switchState = currentSwitchState;

  // KEYPAD
  char key = keypad.getKey();

  if (key != NO_KEY)
  {
    // RESET TOTAL
    if (key == '*')
    {
      clearEEPROM();
      targetTurns = 0;
      keypadLocked = false;
      motorRunning = false;
      digitalWrite(motorPin, LOW);

      lcd.clear();
      lcd.print("Reset Complete ");
      delay(1500);
      lcd.clear();
      lcd.print("Set Turns:");
      lcd.setCursor(0,1);
      lcd.print("Input & Lock # ");
      return;
    }

    if (!keypadLocked)
    {
      if (key >= '0' && key <= '9')
      {
        if (targetTurns <= 9999)
        {
          targetTurns = targetTurns * 10 + (key - '0');
          lcd.setCursor(11,0);
          lcd.print("    ");
          lcd.setCursor(11,0);
          lcd.print(targetTurns);
          saveToEEPROM(targetTurns);
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
        targetTurns /= 10;
        lcd.setCursor(11,0);
        lcd.print("    ");
        if (targetTurns > 0)
        {
          lcd.setCursor(11,0);
          lcd.print(targetTurns);
        }
        saveToEEPROM(targetTurns);
      }
    }
  }
}

// ========================== MOTOR ==========================
void startMotor()
{
  digitalWrite(motorPin, HIGH);
  motorRunning = true;
  count = 0;
  switchPressed = false;
  switchState = digitalRead(switchPin);

  lcd.setCursor(0,1);
  lcd.print("Turns: 0        ");
}

void stopMotor()
{
  digitalWrite(motorPin, LOW);
  motorRunning = false;
  switchPressed = false;

  // 🔒 TETAP LOCK → PAKAI TARGET LAMA
  keypadLocked = true;

  lcd.setCursor(0,1);
  lcd.print("Completed       ");
  delay(2000);
  lcd.setCursor(0,1);
  lcd.print("Ready - Press ▶ ");
}

// ========================== EEPROM ==========================
void saveToEEPROM(int value)
{
  EEPROM.write(EEPROM_ADDR_TARGET_TURNS, value & 0xFF);
  EEPROM.write(EEPROM_ADDR_TARGET_TURNS + 1, (value >> 8) & 0xFF);
  EEPROM.write(EEPROM_ADDR_VALID_FLAG, EEPROM_VALID_VALUE);
}

void loadFromEEPROM()
{
  if (EEPROM.read(EEPROM_ADDR_VALID_FLAG) == EEPROM_VALID_VALUE)
  {
    int low = EEPROM.read(EEPROM_ADDR_TARGET_TURNS);
    int high = EEPROM.read(EEPROM_ADDR_TARGET_TURNS + 1);
    targetTurns = (high << 8) | low;
    if (targetTurns < 0 || targetTurns > 9999)
      targetTurns = 0;
  }
  else
  {
    targetTurns = 0;
  }
}

void clearEEPROM()
{
  EEPROM.write(EEPROM_ADDR_VALID_FLAG, 0);
}
