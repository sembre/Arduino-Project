#include <Wire.h>

#define SDA_PIN 8
#define SCL_PIN 9

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("\nMemulai Pencarian I2C...");
}

void loop() {
  byte error, address;
  int nDevices = 0;
  
  Serial.println("Mencari...");
  
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Ditemukan perangkat di alamat: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println("KOSONG! Tidak ada perangkat I2C. Cek Kabel!");
  } else {
    Serial.println("Pencarian Selesai.\n");
  }
  
  delay(5000);
}
