#include <LiquidCrystal.h>

byte bar[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};  // custom character for the bar
// Definisikan pin yang digunakan

const int waterFlowSensorPin = 2;
const int tandonInputValvePin = 3;
const int tandonOutputValvePin = 4;
const int waterPumpPin = 5;
const int levelSwitchPin1 = 6;
const int levelSwitchPin2 = 7;
const int LCD_Backlight = A14;

const int rs = 8, en = 9, d4 = 10, d5 = 11, d6 = 12, d7 = 13;

// Deklarasi objek untuk tampilan LCD
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Variabel untuk menghitung jumlah pulsa dari sensor air
volatile int flowPulseCount = 0;
// Konstanta untuk menghitung laju air
const float flowRateConstant = 7.5;

void setup() {
  // Set pin yang digunakan sebagai input atau output
  pinMode(LCD_Backlight, OUTPUT); 
  analogWrite(LCD_Backlight, 200);//Adjust for LCD_Backlight
  pinMode(waterFlowSensorPin, INPUT);
  pinMode(tandonInputValvePin, OUTPUT);
  pinMode(tandonOutputValvePin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(levelSwitchPin1, INPUT_PULLUP);
  pinMode(levelSwitchPin2, INPUT_PULLUP);


  // Set keadaan awal pin
  digitalWrite(tandonInputValvePin, LOW);
  digitalWrite(tandonOutputValvePin, LOW);
  digitalWrite(waterPumpPin, LOW);

  // Inisialisasi tampilan LCD
  lcd.begin(16, 2);
  lcd.createChar(0, bar);  // create the custom character at location 0

  // Register interrupt service routine untuk sensor air
  attachInterrupt(digitalPinToInterrupt(waterFlowSensorPin), flowSensorISR, RISING);

  // Tampilkan pesan awal pada tampilan LCD
  lcd.print("Sedang menunggu");
}

void loop() {
  // Hitung jumlah pulsa dari sensor air selama 1 detik
  flowPulseCount = 0;
  sei();
  delay(1000);
  cli();

  // Cek apakah ada aliran air atau tidak
  if (flowPulseCount > 0) {
    // Jika ada aliran air, nyalakan pompa air dan valve input tandon air
    digitalWrite(waterPumpPin, HIGH);
    digitalWrite(tandonInputValvePin, HIGH);//valve nutup
    digitalWrite(tandonOutputValvePin, LOW);//valve buka
    // Hitung laju air dalam satuan L/det
    float flowRate = (float) flowPulseCount / flowRateConstant;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Laju Keluar Air:");
    lcd.setCursor(0, 1);
    lcd.print(flowRate);
    lcd.setCursor(8, 1);
    lcd.print(" L/Menit");
    }else if (digitalRead(levelSwitchPin1) == HIGH){
    digitalWrite(waterPumpPin, HIGH);
    digitalWrite(tandonInputValvePin, LOW);
    digitalWrite(tandonOutputValvePin, HIGH);
    lcd.clear();
    lcd.print("Isi Dari bawah");
    for (int i = 0; i < 8; i++) {
    lcd.setCursor(i, 1);  // set the cursor to the first row and i-th column
    lcd.write(byte(0));  // display a bar character (custom character)
    delay(50000);  // delay for animation
    lcd.setCursor(i, 1);  // set the cursor to the first row and i-th column
    delay(500);
      }
    } else if (digitalRead(levelSwitchPin2) == HIGH){
    digitalWrite(waterPumpPin, HIGH);
    digitalWrite(tandonInputValvePin, LOW);
    digitalWrite(tandonOutputValvePin, HIGH);
    delay(50000);
    lcd.clear();
    lcd.print("Isi Air Setengah");
    for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 1); // set the cursor to the first row and i-th column
    lcd.write(byte(0)); // display a bar character (custom character)
    delay(50000); // delay for animation
    lcd.setCursor(i, 1); // set the cursor to the first row and i-th column
    delay(500);
    }
    }else {
    // Jika tandon air penuh, matikan pompa air dan valve input tandon air buka semua
    digitalWrite(waterPumpPin, LOW);
    digitalWrite(tandonInputValvePin, LOW);
    digitalWrite(tandonOutputValvePin, LOW);
    lcd.clear();
    lcd.print("Tandon Air Penuh");
    lcd.setCursor(0, 1);
    lcd.print("=SYSTEM STANDBY=");
    delay(1000);
  }
}

// Fungsi untuk menghitung jumlah pulsa dari sensor air
void flowSensorISR() {
  flowPulseCount++;
}
