#include <LiquidCrystal.h>

// Pin definitions for IR sensors
const int IR_LEFT1 = A0;
const int IR_LEFT2 = A1;
const int IR_RIGHT1 = A2;
const int IR_RIGHT2 = A3;

// Pin definitions for motor drivers
const int RPWM1 = 5;
const int LPWM1 = 6;
const int RPWM2 = 10;
const int LPWM2 = 11;

const int L_EN1 = 3; // Enable pin for left motor driver 1
const int R_EN1 = 4; // Enable pin for right motor driver 1
const int L_EN2 = 8; // Enable pin for left motor driver 2
const int R_EN2 = 9; // Enable pin for right motor driver 2

// Pin definitions for ultrasonic sensor
const int TRIG_PIN = 22;
const int ECHO_PIN = 23;

// LCD pin definitions
const int LCD_RS = 14;
const int LCD_E = 15;
const int LCD_D4 = 16;
const int LCD_D5 = 17;
const int LCD_D6 = 18;
const int LCD_D7 = 19;

// Battery voltage pin
const int BATTERY_PIN = A4;

// Pin definition for buzzer
const int BUZZER_PIN = 53; // You can change this pin as needed

// Pin definitions for push buttons
const int BUTTON1_PIN = A14;
const int BUTTON2_PIN = A15;

// Threshold distance for obstacle detection (in centimeters)
const int DIST_THRESHOLD = 20;
const int BUZZER_DISTANCE_THRESHOLD = 20; // Distance threshold for buzzer

// Define min and max battery voltage (in volts)
const float MIN_BATTERY_VOLTAGE = 10.0;  // Example min voltage for a 12V battery
const float MAX_BATTERY_VOLTAGE = 12.6; // Example max voltage for a fully charged 12V battery

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Variable to track the motor state
bool isStopped = false;
bool button1Pressed = false;
bool button2Pressed = false;
int currentMode = 0; // 0: Normal, 1: Reversed


void setup() {
  // Initialize sensor pins
  pinMode(IR_LEFT1, INPUT);
  pinMode(IR_LEFT2, INPUT);
  pinMode(IR_RIGHT1, INPUT);
  pinMode(IR_RIGHT2, INPUT);

  // Initialize motor driver pins
  pinMode(RPWM1, OUTPUT);
  pinMode(LPWM1, OUTPUT);
  pinMode(RPWM2, OUTPUT);
  pinMode(LPWM2, OUTPUT);
  pinMode(L_EN1, OUTPUT);
  pinMode(R_EN1, OUTPUT);
  pinMode(L_EN2, OUTPUT);
  pinMode(R_EN2, OUTPUT);

  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize button pins
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  // Enable motor drivers
  digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN1, HIGH);
  digitalWrite(L_EN2, HIGH);
  digitalWrite(R_EN2, HIGH);

  // Initialize LCD
  lcd.begin(16, 2);
  lcd.print("Batt: ");

  Serial.begin(9600);
}

void loop() {
  // Check for button presses
  if (digitalRead(BUTTON1_PIN) == HIGH && !button1Pressed) {
    button1Pressed = true;
    button2Pressed = false;
    currentMode = 0;
  } else if (digitalRead(BUTTON2_PIN) == HIGH && !button2Pressed) {
    button2Pressed = true;
    button1Pressed = false;
    currentMode = 1;
  }

  if (currentMode == 0) {
    performNormalMovement();
  } else if (currentMode == 1) {
    // Implementasi mode mundur (reversed) bisa ditambahkan di sini jika diperlukan
  }

  delay(200); // Debounce delay
}

void performNormalMovement() {
  // Read IR sensors
  int left1 = digitalRead(IR_LEFT1); // Dalam
  int left2 = digitalRead(IR_LEFT2); // Luar
  int right1 = digitalRead(IR_RIGHT1); // Dalam
  int right2 = digitalRead(IR_RIGHT2); // Luar

  // Read distance from ultrasonic sensor
  long distance = getDistance();

  // Read battery voltage and calculate percentage
  float batteryVoltage = analogRead(BATTERY_PIN) * (5.0 / 1023.0) * ((15.0 + 10.0) / 10.0); // Apply voltage divider ratio
  int batteryPercentage = map(batteryVoltage * 100, MIN_BATTERY_VOLTAGE * 100, MAX_BATTERY_VOLTAGE * 100, 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);

  // Update LCD with battery percentage and voltage
  lcd.setCursor(0, 1);
  lcd.print("                "); // Clear the previous value
  lcd.setCursor(7, 0);
  lcd.print(batteryPercentage);
  lcd.print("% ");
  lcd.print(batteryVoltage, 1); // Print voltage with 1 decimal place
  lcd.print("V   ");

  // Debugging - print sensor values
  Serial.print("Left1: ");
  Serial.print(left1);
  Serial.print(" Left2: ");
  Serial.print(left2);
  Serial.print(" Right1: ");
  Serial.print(right1);
  Serial.print(" Right2: ");
  Serial.print(right2);
  Serial.print(" Distance: ");
  Serial.print(distance);
  Serial.print(" cm ");
  Serial.print(" Battery: ");
  Serial.print(batteryPercentage);
  Serial.print("% ");
  Serial.print(batteryVoltage, 1);
  Serial.println("V");

  // Activate buzzer if an object is detected within the buzzer distance threshold
  if (distance <= BUZZER_DISTANCE_THRESHOLD) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Stop motors if an obstacle is detected
  if (distance <= DIST_THRESHOLD) {
    if (!isStopped) {
      stopMotors();
      isStopped = true;
      lcd.setCursor(0, 1);
      lcd.print("BERHENTI        ");
    }
  } else {
    isStopped = false; // Reset the stopped state when moving
    // Line following logic
    if (left1 == HIGH && right1 == HIGH) {
      // All sensors on line, move forward
      maju();
      lcd.setCursor(0, 1);
      lcd.print("MAJU       ");
    } else if (left1 == HIGH && right1 == LOW) {
      kanan();
      lcd.setCursor(0, 1);
      lcd.print("BELOK KANAN");
    } else if (left1 == LOW && right1 == HIGH) {
      kiri();
      lcd.setCursor(0, 1);
      lcd.print("BELOK KIRI      ");
    } else if (left1 == LOW && right1 == LOW) {
      mundur();  
      lcd.setCursor(0, 1);
      lcd.print("MUNDUR      ");
    } else if (left2 == HIGH && left1 == HIGH && right1 == LOW && right2 == LOW) {
      // BELOK KIRI
      kiriTajam();
      lcd.setCursor(0, 1);
      lcd.print("KIRI TAJAM  ");
    } else if (left2 == LOW && left1 == LOW && right1 == HIGH && right2 == HIGH) {
      // BELOK KANAN
      kananTajam();
      lcd.setCursor(0, 1);
      lcd.print("KANAN TAJAM    ");
    } else if (left1 == LOW && left2 == LOW && right1 == LOW && right2 == LOW) {
      // MUNDUR CEPAT
      mundurCepat(); 
      lcd.setCursor(0, 1);
      lcd.print("MUNDUR CEPAT    ");
    } else {
      // Default action, stop
      stopMotors();
      lcd.setCursor(0, 1);
      lcd.print("BERHENTI        ");
    }
  }
}

long getDistance() {
  // Send a 10us pulse to trigger the ultrasonic sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the echo pin
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Calculate the distance (in cm)
  long distance = (duration / 2) / 29.1;

  return distance;
}

void maju() {
  analogWrite(RPWM1, 0);   // maju  M1
  analogWrite(LPWM1, 255); // maju  M1
  analogWrite(RPWM2, 0);   // maju  M2
  analogWrite(LPWM2, 255); // maju  M2
  Serial.println("Motor bergerak maju");
}

void kanan() {
  analogWrite(RPWM1, 255);   // mundur  M1
  analogWrite(LPWM1, 0); // berhenti M1
  analogWrite(RPWM2, 0);   // maju  M2
  analogWrite(LPWM2, 255); // maju  M2
  Serial.println("Motor berbelok sedikit ke kanan");
}

void kiri() {
  analogWrite(RPWM1, 0);   // maju  M1
  analogWrite(LPWM1, 255); // maju  M1
  analogWrite(RPWM2, 255);   // mundur  M2
  analogWrite(LPWM2, 0); // berhenti M2
  Serial.println("Motor berbelok sedikit ke kiri");
}

void mundur() {
  analogWrite(RPWM1, 255);   // mundur  M1
  analogWrite(LPWM1, 0); // berhenti M1
  analogWrite(RPWM2, 255);   // mundur  M2
  analogWrite(LPWM2, 0); // berhenti M2
  Serial.println("Motor bergerak mundur");
}

void kananTajam() {
  analogWrite(RPWM1, 255);   // mundur  M1
  analogWrite(LPWM1, 0); // berhenti M1
  analogWrite(RPWM2, 0);   // maju  M2
  analogWrite(LPWM2, 255); // maju  M2
  Serial.println("Motor berbelok tajam ke kanan");
}

void kiriTajam() {
  analogWrite(RPWM1, 0);   // maju  M1
  analogWrite(LPWM1, 255); // maju  M1
  analogWrite(RPWM2, 255);   // mundur  M2
  analogWrite(LPWM2, 0); // berhenti M2
  Serial.println("Motor berbelok tajam ke kiri");
}

void mundurCepat() {
  analogWrite(RPWM1, 255);   // mundur  M1
  analogWrite(LPWM1, 0); // berhenti M1
  analogWrite(RPWM2, 255);   // mundur  M2
  analogWrite(LPWM2, 0); // berhenti M2
  Serial.println("Motor bergerak mundur cepat");
}

void stopMotors() {
  analogWrite(RPWM1, 0);   // berhenti M1
  analogWrite(LPWM1, 0); // berhenti M1
  analogWrite(RPWM2, 0);   // berhenti M2
  analogWrite(LPWM2, 0); // berhenti M2
  Serial.println("Motor berhenti");
}
