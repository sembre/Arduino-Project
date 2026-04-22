int led1 = 2;
int led2 = 3;
int buzzer = 4;

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
}

void loop() {

  // LED 1 nyala + suara 1
  digitalWrite(led1, HIGH);
  digitalWrite(led2, LOW);
  tone(buzzer, 800);   // suara rendah
  delay(500);

  // LED 2 nyala + suara 2
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  tone(buzzer, 1200);  // suara lebih tinggi
  delay(500);

}
