# Arduino Project Collection 🚀

Kumpulan lengkap proyek-proyek Arduino untuk berbagai aplikasi industri, IoT, robotika, dan automasi.

## 📋 Daftar Isi

- [Tentang Repository](#tentang-repository)
- [Kategori Proyek](#kategori-proyek)
- [Daftar Proyek](#daftar-proyek)
- [Cara Penggunaan](#cara-penggunaan)
- [Komponen yang Dibutuhkan](#komponen-yang-dibutuhkan)
- [Kontribusi](#kontribusi)
- [Lisensi](#lisensi)

## 🎯 Tentang Repository

Repository ini berisi koleksi proyek Arduino yang dikembangkan untuk berbagai keperluan, mulai dari sistem monitoring industri, kontrol otomatis, robotika, hingga aplikasi IoT. Setiap proyek dilengkapi dengan kode yang siap pakai dan dokumentasi penggunaan.

## 📂 Kategori Proyek

### 🏭 **Sistem Monitoring & Kontrol Industri**
- **Andon System** - Sistem monitoring produksi dengan display LED matrix
- **Counter Systems** - Berbagai sistem penghitung untuk aplikasi industri
- **Twisting Control** - Kontrol sistem twisting dengan berbagai konfigurasi waktu
- **EC Circuit Checker** - Sistem pengecekan rangkaian elektronik

### 🤖 **Robotika & Otomasi**
- **Line Follower Robot** - Robot pengikut garis dengan sensor IR dan LCD
- **Sensor Testing** - Berbagai proyek pengujian sensor
- **Motor Control** - Kontrol solenoid dan motor

### 🏠 **Smart Home & IoT**
- **RFID Door Lock** - Sistem kunci pintu dengan multiple RFID
- **Digital Clock** - Jam digital dengan buzzer alarm
- **Water Pump Controller** - Kontrol pompa air otomatis dengan sensor level
- **Timer Relay** - Sistem timer untuk kontrol relay

### 📏 **Sistem Pengukuran**
- **Length Measurement** - Alat ukur panjang dengan rotary encoder
- **Color Detection** - Sistem deteksi warna hitam-putih dengan ESP32 camera

### 🧮 **Alat Bantu & Utilitas**
- **Calculator** - Alat hitung elektronik
- **Pin Tester** - Alat test pin dan rangkaian
- **Switch Tester** - Pengujian berbagai jenis switch

## 📝 Daftar Proyek

### A
- `Alat_Hitung/` - Kalkulator elektronik
- `andon_dengan_P5/` - Sistem andon dengan display P5
- `andon_p5_koito/` - Sistem andon khusus untuk Koito

### C
- `cek_5_circuit/` - Pengecekan 5 rangkaian sekaligus
- `cek_pin/` - Alat cek pin elektronik
- `cek_pin_MAX-37/` - Cek pin untuk sensor kit MAX-37
- `Cek_sambungan_EC/` - Pengecekan sambungan EC
- `cek_switch_3_kaki/` - Test switch 3 pin
- `cek_switch_3p/` - Test switch 3 pole
- `cek_switch3p_dengan_lcd/` - Test switch dengan display LCD
- `coba_counter_up_by_time/` - Counter yang bertambah berdasarkan waktu
- `counter_dengan_panel_p5/` - Counter dengan panel P5
- `Counter_UP_30/` - Counter sampai 30

### E
- `ec_10_circuit/` - Pengecekan 10 rangkaian EC
- `ec_30_circuit/` - Pengecekan 30 rangkaian EC  
- `esp32_kamera_cek_warna_hitam_putih/` - Deteksi warna dengan ESP32 camera

### G-J
- `gerak_gerak_maju_mundur_solenoid/` - Kontrol solenoid maju mundur
- `GG5-0986/` - Proyek khusus GG5-0986
- `guyur_WC/` - Sistem otomatis toilet
- `Hitung_Konektor/` - Penghitung konektor
- `JAM/` - Proyek jam
- `jam_digital/` - Jam digital dengan alarm

### K-L
- `kontol_pompa_air/` - Kontrol pompa air
- `kontol_pompa_air_LCD20X4/` - Kontrol pompa air dengan LCD 20x4
- `led_running/` - LED running text
- `Line_Follower_Robot/` - Robot pengikut garis

### M-R
- `Measurino_length/` - Alat ukur panjang
- `Multi_rfid_door_lock/` - Kunci pintu multi RFID
- `ReadAndWrite/` - Sistem baca tulis data
- `relay_wfa/` - Kontrol relay WFA
- `rotay_encoder_sensor_kabel/` - Sensor kabel dengan rotary encoder

### S-T
- `Sensor warna HITAM PUTIH/` - Sensor deteksi warna
- `test_running_text/` - Test running text
- `Timer_relay_WP/` - Timer relay WP
- `twisting_control_multi_time/` - Kontrol twisting multi waktu
- `twisting_control_multi_time_16_time/` - Twisting 16 waktu
- `twisting_control_multi_time_24_time/` - Twisting 24 waktu
- `twisting_control_multi_time_3_Motor/` - Twisting 3 motor
- `twisting_control_multi_time_32_time/` - Twisting 32 waktu
- `twisting_control_multi_time_36time/` - Twisting 36 waktu
- `Twisting_counter/` - Counter twisting
- `Twisting_counter_maju_mundur/` - Counter twisting maju mundur

### U
- `ukur_panjang/` - Alat ukur panjang
- `update_1_10_ceker/` - Update sistem ceker

## 🛠️ Cara Penggunaan

### Persiapan
1. **Install Arduino IDE** - Download dari [arduino.cc](https://www.arduino.cc/en/software)
2. **Clone Repository**:
   ```bash
   git clone https://github.com/sembre/Arduino-Project.git
   ```
3. **Buka Proyek**: Pilih folder proyek yang diinginkan dan buka file `.ino`

### Langkah Upload
1. Hubungkan Arduino ke komputer via USB
2. Pilih board dan port yang sesuai di Arduino IDE
3. Install library yang diperlukan (lihat bagian komponen)
4. Upload kode ke Arduino

### Struktur Kode
Setiap proyek mengikuti struktur standar Arduino:
```cpp
// Library includes
#include <Library.h>

// Pin definitions & variables
const int PIN_NAME = 2;

void setup() {
    // Initialization code
}

void loop() {
    // Main program loop
}
```

## 🔧 Komponen yang Dibutuhkan

### Hardware Umum
- **Arduino Uno/Nano/Mega** - Mikrokontroler utama
- **ESP32** - Untuk proyek IoT dan kamera
- **Breadboard & Jumper Wires** - Untuk prototyping
- **Resistor** - Berbagai nilai
- **LED** - Indikator status

### Sensor & Input
- **RFID Module (MFRC522)** - Untuk door lock
- **IR Sensors** - Line follower, object detection
- **Ultrasonic Sensor (HC-SR04)** - Pengukuran jarak
- **Rotary Encoder** - Pengukuran putaran
- **Water Flow Sensor** - Pengukuran aliran air
- **Level Switch** - Sensor level air
- **Color Sensor** - Deteksi warna
- **Keypad 4x4** - Input angka

### Display & Feedback
- **LCD 16x2 / 20x4** - Display informasi
- **LED Matrix P5** - Display andon
- **TM1637 7-Segment** - Display jam digital
- **OLED SSD1306** - Display kecil
- **Buzzer** - Audio feedback

### Actuator & Control
- **Relay Module** - Kontrol beban AC
- **Motor Driver** - Kontrol motor DC
- **Solenoid Valve** - Kontrol aliran
- **Water Pump** - Pompa air

### Library Arduino yang Diperlukan
```cpp
// Display Libraries
#include <LiquidCrystal.h>      // LCD
#include <U8g2lib.h>            // OLED
#include <TM1637Display.h>      // 7-Segment
#include <Adafruit_GFX.h>       // Graphics
#include <RGBmatrixPanel.h>     // LED Matrix

// Sensor Libraries
#include <SPI.h>                // SPI Communication
#include <MFRC522.h>            // RFID
#include <Keypad.h>             // Keypad

// ESP32 Libraries
#include <WiFi.h>               // WiFi
#include <WebServer.h>          // Web Server
#include <esp_camera.h>         // Camera
```

## 🎮 Fitur Utama Proyek

### 🏭 Andon System
- Display real-time production data
- Keypad input untuk data entry
- LED matrix display dengan warna-warni
- Monitoring plan, actual, dan balance

### 🤖 Line Follower Robot
- Sensor IR untuk deteksi garis
- Motor control dengan PWM
- LCD display untuk status
- Obstacle detection dengan ultrasonik
- Battery monitoring

### 🔐 Multi RFID Door Lock
- Support multiple RFID cards
- LED indikator akses
- Buzzer untuk feedback
- Relay control untuk kunci elektronik

### ⏰ Digital Clock
- Display 7-segment dengan TM1637
- Multiple alarm times
- Brightness control
- Buzzer alarm

### 💧 Water Pump Controller
- Otomatis ON/OFF berdasarkan level air
- Flow rate monitoring
- LCD display dengan progress bar
- Safety features

### 📏 Length Measurement
- Rotary encoder untuk pengukuran presisi
- OLED display
- Konversi satuan (mm/inch)
- Reset dan kalibrasi

### 🎨 Color Detection (ESP32)
- Real-time color detection
- Web interface
- Image processing
- Object counting

## 📊 Statistik Repository

- **Total Proyek**: 50+ proyek Arduino
- **Kategori**: 6 kategori utama
- **Bahasa**: C++ (Arduino)
- **Platform**: Arduino, ESP32
- **Status**: Aktif dikembangkan

## 🤝 Kontribusi

Kontribusi sangat diterima! Silakan:

1. **Fork** repository ini
2. **Buat branch** untuk fitur baru (`git checkout -b feature/AmazingFeature`)
3. **Commit** perubahan (`git commit -m 'Add some AmazingFeature'`)
4. **Push** ke branch (`git push origin feature/AmazingFeature`)
5. **Buat Pull Request**

### Guidelines Kontribusi
- Gunakan naming convention yang konsisten
- Tambahkan komentar yang jelas pada kode
- Test kode sebelum submit
- Update README jika menambah proyek baru

## 📋 TODO List

- [ ] Menambahkan schematic diagram untuk setiap proyek
- [ ] Dokumentasi video untuk proyek kompleks
- [ ] Unit testing untuk fungsi kritis
- [ ] Migration ke PlatformIO
- [ ] Integrasi dengan IoT platform
- [ ] Mobile app companion

## 🐛 Bug Reports & Feature Requests

Jika menemukan bug atau ingin request fitur baru, silakan buat [Issue](https://github.com/sembre/Arduino-Project/issues) dengan template:

**Bug Report:**
- Nama proyek
- Deskripsi masalah
- Steps to reproduce
- Expected vs actual behavior
- Environment (Arduino IDE version, board, etc.)

**Feature Request:**
- Nama proyek
- Deskripsi fitur yang diinginkan
- Use case
- Benefit yang diharapkan

## 📚 Resources & References

### Dokumentasi
- [Arduino Official Documentation](https://docs.arduino.cc/)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Library References](https://www.arduino.cc/reference/en/libraries/)

### Tutorials
- [Arduino Getting Started](https://www.arduino.cc/en/Guide)
- [ESP32 Camera Tutorial](https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/)
- [RFID Tutorial](https://randomnerdtutorials.com/security-access-using-mfrc522-rfid-reader-with-arduino/)

## 👤 Author

**Sembre**
- GitHub: [@sembre](https://github.com/sembre)
- Repository: [Arduino-Project](https://github.com/sembre/Arduino-Project)

## 📄 Lisensi

Distributed under the MIT License. See `LICENSE` for more information.

---

## 🚀 Quick Start

```bash
# Clone repository
git clone https://github.com/sembre/Arduino-Project.git

# Masuk ke directory proyek yang diinginkan
cd Arduino-Project/Line_Follower_Robot

# Buka file .ino di Arduino IDE
# Install library yang diperlukan
# Upload ke Arduino board
```

## 📞 Support

Jika butuh bantuan:
1. Cek [Issues](https://github.com/sembre/Arduino-Project/issues) yang sudah ada
2. Buat issue baru jika belum ada solusinya
3. Baca dokumentasi di folder masing-masing proyek

---

⭐ **Jangan lupa kasih star jika repository ini bermanfaat!** ⭐
