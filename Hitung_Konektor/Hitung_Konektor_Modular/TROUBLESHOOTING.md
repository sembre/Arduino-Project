# Troubleshooting Guide - SD Card Connection Issues

## 🔧 Perbaikan untuk Masalah SD Card yang Tidak Stabil

### Masalah yang Diperbaiki:

1. ❌ **SD Card selalu disconnect**
2. ❌ **Upload file gagal dengan "Failed to fetch"**
3. ❌ **Capture image tidak berfungsi**

### ✅ Solusi yang Diterapkan:

#### 1. **Enhanced SD Card Initialization**

- **Auto-retry mechanism**: 3x retry untuk setiap mode (MMC dan SPI)
- **Connection validation**: Test akses root directory setelah initialization
- **Periodic health check**: Cek koneksi SD card setiap 5 detik
- **Lower frequency**: SPI mode menggunakan 4MHz untuk stabilitas

#### 2. **Improved Error Handling**

- **Robust upload handler**: Retry mechanism untuk file operations
- **File verification**: Cek ukuran file setelah upload
- **Progress tracking**: Monitor upload progress dengan logging
- **Better error messages**: Pesan error yang lebih jelas

#### 3. **Auto-Reconnection Features**

- **Force reconnect function**: Manual reconnect SD card dari web interface
- **Automatic recovery**: Auto-reconnect saat deteksi koneksi lost
- **Connection monitoring**: Real-time monitoring status SD card

#### 4. **Web Interface Improvements**

- **Reconnect button**: Tombol "🔗 Reconnect SD" untuk manual reconnect
- **Extended timeouts**: Upload timeout 30 detik untuk file besar
- **Better progress feedback**: Visual feedback selama upload dan reconnect

### 🚀 Cara Menggunakan Fitur Baru:

#### **Manual SD Card Reconnect:**

1. Jika file manager menampilkan error atau "disconnected"
2. Klik tombol **"🔗 Reconnect SD"**
3. Tunggu konfirmasi reconnection
4. Refresh file list dengan tombol **"🔄 Refresh"**

#### **Upload File yang Lebih Stabil:**

1. Pilih file dengan tombol **"📤 Upload"**
2. Monitor progress indicator
3. Jika upload gagal, coba reconnect SD card terlebih dahulu
4. Ulangi upload setelah SD card terhubung kembali

#### **Monitoring Connection Status:**

- Perhatikan status di kanan atas (🟢 Connected / 🔴 Disconnected)
- System info (`/system_info`) menampilkan detail SD card
- Serial monitor menampilkan detailed logging

### 🔍 Debugging & Monitoring:

#### **Serial Monitor Messages:**

```
🔄 Initializing SD Card...
✅ SD_MMC initialized successfully
📂 Listing files in: /
📤 Upload starting: image.jpg (1234567 bytes)
📊 Upload progress: 500000 bytes written
✅ Upload completed successfully
✅ Upload verification successful
```

#### **Error Indicators:**

```
❌ SD Card connection lost, attempting reconnection...
❌ Failed to open directory, retrying... (2 left)
❌ Upload write error: 500/1024 bytes
⚠️ Upload verification failed - file size mismatch
```

### 📋 Hardware Checklist:

#### **SD Card Requirements:**

- ✅ Format: FAT32 (recommended)
- ✅ Speed: Class 10 atau lebih tinggi
- ✅ Size: Maksimal 32GB untuk kompatibilitas optimal
- ✅ Physical connection: Pastikan SD card terpasang dengan benar

#### **Power Supply:**

- ✅ Voltage: 5V stabil dengan minimal 2A
- ✅ Cable: Gunakan kabel USB yang berkualitas baik
- ✅ Connection: Pastikan tidak ada koneksi yang loose

### 🎯 Tips Optimalisasi:

#### **Untuk Upload File Besar:**

1. Upload file satu per satu untuk file > 1MB
2. Gunakan format yang terkompresi (JPG vs PNG)
3. Monitor serial output untuk progress tracking
4. Jika gagal, coba reconnect SD card

#### **Untuk Stabilitas Maksimal:**

1. Restart ESP32 jika masalah persisten
2. Cek kualitas SD card (ganti jika perlu)
3. Pastikan power supply stabil
4. Hindari disconnect fisik saat operasi file

### 🔄 Recovery Steps:

#### **Jika Upload Tetap Gagal:**

1. **Reconnect SD Card**: Klik tombol reconnect
2. **Check Serial Monitor**: Lihat error messages detail
3. **Restart ESP32**: Power cycle device
4. **Format SD Card**: Format ulang dengan FAT32
5. **Check Hardware**: Periksa koneksi fisik

#### **Jika File Manager Tidak Load:**

1. **Refresh Browser**: F5 atau Ctrl+F5
2. **Reconnect WiFi**: Disconnect dan connect ulang ke ESP32-CAM-AP
3. **Check Device Status**: Pastikan ESP32 menyala dan responsive
4. **Serial Monitor**: Cek ada error di system initialization

---

**Status Perbaikan:** ✅ **FIXED - Fully Functional**  
**Last Updated:** August 19, 2025  
**Version:** v2.0 - Enhanced Stability
