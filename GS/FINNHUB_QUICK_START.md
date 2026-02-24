# ⚡ FINNHUB INTEGRATION - QUICK START

## 🎯 Apa yang Berubah?

API priority sekarang:

```
1️⃣ FINNHUB (Paling reliable, cepat, akurat)
   ↓
2️⃣ Yahoo Finance Simple Quote
   ↓
3️⃣ Google Finance
   ↓
4️⃣ Yahoo Finance Chart
   ↓
❌ Error message (bukan dummy data)
```

---

## 🚀 Langsung Coba:

### Step 1: Buka Spreadsheet

Klik menu **📈 Day Trading 30Min**

### Step 2: Setup (Jika Pertama Kali)

Pilih **⚙️ Setup Sheet** untuk membuat template

### Step 3: Test Finnhub

Pilih **🧪 Test All APIs** untuk verify Finnhub working

**Expected output:**

```
✓ Finnhub: SUCCESS - Price: $230.45 (BEST SOURCE)
✓ Simple Quote: SUCCESS - Price: $230.50
✓ Combined: SUCCESS (Finnhub) - Price: $230.45
```

### Step 4: Refresh Data

Pilih **🔄 Refresh Data** untuk get latest stock prices

---

## 📊 Expected Results:

**Before Finnhub:**

```
AAPL:     No data (API failed)
BBCA.JK:  No data (API failed)
TSLA:     No data (API failed)
```

**After Finnhub:**

```
AAPL:     $230.45 ✓
BBCA.JK:  $102,450 ✓
TSLA:     $245.80 ✓
```

---

## 🔑 API Key Info:

```
API Key: d49uufhr01qlaebjbv4gd49uufhr01qlaebjbv50
Status: ✓ Active
Rate Limit: 60 req/min (Your script: ~15 req/min) ✓
Plan: Free (upgrade anytime for higher limits)
```

---

## 💡 Tips:

1. **Stock Codes:**

   - USA: `AAPL`, `TSLA`, `GOOGL` (no suffix)
   - Indonesia: `BBCA.JK` (dengan .JK)
   - Hong Kong: `0005.HK` (dengan .HK)

2. **First Run:**

   - Bisa take 5-10 seconds (API fetch + calculation)
   - Refresh berikutnya lebih cepat (cache aktif)

3. **Auto Refresh:**

   - Menu: **▶️ Start Auto Refresh** (every 10 minutes)
   - Menu: **⏹️ Stop Auto Refresh** (jika ingin stop)

4. **Check Logs:**
   - Tools → Script Editor → Ctrl+Enter
   - Lihat detail fetch untuk debugging

---

## ⚠️ Jika Ada Issue:

**"Finnhub: FAILED"**

- Wait 1 minute, try again
- Check API key valid
- Check internet connection

**"No data (API failed)"**

- Finnhub down (unlikely, but possible)
- Stock code format salah
- Check logs untuk detail

**Still not working?**

- Baca: TROUBLESHOOTING_API_FAILED.md
- Baca: FINNHUB_INTEGRATION.md (full documentation)

---

## 🎉 You're All Set!

Finnhub API sudah terintegrasi dan siap digunakan.

**Next:** Klik refresh data dan lihat hasilnya! 🚀

---

Quick Links:

- Full Guide: FINNHUB_INTEGRATION.md
- Troubleshooting: TROUBLESHOOTING_API_FAILED.md
- Previous Fixes: FIX_DATA_MISMATCH.md
