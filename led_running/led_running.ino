void setup()
{
  //-------Pengaturan Pin-------
  //Pin pada Arduino
  pinMode(13, OUTPUT);//Pin 13
  pinMode(12, OUTPUT);//Pin 12
  pinMode(11, OUTPUT);//Pin 11
  pinMode(10, OUTPUT);//Pin 10
  pinMode(9, OUTPUT);//Pin 9
  pinMode(8, OUTPUT);//Pin 8
  pinMode(7, OUTPUT);//Pin 7
  
  //-------Kondisi Running LED-------
  
  //Kondisi ke 1
  digitalWrite(13, HIGH);//Pin 13 Hidup
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, LOW);//Pin 9 Mati
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(200);
  //Kondisi ke 2
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, HIGH);//Pin 12 Hidup
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, LOW);//Pin 9 Mati
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(1000);
  //Kondisi ke 3
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, HIGH);//Pin 11 Hidup
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, LOW);//Pin 9 Mati
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(1000);
  //Kondisi ke 4
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, HIGH);//Pin 10 Hidup
  digitalWrite(9, LOW);//Pin 9 Mati
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(1000);//
  //Kondisi ke 5
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, HIGH);//Pin 9 Hidup
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(1000);
  //Kondisi ke 6
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, LOW);
  digitalWrite(8, HIGH);
  digitalWrite(7, LOW);//Pin 8 Mati
  delay(1000);

  //Kondisi ke 7
  digitalWrite(13, LOW);//Pin 13 Mati
  digitalWrite(12, LOW);//Pin 12 Mati
  digitalWrite(11, LOW);//Pin 11 Mati
  digitalWrite(10, LOW);//Pin 10 Mati
  digitalWrite(9, LOW);//Pin 9 Mati
  digitalWrite(8, LOW);//Pin 8 Mati
  digitalWrite(7, HIGH);//Pin 8 nyala
  delay(500);
  

  
}

void loop()
{
  
}
