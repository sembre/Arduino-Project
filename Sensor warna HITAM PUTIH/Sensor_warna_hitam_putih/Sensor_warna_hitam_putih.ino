const int irSensorPin = A6;  // Pin analog untuk sensor inframerah
const int relayPin = 3;     // Pin digital untuk relay
const int threshold = 22;   // Ambang batas untuk mendeteksi hitam atau putih
const int minThreshold = 24; // Ambang batas minimum untuk mematikan relay

void setup() {
  Serial.begin(9600);          // Memulai komunikasi serial pada baud rate 9600
  pinMode(relayPin, OUTPUT);  // Mengatur pin relay sebagai output
}

void loop() {
  int sensorValue = analogRead(irSensorPin);  // Membaca nilai analog dari sensor

  Serial.print("Nilai sensor: ");
  Serial.println(sensorValue);  // Menampilkan nilai sensor di Serial Monitor

  if (sensorValue < minThreshold) {  // Jika nilai sensor kurang dari 50
    Serial.println("Nilai sensor terlalu rendah, relay dimatikan");
    digitalWrite(relayPin, LOW);  // Mematikan relay
  } 
  else if (sensorValue > threshold) {  // Jika nilai sensor di bawah ambang batas (hitam)
    Serial.println("Hitam terdeteksi");
    Serial.println("Menyalakan relay");
    digitalWrite(relayPin, HIGH);  // Menyalakan relay
  } 
  else {  // Jika nilai sensor di atas ambang batas (putih)
    Serial.println("Putih terdeteksi");
    digitalWrite(relayPin, LOW);   // Mematikan relay
  }

  delay(200);  // Menunggu 500ms sebelum membaca kembali
}
