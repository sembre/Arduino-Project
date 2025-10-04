#define waktutunda 15 //untuk 10 detik
 
const int ledPin = 41; //Menggunakan led built in Arduino
const int SensorPir = 2; //menggunakan pin ke-2 arduino
 
unsigned long now = millis();
unsigned long TriggerAkhir = 0;
boolean waktumulai = false;
 
void setup() {
  Serial.begin(115200); //baut komunikasi serial monitor
  
  pinMode(SensorPir, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SensorPir), deteksigerakan, RISING);
 
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
}
 
void loop() {
  now = millis();
  if(waktumulai && (now - TriggerAkhir > (waktutunda*1000))) {
    Serial.println("Tidak ada gerakan!");
    delay(10000);
    Serial.println("waktu jeda");
    digitalWrite(ledPin, HIGH);
    Serial.println("Air Keluar!");
    waktumulai = false;
    delay(10000); 
    digitalWrite(ledPin, LOW);
    Serial.println("Air Mati!");
  }
}
 
void deteksigerakan() {
  Serial.println("GERAKAN TERDETEKSI");
  digitalWrite(ledPin, LOW);
  waktumulai = true;
  TriggerAkhir = millis();
}
 
 
