#include <Wire.h>
#include <LiquidCrystal.h>

#define encoder0PinA 3
#define encoder0PinB 2
#define encoder0Btn 4
#define ledMajuPin 51
#define ledMundurPin 53
#define resetPin A13
byte arrowRight[8] = {
  B00100,
  B00110,
  B11111,
  B11110,
  B11111,
  B00110,
  B00100,
  B00000
};

byte arrowLeft[8] = {
  B00100,
  B01100,
  B11111,
  B01111,
  B11111,
  B01100,
  B00100,
  B00000
};
const int rs = 8, en = 9, d4 = 10, d5 = 11, d6 = 12, d7 = 13;
const int LCD_Backlight = A14;
const int resetCounterClearPin = A15;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const float wheelDia = 65.0;
const float singleTick = (wheelDia * PI) / 400;

long encoder0Pos = 0;
float valRotary = 0.0;
float lastValRotary = 0.0;
float totalDistance = 0.0;
int resetCounter = 0;
const int buzzerPin = 7; // Definisikan pin untuk buzzer

const int LED_ON_INTERVAL = 10;  // Interval menyala (dalam singleticks)
const int LED_OFF_INTERVAL = 20; // Interval mati (dalam singleticks)
int onCounter = 0; // Variabel untuk menghitung 3 singletick menyala
int offCounter = 0; // Variabel untuk menghitung 10 singletick mati
bool ledOn = false; // Status LED (true jika menyala, false jika mati)

void setup() {
  lcd.begin(20, 4);
  lcd.createChar(1, arrowRight);
  lcd.createChar(2, arrowLeft);
  pinMode(resetCounterClearPin, INPUT);
  pinMode(buzzerPin, OUTPUT); // Set pin buzzer sebagai output
  digitalWrite(buzzerPin, LOW); // Matikan buzzer pada awalnya

  pinMode(LCD_Backlight, OUTPUT);
  analogWrite(LCD_Backlight, 200);
  Serial.begin(9600);
  pinMode(encoder0PinA, INPUT_PULLUP);
  pinMode(encoder0PinB, INPUT_PULLUP);
  pinMode(encoder0Btn, INPUT_PULLUP);
  pinMode(ledMajuPin, OUTPUT);
  pinMode(ledMundurPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(resetCounterClearPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoder0PinA), doEncoder, CHANGE);
}

void loop() {
  int btn = digitalRead(encoder0Btn);
  Serial.print(btn);
  Serial.print(" ");
  Serial.print(valRotary);

  if (valRotary > lastValRotary) {
    Serial.print("  CW");
  } else if (valRotary < lastValRotary) {
    Serial.print("  CCW");
  } else {
  }

  // Perbarui LED setiap 10 singletick untuk kedua interval menyala dan mati
  if (ledOn) {
    onCounter++;
    if (onCounter >= LED_ON_INTERVAL) {
      onCounter = 0;
      ledOn = false;
      digitalWrite(ledMajuPin, LOW); // Matikan LED merah
      digitalWrite(ledMundurPin, LOW); // Matikan LED biru
    }
  } else {
    offCounter++;
    if (offCounter >= LED_OFF_INTERVAL) {
      offCounter = 0;
      ledOn = true;
      if (valRotary > lastValRotary) {
        digitalWrite(ledMajuPin, HIGH); // Nyalakan LED merah saat gerakan maju
        digitalWrite(ledMundurPin, LOW);
      } else if (valRotary < lastValRotary) {
        digitalWrite(ledMajuPin, LOW);
        digitalWrite(ledMundurPin, HIGH); // Nyalakan LED biru saat gerakan mundur
      } else {
        digitalWrite(ledMajuPin, LOW);
        digitalWrite(ledMundurPin, LOW);
      }
    }
  }

  float distance = valRotary * singleTick;
  totalDistance += distance;
  Serial.print(" Distance: ");
  Serial.print(distance);
  Serial.println(" mm");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Pengukuran Panjang");
  lcd.setCursor(7, 1);
  lcd.print("Wire :");
  lcd.setCursor(1, 2);
  lcd.print("Length : ");
  lcd.print(distance);
  lcd.print("mm");

  lcd.setCursor(0, 2);
  if (valRotary > lastValRotary) {
    lcd.write(byte(1));
  } else if (valRotary < lastValRotary) {
    lcd.write(byte(2));
  } else {
    lcd.print(" ");
  }

  lcd.setCursor(1, 3);
  lcd.print("Counter: ");
  lcd.print(resetCounter);
  lcd.print(" Pcs");

  lcd.setCursor(10, 3);
  if (digitalRead(resetCounterClearPin) == LOW) {
    lcd.print("Reset");
    resetCounter = 0 ;
        // Nyala buzzer ketika tombol dipencet
    digitalWrite(buzzerPin, HIGH);
    delay(100); // Tahan bunyi buzzer selama 100ms
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
  } else {
 
  }


  lastValRotary = valRotary;
  delay(100);

  checkReset();
}

void checkReset() {
  if (digitalRead(resetPin) == LOW) {
    encoder0Pos = 0;
    valRotary = 0.0;
    lastValRotary = 0.0;
    totalDistance = 0.0;
    resetCounter++;
    lcd.setCursor(10, 2);
    while (digitalRead(resetPin) == LOW) {
      lcd.print("Cutting");
    // Nyala buzzer ketika tombol dipencet
    digitalWrite(buzzerPin, HIGH);
    delay(100); // Tahan bunyi buzzer selama 100ms
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
    delay(500);
    }    
  }
}


void doEncoder() {
  if (digitalRead(encoder0PinA) == digitalRead(encoder0PinB)) {
    encoder0Pos++;
  } else {
    encoder0Pos--;
  }
  valRotary = encoder0Pos / 2.5L;
}
