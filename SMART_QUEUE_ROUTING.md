# Smart Queue Routing System - Dokumentasi Lengkap

## 🎯 Overview

Sistem baru yang memungkinkan **Pemanggil** untuk secara otomatis menemukan **Penerima** yang sesuai berdasarkan **jenis panggilan**, dan menampilkan **antrian real-time** dari Penerima tersebut.

**Fitur Utama:**

- ✅ Pemanggil menentukan antrian hanya dari Penerima yang cocok filter
- ✅ Tampil nama Penerima target di dashboard Pemanggil
- ✅ Tampil posisi antrian dari Penerima yang dipilih
- ✅ Web interface untuk manage daftar Penerima
- ✅ Real-time matching berdasarkan call type

---

## 📊 Alur Sistem

### User Flow - Pemanggil

```
1. Buka Pemanggil Dashboard
   ↓
2. Klik "👥 Kelola Penerima"
   ↓
3. Register Penerima:
   - Nama: MEJA02
   - IP: 192.168.1.105
   - Filter: PAKU_PALU, LEM_LILIN (checked)
   ↓
   - Nama: MEJA03
   - IP: 192.168.1.106
   - Filter: DUMMY, REPAIR_BOARD (checked)
   ↓
4. Selesai! Penerima list tersimpan

5. Tekan tombol PAKU_PALU
   ↓
6. Dashboard Pemanggil tampil:
   - Jenis Panggilan: PAKU_PALU
   - Penerima Target: MEJA02 ✓ Cocok
   - IP Penerima: 192.168.1.105
   - Antrian di Penerima: 3 (posisi antrian)
   - Status: ✓ Cocok
   ↓
7. Penerima MEJA02 terima call dengan filter match
```

### Call Flow - Dari Panggilan Sampai Queue

```
[Pemanggil] Tekan PAKU_PALU
    ↓
[Pemanggil Logic]
find MatchingPenerima("PAKU_PALU")
    → Loop penerimalist[]
    → Cari yang allowPakuPalu = true
    → Return index 0 (MEJA02)
    ↓
[MQTT] Publish ke "panggilan/alat"
{
  "meja": "MEJA01",
  "jenis": "PAKU_PALU",
  "status": "PANGGIL"
}
    ↓
[Penerima MEJA02] Subscribe panggilan/alat
    ↓
[Penerima Filter] isCallTypeAllowed("PAKU_PALU")?
    → YES: callTypeFilter.allowPakuPalu = true
    ↓
[Penerima] addIncomingCall()
    → Buzzer: 3x beep ✓
    → LCD: Show call
    → Queue: Add to callHistory[]
    ↓
[Penerima API] /filterinfo endpoint
{
  "nama": "MEJA02",
  "filters": {...},
  "queueInfo": {
    "totalQueue": 3,
    "queue": [
      {"position": 1, "mejaID": "MEJA04", "jenisCall": "PAKU_PALU"},
      {"position": 2, "mejaID": "MEJA05", "jenisCall": "PAKU_PALU"},
      {"position": 3, "mejaID": "MEJA01", "jenisCall": "PAKU_PALU"}
    ]
  }
}
    ↓
[Pemanggil Dashboard] updateQueueStatus()
    fetch("/queueinfo")
    → Return:
    {
      "callType": "PAKU_PALU",
      "penerimaNama": "MEJA02",
      "penerimarIP": "192.168.1.105",
      "queueCount": 3,
      "matchFound": true,
      "status": "WAITING"
    }
    ↓
[Dashboard] Update tampilan:
- Penerima Target: MEJA02
- Antrian di Penerima: 3
- Status: ✓ Cocok
```

---

## 🔧 Implementasi Teknis

### A. Pemanggil (Pemanggil_dummy.ino)

#### 1. Data Structure

```cpp
#define MAX_PENERIMA 5

struct PenerimaMeta {
    String nama;                  // e.g., "MEJA02"
    String ip;                    // e.g., "192.168.1.105"
    bool allowDummy;              // Filter for DUMMY
    bool allowPakuPalu;           // Filter for PAKU_PALU
    bool allowLemLinlin;          // Filter for LEM_LILIN
    bool allowAdjustTwisting;     // Filter for ADJUST_TWISTING
    bool allowRepairBoard;        // Filter for REPAIR_BOARD
    int queueCount;               // Current queue position
    unsigned long lastUpdateTime; // Last fetch time
};

PenerimaMeta penerimalist[MAX_PENERIMA];
int penerimarCount = 0;
```

#### 2. Core Functions

**penerimarAcceptsCallType()**

```cpp
bool penerimarAcceptsCallType(int idx, String callType)
{
    if (callType == "DUMMY") return penerimalist[idx].allowDummy;
    if (callType == "PAKU_PALU") return penerimalist[idx].allowPakuPalu;
    // ... etc
}
```

**findMatchingPenerima()**

```cpp
int findMatchingPenerima(String callType)
{
    for (int i = 0; i < penerimarCount; i++) {
        if (penerimarAcceptsCallType(i, callType)) {
            return i;  // Found matching Penerima
        }
    }
    return -1;  // No match
}
```

**addPenerimarToList()**

```cpp
void addPenerimarToList(String nama, String ip,
                        bool dummy, bool paku, bool lem,
                        bool adjust, bool repair)
{
    // Add to penerimalist[] at index penerimarCount++
    // Save to Serial log
}
```

#### 3. Web Endpoints

**GET `/penerimarconfig`**

- Display HTML form
- List registered Penerima (count: X/5)
- Form to add new Penerima

**POST `/addpenerima`**

- Args: nama, ip, dummy, paku, lem, adjust, repair (checkboxes)
- Call: addPenerimarToList()
- Redirect: /penerimarconfig with success message

#### 4. API Response - Modified

**GET `/queueinfo` (Updated)**
Old response:

```json
{
  "queueCount": 5,
  "isProcessing": true,
  "currentCall": "PAKU_PALU",
  "status": "WAITING",
  "respondedBy": "MEJA02"
}
```

New response:

```json
{
  "callType": "PAKU_PALU",
  "status": "WAITING",
  "penerimaNama": "MEJA02",
  "penerimarIP": "192.168.1.105",
  "queueCount": 3,
  "matchFound": true
}
```

Logic:

```cpp
if (sedangMemanggil && jenisPanggilan != "") {
    int idx = findMatchingPenerima(jenisPanggilan);
    if (idx >= 0) {
        // Found matching Penerima
        response += "\"penerimaNama\":\"" + penerimalist[idx].nama + "\",";
        response += "\"penerimarIP\":\"" + penerimalist[idx].ip + "\",";
        response += "\"queueCount\":" + String(penerimalist[idx].queueCount) + ",";
        response += "\"matchFound\":true";
    } else {
        // No match found
        response += "\"penerimaNama\":\"-\",";
        response += "\"matchFound\":false";
    }
}
```

#### 5. Dashboard Update

**Main button group:**

```html
<a href="/penerimarconfig" class="btn-settings">👥 Kelola Penerima</a>
<a href="/settings" class="btn-settings">Pengaturan</a>
<a href="/resetwifi" class="btn-resetwifi">Reset WiFi</a>
```

**Status Card (Updated):**

```
Jenis Panggilan: PAKU_PALU
Penerima Target: MEJA02
IP Penerima: 192.168.1.105
Antrian di Penerima: 3
Status Match: ✓ Cocok
```

**JavaScript updateQueueStatus():**

```javascript
async function updateQueueStatus() {
  const response = await fetch("/queueinfo");
  const data = await response.json();

  document.getElementById("currentCall").textContent = data.callType || "-";
  document.getElementById("penerimaNama").textContent =
    data.penerimaNama || "-";
  document.getElementById("penerimarIP").textContent = data.penerimarIP || "-";
  document.getElementById("queueCount").textContent = data.queueCount || 0;

  // Match indicator
  if (data.matchFound) {
    matchEl.textContent = "✓ Cocok";
    matchEl.style.color = "#4CAF50";
  } else if (data.callType === "-") {
    matchEl.textContent = "Idle";
  } else {
    matchEl.textContent = "❌ Tidak Ada";
    matchEl.style.color = "#f44336";
  }
}
```

---

### B. Penerima Enhancement (Penerima.ino)

#### 1. New Endpoint: `/filterinfo`

Returns filter + queue info for Pemanggil to fetch

**GET `/filterinfo`** response:

```json
{
  "nama": "MEJA02",
  "ip": "192.168.1.105",
  "filters": {
    "allowDummy": false,
    "allowPakuPalu": true,
    "allowLemLinlin": true,
    "allowAdjustTwisting": false,
    "allowRepairBoard": false
  },
  "queueInfo": {
    "totalQueue": 3,
    "adaPanggilan": true,
    "queue": [
      {
        "position": 1,
        "mejaID": "MEJA04",
        "jenisCall": "PAKU_PALU",
        "sudahDikonfirmasi": false
      },
      {
        "position": 2,
        "mejaID": "MEJA05",
        "jenisCall": "PAKU_PALU",
        "sudahDikonfirmasi": false
      },
      {
        "position": 3,
        "mejaID": "MEJA01",
        "jenisCall": "PAKU_PALU",
        "sudahDikonfirmasi": false
      }
    ]
  }
}
```

**Implementation:**

```cpp
void handleFilterInfo() {
    String response = "{";
    response += "\"nama\":\"" + ID_PENERIMA + "\",";
    response += "\"filters\":{";
    response += "\"allowDummy\":" + String(callTypeFilter.allowDummy ? "true" : "false") + ",";
    // ... etc
    response += "},";
    response += "\"queueInfo\":{";
    response += "\"totalQueue\":" + String(callHistoryCount) + ",";
    response += "\"queue\":[";

    for (int i = 0; i < callHistoryCount; i++) {
        response += "{";
        response += "\"position\":" + String(i + 1) + ",";
        response += "\"mejaID\":\"" + callHistory[i].mejaID + "\",";
        response += "\"jenisCall\":\"" + callHistory[i].jenisCall + "\"";
        response += "},";
    }
    response += "]};";
}
```

---

## 🎛️ Configuration Steps

### Setup Awal

**1. Power On Pemanggil & Penerima**

- Keduanya akses AP/WiFi
- Masuk ke dashboard masing-masing

**2. Configure Penerima Filter (Optional)**

- Buka Penerima: `http://[penerima-ip]/callfilter`
- Uncheck jenis panggilan yang tidak ingin diterima
- Save

**3. Register Penerima di Pemanggil**

- Buka Pemanggil: `http://[pemanggil-ip]/penerimarconfig`
- Klik "Tambah Penerima Baru"
- Input:
  - Nama: MEJA02
  - IP: 192.168.1.105
  - Filter: Centang PAKU_PALU, LEM_LILIN
  - Klik "Tambah Penerima"
- Ulangi untuk Penerima lainnya (max 5)

**4. Test Sistem**

- Pemanggil: Tekan tombol PAKU_PALU
- Check dashboard: Tampil nama Penerima target
- Check Penerima: Terima panggilan (buzzer, LCD)
- Check antrian: Dashboard Pemanggil tampil queue count

---

## 📋 Files Modified

### Pemanggil_dummy.ino

**New Structures:**

- Lines 96-112: PenerimaMeta struct + penerimalist[]

**New Functions:**

- Lines 114-152: penerimarAcceptsCallType(), addPenerimarToList(), findMatchingPenerima()
- Lines 576-651: handlePenerimarConfig()
- Lines 653-705: handleAddPenerima()

**Modified Functions:**

- handleQueueInfo() - Sekarang return matching Penerima info
- handleMainPage() - Add link ke `/penerimarconfig`
- updateQueueStatus() JavaScript - Updated untuk tampil match info

**Routes:**

- Line 1856-1857: Added `/penerimarconfig` dan `/addpenerima`
- Line 1941-1942: Same in dynamic registration

### Penerima.ino

**New Functions:**

- Lines 571-605: handleFilterInfo() - Return filter + queue info

**Routes:**

- Line 1174, 1553, 1628: Added `/filterinfo` endpoint

---

## 🔄 Real-Time Flow Example

**Timeline:**

```
T=0s:  Pemanggil user tekan tombol PAKU_PALU
T=0.1s: addToQueue("PAKU_PALU")
T=0.1s: publishMQTT("PANGGIL", "PAKU_PALU")
T=0.2s: Penerima MEJA02 receive MQTT
T=0.2s: Check filter: allowPakuPalu = true ✓
T=0.3s: addIncomingCall() → Buzzer start
T=0.5s: Dashboard Pemanggil fetch /queueinfo
T=0.6s: Return penerimaNama="MEJA02", queueCount=1
T=1.0s: Dashboard update: show MEJA02 + queue
T=1.5s: Penerima show call on LCD
T=2.0s: User click "Sudah Terima" button
T=2.1s: Publish ACK response
T=2.2s: Pemanggil receive ACK → status = OTW
T=2.3s: Dashboard update: show "OTW" status
```

---

## 💾 Data Persistence

**Pemanggil Penerima List:**

- Currently: RAM only (lost on reboot)
- Future enhancement: Save to Preferences/EEPROM

**Penerima Filter:**

- Saved to Preferences ("filter" namespace)
- Keys: "dummy", "paku", "lem", "adjust", "repair"
- Persists across reboot ✓

---

## 🚀 Future Enhancements

1. **Save Penerima List**
   - Save to Preferences in Pemanggil
   - Auto-load on boot

2. **HTTP Fetch Real Queue**
   - Pemanggil periodically GET `/filterinfo` from matched Penerima
   - Update queueCount automatically

3. **Position Display**
   - Show "Antrian ke 3 dari 5"
   - Not just count

4. **Load Balancing**
   - Multiple Penerima accept same type
   - Route to one with shortest queue

5. **Schedule-Based Routing**
   - Route PAKU_PALU only to MEJA02 during 8am-5pm
   - Route to MEJA03 after 5pm

6. **Fallback Handling**
   - If primary Penerima offline, try secondary
   - Automatic failover

---

## 🐛 Troubleshooting

**Dashboard tampil "❌ Tidak Ada"**

- Check Penerima list registered
- Verify Penerima filter includes call type
- Check IP address correct

**Penerima tidak terima call**

- Check filter setting: /callfilter
- Check MQTT connection
- Check Penerima accepting right call type

**Queue count always 0**

- Check Penerima has incoming calls
- Verify /filterinfo endpoint accessible
- Check queueCount calculation logic

---

## 📚 Code References

| File      | Location | Description              |
| --------- | -------- | ------------------------ |
| Pemanggil | L96-112  | PenerimaMeta struct      |
| Pemanggil | L114-152 | Matching functions       |
| Pemanggil | L576-705 | Penerima config handlers |
| Pemanggil | L530     | /queueinfo endpoint      |
| Penerima  | L571-605 | /filterinfo endpoint     |
| Pemanggil | L1024    | updateQueueStatus() JS   |
