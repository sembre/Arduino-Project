# 🚀 Quick Start Guide

## Step 1: Buka Google Apps Script

1. Buka spreadsheet Anda
2. Tools → Script editor
3. Ganti kode dengan versi terbaru dari `kode.gs`

## Step 2: Setup Sheet (First Time Only)

```javascript
setupSheet30Min();
```

- Akan membuat header dan struktur sheet
- Auto populate sample stocks: BBCA.JK, AAPL, TSLA, GOOGL, MSFT
- **AUTO:** Column width otomatis adjust sesuai header text
- **AUTO:** Spreadsheet lebih rapi, tidak perlu manual resize kolom

## Step 3: Manual Refresh (Test)

```javascript
refreshSheet30Min();
```

- Ambil data dari Google Finance / Yahoo Finance
- Hitung indikator teknikal
- Display hasil di sheet

## Step 4: Enable Auto-Refresh

```javascript
createAutoRefresh30Min();
```

- Refresh otomatis setiap 10 menit
- Lihat di Triggers (Edit → Current project triggers)

## Step 5: Monitor Execution

```
Ctrl + Enter (atau View → Logs)
```

- Lihat execution logs
- Check success/failed count
- Verify data sources digunakan

---

## 📋 Menu Options (onOpen):

Setelah refresh, akan muncul menu "📈 Day Trading 30Min" di toolbar:

| Menu Item             | Fungsi                              |
| --------------------- | ----------------------------------- |
| 🔄 Refresh Data       | Manual refresh data                 |
| ⚙️ Setup Sheet        | Reset sheet structure               |
| 🧪 Test Data Sources  | Test Google Finance & Yahoo Finance |
| ▶️ Start Auto Refresh | Enable auto-refresh timer           |
| ⏹️ Stop Auto Refresh  | Disable auto-refresh timer          |

---

## 🎯 Workflow:

```
1. Setup Sheet (sekali)
   ↓
2. Add stock codes ke kolom A (AAPL, BBCA.JK, etc)
   ↓
3. Run Refresh Data
   ↓
4. Check hasil di sheet (kolom B-T)
   ↓
5. Enable Auto Refresh untuk update otomatis
   ↓
6. Monitor logs untuk tracking
```

---

## 🧪 Testing Quick Commands:

```javascript
// Test 1: Setup sheet
setupSheet30Min();

// Test 2: Refresh data sekali
refreshSheet30Min();

// Test 3: Check logs
// Ctrl + Enter untuk buka logs

// Test 4: Test data sources
testDataSources();

// Test 5: Enable auto-refresh
createAutoRefresh30Min();

// Test 6: Check jika auto-refresh sudah jalan
// Wait 10 minutes dan lihat timestamp di A1 update

// Test 7: Stop auto-refresh
stopAutoRefresh30Min();
```

---

## 📊 Output Examples:

### Jika data dari Google Finance (Real-time):

```
A: AAPL
B: 238.45
C: 237.12  (EMA 10)
D: 235.89  (EMA 30)
E: 230.45  (EMA 100)
F: 65.34   (RSI 10)
G: 60.12   (RSI 30)
H: 55.67   (RSI 100)
I: BULLISH_TREND
J: BUY
...
T: 14:30:22 (Google Finance)
```

### Jika data dari Yahoo Finance (Historis):

```
T: 14:30:22 (Yahoo Finance)
```

### Jika data dari Fallback (Simulated):

```
T: 14:30:22 (Fallback (Simulated))
```

_Note: Data simulated hanya untuk UI/UX, bukan untuk trading real_

---

## ⚠️ Common Issues & Solutions:

### Issue: "Sheet is undefined"

**Solution:** Pastikan ada sheet yang active di spreadsheet

### Issue: "Invalid stock code"

**Solution:** Check kolom A memiliki valid format (contoh: AAPL, BBCA.JK)

### Issue: "No data available"

**Solution:**

- Check internet connection
- Jika network down, akan auto fallback ke simulated data
- Check logs untuk details

### Issue: "Timeout"

**Solution:** Network terlalu lambat

- Increase `Utilities.sleep()` value
- Atau reduce max stocks `processedCount < 8`

### Issue: Auto-refresh tidak berjalan

**Solution:**

1. Check Triggers (Edit → Current project triggers)
2. Verify function `refreshSheet30Min` listed
3. Delete dan recreate dengan `createAutoRefresh30Min()`

---

## 🔧 Customization:

### Ubah jumlah max stocks per refresh:

Edit line di `refreshSheet30Min()`:

```javascript
if (isValidStockCode(kode) && processedCount < 8) {  // Change 8 to 12, 20, etc
```

### Ubah delay antar fetch:

Edit line di `refreshSheet30Min()`:

```javascript
Utilities.sleep(4000); // 4000ms = 4 detik, ubah sesuai kebutuhan
```

### Ubah column width settings:

Edit line di `autoAdjustColumnWidths()`:

```javascript
// Ubah jumlah baris yang dicek untuk calculate width
const maxRows = Math.min(range.getLastRow(), 100); // Change 100 to 50, 150, etc

// Ubah minimum width
const maxWidth = 50; // Minimum width 50px, change if needed

// Ubah estimasi width formula
const estimatedWidth = stringValue.length * 7 + 15; // Adjust multiplier jika perlu

// Ubah final width limit
const finalWidth = Math.max(60, Math.min(maxWidth, 300)); // Change 60 (min) or 300 (max)
```

### Ubah cache duration:

Edit line di `fetchYahooFinanceWithCache()`:

```javascript
cache.put(cacheKey, JSON.stringify(processedData), 1800); // 1800 = 30 menit
```

### Ubah fallback data periods:

Edit line di `processStockDataCombined()`:

```javascript
const fallbackData = generateFallbackData(kode, 50); // 50 = jumlah candles
```

---

## 📈 Expected Performance:

- **Refresh time:** ~32 detik untuk 8 stocks (4s delay × 8)
- **Data latency:** Real-time Google Finance + Cached Yahoo (30 min)
- **Sheet update:** <1 detik setelah data siap
- **Quota usage:** ~16 requests per refresh (2 per stock: Google + Yahoo)

---

## 🎓 Understanding the Output:

### Trading Signal (Kolom J):

- **STRONG BUY** - Golden cross + RSI oversold + Near support
- **BUY** - Bullish trend + Pullback
- **SELL** - Bearish trend + Rally
- **STRONG SELL** - Death cross + RSI overbought + Near resistance
- **HOLD** - Tidak ada sinyal kuat

### Confidence Levels (Kolom O):

- **HIGH** - Strong signals dengan multiple indicators
- **MEDIUM** - Mixed signals
- **LOW** - Uncertain

### Volume Trend (Kolom S):

- **VERY_HIGH** - Volume > 2x average
- **HIGH** - Volume > 1.5x average
- **NORMAL** - Volume normal
- **LOW** - Volume < 0.5x average

---

## 📱 Mobile Compatibility:

- Spreadsheet dapat diakses via mobile
- Real-time update jika auto-refresh enabled
- Scroll untuk lihat semua kolom (K-T)

---

## 🔐 Permissions:

Script membutuhkan akses ke:

- Spreadsheet (read/write)
- UrlFetchApp (untuk ambil data dari API)
- Cache Service (untuk caching)
- Script Properties (untuk triggers)

Semua permissions standard dan aman.

---

## 📞 Troubleshooting:

### Step 1: Check Logs

```
Ctrl + Enter di script editor
Lihat execution details dan errors
```

### Step 2: Test Individual Functions

```javascript
// Test Google Finance
const data1 = getStockDataCombined("AAPL");
console.log(data1);

// Test Yahoo Finance
const data2 = fetchYahooFinanceWithCache("AAPL");
console.log(data2);

// Test Fallback
const data3 = generateFallbackData("AAPL", 50);
console.log(data3);
```

### Step 3: Check Sheet Status

- A1 menampilkan last update timestamp
- Verify timestamp updated setiap refresh

---

Version: 2.0
Last Updated: November 15, 2025
Status: ✅ Ready to Use

Untuk info lebih detail, baca:

- DOKUMENTASI_PERBAIKAN.md (full technical details)
- TESTING_CHECKLIST.md (comprehensive test cases)
