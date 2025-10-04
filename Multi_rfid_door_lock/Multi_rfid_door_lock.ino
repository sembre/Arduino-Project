#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 53
#define RST_PIN 5
#define LED_G 3 // define green LED pin
#define LED_R 2 // define red LED
#define RELAY 4 // relay pin
#define BUZZER 6 // buzzer pin
#define ACCESS_DELAY 2000
#define DENIED_DELAY 1000

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance.

// List of valid UID values
byte validUIDs[][4] = {
  {0xCA, 0xFA, 0xBB, 0x80},// Add more UID values as needed
  {0xCA, 0x31, 0xB8, 0x89},// Add more UID values as needed
  {0xED, 0x90, 0x3C, 0x2D},// Add more UID values as needed
  {0x53, 0xD3, 0xF8, 0x50},// Add more UID values as needed
  

};

void setup()
{
  Serial.begin(9600); // Initiate a serial communication
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);
  digitalWrite(RELAY, HIGH);
  Serial.println("Put your card to the reader...");
  Serial.println();
}

void loop()
{
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  // Show UID on serial monitor
  Serial.print("UID tag :");
  byte content[4];
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    content[i] = mfrc522.uid.uidByte[i];
    Serial.print(content[i] < 0x10 ? " 0x0" : " 0x");
    Serial.print(content[i], HEX);
  }
  Serial.println();
  Serial.print("Message : ");

  // Check if the content matches any valid UID
  bool authorized = false;
  for (int i = 0; i < sizeof(validUIDs) / sizeof(validUIDs[0]); i++)
  {
    if (memcmp(content, validUIDs[i], 4) == 0)
    {
      authorized = true;
      break;
    }
  }

  if (authorized)
  {
    Serial.println("Authorized access");
    Serial.println();
    delay(500);
    digitalWrite(RELAY, HIGH);
    digitalWrite(LED_G, HIGH);
    Serial.println("Pintu Terbuka");
    Serial.println();
    delay(ACCESS_DELAY);
    digitalWrite(RELAY, LOW);
    digitalWrite(LED_G, LOW);
    Serial.println("Pintu Tertutup");
    Serial.println();
  }
  else
  {
    Serial.println("Access denied");
    digitalWrite(LED_R, HIGH);
    tone(BUZZER, 1000);
    delay(DENIED_DELAY);
    digitalWrite(LED_R, LOW);
    noTone(BUZZER);
  }
}
