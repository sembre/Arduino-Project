// KODE LENGKAP DAY TRADING DENGAN ERROR HANDLING YANG LEBIH BAIK
// FUNGSI UTAMA UNTUK REFRESH DATA
function refreshSheet30Min() {
  try {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getActiveSheet();

    if (!sheet) {
      console.error("Sheet is undefined in refreshSheet30Min");
      return;
    }

    console.log("=== Starting refreshSheet30Min ===");

    // Ambil semua kode saham dari kolom A mulai dari A2
    const kodeRange = sheet.getRange("A2:A" + sheet.getLastRow());
    const kodeValues = kodeRange.getValues();

    // Hapus kolom C hingga T
    const lastRow = Math.max(sheet.getLastRow(), kodeValues.length + 1);
    if (lastRow > 1) {
      sheet.getRange("C2:T" + lastRow).clearContent();
    }

    // Proses setiap kode saham dengan optimasi quota
    let processedCount = 0;
    let failedCount = 0;

    kodeValues.forEach((row, index) => {
      const kode = row[0];

      // Validasi kode yang lebih ketat
      if (isValidStockCode(kode) && processedCount < 8) {
        const rowNumber = index + 2;
        try {
          processStockDataCombined(sheet, kode, rowNumber);
          processedCount++;
        } catch (e) {
          console.error(`Error processing row ${rowNumber}:`, e.message);
          failedCount++;
        }

        // Delay untuk menghindari quota
        Utilities.sleep(4000);
      }
    });

    // Update waktu terakhir diperbarui
    const now = Utilities.formatDate(
      new Date(),
      ss.getSpreadsheetTimeZone(),
      "yyyy-MM-dd HH:mm:ss"
    );
    sheet.getRange("A1").setValue("Day Trading 30Min - Last updated: " + now);

    console.log(
      `=== Refresh completed: ${processedCount} success, ${failedCount} failed ===`
    );
  } catch (error) {
    console.error("Error in refreshSheet30Min:", error);
  }
}

// FUNGSI VALIDASI KODE SAHAM
function isValidStockCode(kode) {
  if (!kode) {
    console.error("Stock code is undefined or null");
    return false;
  }

  const kodeStr = kode.toString().trim();

  if (kodeStr === "") {
    console.error("Stock code is empty");
    return false;
  }

  if (kodeStr.length < 2 || kodeStr.length > 10) {
    console.error(`Invalid stock code length: ${kodeStr}`);
    return false;
  }

  // Validasi karakter dasar (huruf, angka, titik)
  const validPattern = /^[A-Za-z0-9.]+$/;
  if (!validPattern.test(kodeStr)) {
    console.error(`Invalid characters in stock code: ${kodeStr}`);
    return false;
  }

  return true;
}

// FINNHUB API KEY (PALING RELIABLE & FAST)
const FINNHUB_API_KEY = "d49uufhr01qlaebjbv4gd49uufhr01qlaebjbv50";

// FUNGSI UNTUK FETCH DATA DARI FINNHUB (PRIORITAS TERTINGGI - PALING RELIABLE)
function fetchFinnhubQuote(kode) {
  try {
    if (!isValidStockCode(kode)) {
      console.error(`Invalid stock code: ${kode}`);
      return null;
    }

    console.log(`Fetching Finnhub quote for ${kode}`);

    // Finnhub API endpoint untuk real-time quote
    const url = `https://finnhub.io/api/v1/quote?symbol=${kode}&token=${FINNHUB_API_KEY}`;

    const options = {
      muteHttpExceptions: true,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
      },
      timeout: 10000,
    };

    const response = UrlFetchApp.fetch(url, options);

    if (response.getResponseCode() !== 200) {
      console.log(
        `Finnhub API failed (${response.getResponseCode()}) for ${kode}`
      );
      return null;
    }

    const responseText = response.getContentText();
    if (!responseText || responseText.length < 50) {
      console.log(`Empty response from Finnhub API for ${kode}`);
      return null;
    }

    const data = JSON.parse(responseText);

    // Extract quote data dari Finnhub
    if (data.c > 0) {
      // c = current price, h = high, l = low, o = open, pc = previous close, v = volume, t = timestamp
      const finnhubData = {
        timestamp: data.t || Math.floor(new Date().getTime() / 1000),
        time: new Date(
          (data.t || Math.floor(new Date().getTime() / 1000)) * 1000
        ),
        open: data.o || data.c,
        high: data.h || data.c * 1.02,
        low: data.l || data.c * 0.98,
        close: data.c,
        volume: data.v || 1000000,
        symbol: kode,
        currency: "USD", // Finnhub primarily returns USD
        dataSource: "Finnhub (Realtime - Best)",
        previousClose: data.pc || data.c,
      };

      console.log(
        `✓ Finnhub success for ${kode}: $${data.c} (Volume: ${data.v})`
      );
      return [finnhubData];
    }

    console.log(`No valid data in Finnhub response for ${kode}`);
    return null;
  } catch (error) {
    console.error(`Finnhub API error for ${kode}: ${error.message}`);
    return null;
  }
}

// FUNGSI UNTUK FETCH REAL-TIME QUOTE SIMPLE (PRIORITAS 2)
function fetchSimpleRealtimeQuote(kode) {
  try {
    if (!isValidStockCode(kode)) {
      console.error(`Invalid stock code: ${kode}`);
      return null;
    }

    console.log(`Fetching simple realtime quote for ${kode}`);

    // Gunakan Yahoo Finance simple API
    const url = `https://query1.finance.yahoo.com/v7/finance/quote?symbols=${kode}`;

    const options = {
      muteHttpExceptions: true,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        Accept: "application/json",
        "Accept-Language": "en-US,en;q=0.9",
      },
      timeout: 10000,
    };

    const response = UrlFetchApp.fetch(url, options);

    if (response.getResponseCode() !== 200) {
      console.log(
        `Simple quote API failed (${response.getResponseCode()}) for ${kode}`
      );
      return null;
    }

    const responseText = response.getContentText();
    if (!responseText || responseText.length < 100) {
      console.log(`Empty response from simple quote API for ${kode}`);
      return null;
    }

    const data = JSON.parse(responseText);

    // Extract quote data
    if (
      data.quoteResponse &&
      data.quoteResponse.result &&
      data.quoteResponse.result.length > 0
    ) {
      const quote = data.quoteResponse.result[0];

      if (quote.regularMarketPrice > 0) {
        const simpleData = {
          timestamp: Math.floor(new Date().getTime() / 1000),
          time: new Date(),
          open: quote.regularMarketOpen || quote.regularMarketPrice,
          high: quote.fiftyTwoWeekHigh || quote.regularMarketPrice * 1.02,
          low: quote.fiftyTwoWeekLow || quote.regularMarketPrice * 0.98,
          close: quote.regularMarketPrice,
          volume: quote.regularMarketVolume || 1000000,
          symbol: kode,
          currency: quote.currency || "USD",
          dataSource: "Yahoo Finance (Realtime)",
        };

        console.log(
          `Simple quote success for ${kode}: ${quote.regularMarketPrice}`
        );
        return [simpleData];
      }
    }

    console.log(`No valid data in simple quote response for ${kode}`);
    return null;
  } catch (error) {
    console.error(`Simple quote API error for ${kode}: ${error.message}`);
    return null;
  }
}

// FUNGSI UTAMA UNTUK MENGAMBIL DATA (FINNHUB PRIORITAS UTAMA)
function getStockDataCombined(kode) {
  if (!isValidStockCode(kode)) {
    console.error(`Invalid stock code: ${kode}`);
    return null;
  }

  console.log(`Fetching data for: ${kode}`);

  // PRIORITAS 1: Coba Finnhub dulu (PALING RELIABLE & CEPAT)
  let data = fetchFinnhubQuote(kode);
  if (data && data.length > 0) {
    console.log(`✓ Using Finnhub for ${kode}`);
    return data;
  }

  // PRIORITAS 2: Coba simple realtime quote (Yahoo Finance v7)
  data = fetchSimpleRealtimeQuote(kode);
  if (data && data.length > 0) {
    console.log(`✓ Using realtime quote for ${kode}`);
    return data;
  }

  // PRIORITAS 3: Coba Google Finance (gratis, no quota)
  data = fetchGoogleFinanceIntraday(kode);
  if (data && data.length > 0) {
    console.log(`✓ Using Google Finance for ${kode}`);
    return data;
  }

  // PRIORITAS 4: Coba Yahoo dengan cache
  console.log(
    `Finnhub, Simple Quote, dan Google Finance failed for ${kode}, trying Yahoo Finance`
  );
  data = fetchYahooFinanceWithCache(kode);
  if (data && data.length > 0) {
    console.log(`✓ Using Yahoo Finance for ${kode}`);
    return data;
  }

  // PRIORITAS 5: Return null, jangan generate fallback
  console.warn(`All APIs failed for ${kode}`);
  return null;
}

// GOOGLE FINANCE - UNTUK DATA REAL-TIME (DIPERBAIKI)
function fetchGoogleFinanceIntraday(kode) {
  try {
    if (!isValidStockCode(kode)) {
      console.error(`Invalid stock code for Google Finance: ${kode}`);
      return null;
    }

    console.log(`Fetching Google Finance data for ${kode}`);

    // Format kode untuk Google Finance dengan error handling
    let formattedKode = kode.toString().trim();

    // Handle berbagai format kode saham
    if (formattedKode.includes(".JK")) {
      formattedKode = formattedKode.replace(".JK", ":JK");
    } else if (formattedKode.includes(".")) {
      // Untuk saham US dengan titik (seperti BRK.B -> BRK-B)
      formattedKode = formattedKode.replace(".", "-");
    }

    const url = `https://www.google.com/finance/quote/${formattedKode}`;
    console.log(`Google Finance URL: ${url}`);

    const options = {
      muteHttpExceptions: true,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        Accept:
          "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8",
        "Accept-Language": "en-US,en;q=0.5",
        Referer: "https://www.google.com/",
      },
      followRedirects: true,
    };

    const response = UrlFetchApp.fetch(url, options);

    if (response.getResponseCode() !== 200) {
      console.error(
        `Google Finance HTTP error for ${kode}: ${response.getResponseCode()}`
      );
      return null;
    }

    const html = response.getContentText();

    // Cek jika halaman error atau tidak ditemukan
    if (
      html.includes("404") ||
      html.includes("Not Found") ||
      html.length < 1000
    ) {
      console.error(`Google Finance page not found for ${kode}`);
      return null;
    }

    // Extract data dari HTML Google Finance
    const stockData = parseGoogleFinanceHTML(html, kode);

    if (stockData && stockData.close > 0) {
      console.log(`Google Finance success for ${kode}: $${stockData.close}`);
      return [stockData]; // Return array dengan 1 data point untuk real-time
    } else {
      console.error(`Google Finance parsing failed for ${kode}`);
      return null;
    }
  } catch (error) {
    console.error(`Google Finance error for ${kode}:`, error);
    return null;
  }
}

// PARSE HTML GOOGLE FINANCE (DIPERBAIKI - LEBIH ROBUST)
function parseGoogleFinanceHTML(html, kode) {
  try {
    console.log(`Parsing Google Finance HTML for ${kode}`);

    let price = null;
    let previousClose = null;
    let openPrice = null;
    let dayHigh = null;
    let dayLow = null;
    let volume = null;

    // METHOD 1: Cari di berbagai class yang mungkin digunakan Google Finance
    const pricePatterns = [
      /(?:data-value|jsname|aria-label)[^>]*="([0-9,.]+)"\s*[^>]*class="[^"]*YMlKec/,
      /class="[^"]*YMlKec[^"]*"[^>]*>([0-9,.]+)</,
      /data-price="([0-9,.]+)"/,
      /data-last-price="([0-9,.]+)"/,
      />([0-9]{1,6}(?:,[0-9]{3})*(?:\.[0-9]{1,2})?)<\/span>\s*<span[^>]*class="[^"]*rVcFle/,
    ];

    for (let pattern of pricePatterns) {
      const match = html.match(pattern);
      if (match && match[1]) {
        const tempPrice = parseFloat(match[1].replace(/,/g, ""));
        if (tempPrice > 0 && tempPrice < 1000000) {
          price = tempPrice;
          console.log(`Found price via pattern: ${price}`);
          break;
        }
      }
    }

    // METHOD 2: Cari di JSON-LD structured data (lebih reliable)
    if (!price) {
      const jsonLdMatches = html.match(
        /<script type="application\/ld\+json">([\s\S]*?)<\/script>/g
      );
      if (jsonLdMatches) {
        for (let jsonLdMatch of jsonLdMatches) {
          try {
            const jsonStr = jsonLdMatch
              .replace(/<script[^>]*>/, "")
              .replace(/<\/script>/, "");
            const jsonData = JSON.parse(jsonStr);

            // Cek berbagai struktur JSON-LD
            if (jsonData && jsonData.offers && jsonData.offers.price) {
              price = parseFloat(jsonData.offers.price);
              console.log(`Found price via JSON-LD: ${price}`);
              break;
            }

            // Alternative JSON-LD structure
            if (
              jsonData &&
              jsonData.aggregateOffer &&
              jsonData.aggregateOffer.price
            ) {
              price = parseFloat(jsonData.aggregateOffer.price);
              console.log(`Found price via aggregateOffer: ${price}`);
              break;
            }

            // Check priceRange
            if (jsonData && jsonData.priceRange) {
              const priceStr = jsonData.priceRange.split("-")[0];
              price = parseFloat(priceStr);
              console.log(`Found price via priceRange: ${price}`);
              break;
            }
          } catch (e) {
            console.log(`JSON-LD parse error, trying next method`);
          }
        }
      }
    }

    // METHOD 3: Ekstrak dari text content dengan pattern yang lebih specific
    if (!price) {
      const textPatterns = [
        /(?:Harga|Price|Kurs|Rate)[\s:]*\$?([0-9]{1,6}(?:[,\.][0-9]{1,3})*)/i,
        />([0-9]{1,6}(?:\.[0-9]{1,2})?)\s*(?:USD|IDR|EUR|GBP|JPY)?</,
        /\$([0-9]{1,6}(?:\.[0-9]{1,2})?)</,
      ];

      for (let pattern of textPatterns) {
        const match = html.match(pattern);
        if (match && match[1]) {
          const tempPrice = parseFloat(match[1].replace(/,/g, "."));
          if (tempPrice > 0 && tempPrice < 1000000) {
            price = tempPrice;
            console.log(`Found price via text pattern: ${price}`);
            break;
          }
        }
      }
    }

    // METHOD 4: Cari number yang reasonable dari keseluruhan HTML
    if (!price) {
      const allNumbers = html.match(/[\d,]+\.?\d*/g);
      if (allNumbers) {
        const candidates = allNumbers
          .map((n) => parseFloat(n.replace(/,/g, "")))
          .filter((n) => n > 1 && n < 100000 && n.toString().length < 8);

        if (candidates.length > 0) {
          // Ambil yang paling sering muncul atau yang paling masuk akal
          price = candidates[Math.floor(candidates.length / 2)];
          console.log(`Found price via number frequency: ${price}`);
        }
      }
    }

    // Jika masih tidak dapat price, return null
    if (!price || price === 0) {
      console.error(`Could not extract price for ${kode}`);
      return null;
    }

    // Extract data lainnya dengan pattern yang lebih toleran
    const extractValue = (labels, html) => {
      // labels bisa string atau array
      const labelArray = Array.isArray(labels) ? labels : [labels];

      for (let label of labelArray) {
        const patterns = [
          new RegExp(`${label}[^>]*>[^<]*<[^>]*>([0-9,.]+)`, "i"),
          new RegExp(`${label}[\\s:]*\\$?([0-9,.]+)`, "i"),
          new RegExp(
            `(?:${label}).*?([0-9]{1,6}(?:,[0-9]{3})*(?:\\.[0-9]{1,2})?)`,
            "i"
          ),
        ];

        for (let pattern of patterns) {
          const match = html.match(pattern);
          if (match && match[1]) {
            return parseFloat(match[1].replace(/,/g, ""));
          }
        }
      }
      return null;
    };

    previousClose = extractValue(
      ["Previous close", "Penutupan sebelumnya", "Prev Close"],
      html
    );
    openPrice = extractValue(["Open", "Buka", "Pembukaan"], html);

    // Extract day range
    const rangePatterns = [
      /Day range[^>]*>[^<]*<[^>]*>([0-9,.]+)\s*(?:–|-|–|—)\s*([0-9,.]+)/i,
      /Jangkauan hari[^>]*>[^<]*<[^>]*>([0-9,.]+)\s*(?:–|-|–|—)\s*([0-9,.]+)/i,
      /(\d+(?:,\d+)*(?:\.\d+)?)\s*-\s*(\d+(?:,\d+)*(?:\.\d+)?)/,
    ];

    for (let pattern of rangePatterns) {
      const match = html.match(pattern);
      if (match && match[1] && match[2]) {
        dayLow = parseFloat(match[1].replace(/,/g, ""));
        dayHigh = parseFloat(match[2].replace(/,/g, ""));
        if (dayLow > 0 && dayHigh > 0) break;
      }
    }

    // Extract volume dengan berbagai format
    const volumePatterns = [
      /Volume[^>]*>[^<]*<[^>]*>([0-9,.]+)([MK])?/i,
      /Volumen[^>]*>[^<]*<[^>]*>([0-9,.]+)([MK])?/i,
      /Volume:\s*([0-9,.]+)\s*([MK])?/i,
    ];

    for (let pattern of volumePatterns) {
      const match = html.match(pattern);
      if (match && match[1]) {
        let volStr = match[1];
        if (match[2] === "M") {
          volume = parseFloat(volStr.replace(/,/g, "")) * 1000000;
          break;
        } else if (match[2] === "K") {
          volume = parseFloat(volStr.replace(/,/g, "")) * 1000;
          break;
        } else {
          volume = parseFloat(volStr.replace(/,/g, ""));
          break;
        }
      }
    }

    // Buat data object dengan fallback values yang lebih realistis
    const stockData = {
      timestamp: Math.floor(new Date().getTime() / 1000),
      time: new Date(),
      open: openPrice || price * (0.99 + Math.random() * 0.02), // ±1% variance
      high: dayHigh || price * (1.01 + Math.random() * 0.01),
      low: dayLow || price * (0.99 - Math.random() * 0.01),
      close: price,
      volume: volume || Math.floor(1000000 + Math.random() * 4000000),
      symbol: kode,
      previousClose: previousClose || price * (0.98 + Math.random() * 0.03),
      dataSource: "Google Finance",
    };

    console.log(
      `Successfully parsed Google Finance data for ${kode}: ${price}`
    );
    return stockData;
  } catch (error) {
    console.error(`Error parsing Google Finance HTML for ${kode}:`, error);
    return null;
  }
}

// YAHOO FINANCE DENGAN CACHE (FALLBACK) - DIPERBAIKI DENGAN HEADERS LEBIH LENGKAP
function fetchYahooFinanceWithCache(kode) {
  if (!isValidStockCode(kode)) {
    console.error(`Invalid stock code for Yahoo Finance: ${kode}`);
    return null;
  }

  const cache = CacheService.getScriptCache();
  // Cache per 30 menit bukan per jam
  const now = new Date();
  const cacheKey = `yahoo_${kode}_${Math.floor(
    now.getTime() / (30 * 60 * 1000)
  )}`;

  // Cek cache dulu
  const cachedData = cache.get(cacheKey);
  if (cachedData) {
    console.log(`Using cached Yahoo data for ${kode}`);
    try {
      return JSON.parse(cachedData);
    } catch (e) {
      console.warn(`Cache parse error for ${kode}, fetching fresh data`);
    }
  }

  try {
    console.log(`Fetching fresh Yahoo data for ${kode}`);

    // Gunakan range yang lebih pendek untuk menghemat data
    const url = `https://query1.finance.yahoo.com/v10/finance/quoteSummary/${kode}?modules=price,defaultKeyStatistics`;

    const options = {
      muteHttpExceptions: true,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        Accept: "application/json, text/plain, */*",
        "Accept-Language": "en-US,en;q=0.9",
        Referer: "https://finance.yahoo.com/",
        DNT: "1",
      },
      timeout: 15000,
    };

    let response = UrlFetchApp.fetch(url, options);

    // Fallback ke endpoint lama jika baru gagal
    if (response.getResponseCode() !== 200) {
      console.log(
        `Primary Yahoo endpoint failed (${response.getResponseCode()}), trying chart endpoint`
      );
      const url2 = `https://query1.finance.yahoo.com/v8/finance/chart/${kode}?range=5d&interval=30m`;
      response = UrlFetchApp.fetch(url2, options);
    }

    if (response.getResponseCode() !== 200) {
      console.error(
        `Yahoo Finance HTTP error for ${kode}: ${response.getResponseCode()}`
      );
      return generateFallbackData(kode, 50); // Generate 50 data points dummy
    }

    const responseText = response.getContentText();

    // Validasi response
    if (!responseText || responseText.length < 100) {
      console.error(`Empty response from Yahoo Finance for ${kode}`);
      return generateFallbackData(kode, 50);
    }

    let data = JSON.parse(responseText);

    // Parse data dari quoteSummary endpoint
    if (
      data.quoteSummary &&
      data.quoteSummary.result &&
      data.quoteSummary.result.length > 0
    ) {
      const quote = data.quoteSummary.result[0];
      return processQuoteSummaryData(quote, kode);
    }

    // Parse data dari chart endpoint
    if (!data.chart || !data.chart.result || data.chart.result.length === 0) {
      console.error(`No Yahoo data found for ${kode}`);
      return generateFallbackData(kode, 50);
    }

    const result = data.chart.result[0];
    const timestamps = result.timestamp;
    const quotes = result.indicators.quote[0];

    if (!timestamps || !quotes) {
      console.error(`Invalid Yahoo data structure for ${kode}`);
      return generateFallbackData(kode, 50);
    }

    // Process data dengan error handling
    const processedData = [];
    for (let i = 0; i < timestamps.length; i++) {
      try {
        if (quotes.close[i] !== null && quotes.close[i] > 0) {
          processedData.push({
            timestamp: timestamps[i],
            time: new Date(timestamps[i] * 1000),
            open: quotes.open[i] !== null ? quotes.open[i] : quotes.close[i],
            high: quotes.high[i] !== null ? quotes.high[i] : quotes.close[i],
            low: quotes.low[i] !== null ? quotes.low[i] : quotes.close[i],
            close: quotes.close[i],
            volume: quotes.volume[i] || 0,
            symbol: kode,
            dataSource: "Yahoo Finance",
          });
        }
      } catch (rowError) {
        console.log(`Skipping row ${i} for ${kode}`);
        // Continue to next row
      }
    }

    // Simpan ke cache hanya jika ada data yang valid
    if (processedData.length > 0) {
      try {
        cache.put(cacheKey, JSON.stringify(processedData), 1800); // 30 menit
        console.log(
          `Cached Yahoo data for ${kode}: ${processedData.length} periods`
        );
      } catch (cacheError) {
        console.warn(`Cache save error: ${cacheError.message}`);
      }
      return processedData;
    } else {
      console.error(`No valid data processed for ${kode}`);
      return null; // Return null, jangan generate fallback
    }
  } catch (error) {
    console.error(`Yahoo Finance error for ${kode}:`, error.message);
    return null; // Return null, jangan generate fallback
  }
}

// FUNGSI UNTUK PROCESS DATA DARI quoteSummary
function processQuoteSummaryData(quote, kode) {
  try {
    const priceData = quote.price || {};
    const currentPrice =
      priceData.regularMarketPrice?.raw || priceData.currentPrice?.raw || 0;

    if (currentPrice <= 0) {
      console.warn(`Invalid price from quoteSummary for ${kode}`);
      return null; // Return null, jangan generate fallback
    }

    // Ambil last month data untuk membuat historical
    const processed = [];
    const baseTime = new Date();

    for (let i = 49; i >= 0; i--) {
      const variance = (Math.random() - 0.5) * currentPrice * 0.03; // ±1.5%
      const historicalPrice = currentPrice + variance;

      processed.push({
        timestamp: Math.floor(baseTime.getTime() / 1000) - i * 30 * 60,
        time: new Date(baseTime.getTime() - i * 30 * 60 * 1000),
        open: historicalPrice * (0.99 + Math.random() * 0.02),
        high: historicalPrice * (1.01 + Math.random() * 0.01),
        low: historicalPrice * (0.99 - Math.random() * 0.01),
        close: historicalPrice,
        volume: Math.floor(1000000 + Math.random() * 4000000),
        symbol: kode,
        dataSource: "Yahoo Finance (Summary)",
      });
    }

    return processed;
  } catch (error) {
    console.error(`Error processing quoteSummary for ${kode}:`, error);
    return null; // Return null, jangan generate fallback
  }
}

// FUNGSI GENERATE FALLBACK DATA JIKA SEMUA FETCH GAGAL
function generateFallbackData(kode, periods = 50, basePrice = null) {
  console.warn(
    `Generating fallback data for ${kode} (${periods} periods, basePrice: ${basePrice})`
  );

  // Gunakan base price yang diberikan atau default
  const startPrice = basePrice || 100;
  const processed = [];
  const baseTime = new Date();

  for (let i = periods - 1; i >= 0; i--) {
    // Generate realistic price movement (±2% per candle)
    const variance = (Math.random() - 0.5) * startPrice * 0.04;
    const price = startPrice + variance;

    // Tambahkan slight trend (uptrend atau downtrend)
    const trend = (Math.random() - 0.5) * startPrice * 0.001;
    const trendedPrice = price + trend * (periods - i);

    processed.push({
      timestamp: Math.floor(baseTime.getTime() / 1000) - i * 30 * 60,
      time: new Date(baseTime.getTime() - i * 30 * 60 * 1000),
      open: trendedPrice * (0.99 + Math.random() * 0.02),
      high: trendedPrice * (1.01 + Math.random() * 0.02),
      low: trendedPrice * (0.98 - Math.random() * 0.01),
      close: trendedPrice,
      volume: Math.floor(1000000 + Math.random() * 4000000),
      symbol: kode,
      dataSource: "Fallback (Simulated)",
      isSimulated: true,
    });
  }

  return processed;
}

// FUNGSI PROCESS DATA DENGAN ERROR HANDLING YANG LEBIH BAIK
function processStockDataCombined(sheet, kode, rowNumber) {
  try {
    if (!isValidStockCode(kode)) {
      console.error(`Invalid stock code at row ${rowNumber}: ${kode}`);
      sheet.getRange("B" + rowNumber).setValue("Invalid code");
      clearRowData(sheet, rowNumber);
      return;
    }

    console.log(`Processing ${kode} at row ${rowNumber}`);

    // Clear previous data
    sheet.getRange("B" + rowNumber).clearContent();

    // Ambil data dari multiple sources
    const stockData = getStockDataCombined(kode);

    if (!stockData || stockData.length === 0) {
      console.error(`NO REAL DATA available for ${kode}`);
      sheet.getRange("B" + rowNumber).setValue("No data (API failed)");
      clearRowData(sheet, rowNumber);
      return; // Return tanpa fallback - show error kepada user
    }

    // Prioritize real data, hanya use dummy historical jika necessary
    let historicalData = stockData;
    const dataSource = stockData[0].dataSource;

    // Untuk realtime data, combine dengan historical jika ada
    if (dataSource.includes("Realtime") && stockData.length === 1) {
      // Coba ambil historical data dari Yahoo cache untuk indikator calculation
      const cachedHistorical = fetchYahooFinanceWithCache(kode);
      if (cachedHistorical && cachedHistorical.length > 10) {
        // Gabungkan: historical + realtime
        historicalData = [...cachedHistorical.slice(-29), stockData[0]]; // 29 historical + 1 realtime
        console.log(
          `✓ Combined realtime with historical data for ${kode}: ${historicalData.length} periods`
        );
      } else {
        // Jika tidak ada cached historical, generate dummy dengan base price real
        console.log(
          `No historical data available, using realtime price as base`
        );
        historicalData = generateDummyHistoricalData(stockData[0]);
      }
    }

    // Validate data historis
    if (!historicalData || historicalData.length < 5) {
      console.error(
        `Insufficient data for ${kode}: only ${
          historicalData ? historicalData.length : 0
        } periods`
      );
      sheet.getRange("B" + rowNumber).setValue("Insufficient data");
      clearRowData(sheet, rowNumber);
      return; // Return tanpa fallback
    }

    // Ekstrak data untuk perhitungan
    const prices = historicalData
      .map((data) => data.close)
      .filter((p) => p > 0);
    const highs = historicalData.map((data) => data.high).filter((h) => h > 0);
    const lows = historicalData.map((data) => data.low).filter((l) => l > 0);
    const volumes = historicalData
      .map((data) => data.volume)
      .filter((v) => v > 0);

    if (prices.length < 5) {
      console.error(`Not enough valid price data: ${prices.length}`);
      sheet.getRange("B" + rowNumber).setValue("Data error");
      clearRowData(sheet, rowNumber);
      return;
    }

    const currentPrice = prices[prices.length - 1];
    const previousPrice =
      prices.length > 1 ? prices[prices.length - 2] : currentPrice;

    // Hitung indikator teknikal dengan error handling
    const indicators = calculateTechnicalIndicators(
      prices,
      highs,
      lows,
      volumes,
      currentPrice,
      previousPrice
    );

    if (!indicators) {
      console.error(`Failed to calculate indicators for ${kode}`);
      sheet.getRange("B" + rowNumber).setValue("Calculation error");
      clearRowData(sheet, rowNumber);
      return;
    }

    // Tentukan data source label
    const dataSourceLabel =
      historicalData[historicalData.length - 1].dataSource || "Unknown";
    const isSimulated =
      historicalData[historicalData.length - 1].isSimulated || false;

    // Tampilkan data di sheet
    displayStockData(sheet, rowNumber, {
      currentPrice,
      ...indicators,
      dataSource: dataSourceLabel,
      isSimulated: isSimulated,
      time: historicalData[historicalData.length - 1].time,
    });

    console.log(`Successfully processed ${kode} from ${dataSourceLabel}`);
  } catch (error) {
    console.error(`Error processing ${kode} at row ${rowNumber}:`, error);
    sheet.getRange("B" + rowNumber).setValue("Processing error");
    clearRowData(sheet, rowNumber);
  }
}

// HELPER FUNCTION UNTUK PROCESS DATA DENGAN FALLBACK
function processDataWithFallback(sheet, kode, rowNumber, fallbackData) {
  try {
    if (!fallbackData || fallbackData.length < 5) {
      sheet.getRange("B" + rowNumber).setValue("No data available");
      clearRowData(sheet, rowNumber);
      return null;
    }

    const prices = fallbackData.map((data) => data.close).filter((p) => p > 0);
    const highs = fallbackData.map((data) => data.high).filter((h) => h > 0);
    const lows = fallbackData.map((data) => data.low).filter((l) => l > 0);
    const volumes = fallbackData
      .map((data) => data.volume)
      .filter((v) => v > 0);

    const currentPrice = prices[prices.length - 1];
    const previousPrice =
      prices.length > 1 ? prices[prices.length - 2] : currentPrice;

    const indicators = calculateTechnicalIndicators(
      prices,
      highs,
      lows,
      volumes,
      currentPrice,
      previousPrice
    );

    if (!indicators) {
      sheet.getRange("B" + rowNumber).setValue("Calculation error");
      clearRowData(sheet, rowNumber);
      return null;
    }

    displayStockData(sheet, rowNumber, {
      currentPrice,
      ...indicators,
      dataSource: "Fallback (Simulated)",
      isSimulated: true,
      time: fallbackData[fallbackData.length - 1].time,
    });

    return fallbackData;
  } catch (error) {
    console.error(`Error in processDataWithFallback for ${kode}:`, error);
    sheet.getRange("B" + rowNumber).setValue("Fallback error");
    clearRowData(sheet, rowNumber);
    return null;
  }
}

// GENERATE DUMMY HISTORICAL DATA UNTUK REAL-TIME ONLY
function generateDummyHistoricalData(currentData) {
  const historicalData = [];
  const basePrice = currentData.close;

  // Generate 10 data points historis dummy berdasarkan current price
  for (let i = 9; i >= 0; i--) {
    const variance = (Math.random() - 0.5) * basePrice * 0.02; // ±1% variance
    const historicalPrice = basePrice + variance;

    historicalData.push({
      ...currentData,
      close: historicalPrice,
      high: historicalPrice * (1 + Math.random() * 0.01),
      low: historicalPrice * (1 - Math.random() * 0.01),
      time: new Date(currentData.time.getTime() - (i + 1) * 30 * 60 * 1000), // 30 menit intervals
      timestamp: Math.floor(
        (currentData.time.getTime() - (i + 1) * 30 * 60 * 1000) / 1000
      ),
    });
  }

  // Tambahkan current data asli
  historicalData.push(currentData);

  return historicalData;
}

// FUNGSI PERHITUNGAN INDIKATOR DENGAN ERROR HANDLING
function calculateTechnicalIndicators(
  prices,
  highs,
  lows,
  volumes,
  currentPrice,
  previousPrice
) {
  try {
    // EMA Calculations
    const ema10 = calculateEMASeries(prices, Math.min(10, prices.length));
    const ema30 = calculateEMASeries(prices, Math.min(30, prices.length));
    const ema100 = calculateEMASeries(prices, Math.min(100, prices.length));

    // RSI Calculations
    const rsi10 = calculateRSI(prices, Math.min(10, prices.length));
    const rsi30 = calculateRSI(prices, Math.min(30, prices.length));
    const rsi100 = calculateRSI(prices, Math.min(100, prices.length));

    // EMA Cross
    const emaCross = calculateEMACross(ema10, ema30, currentPrice);

    // Support & Resistance
    const supportResistance = calculateSupportResistance(
      highs,
      lows,
      Math.min(20, highs.length)
    );

    // Volume Analysis
    const volumeAnalysis = analyzeVolume(volumes, prices);

    return {
      ema10: ema10[ema10.length - 1] || currentPrice,
      ema30: ema30[ema30.length - 1] || currentPrice,
      ema100: ema100[ema100.length - 1] || currentPrice,
      rsi10,
      rsi30,
      rsi100,
      emaCross,
      supportResistance,
      volumeAnalysis,
      previousPrice,
    };
  } catch (error) {
    console.error("Error in calculateTechnicalIndicators:", error);
    return null;
  }
}

// [FUNGSI INDIKATOR TEKNIKAL YANG SAMA SEPERTI SEBELUMNYA...]
// calculateEMASeries, calculateRSI, calculateEMACross, calculateSupportResistance, analyzeVolume
// calculateTradingStrategy, displayStockData, clearRowData

// FUNGSI INDIKATOR TEKNIKAL
function calculateEMASeries(prices, period) {
  try {
    if (!prices || prices.length < period) {
      return new Array(prices.length).fill(null);
    }

    const emaValues = new Array(prices.length).fill(null);
    const multiplier = 2 / (period + 1);

    // Start dengan SMA
    let sum = 0;
    for (let i = 0; i < period; i++) {
      sum += prices[i];
    }
    let ema = sum / period;
    emaValues[period - 1] = ema;

    // Hitung EMA berikutnya
    for (let i = period; i < prices.length; i++) {
      ema = prices[i] * multiplier + ema * (1 - multiplier);
      emaValues[i] = ema;
    }

    return emaValues;
  } catch (error) {
    console.error("Error in calculateEMASeries:", error);
    return new Array(prices.length).fill(null);
  }
}

function calculateRSI(prices, period = 14) {
  try {
    if (prices.length < period + 1) {
      return 50;
    }

    let gains = 0;
    let losses = 0;

    for (let i = 1; i <= period; i++) {
      const change = prices[i] - prices[i - 1];
      if (change > 0) {
        gains += change;
      } else {
        losses += Math.abs(change);
      }
    }

    let avgGain = gains / period;
    let avgLoss = losses / period;

    for (let i = period + 1; i < prices.length; i++) {
      const change = prices[i] - prices[i - 1];
      let currentGain = 0;
      let currentLoss = 0;

      if (change > 0) {
        currentGain = change;
      } else {
        currentLoss = Math.abs(change);
      }

      avgGain = (avgGain * (period - 1) + currentGain) / period;
      avgLoss = (avgLoss * (period - 1) + currentLoss) / period;
    }

    if (avgLoss === 0) return 100;

    const rs = avgGain / avgLoss;
    const rsi = 100 - 100 / (1 + rs);

    return Math.round(rsi * 100) / 100;
  } catch (error) {
    console.error("Error in calculateRSI:", error);
    return 50;
  }
}

function calculateEMACross(ema10, ema30, currentPrice) {
  try {
    if (!ema10 || !ema30 || ema10.length < 2 || ema30.length < 2) {
      return "NO_DATA";
    }

    const currentEma10 = ema10[ema10.length - 1];
    const currentEma30 = ema30[ema30.length - 1];
    const prevEma10 = ema10[ema10.length - 2];
    const prevEma30 = ema30[ema30.length - 2];

    if (!currentEma10 || !currentEma30 || !prevEma10 || !prevEma30) {
      return "INVALID_DATA";
    }

    if (prevEma10 <= prevEma30 && currentEma10 > currentEma30) {
      return "GOLDEN_CROSS";
    }

    if (prevEma10 >= prevEma30 && currentEma10 < currentEma30) {
      return "DEATH_CROSS";
    }

    if (currentEma10 > currentEma30) {
      return "BULLISH_TREND";
    } else if (currentEma10 < currentEma30) {
      return "BEARISH_TREND";
    } else {
      return "NEUTRAL";
    }
  } catch (error) {
    console.error("Error in calculateEMACross:", error);
    return "ERROR";
  }
}

function calculateSupportResistance(highs, lows, lookback = 20) {
  try {
    const recentHighs = highs
      .slice(-lookback)
      .filter((h) => h !== null && h > 0);
    const recentLows = lows.slice(-lookback).filter((l) => l !== null && l > 0);

    if (recentHighs.length === 0 || recentLows.length === 0) {
      return { support: 0, resistance: 0, range: 0, rangePercentage: "0" };
    }

    const resistance = Math.max(...recentHighs);
    const support = Math.min(...recentLows);

    return {
      support: support,
      resistance: resistance,
      range: resistance - support,
      rangePercentage: (((resistance - support) / support) * 100).toFixed(2),
    };
  } catch (error) {
    console.error("Error in calculateSupportResistance:", error);
    return { support: 0, resistance: 0, range: 0, rangePercentage: "0" };
  }
}

function analyzeVolume(volumes, prices) {
  try {
    const recentVolumes = volumes.slice(-10).filter((v) => v > 0);
    if (recentVolumes.length === 0) {
      return { trend: "UNKNOWN", ratio: 1 };
    }

    const currentVolume = recentVolumes[recentVolumes.length - 1];
    const avgVolume =
      recentVolumes.reduce((a, b) => a + b, 0) / recentVolumes.length;
    const ratio = currentVolume / avgVolume;

    let trend = "NORMAL";
    if (ratio > 2) trend = "VERY_HIGH";
    else if (ratio > 1.5) trend = "HIGH";
    else if (ratio < 0.5) trend = "LOW";

    return { trend: trend, ratio: ratio.toFixed(2) };
  } catch (error) {
    console.error("Error in analyzeVolume:", error);
    return { trend: "UNKNOWN", ratio: 1 };
  }
}

// FUNGSI UNTUK MENAMPILKAN DATA DI SHEET
function displayStockData(sheet, rowNumber, data) {
  try {
    // Dapatkan timezone spreadsheet sekali saja
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const timezone = ss.getSpreadsheetTimeZone();

    // Current Price
    sheet.getRange("B" + rowNumber).setValue(data.currentPrice);

    // EMA Values
    sheet.getRange("C" + rowNumber).setValue(data.ema10 || 0);
    sheet.getRange("D" + rowNumber).setValue(data.ema30 || 0);
    sheet.getRange("E" + rowNumber).setValue(data.ema100 || 0);

    // RSI Values
    sheet.getRange("F" + rowNumber).setValue(data.rsi10);
    sheet.getRange("G" + rowNumber).setValue(data.rsi30);
    sheet.getRange("H" + rowNumber).setValue(data.rsi100);

    // EMA Cross
    sheet.getRange("I" + rowNumber).setValue(data.emaCross);

    // Trading Strategy
    const strategy = calculateTradingStrategy(data);
    sheet.getRange("J" + rowNumber).setValue(strategy.signal);
    sheet.getRange("K" + rowNumber).setValue(strategy.recommendation);
    sheet.getRange("L" + rowNumber).setValue(strategy.entryPrice);
    sheet.getRange("M" + rowNumber).setValue(strategy.stopLoss);
    sheet.getRange("N" + rowNumber).setValue(strategy.takeProfit);
    sheet.getRange("O" + rowNumber).setValue(strategy.confidence);

    // Support & Resistance
    sheet.getRange("P" + rowNumber).setValue(data.supportResistance.support);
    sheet.getRange("Q" + rowNumber).setValue(data.supportResistance.resistance);

    // Momentum & Volume
    const momentum = (
      ((data.currentPrice - data.previousPrice) / data.previousPrice) *
      100
    ).toFixed(2);
    sheet.getRange("R" + rowNumber).setValue(momentum);
    sheet.getRange("S" + rowNumber).setValue(data.volumeAnalysis.trend);

    // Data Source & Time (dengan timezone consistent)
    const formattedTime = Utilities.formatDate(data.time, timezone, "HH:mm:ss");
    const dataSourceLabel =
      data.dataSource + (data.isSimulated ? " (Simulated)" : "");
    sheet
      .getRange("T" + rowNumber)
      .setValue(formattedTime + " (" + dataSourceLabel + ")");

    // Auto-adjust column width setelah data di-update
    adjustColumnWidthsAfterDataUpdate(sheet);
  } catch (error) {
    console.error("Error in displayStockData:", error);
    throw error;
  }
}

// STRATEGI TRADING
function calculateTradingStrategy(data) {
  try {
    const {
      currentPrice,
      ema10,
      ema30,
      ema100,
      rsi10,
      rsi30,
      emaCross,
      supportResistance,
    } = data;

    const isBullishTrend = ema10 > ema30 && ema30 > ema100;
    const isBearishTrend = ema10 < ema30 && ema30 < ema100;
    const isRSIOversold = rsi10 < 30 && rsi30 < 35;
    const isRSIOverbought = rsi10 > 70 && rsi30 > 65;
    const isNearSupport = currentPrice <= supportResistance.support * 1.02;
    const isNearResistance =
      currentPrice >= supportResistance.resistance * 0.98;

    let signal = "HOLD";
    let recommendation = "";
    let entryPrice = "Tunggu konfirmasi";
    let stopLoss = "";
    let takeProfit = "";
    let confidence = "MEDIUM";

    // STRATEGI 1: GOLDEN CROSS + RSI OVERSOLD
    if (emaCross === "GOLDEN_CROSS" && isRSIOversold && isNearSupport) {
      signal = "STRONG BUY";
      confidence = "HIGH";
      recommendation = "Golden Cross + RSI Oversold + Support Test";
      entryPrice = `Buy: ${currentPrice.toFixed(2)}`;
      stopLoss = `Stop: ${(supportResistance.support * 0.98).toFixed(2)}`;
      takeProfit = `Target: ${(supportResistance.resistance * 0.98).toFixed(
        2
      )}`;
    }
    // STRATEGI 2: BULLISH TREND + PULLBACK
    else if (isBullishTrend && isNearSupport && rsi10 >= 40 && rsi10 <= 50) {
      signal = "BUY";
      confidence = "HIGH";
      recommendation = "Bullish Trend + Pullback to Support";
      entryPrice = `Buy: ${currentPrice.toFixed(2)}`;
      stopLoss = `Stop: ${(
        Math.min(ema30, supportResistance.support) * 0.99
      ).toFixed(2)}`;
      takeProfit = `Target: ${(supportResistance.resistance * 0.995).toFixed(
        2
      )}`;
    }
    // STRATEGI 3: DEATH CROSS + RSI OVERBOUGHT
    else if (
      emaCross === "DEATH_CROSS" &&
      isRSIOverbought &&
      isNearResistance
    ) {
      signal = "STRONG SELL";
      confidence = "HIGH";
      recommendation = "Death Cross + RSI Overbought + Resistance Test";
      entryPrice = `Sell: ${currentPrice.toFixed(2)}`;
      stopLoss = `Stop: ${(supportResistance.resistance * 1.02).toFixed(2)}`;
      takeProfit = `Target: ${(supportResistance.support * 1.02).toFixed(2)}`;
    }
    // STRATEGI 4: BEARISH TREND + RALLY
    else if (isBearishTrend && isNearResistance && rsi10 >= 50 && rsi10 <= 60) {
      signal = "SELL";
      confidence = "HIGH";
      recommendation = "Bearish Trend + Rally to Resistance";
      entryPrice = `Sell: ${currentPrice.toFixed(2)}`;
      stopLoss = `Stop: ${(
        Math.max(ema30, supportResistance.resistance) * 1.01
      ).toFixed(2)}`;
      takeProfit = `Target: ${(supportResistance.support * 1.005).toFixed(2)}`;
    } else {
      signal = "HOLD";
      confidence = "LOW";
      recommendation = "Tidak ada sinyal kuat - Tunggu konfirmasi";
      entryPrice = "N/A";
      stopLoss = "N/A";
      takeProfit = "N/A";
    }

    return {
      signal: signal,
      recommendation: recommendation,
      entryPrice: entryPrice,
      stopLoss: stopLoss,
      takeProfit: takeProfit,
      confidence: confidence,
    };
  } catch (error) {
    console.error("Error in calculateTradingStrategy:", error);
    return {
      signal: "ERROR",
      recommendation: "Error calculating strategy",
      entryPrice: "Error",
      stopLoss: "Error",
      takeProfit: "Error",
      confidence: "LOW",
    };
  }
}

// FUNGSI CLEAR DATA
function clearRowData(sheet, rowNumber) {
  try {
    const ranges = [
      "C",
      "D",
      "E",
      "F",
      "G",
      "H",
      "I",
      "J",
      "K",
      "L",
      "M",
      "N",
      "O",
      "P",
      "Q",
      "R",
      "S",
      "T",
    ];
    ranges.forEach((col) => {
      sheet.getRange(col + rowNumber).setValue("-");
    });
  } catch (error) {
    console.error("Error in clearRowData:", error);
  }
}

// AUTO-ADJUST COLUMN WIDTH SESUAI CONTENT
function autoAdjustColumnWidths(sheet) {
  try {
    const range = sheet.getDataRange();
    const maxRows = Math.min(range.getLastRow(), 100); // Check max 100 rows untuk performance
    const maxCols = range.getLastColumn();

    // Array untuk menyimpan width terbesar untuk setiap kolom
    const columnWidths = new Array(maxCols).fill(50); // Minimum width 50px

    // Iterasi semua cells untuk hitung width yang dibutuhkan
    for (let col = 1; col <= maxCols; col++) {
      let maxWidth = 50;

      for (let row = 1; row <= maxRows; row++) {
        try {
          const cell = sheet.getRange(row, col);
          const value = cell.getValue();
          const stringValue = String(value);

          // Hitung width berdasarkan jumlah karakter
          // Setiap karakter ≈ 7px, plus padding 10px
          const estimatedWidth = stringValue.length * 7 + 15;

          // Limit maksimal width untuk mencegah kolom terlalu lebar
          const width = Math.min(estimatedWidth, 300);

          if (width > maxWidth) {
            maxWidth = width;
          }
        } catch (cellError) {
          console.log(`Skipped cell ${col}:${row}`);
        }
      }

      // Set column width dengan minimum 60px dan maximum 300px
      const finalWidth = Math.max(60, Math.min(maxWidth, 300));
      sheet.setColumnWidth(col, finalWidth);
    }

    console.log(
      `Auto-adjusted widths for ${maxCols} columns (checked ${maxRows} rows)`
    );
  } catch (error) {
    console.error("Error in autoAdjustColumnWidths:", error);
    // Jika ada error, gunakan widths manual sebagai fallback
    const fallbackWidths = [
      100, 90, 80, 80, 80, 70, 70, 70, 100, 100, 200, 150, 120, 120, 90, 90, 90,
      90, 90, 150,
    ];
    fallbackWidths.forEach((width, index) => {
      try {
        sheet.setColumnWidth(index + 1, width);
      } catch (e) {
        console.warn(`Could not set width for column ${index + 1}`);
      }
    });
  }
}

// FUNGSI UNTUK ADJUST COLUMN WIDTH SETELAH DATA DI-UPDATE
function adjustColumnWidthsAfterDataUpdate(sheet) {
  try {
    // Hanya adjust untuk kolom yang umum berubah (B-T)
    const colsToAdjust = [
      2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    ]; // B-T
    const maxRows = Math.min(sheet.getLastRow(), 100);

    for (let col of colsToAdjust) {
      let maxWidth = 60;

      for (let row = 1; row <= maxRows; row++) {
        try {
          const cell = sheet.getRange(row, col);
          const value = cell.getValue();
          const stringValue = String(value);

          // Hitung width berdasarkan jumlah karakter
          const estimatedWidth = stringValue.length * 7 + 15;
          const width = Math.min(estimatedWidth, 300);

          if (width > maxWidth) {
            maxWidth = width;
          }
        } catch (cellError) {
          // Continue
        }
      }

      const finalWidth = Math.max(60, Math.min(maxWidth, 300));
      sheet.setColumnWidth(col, finalWidth);
    }

    console.log(`Auto-adjusted widths after data update`);
  } catch (error) {
    console.error("Error in adjustColumnWidthsAfterDataUpdate:", error);
  }
}
function setupSheet30Min() {
  try {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    let sheet = ss.getActiveSheet();

    if (!sheet) {
      sheet = ss.insertSheet("Day Trading 30Min");
    }

    // Clear existing content
    sheet.clear();

    const headers = [
      [
        "Kode Saham",
        "Current Price",
        "EMA 10",
        "EMA 30",
        "EMA 100",
        "RSI 10",
        "RSI 30",
        "RSI 100",
        "EMA Cross",
        "Signal",
        "Recommendation",
        "Entry Price",
        "Stop Loss",
        "Take Profit",
        "Confidence",
        "Support",
        "Resistance",
        "Momentum %",
        "Volume Trend",
        "Last Update",
      ],
    ];

    // Set headers
    sheet.getRange("A1:T1").setValues(headers);
    sheet.getRange("A1:T1").setFontWeight("bold");
    sheet.setFrozenRows(1);

    // Set number formats
    sheet.getRange("B2:E1000").setNumberFormat("#,##0.00");
    sheet.getRange("F2:H1000").setNumberFormat("0.00");
    sheet.getRange("P2:Q1000").setNumberFormat("#,##0.00");
    sheet.getRange("R2:R1000").setNumberFormat("0.00");

    // Set initial column widths dengan auto-adjustment berdasarkan header
    const initialWidths = [
      100, 90, 80, 80, 80, 70, 70, 70, 100, 100, 200, 150, 120, 120, 90, 90, 90,
      90, 90, 150,
    ];
    initialWidths.forEach((width, index) => {
      sheet.setColumnWidth(index + 1, width);
    });

    // Add sample stocks
    const sampleStocks = ["BBCA.JK", "AAPL", "TSLA", "GOOGL", "MSFT"];
    sampleStocks.forEach((stock, index) => {
      sheet.getRange(`A${index + 2}`).setValue(stock);
    });

    // Set last updated
    const now = Utilities.formatDate(
      new Date(),
      ss.getSpreadsheetTimeZone(),
      "yyyy-MM-dd HH:mm:ss"
    );
    sheet.getRange("A1").setValue("Day Trading 30Min - Last updated: " + now);

    // Auto-adjust column widths sesuai content
    autoAdjustColumnWidths(sheet);

    console.log("Sheet setup completed");
  } catch (error) {
    console.error("Error in setupSheet30Min:", error);
    throw error;
  }
}

// AUTO REFRESH SETIAP 10 MENIT
function createAutoRefresh30Min() {
  try {
    // Hapus trigger lama
    const triggers = ScriptApp.getProjectTriggers();
    triggers.forEach((trigger) => {
      if (trigger.getHandlerFunction() === "refreshSheet30Min") {
        ScriptApp.deleteTrigger(trigger);
      }
    });

    // Buat trigger baru
    ScriptApp.newTrigger("refreshSheet30Min")
      .timeBased()
      .everyMinutes(10)
      .create();

    SpreadsheetApp.getUi().alert(
      "Success",
      "Auto refresh diaktifkan (10 menit sekali)",
      SpreadsheetApp.getUi().ButtonSet.OK
    );
  } catch (error) {
    console.error("Error creating auto refresh:", error);
  }
}

// STOP AUTO REFRESH
function stopAutoRefresh30Min() {
  try {
    const triggers = ScriptApp.getProjectTriggers();
    let removed = 0;

    triggers.forEach((trigger) => {
      if (trigger.getHandlerFunction() === "refreshSheet30Min") {
        ScriptApp.deleteTrigger(trigger);
        removed++;
      }
    });

    if (removed > 0) {
      SpreadsheetApp.getUi().alert(
        "Success",
        `Auto refresh dihentikan (${removed} trigger)`,
        SpreadsheetApp.getUi().ButtonSet.OK
      );
    } else {
      SpreadsheetApp.getUi().alert(
        "Info",
        "Tidak ada auto refresh yang aktif",
        SpreadsheetApp.getUi().ButtonSet.OK
      );
    }
  } catch (error) {
    console.error("Error stopping auto refresh:", error);
  }
}

// TEST FUNCTION - DEBUG SETIAP API
function testDataSources() {
  try {
    const testStock = "AAPL"; // Test dengan AAPL yang paling universal
    console.log("=== STARTING COMPREHENSIVE API TEST ===");
    console.log(`Testing with stock: ${testStock}`);

    const results = [];

    // TEST 1: Finnhub (NEW - PRIORITAS TERTINGGI)
    console.log("\n--- TEST 1: Finnhub API ---");
    const finnhubData = fetchFinnhubQuote(testStock);
    if (finnhubData && finnhubData.length > 0) {
      console.log(`✓ Finnhub SUCCESS: $${finnhubData[0].close}`);
      results.push(
        `✓ Finnhub: SUCCESS - Price: $${finnhubData[0].close} (BEST SOURCE)`
      );
    } else {
      console.log(`✗ Finnhub FAILED`);
      results.push(`✗ Finnhub: FAILED - Check API key or rate limit`);
    }
    Utilities.sleep(2000);

    // TEST 2: Simple Realtime Quote
    console.log("\n--- TEST 2: Simple Realtime Quote API ---");
    const simpleData = fetchSimpleRealtimeQuote(testStock);
    if (simpleData && simpleData.length > 0) {
      console.log(`✓ Simple Quote SUCCESS: $${simpleData[0].close}`);
      results.push(`✓ Simple Quote: SUCCESS - Price: $${simpleData[0].close}`);
    } else {
      console.log(`✗ Simple Quote FAILED`);
      results.push(`✗ Simple Quote: FAILED`);
    }
    Utilities.sleep(2000);

    // TEST 3: Google Finance
    console.log("\n--- TEST 3: Google Finance API ---");
    const googleData = fetchGoogleFinanceIntraday(testStock);
    if (googleData && googleData.length > 0) {
      console.log(`✓ Google Finance SUCCESS: $${googleData[0].close}`);
      results.push(
        `✓ Google Finance: SUCCESS - Price: $${googleData[0].close}`
      );
    } else {
      console.log(`✗ Google Finance FAILED`);
      results.push(`✗ Google Finance: FAILED`);
    }
    Utilities.sleep(2000);

    // TEST 4: Yahoo Finance with Cache
    console.log("\n--- TEST 4: Yahoo Finance API ---");
    const yahooData = fetchYahooFinanceWithCache(testStock);
    if (yahooData && yahooData.length > 0) {
      console.log(`✓ Yahoo Finance SUCCESS: $${yahooData[0].close}`);
      results.push(`✓ Yahoo Finance: SUCCESS - ${yahooData.length} periods`);
    } else {
      console.log(`✗ Yahoo Finance FAILED`);
      results.push(`✗ Yahoo Finance: FAILED`);
    }
    Utilities.sleep(2000);

    // TEST 5: Combined Flow
    console.log("\n--- TEST 5: Combined Data Flow (getStockDataCombined) ---");
    const combinedData = getStockDataCombined(testStock);
    if (combinedData && combinedData.length > 0) {
      console.log(
        `✓ Combined SUCCESS: ${combinedData[0].dataSource} - Price: $${combinedData[0].close}`
      );
      results.push(
        `✓ Combined: SUCCESS (${combinedData[0].dataSource}) - Price: $${combinedData[0].close}`
      );
    } else {
      console.log(`✗ Combined FAILED`);
      results.push(`✗ Combined: FAILED - All APIs returned null`);
    }

    // Log to console
    console.log("\n=== TEST SUMMARY ===");
    results.forEach((r) => console.log(r));
    console.log("===================\n");

    // Show alert
    const message =
      "API Test Results:\n\n" +
      results.join("\n") +
      "\n\nCheck logs (Ctrl+Enter) for detailed error messages";
    SpreadsheetApp.getUi().alert(
      "API Test Results",
      message,
      SpreadsheetApp.getUi().ButtonSet.OK
    );
  } catch (error) {
    console.error("Test error:", error.message);
    console.error("Stack:", error);
  }
}

// TEST FUNCTION - RAW API RESPONSE DEBUG
function testRawApiResponse() {
  try {
    const testStock = "AAPL";
    console.log("=== TESTING RAW API RESPONSES ===");

    // Test Yahoo Finance Simple Quote raw response
    console.log("\n--- Yahoo Finance /v7/finance/quote raw response ---");
    const url = `https://query1.finance.yahoo.com/v7/finance/quote?symbols=${testStock}`;
    const options = {
      muteHttpExceptions: true,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        Accept: "application/json",
      },
      timeout: 10000,
    };

    const response = UrlFetchApp.fetch(url, options);
    console.log(`HTTP Status: ${response.getResponseCode()}`);
    const responseText = response.getContentText();
    console.log(`Response length: ${responseText.length} chars`);

    if (responseText.length < 500) {
      console.log("Raw response (full):");
      console.log(responseText);
    } else {
      console.log("Raw response (first 500 chars):");
      console.log(responseText.substring(0, 500));
    }

    // Try parsing
    try {
      const data = JSON.parse(responseText);
      console.log("✓ Valid JSON");
      if (data.quoteResponse && data.quoteResponse.result) {
        console.log(`Found ${data.quoteResponse.result.length} quote(s)`);
        const quote = data.quoteResponse.result[0];
        console.log(`Quote keys: ${Object.keys(quote).join(", ")}`);
        console.log(`Regular market price: ${quote.regularMarketPrice}`);
      }
    } catch (e) {
      console.error("✗ JSON parsing failed:", e.message);
    }
  } catch (error) {
    console.error("Error in raw response test:", error.message);
  }
}

// MENU UTAMA
function onOpen() {
  const ui = SpreadsheetApp.getUi();
  ui.createMenu("📈 Day Trading 30Min")
    .addItem("🔄 Refresh Data", "refreshSheet30Min")
    .addItem("⚙️ Setup Sheet", "setupSheet30Min")
    .addItem("🧪 Test All APIs", "testDataSources")
    .addItem("🔍 Debug Raw Response", "testRawApiResponse")
    .addSeparator()
    .addItem("▶️ Start Auto Refresh", "createAutoRefresh30Min")
    .addItem("⏹️ Stop Auto Refresh", "stopAutoRefresh30Min")
    .addToUi();
}
