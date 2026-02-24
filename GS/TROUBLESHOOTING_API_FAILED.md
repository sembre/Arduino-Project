# 🔧 TROUBLESHOOTING: "No data (API failed)" Error

## 📌 Problem:

Spreadsheet menampilkan **"No data (API failed)"** di kolom B (Current Price) untuk semua atau beberapa saham.

Ini berarti **semua API gagal** mengambil data:

- ❌ Simple Realtime Quote API (Yahoo Finance /v7)
- ❌ Google Finance (HTML Parsing)
- ❌ Yahoo Finance Chart API

---

## 🔍 Diagnosis Steps:

### Step 1: Check Logs untuk Detail Error

**Cara akses logs:**

1. Buka **Google Apps Script** (Tools → Script editor)
2. Tekan **Ctrl + Enter** untuk buka Execution log
3. Lihat message untuk setiap API attempt

**Expected logs untuk AAPL:**

```
Fetching data for: AAPL
Fetching simple realtime quote for AAPL
✓ Simple quote success for AAPL: 230.45     ← GOOD, API berhasil
```

**Error logs contoh:**

```
Fetching data for: AAPL
Fetching simple realtime quote for AAPL
Simple quote API failed (403) for AAPL     ← HTTP 403: Access Denied
Google Finance error for AAPL: timeout     ← Network timeout
```

---

## 🚀 Cara Debug:

### Debug Opsi 1: Run Test All APIs (RECOMMENDED)

**Langkah:**

1. Di spreadsheet, klik menu **📈 Day Trading 30Min**
2. Pilih **🧪 Test All APIs**
3. Dialog akan show hasil test untuk AAPL:
   - ✓ atau ✗ untuk setiap API
   - Price jika berhasil
4. Check logs (Ctrl+Enter) untuk detail

**Expected output jika normal:**

```
✓ Simple Quote: SUCCESS - Price: $230.45
✓ Google Finance: SUCCESS - Price: $230.50
✓ Yahoo Finance: SUCCESS - 50 periods
✓ Combined: SUCCESS (Yahoo Finance (Realtime)) - Price: $230.45
```

**Expected output jika ada issue:**

```
✗ Simple Quote: FAILED
✗ Google Finance: FAILED
✗ Yahoo Finance: FAILED
✗ Combined: FAILED - All APIs returned null
```

---

### Debug Opsi 2: Check Raw API Response

**Langkah:**

1. Di spreadsheet, klik menu **📈 Day Trading 30Min**
2. Pilih **🔍 Debug Raw Response**
3. Check logs (Ctrl+Enter) untuk raw HTTP response

**Apa yang dicari di logs:**

```
--- Yahoo Finance /v7/finance/quote raw response ---
HTTP Status: 200                    ← GOOD, API accessible
Response length: 2453 chars         ← GOOD, data received
✓ Valid JSON                        ← GOOD, response valid
Found 1 quote(s)                    ← GOOD, data found
Regular market price: 230.45        ← GOOD, price extracted
```

**Jika error:**

```
HTTP Status: 403                    ← BAD, Access Denied / Rate Limited
HTTP Status: 503                    ← BAD, Service Unavailable
Response length: 45 chars           ← BAD, Response terlalu singkat
✗ JSON parsing failed               ← BAD, Invalid response format
```

---

## 🛠️ Common Issues & Fixes:

### Issue 1: HTTP 403 (Access Denied) atau 429 (Rate Limited)

**Penyebab:**

- Yahoo Finance API memblok requests yang berulang
- Request headers tidak valid
- Rate limit exceeded

**Fix:**

1. **Tunggu 10-15 menit** - API rate limit reset
2. **Ubah User-Agent** di code:

   - Buka **Tools → Script editor**
   - Cari: `"User-Agent": "Mozilla/5.0...`
   - Ganti dengan user-agent baru dari https://useragentstring.com/

3. **Kurangi frekuensi refresh**
   - Jika auto-refresh tiap 10 menit, ubah jadi 30 menit
   - Buka **createAutoRefresh30Min()** di code
   - Ubah `.everyMinutes(10)` ke `.everyMinutes(30)`

**Code to modify (fetchSimpleRealtimeQuote):**

```javascript
const options = {
  muteHttpExceptions: true,
  headers: {
    "User-Agent":
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36", // ← Update ini
    Accept: "application/json",
    "Accept-Language": "en-US,en;q=0.9",
  },
  timeout: 10000,
};
```

---

### Issue 2: Network Timeout

**Penyebab:**

- Internet connection slow
- Request timeout (default 10s)
- API server slow

**Fix:**

1. **Check internet connection** - buka browser, test website
2. **Increase timeout** dari 10000ms ke 15000ms:

```javascript
timeout: 15000,  // 15 detik instead of 10
```

3. **Reduce simultaneous requests** - tunggu lebih lama antar request:

```javascript
Utilities.sleep(6000); // 6 detik instead of 4
```

---

### Issue 3: Invalid Stock Code Format

**Penyebab:**

- Stock code tidak valid (e.g., "BBCA" tanpa ".JK")
- Spaces atau character tidak valid
- Exchange suffix salah

**Fix - Stock Code Format:**

| Stock              | Format      | Exchange  |
| ------------------ | ----------- | --------- |
| Bank BCA (IDX)     | `BBCA.JK`   | Jakarta   |
| Apple (NASDAQ)     | `AAPL`      | USA       |
| Tesla (NASDAQ)     | `TSLA`      | USA       |
| Google (NASDAQ)    | `GOOGL`     | USA       |
| Microsoft (NASDAQ) | `MSFT`      | USA       |
| Singapore Stock    | `TICKER.SI` | Singapore |
| Hong Kong Stock    | `TICKER.HK` | Hong Kong |

**Verify kode di spreadsheet:**

- Check column A untuk format yang tepat
- Tidak boleh ada spaces atau special chars (hanya A-Z, 0-9, .)

---

### Issue 4: Google Finance HTML Parsing Failed

**Penyebab:**

- Google Finance mengubah HTML structure
- Class names yang di-parse tidak lagi valid
- Network blocking Google requests

**Fix:**

- Ini fallback dari Simple Quote API, jadi jika Simple Quote work, bukan masalah
- Jika semua API fail, skip Google Finance dan fokus ke Simple Quote fix

---

## 🎯 Troubleshooting Flowchart:

```
"No data (API failed)" error?
    │
    ├─→ Run "🧪 Test All APIs"
    │   │
    │   ├─ ✓ All tests pass?
    │   │   └─ Issue mungkin di data processing, bukan API
    │   │
    │   └─ ✗ Tests fail?
    │       └─ Run "🔍 Debug Raw Response"
    │           │
    │           ├─ HTTP 403?
    │           │   └─ Rate limited → Tunggu 15 min atau ubah User-Agent
    │           │
    │           ├─ HTTP 503?
    │           │   └─ Server error → Yahoo Finance down → Tunggu
    │           │
    │           ├─ Timeout?
    │           │   └─ Slow connection → Increase timeout atau reduce requests
    │           │
    │           └─ JSON parse error?
    │               └─ Response format invalid → Check API endpoint
    │
    └─→ Masih error?
        └─ Check logs detail di Script Editor (Ctrl+Enter)
```

---

## 📊 Full Test Example:

**Copy-paste ini ke Script Editor untuk manual test:**

```javascript
function manualTestAAP() {
  const kode = "AAPL";

  console.log("=== MANUAL TEST FOR " + kode + " ===");

  // Test 1
  console.log("\n1. Testing fetchSimpleRealtimeQuote...");
  try {
    const result = fetchSimpleRealtimeQuote(kode);
    console.log("Result:", result);
  } catch (e) {
    console.error("Error:", e.toString());
  }

  // Test 2
  console.log("\n2. Testing fetchGoogleFinanceIntraday...");
  try {
    const result = fetchGoogleFinanceIntraday(kode);
    console.log("Result:", result);
  } catch (e) {
    console.error("Error:", e.toString());
  }

  // Test 3
  console.log("\n3. Testing fetchYahooFinanceWithCache...");
  try {
    const result = fetchYahooFinanceWithCache(kode);
    console.log("Result:", result);
  } catch (e) {
    console.error("Error:", e.toString());
  }
}
```

**Cara run:**

1. Paste code di Script Editor
2. Klik Run button (▶️)
3. Check logs (Ctrl+Enter) untuk hasil

---

## ✅ Quick Checklist:

- [ ] Check logs detail di Script Editor (Ctrl+Enter)
- [ ] Run "🧪 Test All APIs" menu
- [ ] Run "🔍 Debug Raw Response" menu
- [ ] Verify stock codes di column A (format valid)
- [ ] Check internet connection (open browser, test)
- [ ] Wait 15 minutes if HTTP 403 (rate limit)
- [ ] Increase timeout if network timeout
- [ ] Update User-Agent if consistently blocked

---

## 📞 Still Not Working?

**Informasi untuk debug lebih lanjut:**

1. **Screenshot logs** (Ctrl+Enter dalam Script Editor)
2. **Stock codes** yang dicoba (AAPL, BBCA.JK, etc)
3. **HTTP status code** dari raw response test
4. **Timestamp** ketika error terjadi
5. **Network condition** (online/offline, speed)

---

## 🔄 Quick Recovery:

**Jika urgent butuh data sekarang:**

1. **Gunakan fallback** - Edit `processStockDataCombined()`

   - Uncomment `generateDummyHistoricalData()` untuk trading signals
   - Note: Harga akan generated, bukan real

2. **Manual input** - Copy paste latest data dari Yahoo Finance

3. **Wait & Retry** - Tunggu 15 menit, coba refresh lagi

---

Version: 2.0
Updated: November 15, 2025
Status: Ready to use for troubleshooting "No data (API failed)" errors
