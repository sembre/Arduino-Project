int sensorSuara = A7; // Input Analog Dari sensor Suara
int pinLED = A1; //Ke BASIS TR BC547

void setup() {
  Serial.begin(9600); // Memulai komunikasi serial dengan baud rate 9600
  pinMode(pinLED, OUTPUT);
  pinMode(sensorSuara, INPUT);
}

void loop() {
  int sensorValue = analogRead(sensorSuara);
  Serial.print("Sensor Value: "); // Menampilkan teks "Sensor Value: " di serial monitor
  Serial.println(sensorValue); // Menampilkan nilai sensor di serial monitor

  if (sensorValue > 800) { // Sesuaikan nilai threshold sesuai kebutuhan
    digitalWrite(pinLED, HIGH);
    Serial.println("LED ON"); // Menampilkan teks "LED ON" di serial monitor
    delay(10000);
  } else {
    digitalWrite(pinLED, LOW);
    Serial.println("LED OFF"); // Menampilkan teks "LED OFF" di serial monitor
  }

  delay(100); // Menambahkan sedikit delay agar serial monitor lebih mudah dibaca
}
