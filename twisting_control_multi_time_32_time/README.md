# twisting_control_multi_time_32_time — Twisting Control Multi-Time System (32 Mode)

Dokumentasi ini untuk sketch:
- `twisting_control_multi_time_32_time/twisting_control_multi_time_32_time.ino`
- Repo: `sembre/Arduino-Project`

Sistem ini adalah kontrol timer **32 mode** untuk mesin twisting/automation berbasis **Arduino Mega 2560** dengan penyimpanan EEPROM, proteksi boot counter, relay safety, dan emergency stop.

## Fitur Utama
- 32 mode timer (A1–A8, B1–B8, C1–C8, D1–D8)
- Penyimpanan permanen ke **EEPROM** (magic number + data validation)
- **Boot Counter Protection**: limit `7000` boot → sistem terkunci
- LCD **20x4 I2C** (alamat umum 0x27)
- TM1637 4-digit 7-seg untuk indikator mode/status
- Keypad 4x4 untuk setting mode dan waktu
- Switch start/next (PIN 53) dengan debounce dan constant-switch mode
- **Emergency Stop** (PIN 51) prioritas tertinggi
- Relay safety: **OFF selalu instan**, ON dibatasi 100ms (ratelimit ON-only)

## Kebutuhan Hardware
- Arduino Mega 2560
- LCD 20x4 I2C (0x27)
- TM1637 display
- Keypad 4x4 matrix
- Relay module (aktif LOW)
- Switch manual start/next
- Tombol emergency stop (push button) untuk PIN 51
- Power supply stabil (disarankan industrial) + wiring rapi (EMI)

## Pin Mapping (Wiring)
### Output
- Relay: `D2` (`relayPin = 2`)
  - Logika: `LOW = ON`, `HIGH = OFF`

### Input
- Switch start/next: `D53` (`switchPin = 53`, `INPUT_PULLUP`)
  - `LOW` = ditekan
- Emergency stop: `D51` (`emergencyStopPin = 51`, `INPUT_PULLUP`)
  - `LOW` = ditekan

### TM1637
- CLK: `D4`
- DIO: `D5`

### Keypad 4x4
- Row: `22, 24, 26, 28`
- Col: `30, 32, 34, 36`
- Layout tombol:
  - `1 2 3 A`
  - `4 5 6 B`
  - `7 8 9 C`
  - `* 0 # D`

### LCD I2C
- Mega: SDA `D20`, SCL `D21`
- Address: `0x27`

## Cara Pakai (Ringkas)
### Setting waktu mode
1. Tekan `#` (masuk input mode)
2. Tekan `A/B/C/D` (pilih grup)
3. Tekan `1..8` (pilih mode)
4. Input angka waktu dalam **ms**
5. Tekan `#` untuk simpan permanen ke EEPROM

### Navigasi tampilan tabel mode (saat idle)
- `D`: scroll grup (A–C vs D)
- `C`: geser kolom (1–4 vs 5–8)

### Toggle constant switch mode
- `B` (idle): toggle `constantSwitchMode`
  - Indikator pojok kanan atas LCD: `K`=Konstan, `N`=Normal

### Start / Next
- Tekan switch (D53):
  - Idle → start mode pertama yang `modeTimes>0`
  - Setelah mode selesai → tekan lagi untuk start `nextMode`

## Emergency Stop (PIN 51)
Emergency stop dicek **paling awal** di `loop()`.
- Tekan 1x: sistem LOCK, relay dipaksa OFF, tampilan peringatan
- Tekan lagi: reset state runtime (kembali ke menu) tanpa menghapus data EEPROM/boot counter

## Boot Counter Lock (7000 boot)
- `bootCount` naik setiap device boot
- Jika `bootCount >= 7000` → `systemLocked = true`:
  - LCD menampilkan status lock
  - Relay dipastikan OFF
  - Input diabaikan

## Dokumen Teknis
Lihat manual teknis lengkap di:
- `twisting_control_multi_time_32_time/TECHNICAL_MANUAL.md`

## Lisensi
Mengikuti header sketch: **Creative Commons Attribution (CC BY)**.