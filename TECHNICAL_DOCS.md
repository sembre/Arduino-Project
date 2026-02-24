# 📖 Dokumentasi Teknis – Arduino Project Collection

Dokumentasi ini menjelaskan detail teknis setiap proyek dalam repository, meliputi platform mikrokontroler, konfigurasi pin, library yang dibutuhkan, dan cara kerja sistem.

---

## Daftar Isi

1. [Line Follower Robot](#1-line-follower-robot)
2. [Multi RFID Door Lock](#2-multi-rfid-door-lock)
3. [Jam Digital (Digital Clock)](#3-jam-digital-digital-clock)
4. [Measurino – Alat Ukur Panjang Wire](#4-measurino--alat-ukur-panjang-wire)
5. [Ukur Panjang (Rotary Encoder Kabel)](#5-ukur-panjang-rotary-encoder-kabel)
6. [Andon System dengan Panel P5](#6-andon-system-dengan-panel-p5)
7. [Andon P5 Koito](#7-andon-p5-koito)
8. [Counter dengan Panel P5](#8-counter-dengan-panel-p5)
9. [Twisting Counter](#9-twisting-counter)
10. [Twisting Counter Maju Mundur](#10-twisting-counter-maju-mundur)
11. [Twisting Counter Maju Mundur V1](#11-twisting-counter-maju-mundur-v1)
12. [Twisting Counter Maju Mundur LCD I2C](#12-twisting-counter-maju-mundur-lcd-i2c)
13. [Twisting Control Multi Time](#13-twisting-control-multi-time)
14. [Twisting Control Multi Time – Varian 16/24/32/36 Waktu & 3 Motor](#14-twisting-control-multi-time--varian-16243236-waktu--3-motor)
15. [Counter 4 Digit](#15-counter-4-digit)
16. [Counter UP 30](#16-counter-up-30)
17. [Counter Up by Time](#17-counter-up-by-time)
18. [5-Circuit Continuity Tester](#18-5-circuit-continuity-tester)
19. [EC 10 Circuit Tester](#19-ec-10-circuit-tester)
20. [EC 30 Circuit Tester](#20-ec-30-circuit-tester)
21. [Update 1-10 Ceker (10-Circuit Checker)](#21-update-1-10-ceker-10-circuit-checker)
22. [Cek Sambungan EC (37-Pin Tester)](#22-cek-sambungan-ec-37-pin-tester)
23. [GG5-0986 EC Connection Tester](#23-gg5-0986-ec-connection-tester)
24. [Pin Tester (cek_pin)](#24-pin-tester-cek_pin)
25. [Pin Tester MAX-37 (cek_pin_MAX-37)](#25-pin-tester-max-37-cek_pin_max-37)
26. [Switch 3-Pole Tester](#26-switch-3-pole-tester)
27. [Switch 3-Kaki Tester](#27-switch-3-kaki-tester)
28. [Switch 3P dengan LCD](#28-switch-3p-dengan-lcd)
29. [Sensor Warna Hitam Putih](#29-sensor-warna-hitam-putih)
30. [ESP32 Kamera – Deteksi Warna Hitam Putih](#30-esp32-kamera--deteksi-warna-hitam-putih)
31. [Kontrol Pompa Air](#31-kontrol-pompa-air)
32. [Kontrol Pompa Air LCD 20x4](#32-kontrol-pompa-air-lcd-20x4)
33. [Guyur WC (Auto Flush Toilet)](#33-guyur-wc-auto-flush-toilet)
34. [Timer Relay WP](#34-timer-relay-wp)
35. [Relay WFA (Sound-Triggered Relay)](#35-relay-wfa-sound-triggered-relay)
36. [Gerak Maju Mundur Solenoid](#36-gerak-maju-mundur-solenoid)
37. [Alat Hitung (ESP32 Camera Counter)](#37-alat-hitung-esp32-camera-counter)
38. [Hitung Konektor (ESP32-S3 File Manager)](#38-hitung-konektor-esp32-s3-file-manager)
39. [LED Running](#39-led-running)
40. [Test Running Text](#40-test-running-text)
41. [ReadAndWrite (RFID)](#41-readandwrite-rfid)
42. [JAM (Sketch Jam LCD)](#42-jam-sketch-jam-lcd)

---

## 1. Line Follower Robot

**Platform:** Arduino Mega 2560  
**File:** `Line_Follower_Robot/Line_Follower_Robot.ino`

### Library
```cpp
#include <LiquidCrystal.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Sensor IR Kiri 1 (dalam) | A0 |
| Sensor IR Kiri 2 (luar) | A1 |
| Sensor IR Kanan 1 (dalam) | A2 |
| Sensor IR Kanan 2 (luar) | A3 |
| Motor Driver 1 – RPWM | 5 |
| Motor Driver 1 – LPWM | 6 |
| Motor Driver 2 – RPWM | 10 |
| Motor Driver 2 – LPWM | 11 |
| Enable Motor Driver 1 L/R | 3, 4 |
| Enable Motor Driver 2 L/R | 8, 9 |
| Ultrasonic TRIG | 22 |
| Ultrasonic ECHO | 23 |
| LCD RS, E, D4–D7 | 14, 15, 16–19 |
| Baterai (voltage divider) | A4 |
| Buzzer | 53 |
| Tombol Mode Normal | A14 |
| Tombol Mode Mundur | A15 |

### Cara Kerja
1. Empat sensor IR membaca posisi garis; logika sensor menentukan arah gerak (maju, kanan, kiri, mundur, belok tajam).
2. Sensor ultrasonik HC-SR04 mengukur jarak objek di depan robot; jika ≤ 20 cm motor berhenti dan buzzer aktif.
3. Tegangan baterai diukur melalui pembagi tegangan (15 kΩ / 10 kΩ) kemudian dikonversi ke persentase untuk ditampilkan di LCD.
4. Dua tombol memilih mode gerak Normal atau Mundur.

### Parameter Penting
| Parameter | Nilai |
|---|---|
| Batas jarak berhenti | 20 cm |
| Batas tegangan minimum baterai | 10,0 V |
| Batas tegangan maksimum baterai | 12,6 V |
| Rasio pembagi tegangan | (15 + 10) / 10 |

---

## 2. Multi RFID Door Lock

**Platform:** Arduino Mega 2560  
**File:** `Multi_rfid_door_lock/Multi_rfid_door_lock.ino`

### Library
```cpp
#include <SPI.h>
#include <MFRC522.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| RFID SS (SDA) | 53 |
| RFID RST | 5 |
| LED Hijau (akses diterima) | 3 |
| LED Merah (akses ditolak) | 2 |
| Relay (kunci elektronik) | 4 |
| Buzzer | 6 |

### Cara Kerja
1. MFRC522 menggunakan SPI untuk membaca UID kartu RFID.
2. UID yang terbaca dibandingkan dengan array `validUIDs[]` (4 bytes per kartu).
3. Akses diterima → relay aktif (pintu terbuka) + LED hijau selama `ACCESS_DELAY` (2000 ms), lalu relay mati.
4. Akses ditolak → LED merah + buzzer 1000 Hz selama `DENIED_DELAY` (1000 ms).

### Menambah Kartu Baru
Tambahkan UID 4-byte baru pada array `validUIDs` di dalam kode:
```cpp
byte validUIDs[][4] = {
  {0xCA, 0xFA, 0xBB, 0x80},
  // tambahkan baris baru di sini
};
```
UID kartu dapat dibaca lewat Serial Monitor saat pertama kali ditempelkan.

---

## 3. Jam Digital (Digital Clock)

**Platform:** Arduino Uno/Nano  
**File:** `jam_digital/jam_digital.ino`

### Library
```cpp
#include <TM1637Display.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| TM1637 CLK | A0 |
| TM1637 DIO | A1 |
| Buzzer | 13 |
| Tombol Tambah Jam | A2 |
| Tombol Tambah Menit | A3 |
| Tombol Kecerahan | A4 |

### Cara Kerja
1. Jam dijalankan secara software menggunakan `millis()` (bukan RTC) dengan akurasi ± beberapa detik per hari.
2. Dua tombol mengatur jam dan menit secara manual.
3. Titik dua (:) berkedip setiap detik.
4. Array `buzzerTimes[][2]` menyimpan waktu-waktu alarm; buzzer berbunyi selama `buzzerDuration` (10 ms) saat jam dan menit cocok.
5. Kecerahan layar dapat diubah antara level 1 (redup) dan 7 (terang) menggunakan tombol kecerahan.

### Menambah Waktu Alarm
Edit array `buzzerTimes` dalam kode:
```cpp
const int buzzerTimes[][2] = {
  {7, 0},   // Bunyi jam 07:00
  {12, 30}, // Bunyi jam 12:30
  // tambahkan baris sesuai kebutuhan
};
```

---

## 4. Measurino – Alat Ukur Panjang Wire

**Platform:** Arduino Uno/Nano  
**File:** `Measurino_length/Measurino_length.ino`

### Library
```cpp
#include <U8g2lib.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Encoder Fase A (interrupt) | 2 |
| Encoder Fase B | 3 |
| Tombol Reset | 8 |
| Tombol Ganti Satuan | 9 |
| OLED SDA | SDA |
| OLED SCL | SCL |

### Cara Kerja
1. Interrupt pada pin A_PHASE (RISING) membaca Fase B untuk menentukan arah putaran encoder.
2. Counter bertambah atau berkurang sesuai arah, dengan hysteresis 5 tick untuk mengurangi noise.
3. Jarak dihitung: `jarak = tick × (wheelDia × π / 400)` dimana `wheelDia = 51 mm`.
4. Satuan otomatis beralih: mm → m → km (metrik) atau in → yd → mi (imperial).
5. Tombol Reset mengembalikan counter ke 0.
6. Tombol Ganti Satuan mengalihkan antara metrik dan imperial.

### Parameter Kalibrasi
| Parameter | Nilai Default |
|---|---|
| Diameter roda encoder | 51 mm |
| Resolusi encoder | 400 tick/putaran |
| Panjang per tick | ≈ 0,401 mm |
| Hysteresis | 5 tick |

---

## 5. Ukur Panjang (Rotary Encoder Kabel)

**Platform:** Arduino Mega 2560  
**File:** `rotay_encoder_sensor_kabel/rotay_encoder_sensor_kabel.ino`

### Library
```cpp
#include <Wire.h>
#include <LiquidCrystal.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Encoder Pin A (interrupt) | 3 |
| Encoder Pin B | 2 |
| Tombol Encoder (tekan) | 4 |
| LED Maju | 51 |
| LED Mundur | 53 |
| Tombol Reset | A13 |
| LCD RS, E, D4–D7 | 8, 9, 10–13 |
| LCD Backlight | A14 |
| Reset Counter Clear | A15 |

### Cara Kerja
Prinsip sama dengan Measurino, namun menggunakan LCD 16x2 sebagai tampilan dan menampilkan arah gerak (maju/mundur) melalui LED indikator terpisah. Diameter roda encoder `65 mm`.

---

## 6. Andon System dengan Panel P5

**Platform:** Arduino Mega 2560  
**File:** `andon_dengan_P5/andon_dengan_P5.ino`

### Library
```cpp
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
#include <Keypad.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Matrix CLK | 11 |
| Matrix OE | 9 |
| Matrix LAT | 10 |
| Matrix A, B, C, D | A0, A1, A2, A3 |
| Keypad baris (R0–R3) | A8, A9, A10, A11 |
| Keypad kolom (C0–C3) | A12, A13, A14, A15 |

### Cara Kerja
1. Panel RGB LED 64×32 pixel P5 menampilkan tiga baris data produksi: **Pln** (Plan), **Act** (Actual), **Bal** (Balance).
2. Keypad 4×4 digunakan untuk input angka Plan dan kontrol sistem.
3. Nilai Balance dihitung otomatis: `Bal = Plan − Actual`.
4. Warna teks: Plan = oranye, Actual = hijau, Balance = merah.

---

## 7. Andon P5 Koito

**Platform:** Arduino Mega 2560  
**File:** `andon_p5_koito/andon_p5_koito.ino`

Varian Andon untuk lini produksi Koito. Konfigurasi pin dan library identik dengan **Andon dengan P5** namun memiliki penyesuaian layout tampilan dan parameter produksi untuk kebutuhan spesifik Koito.

---

## 8. Counter dengan Panel P5

**Platform:** Arduino Mega 2560  
**File:** `counter_dengan_panel_p5/counter_dengan_panel_p5.ino`

### Library
```cpp
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
```

### Konfigurasi Pin
Sama dengan Andon P5 (pin matrix identik: CLK=11, OE=9, LAT=10, A–D=A0–A3).

### Cara Kerja
Menampilkan counter numerik besar (font ukuran 2) pada panel RGB LED 64×32 pixel. Counter bertambah setiap 100 ms secara otomatis untuk demonstrasi atau keperluan produksi.

---

## 9. Twisting Counter

**Platform:** Arduino Uno  
**File:** `Twisting_counter/Twisting_counter.ino`

### Library
```cpp
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Motor output | A1 |
| Tombol Start | 2 |
| Switch sensor | 3 |
| Keypad baris | 12, 11, 10, 9 |
| Keypad kolom | 8, 7, 6, 5 |
| LCD RS, E, D4–D7 | 14, 15, 16–19 |
| LCD Backlight | 4 |

### Cara Kerja
1. Operator memasukkan target putaran melalui keypad 4×4.
2. Saat tombol Start ditekan, motor aktif dan sensor (switch) menghitung setiap putaran.
3. Ketika count mencapai target, motor berhenti otomatis.
4. Target putaran tersimpan di EEPROM sehingga tidak hilang saat listrik padam.

---

## 10. Twisting Counter Maju Mundur

**Platform:** Arduino Uno  
**File:** `Twisting_counter_maju_mundur/Twisting_counter_maju_mundur.ino`

### Library
```cpp
#include <Keypad.h>
#include <LiquidCrystal.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Motor output | A1 |
| Tombol Start | 2 |
| Sensor IR | 3 |
| Output tambahan | A3 |
| Switch tambahan | A2 |
| Keypad baris | 12, 11, 10, 9 |
| Keypad kolom | 8, 7, 6, 5 |
| LCD RS, E, D4–D7 | 14, 15, 16–19 |
| LCD Backlight | 4 |

### Cara Kerja
Sistem dua fase: tombol **A** mengatur target putaran fase maju (`targetTurnsA`) dan tombol **B** mengatur fase mundur (`targetTurnsB`). Mesin menyelesaikan fase A dulu kemudian berlanjut ke fase B.

---

## 11. Twisting Counter Maju Mundur V1

**Platform:** Arduino Uno  
**File:** `Twisting_counter_maju_mundur_V1/Twisting_counter_maju_mundur_V1.ino`

Versi peningkatan dari Twisting Counter Maju Mundur dengan penambahan penyimpanan EEPROM untuk persistensi data target putaran dan penanganan interrupt yang lebih baik.

---

## 12. Twisting Counter Maju Mundur LCD I2C

**Platform:** Arduino Uno  
**File:** `Twisting_counter_maju_mundur_LCD12C-1/Twisting_counter_maju_mundur_LCD12C-1.ino`  
**File (varian counter):** `Twisting_counter_maju_mundur_LCD12C_counter/.../Twisting_counter_maju_mundur_LCD12C_counter.ino`

### Library
```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
```

### Perbedaan dengan Versi Sebelumnya
Menggunakan LCD I2C (alamat `0x27`) sebagai pengganti LCD parallel, sehingga hanya memerlukan 2 kabel data (SDA/SCL) ke LCD. Konfigurasi keypad dan motor tetap sama.

---

## 13. Twisting Control Multi Time

**Platform:** Arduino Mega 2560  
**File:** `twisting_control_multi_time/twisting_control_multi_time.ino`

### Library
```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Relay (motor output) | 2 |
| Switch START | 3 |
| Keypad baris | 22, 24, 26, 28 |
| Keypad kolom | 30, 32, 34, 36 |
| LCD I2C SDA/SCL | 20/21 |

### Cara Kerja
1. Sistem memiliki 4 mode waktu (A, B, C, D) yang dapat diatur secara independen via keypad (default: A=1000 ms, B=1300 ms, C=900 ms, D=0 ms).
2. Tekan switch START → relay aktif selama durasi mode saat ini → relay mati → mode berikutnya.
3. Siklus berjalan berulang: A → B → C → A → …
4. Durasi tiap mode dapat diubah melalui keypad saat sistem idle; nilai dimasukkan dalam milidetik.
5. Menggunakan `millis()` (non-blocking) untuk menjaga akurasi waktu tanpa `delay()`.

### Parameter Default
| Mode | Durasi Default |
|---|---|
| A | 1000 ms |
| B | 1300 ms |
| C | 900 ms |
| D | 0 ms |

---

## 14. Twisting Control Multi Time – Varian 16/24/32/36 Waktu & 3 Motor

**Platform:** Arduino Mega 2560  
**File:** `twisting_control_multi_time_16_time/`, `twisting_control_multi_time_24_time/`, `twisting_control_multi_time_32_time/`, `twisting_control_multi_time_36time/`, `twisting_control_multi_time_3_Motor/`

Varian-varian ini merupakan perluasan dari **Twisting Control Multi Time** dengan jumlah slot waktu yang lebih banyak (16, 24, 32, atau 36 slot) dan/atau mendukung kontrol 3 motor secara independen. Prinsip kerja, library, dan koneksi dasar sama; perbedaan utama ada pada ukuran array konfigurasi waktu dan jumlah pin relay.

---

## 15. Counter 4 Digit

**Platform:** Arduino Uno/Nano  
**File:** `Counter_4_digit/Counter_4_digit.ino`

### Library
```cpp
#include <TM1637Display.h>
#include <EEPROM.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| TM1637 CLK | 2 |
| TM1637 DIO | 3 |
| Tombol UP | 6 |
| Tombol DOWN | 7 |

### Cara Kerja
1. Tombol UP menambah counter satu per satu; tekan panjang (> 2000 ms) → reset ke 0.
2. Tombol DOWN mengurangi counter.
3. Nilai counter disimpan di EEPROM alamat 0 sehingga tidak hilang saat restart.
4. Debounce 200 ms diterapkan pada kedua tombol.

---

## 16. Counter UP 30

**Platform:** Arduino Uno  
**File:** `Counter_UP_30/Counter_UP_30.ino`

Counter sederhana yang menghitung hingga 30, ditampilkan pada layar. Digunakan untuk keperluan penghitungan produksi dengan batas tetap.

---

## 17. Counter Up by Time

**Platform:** Arduino Uno/Nano  
**File:** `coba_counter_up_by_time/coba_counter_up_by_time.ino`

### Library
```cpp
#include <Keypad.h>
```

### Cara Kerja
1. Counter bertambah otomatis setiap interval waktu yang dapat diatur (default 3 menit = 180.000 ms).
2. Keypad digunakan untuk mengatur interval (dalam menit) dan mereset counter.
3. Timing menggunakan `millis()` (non-blocking) sehingga keypad tetap responsif selama menghitung.

---

## 18. 5-Circuit Continuity Tester

**Platform:** Arduino Uno/Nano  
**File:** `cek_5_circuit/cek_5_circuit.ino`

### Library
```cpp
#include <LiquidCrystal.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| End A (output, 5 pin) | 2, 3, 4, 5, 6 |
| End B (input, 5 pin) | A4, A3, A2, A1, A0 |
| LED Indikator | 13 |
| LCD RS, E, D4–D7 | 7, 8, 9–12 |

### Cara Kerja
1. Program mengirim sinyal HIGH satu per satu ke setiap pin End A.
2. Semua pin End B dibaca untuk melihat di mana sinyal diterima.
3. **GOOD**: Sinyal hanya diterima di pin End B yang bersesuaian.
4. **OPEN**: Tidak ada sinyal yang diterima sama sekali.
5. **CROSS**: Sinyal diterima di pin End B yang tidak bersesuaian (short circuit).
6. LCD menampilkan "PASSED" jika semua 5 sirkuit GOOD, atau menampilkan detail kegagalan.

---

## 19. EC 10 Circuit Tester

**Platform:** Arduino Mega 2560  
**File:** `ec_10_circuit/ec_10_circuit.ino`

### Library
```cpp
#include <LiquidCrystal.h>
#include <EEPROM.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| End A (10 pin) | 22–31 |
| End B (10 pin) | A9–A0 |
| LED Indikator (10 LED) | 42–51 |
| Tombol STOP (10 tombol) | 32–41 |
| LCD RS, E, D4–D7 | 7, 8, 9–12 |

### Cara Kerja
Sama seperti 5-Circuit Tester namun untuk 10 sirkuit sekaligus. Setiap sirkuit memiliki LED indikator dan tombol STOP independen. Pass counter tersimpan di EEPROM dengan mekanisme Magic Key untuk reset.

### Reset Pass Counter
1. Ubah `MAGIC_KEY` menjadi `RESET_MAGIC_KEY` (0x87654321) dan upload.
2. Ubah kembali ke nilai asli dan upload lagi → pass counter direset ke 0.

---

## 20. EC 30 Circuit Tester

**Platform:** Arduino Mega 2560  
**File:** `ec_30_circuit/ec_30_circuit.ino`

Perluasan dari EC 10 Circuit Tester untuk mendukung 30 sirkuit. Membutuhkan lebih banyak pin digital Arduino Mega. Library dan prinsip kerja sama persis.

---

## 21. Update 1-10 Ceker (10-Circuit Checker)

**Platform:** Arduino Mega 2560  
**File:** `update_1_10_ceker/update_1_10_ceker.ino`

### Library
```cpp
#include <LiquidCrystal.h>
#include <EEPROM.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| End A (10 pin) | 22–31 |
| End B (10 pin) | A9–A0 |
| LED Indikator (10 LED) | 42–51 |
| Tombol (10 tombol) | 32–41 |
| LCD RS, E, D4–D7 | 7, 8, 9–12 |

Versi terbaru (update) dari EC 10 Circuit Tester dengan perbaikan logika deteksi dan tampilan LCD yang lebih informatif.

---

## 22. Cek Sambungan EC (37-Pin Tester)

**Platform:** Arduino Mega 2560  
**File:** `Cek_sambungan_EC/Cek_sambungan_EC.ino`

### Library
```cpp
#include <TM1637Display.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| TM1637 CLK | 21 |
| TM1637 DIO | 20 |
| Buzzer | A8 |
| Pin pengujian (37 pin) | 22–53, 2–6 |

### Cara Kerja
1. Semua 37 pin dikonfigurasi sebagai INPUT_PULLUP.
2. Program scan setiap pin; jika pin terhubung ke GND, tampilkan kode **A/B + nomor** pada display TM1637.
3. Sistem penomoran alternating: pin 22 → "A1", pin 23 → "B1", pin 24 → "A2", dst.
4. Buzzer berbunyi setiap kali ada deteksi koneksi.

---

## 23. GG5-0986 EC Connection Tester

**Platform:** Arduino Mega 2560  
**File:** `GG5-0986/GG5-0986.ino`

Alat pengujian sambungan konektor EC model GG5-0986 dengan LCD untuk menampilkan hasil dan LED indikator per sirkuit. Konfigurasi pin dan prinsip kerja serupa dengan seri EC tester lainnya.

---

## 24. Pin Tester (cek_pin)

**Platform:** Arduino Mega 2560  
**File:** `cek_pin/cek_pin.ino`

### Library
```cpp
#include <TM1637Display.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| TM1637 CLK | (sesuai definisi dalam kode) |
| TM1637 DIO | (sesuai definisi dalam kode) |
| Pin yang diuji | Semua pin I/O tersedia |

### Cara Kerja
1. Semua pin digital Arduino dikonfigurasi sebagai INPUT_PULLUP.
2. Program scan semua pin secara berurutan.
3. Jika pin terhubung ke GND → tampilkan nomor pin pada display TM1637.
4. Display otomatis clear setelah 1 detik jika koneksi terputus.

---

## 25. Pin Tester MAX-37 (cek_pin_MAX-37)

**Platform:** Arduino Mega 2560  
**File:** `cek_pin_MAX-37/cek_pin_MAX-37.ino`

### Library
```cpp
#include <LiquidCrystal.h>
#include <EEPROM.h>
```

### Cara Kerja
Varian Pin Tester untuk sensor kit MAX-37 dengan tambahan:
- Pass counter tersimpan di EEPROM.
- Sistem lock setelah mencapai jumlah pengujian maksimum (`MAX_PASS_COUNT = 90.000`).
- Reset dengan metode Magic Key yang sama seperti EC tester.

---

## 26. Switch 3-Pole Tester

**Platform:** Arduino Uno/Mega  
**File:** `cek_switch_3p/cek_switch_3p.ino`

### Library
```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Output supply +5V | 2 |
| Input test tahap 1 (COM→NC) | 3 |
| Input test tahap 2 (COM→NO) | 4 |
| Buzzer | 5 |
| LCD I2C (SDA/SCL) | 20/21 (Mega) |

### Cara Kerja
1. Sistem memvalidasi urutan koneksi switch: **Tahap 1** (COM→NC) HARUS terjadi sebelum **Tahap 2** (COM→NO).
2. Jika urutan benar → LCD "GOOD" + buzzer berbunyi.
3. Jika NO aktif sebelum NC → LCD "ERROR" (urutan salah).

---

## 27. Switch 3-Kaki Tester

**Platform:** Arduino Uno  
**File:** `cek_switch_3_kaki/cek_switch_3_kaki.ino`

Varian lebih sederhana dari Switch 3-Pole Tester tanpa LCD I2C; menggunakan LED indikator dan/atau Serial Monitor untuk menampilkan hasil.

---

## 28. Switch 3P dengan LCD

**Platform:** Arduino Uno/Mega  
**File:** `cek_switch3p_dengan_lcd/cek_switch3p_dengan_lcd.ino`

### Library
```cpp
#include <LiquidCrystal.h>
```

Varian Switch 3P Tester dengan LCD parallel 16×2 (bukan I2C) yang menampilkan posisi switch yang terhubung secara real-time.

---

## 29. Sensor Warna Hitam Putih

**Platform:** Arduino Uno  
**File:** `Sensor warna HITAM PUTIH/Sensor_warna_hitam_putih/Sensor_warna_hitam_putih.ino`

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Sensor IR analog | A6 |
| Relay output | 3 |

### Cara Kerja
1. Sensor IR reflektif membaca intensitas cahaya yang dipantulkan permukaan.
2. Nilai ADC dibandingkan dengan dua threshold: `minThreshold = 24` dan `threshold = 22`.
3. Nilai < minThreshold → relay OFF (permukaan terlalu dekat/gelap).
4. Nilai > threshold → "Hitam terdeteksi" → relay ON.
5. Nilai antara threshold → "Putih terdeteksi" → relay OFF.

---

## 30. ESP32 Kamera – Deteksi Warna Hitam Putih

**Platform:** ESP32-CAM  
**File:** `esp32_kamera_cek_warna_hitam_putih/esp32_kamera_cek_warna_hitam_putih.ino`

### Library
```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SD_MMC.h>
```

### Konfigurasi Jaringan
| Parameter | Nilai |
|---|---|
| WiFi SSID | `ESP32-OV5640` |
| WiFi Password | `12345678` |
| Web server port | 80 |

### Cara Kerja
1. ESP32-CAM membuat Access Point WiFi.
2. Server HTTP menyajikan antarmuka web untuk monitoring dan kontrol kamera.
3. Frame kamera diproses secara real-time: konversi ke grayscale → thresholding → flood fill untuk mendeteksi dan menghitung objek.
4. Threshold dan ukuran minimum objek dapat diatur melalui web interface.
5. Mendukung mode Smart Counting untuk objek serupa berdasarkan aspect ratio.

### Parameter Image Processing
| Parameter | Default |
|---|---|
| Threshold (hitam/putih) | 128 |
| Ukuran minimum objek | 50 pixel |
| Ukuran maksimum objek | 5000 pixel |
| Toleransi aspect ratio | 0,3 |
| Interval penghitungan | 1000 ms |

---

## 31. Kontrol Pompa Air

**Platform:** Arduino Uno  
**File:** `kontol_pompa_air/kontol_pompa_air.ino`

### Library
```cpp
#include <LiquidCrystal.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Sensor aliran air (flow sensor) | 2 (interrupt) |
| Katup input tandon | 3 |
| Katup output tandon | 4 |
| Pompa air | 5 |
| Level switch 1 (bawah) | 6 |
| Level switch 2 (atas) | 7 |
| LCD RS, E, D4–D7 | 8, 9, 10–13 |
| LCD Backlight | A14 |

### Cara Kerja
1. ISR pada pin 2 menghitung pulsa dari flow sensor YF-S201 untuk mengukur laju aliran air.
2. Level switch memantau ketinggian air di dalam tandon.
3. Pompa aktif otomatis saat level switch bawah (LOW) dan mati saat level switch atas (LOW).
4. LCD menampilkan status dan progress bar kustom.
5. Konstanta laju aliran: `flowRateConstant = 7.5`.

---

## 32. Kontrol Pompa Air LCD 20x4

**Platform:** Arduino Uno  
**File:** `kontol_pompa_air_LCD20X4/kontol_pompa_air_LCD20X4.ino`

Varian Kontrol Pompa Air dengan LCD 20×4 untuk tampilan informasi yang lebih lengkap. Konfigurasi hardware dan prinsip kerja sama dengan versi 16×2.

---

## 33. Guyur WC (Auto Flush Toilet)

**Platform:** Arduino Mega 2560  
**File:** `guyur_WC/guyur_WC.ino`

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Sensor PIR | 2 (interrupt) |
| Output (valve/pompa) | 41 |

### Cara Kerja
1. Sensor PIR menggunakan interrupt (RISING) untuk mendeteksi gerakan.
2. Setiap gerakan terdeteksi → timer direset.
3. Setelah `waktutunda` detik (default 15 detik) tanpa gerakan → output aktif HIGH selama 10 detik (flush) → output mati.

---

## 34. Timer Relay WP

**Platform:** Arduino Uno  
**File:** `Timer_relay_WP/Timer_relay_WP.ino`

### Library
```cpp
#include <TM1637Display.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Trigger input | 2 |
| Relay output | 8 |
| TM1637 CLK | 3 |
| TM1637 DIO | 4 |

### Cara Kerja
1. Trigger input (LOW → aktif) memulai countdown timer.
2. Relay aktif selama durasi yang diprogram.
3. Display TM1637 menampilkan sisa waktu dalam format detik.
4. Debounce 50 ms untuk trigger input.

---

## 35. Relay WFA (Sound-Triggered Relay)

**Platform:** Arduino Uno  
**File:** `relay_wfa/relay_wfa.ino`

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Sensor suara (analog) | A7 |
| Output relay/transistor | A1 |

### Cara Kerja
1. Nilai ADC dari sensor suara dibaca setiap 100 ms.
2. Jika nilai > 800 (threshold) → output HIGH selama 10 detik → output LOW.
3. Threshold dapat disesuaikan dalam kode.

---

## 36. Gerak Maju Mundur Solenoid

**Platform:** Arduino Uno  
**File:** `gerak_gerak_maju_mundur_solenoid/gerak_gerak_maju_mundur_solenoid.ino`

### Library
```cpp
#include <TM1637Display.h>
```

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| Solenoid output | 8 |
| Potensiometer | A0 |
| TM1637 CLK | 2 |
| TM1637 DIO | 3 |

### Cara Kerja
1. Nilai potensiometer (0–1023) dikonversi ke waktu delay (50–1000 ms).
2. Solenoid aktif selama delay tersebut, kemudian mati selama delay yang sama.
3. Display TM1637 menampilkan nilai ADC potensiometer secara real-time.
4. Kecepatan gerak solenoid diatur dengan memutar potensiometer.

---

## 37. Alat Hitung (ESP32 Camera Counter)

**Platform:** ESP32-S3  
**File:** `Alat_Hitung/alat hitung/alat_hitung/ESP32S3_Camera_Counter_Fixed/ESP32S3_Camera_Counter_Fixed.ino`

### Library
```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
```

### Cara Kerja
Sistem penghitungan objek berbasis kamera ESP32-S3 dengan antarmuka web. Menggunakan image processing (thresholding + blob detection) untuk menghitung jumlah objek yang melewati frame kamera. Hasil ditampilkan melalui halaman web yang dapat diakses dari perangkat lain di jaringan WiFi yang sama.

---

## 38. Hitung Konektor (ESP32-S3 File Manager)

**Platform:** ESP32-S3  
**File:** `Hitung_Konektor/Hitung_Konektor_Modular/Hitung_Konektor_Modular.ino`

### Library
```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SD_MMC.h>
```

### Cara Kerja
Sistem modular berbasis ESP32-S3 yang menggabungkan fungsi kamera dan file manager berbasis web. Mendukung streaming video, pengambilan gambar, penyimpanan ke SD card, dan manajemen file melalui antarmuka web.

---

## 39. LED Running

**Platform:** Arduino Uno  
**File:** `led_running/led_running.ino`

### Konfigurasi Pin

| Fungsi | Pin |
|---|---|
| LED 1–7 | 7–13 |

### Cara Kerja
LED menyala bergantian satu per satu (running light) menggunakan `delay()` di dalam `setup()`. Setiap LED menyala selama 200–1000 ms kemudian berpindah ke LED berikutnya, menciptakan efek cahaya berjalan.

---

## 40. Test Running Text

**Platform:** Arduino Mega 2560  
**File:** `test_running_text/test_running_text.ino`

### Library
```cpp
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
```

Menampilkan teks berjalan (scrolling text) pada panel RGB LED P5. Digunakan untuk pengujian panel LED sebelum diintegrasikan ke sistem Andon atau Counter.

---

## 41. ReadAndWrite (RFID)

**Platform:** Arduino Mega 2560  
**File:** `ReadAndWrite/ReadAndWrite.ino`

### Library
```cpp
#include <SPI.h>
#include <MFRC522.h>
```

### Cara Kerja
Program demonstrasi untuk membaca dan menulis data ke kartu RFID MIFARE Classic menggunakan MFRC522. Mendukung operasi baca/tulis pada block data kartu RFID. Berguna untuk:
- Membaca UID kartu baru untuk didaftarkan ke sistem door lock.
- Menulis data kustom ke sektor kartu MIFARE.

---

## 42. JAM (Sketch Jam LCD)

**Platform:** Arduino Uno  
**File:** `JAM/sketch_jan21a/sketch_jan21a.ino`

### Library
```cpp
#include <LiquidCrystal.h>
```

Versi jam digital yang lebih sederhana menggunakan LCD 16×2 parallel (bukan TM1637). Menampilkan jam, menit, dan detik pada baris pertama LCD dan informasi tambahan pada baris kedua.

---

## Ringkasan Library Arduino

| Library | Proyek yang Menggunakan |
|---|---|
| `LiquidCrystal.h` | Line Follower, Kontrol Pompa, 5-Circuit Tester, EC Tester, Twisting Counter, JAM |
| `LiquidCrystal_I2C.h` | Twisting Control Multi Time, Switch 3P, Twisting LCD I2C |
| `TM1637Display.h` | Jam Digital, Counter 4 Digit, Timer Relay WP, Cek Sambungan EC, Gerak Solenoid |
| `MFRC522.h` | Multi RFID Door Lock, ReadAndWrite |
| `Keypad.h` | Andon P5, Twisting Counter, Twisting Control, Counter by Time |
| `Adafruit_GFX.h` + `RGBmatrixPanel.h` | Andon P5, Counter Panel P5, Test Running Text |
| `U8g2lib.h` | Measurino Length |
| `WiFi.h` + `WebServer.h` | ESP32 Camera, Alat Hitung, Hitung Konektor |
| `esp_camera.h` | ESP32 Camera, Alat Hitung, Hitung Konektor |
| `SPI.h` | RFID projects |
| `EEPROM.h` | Counter 4 Digit, Twisting Counter V1, EC Tester, cek_pin MAX-37 |
| `Wire.h` | LCD I2C projects, rotary encoder kabel |

---

## Ringkasan Platform Mikrokontroler

| Platform | Proyek |
|---|---|
| **Arduino Uno/Nano** | Jam Digital, Measurino, Sensor Warna, Timer Relay, Relay WFA, Gerak Solenoid, LED Running, Counter 4 Digit, cek_5_circuit, Twisting Counter |
| **Arduino Mega 2560** | Line Follower Robot, Multi RFID, Andon P5, Counter Panel P5, EC Tester (10/30), Twisting Control Multi Time, Pin Tester, cek_pin MAX-37, EC 37-pin, GG5-0986, Test Running Text |
| **ESP32-CAM / ESP32-S3** | ESP32 Kamera, Alat Hitung, Hitung Konektor |

---

*Dokumentasi ini dibuat berdasarkan analisis kode sumber setiap proyek. Untuk informasi lebih lanjut, lihat komentar dalam file `.ino` masing-masing proyek.*
