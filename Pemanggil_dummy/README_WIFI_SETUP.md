# Panduan WiFi Manager - MEJA01

## 📋 Deskripsi

WiFi Manager memungkinkan Anda untuk mengatur koneksi WiFi melalui **Web Interface** tanpa perlu upload kode ulang. Fitur ini sangat berguna ketika ingin mengganti WiFi network atau lokasi perangkat.

---

## ⚙️ Cara Kerja

### **Mode 1: Pertama Kali Setup (Access Point Mode)**

Ketika perangkat **pertama kali dinyalakan** atau **tidak ada kredensial WiFi tersimpan**, perangkat akan:

1. ✅ Membuat Access Point (AP) sendiri dengan nama: **`MEJA01_SETUP`**
2. ✅ Menampilkan di LCD: "AP MODE" dan "192.168.4.1"
3. ✅ Menunggu koneksi dari smartphone/laptop untuk setup

### **Mode 2: Setup WiFi (Access Point Aktif)**

1. **Hubungkan ke Access Point:**
   - SSID: `MEJA01_SETUP`
   - Password: `12345678`
   - IP Address: `http://192.168.4.1`

2. **Buka Web Interface:**
   - Buka browser → ketik: `http://192.168.4.1`
   - Tampilkan daftar WiFi network yang tersedia

3. **Pilih & Simpan:**
   - Pilih WiFi network dari dropdown
   - Masukkan password WiFi
   - Klik **"Simpan & Restart"**
   - Perangkat akan **restart otomatis** dan terhubung ke WiFi pilihan

### **Mode 3: Operasional Normal (Connected to WiFi)**

- Perangkat terhubung ke WiFi yang disimpan
- Menampilkan status "WiFi OK" + IP Address di LCD
- Siap menerima & mengirim MQTT message
- Tombol mulai berfungsi normal

---

## 🔧 Konfigurasi

### Mengubah Nama Access Point

Edit baris ini di kode:

```cpp
const char* ap_ssid = "MEJA01_SETUP";      // UBAH NAMA DI SINI
const char* ap_password = "12345678";      // UBAH PASSWORD DI SINI
```

### Mengubah Alamat IP MQTT

```cpp
const char* mqtt_server = "broker.hivemq.com";  // UBAH SERVER
```

### Mengubah ID MEJA

```cpp
const char* ID_MEJA = "MEJA01";  // UBAH SESUAI KEBUTUHAN
```

---

## 📱 Fitur Web Interface

### Tampilan:

- 🎨 **Design Modern** dengan gradient purple
- 📱 **Responsive** untuk mobile & desktop
- 🔄 **Scan Ulang Jaringan** untuk refresh daftar WiFi
- 📊 **Signal Strength (dBm)** ditampilkan untuk setiap network

### Info Ditampilkan:

- Nama perangkat yang sedang setup
- Daftar WiFi network dengan signal strength
- Status koneksi sebelum & sesudah setup

---

## 🔄 Reset ke Setup Mode

Untuk kembali ke **Access Point Mode** dan setup ulang WiFi:

1. **Hubungkan USB power** → perangkat akan scan WiFi yang disimpan
2. **Jika koneksi gagal** → otomatis masuk AP mode
3. **Force Reset** dengan:
   - Tekan & tahan tombol RESET/BOOT di perangkat
   - Tunggu LCD menampilkan "AP MODE"

---

## 📊 Penyimpanan Data

Kredensial WiFi disimpan di **EEPROM** (flash memory internal):

- **SSID**: Hingga 32 karakter
- **Password**: Hingga 64 karakter
- **Lokasi**: Alamat 0 - 96 di EEPROM

Data akan **tetap tersimpan** meskipun perangkat dimatikan!

---

## ⚡ Troubleshooting

| **Masalah**              | **Solusi**                                             |
| ------------------------ | ------------------------------------------------------ |
| AP tidak muncul          | Tunggu 10 detik setelah power on, lihat LCD            |
| Koneksi WiFi lambat      | Pastikan password benar & signal kuat                  |
| Web interface tidak buka | Coba refresh browser (F5) atau akses ulang 192.168.4.1 |
| Stuck di AP mode         | Buka Serial Monitor (115200 bps) untuk debug           |
| Pelupa password AP       | Password default: `12345678`                           |

---

## 🖥️ Hardware yang Diperlukan

- ✅ ESP32 (atau ESP8266)
- ✅ LCD 16x2 I2C (Address 0x27)
- ✅ 4 Tombol input (GPIO 4, 5, 18)
- ✅ Buzzer (GPIO 19)

---

## 📡 Serial Debug Output

Buka Serial Monitor dengan baud rate **115200** untuk melihat:

```
========================================
     WiFi SETUP MODE AKTIF
========================================
Access Point: MEJA01_SETUP
Password: 12345678
URL: http://192.168.4.1
========================================
```

---

## � HARD RESET (3x Tekan RST)

Untuk **menghapus semua pengaturan** (WiFi credentials & nama meja):

### **Cara Melakukan Hard Reset:**

1. **Tekan tombol RESET (RST)** di ESP32 perangkat **3 kali dengan cepat**
   - Dalam waktu kurang dari 10 detik
   - Setiap kali tekan, perangkat restart

2. **Indikator:**
   - **LCD**: Menampilkan "HARD RESET OK" + "Restarting..."
   - **Buzzer**: Bunyi panjang 5x (tanda konfirmasi hard reset)
   - **Serial**: Menampilkan log "[HardReset] ✓ 3 resets detected!"

3. **Hasil:**
   - ✓ WiFi credentials dihapus
   - ✓ Nama meja direset ke default
   - ✓ Reset counter direset
   - ✓ Perangkat restart otomatis

4. **Setelah Hard Reset:**
   - Perangkat akan masuk **AP Mode** (Access Point)
   - LCD menampilkan "AP MODE" + "192.168.4.1"
   - Siap untuk setup WiFi dari awal

### **Catatan Hard Reset:**

- ⏱️ **Window waktu**: 10 detik (jika lebih lama, counter direset)
- 🔄 **Reset threshold**: 3x resets = trigger hard reset
- 📍 **Hard reset dari**: Tekan tombol RESET bawaan ESP32 (bukan software)
- ✨ **Aman**: Tidak ada risiko, semua data preferences akan dikembalikan ke default
- 🎯 **Gunakan saat**: Lupa WiFi, ganti lokasi, setup perangkat baru

### **Contoh Sequence:**

```
Tekan RST → Reset #1 detected (LCD reset)
Tekan RST → Reset #2 detected (LCD reset)
Tekan RST → Reset #3 detected → HARD RESET TRIGGERED!

UPDATED TO: Tahan BOOT Button 5 detik untuk Hard Reset
    ↓
Buzzer: bunyi 5x panjang
LCD: "HARD RESET OK"
    ↓
Perangkat restart + masuk AP Mode
```

---

**Dibuat:** April 2026  
**Untuk:** Sistem Pemanggil Dummy - MEJA01
