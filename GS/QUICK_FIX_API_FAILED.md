# 🚨 Quick Fix: "No data (API failed)"

## 🎯 The Issue:

Semua API untuk fetch data gagal. Bisa karena:

- ❌ Rate limit (API diblok karena request terlalu sering)
- ❌ Network timeout
- ❌ Stock code format salah
- ❌ API endpoint down

---

## ⚡ 3 Steps untuk Fix:

### **Step 1: Run Diagnostic Test** (2 menit)

1. Di spreadsheet, klik menu **📈 Day Trading 30Min**
2. Pilih **🧪 Test All APIs**
3. Lihat dialog hasil test
4. **Buka logs:** Tekan **Ctrl + Enter** di Script Editor (Tools → Script Editor)

**Lihat hasil test:**

- ✓ berarti API working
- ✗ berarti API failed

---

### **Step 2: Check Stock Code Format**

Verify di column A bahwa format benar:

```
AAPL      ✓ Correct (USA stocks, no suffix)
BBCA.JK   ✓ Correct (Indonesia IDX, need .JK)
TSLA      ✓ Correct
GOOGL     ✓ Correct
```

**Jangan:**

```
BBCA      ✗ Wrong (missing .JK for Indonesia)
 AAPL     ✗ Wrong (space at start)
AA PL     ✗ Wrong (space in middle)
```

---

### **Step 3: Based on Test Results**

**Jika Test Result = ✗ Semua Gagal:**

**Option A: TUNGGU 15 MENIT (Recommended)**

- Berarti API rate-limited
- Yahoo Finance temporary block requests
- Just wait, then try again

**Option B: FORCE REFRESH DENGAN TUNGGU LEBIH LAMA**

- Edit code: Buka Tools → Script Editor
- Find: `Utilities.sleep(4000);`
- Change to: `Utilities.sleep(8000);` (8 detik tunggu antar request)
- Save dan coba refresh lagi

**Option C: GUNAKAN STOCK CODE USA DULU**

- Indonesia stocks (BBCA.JK) sering gagal
- Coba dengan USA stocks dulu (AAPL, TSLA, GOOGL)
- Lihat apakah ada yang work
- Kalo USA work tapi Indonesia tidak, update User-Agent

---

## 🔍 How to Check Logs:

**Cara 1: Dalam Script Editor**

1. Tekan **Ctrl + Enter**
2. Lihat tab "Execution log"
3. Search untuk "API failed" atau "SUCCESS"

**Cara 2: Dalam Script Editor - Recent Runs**

1. Klik "Recent runs" (jarum jam icon)
2. Lihat last execution attempt
3. Klik untuk lihat detail logs

**Apa yang dicari:**

```
✓ Simple quote success for AAPL: 230.45     ← GOOD
✗ Simple quote API failed (403)             ← BAD (rate limited)
Timeout                                      ← BAD (network slow)
Invalid characters in stock code             ← BAD (code format)
```

---

## 🎯 Most Common Fixes:

| Error          | Cause        | Fix                                         |
| -------------- | ------------ | ------------------------------------------- |
| HTTP 403       | Rate limited | Wait 15 min or increase sleep time          |
| HTTP 503       | Server down  | Wait, Yahoo Finance temporarily down        |
| Timeout        | Network slow | Increase timeout 10s → 15s, reduce requests |
| "Invalid code" | Bad format   | Fix stock code format (AAPL vs BBCA.JK)     |
| Empty response | API blocked  | Change User-Agent, wait, or use VPN         |

---

## 🚀 Immediate Recovery:

**Jika urgent, dan tidak ada waktu tunggu:**

```
1. Click "🔄 Refresh Data" again
2. If still fail, wait 5 minutes
3. Try one more time
4. If still fail, use fallback data (check docs)
```

---

## 📚 Full Guide:

Baca **TROUBLESHOOTING_API_FAILED.md** untuk:

- Detailed diagnosis steps
- All error codes & solutions
- Manual test examples
- User-Agent update instructions

---

**TL;DR:** Wait 15 min, then try refresh again. If still fail, check logs.
