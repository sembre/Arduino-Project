#include <EEPROM.h>

int sensorSuara = A7; // Input Analog Dari sensor Suara
int pinLED = A1; //Ke BASIS TR BC547

const int ALAMAT_MAGIC = 0;          // Alamat EEPROM penanda data valid
const int ALAMAT_BOOT = 1;           // Alamat EEPROM jumlah booting (2 byte)
const byte NILAI_MAGIC = 0xA5;       // Penanda EEPROM sudah diinisialisasi
const unsigned int BATAS_BOOT = 3000; // Lebih dari ini alat tidak berfungsi

bool alatTerkunci = false;

void setup() {
  Serial.begin(9600); // Memulai komunikasi serial dengan baud rate 9600
  pinMode(pinLED, OUTPUT);
  digitalWrite(pinLED, LOW);
  pinMode(sensorSuara, INPUT);

  // Inisialisasi EEPROM saat pertama kali dipakai
  if (EEPROM.read(ALAMAT_MAGIC) != NILAI_MAGIC) {
    EEPROM.put(ALAMAT_BOOT, (unsigned int)0);
    EEPROM.write(ALAMAT_MAGIC, NILAI_MAGIC);
  }

  unsigned int jumlahBoot;
  EEPROM.get(ALAMAT_BOOT, jumlahBoot);

  // Berhenti menambah setelah melewati batas agar EEPROM tidak aus
  if (jumlahBoot <= BATAS_BOOT) {
    jumlahBoot++;
    EEPROM.put(ALAMAT_BOOT, jumlahBoot);
  }

  Serial.print("Jumlah Boot: ");
  Serial.println(jumlahBoot);

  if (jumlahBoot > BATAS_BOOT) {
    alatTerkunci = true;
    Serial.println("Batas boot terlampaui, alat tidak berfungsi");
  }
}

void loop() {
  if (alatTerkunci) {
    digitalWrite(pinLED, LOW); // Relay tetap mati
    return;
  }

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
