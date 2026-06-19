/*
  Dottie - The Matrix Clock 'Extended Version'
  ESP32-S3/ESP8266 NTP Dot-Matrix Clock (MAX7219 / MD_MAX72xx)

  Versionshistorie: siehe CHANGELOG.md

  NOTE: Die Hardware-Pins werden zentral in pins.h bzw. per platformio.ini gesetzt.
*/

#include <Arduino.h>
#include "platform_compat.h"
#include "pins.h"
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_BME680.h>
#include <LittleFS.h>
#include "esp_sntp.h"

extern "C" {
  #include "lwip/dns.h"
  #include "lwip/err.h"
  #include "lwip/ip_addr.h"
  #include "lwip/ip4_addr.h"
}

// ============================================================
// Display-Konfiguration
// ============================================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   24
MD_MAX72XX mx(HARDWARE_TYPE, DISPLAY_DATA_PIN, DISPLAY_CLK_PIN, DISPLAY_CS_PIN, MAX_DEVICES);

#include "display.h"

static inline void write2Digits(char* out, uint8_t value) {
  value %= 100;
  out[0] = (char)('0' + value / 10);
  out[1] = (char)('0' + value % 10);
}

static inline void write4Digits(char* out, uint16_t value) {
  value %= 10000;
  out[0] = (char)('0' + (value / 1000) % 10);
  out[1] = (char)('0' + (value / 100) % 10);
  out[2] = (char)('0' + (value / 10) % 10);
  out[3] = (char)('0' + value % 10);
}

static inline void formatDateDmy(char* out, uint8_t day, uint8_t month, uint16_t year) {
  write2Digits(out, day);
  out[2] = '.';
  write2Digits(out + 3, month);
  out[5] = '.';
  write4Digits(out + 6, year);
  out[10] = '\0';
}

#include "config.h"
#include "version_generated.h"

// ============================================================
// Firmware-Version
// ============================================================
static const char FW_VERSION[] PROGMEM = APP_VERSION_LABEL;

Config     cfg;
WiFiManager wifiMgr;
Adafruit_VEML7700 veml;
Adafruit_BME680 bme;

static const char BME_HISTORY_FILE[] PROGMEM = "/bme_hist.bin";

// ============================================================
// Umweltdaten-Historie (24h)
// ============================================================
#define BME_HIST_POINTS 288  // 24h * 12 (alle 5 Min)
struct BmeHistEntry {
  float temp;
  float humi;
  float pres;
  float iaq;
};

static BmeHistEntry bmeHistory[BME_HIST_POINTS];
static uint16_t     bmeHistIdx = 0;
static uint16_t     bmeHistCount = 0;
static uint32_t     lastBmeHistMs = 0;
static uint32_t     lastBmeSaveMs = 0;
static String       lastBmeSaveTime = F("noch nie");

// ============================================================
// Umweltdaten-Historie (30 Tage / Tag-Nacht)
// ============================================================
struct BmeMonthEntry {
  float temp_day, temp_night;
  float humi_day, humi_night;
  float pres_day, pres_night;
  float iaq_day, iaq_night;
};
static BmeMonthEntry bmeMonthHistory[30];
static uint8_t       bmeMonthIdx = 0;
static uint8_t       bmeMonthCount = 0;
static float         bmeSumTemp = 0, bmeSumHumi = 0, bmeSumPres = 0, bmeSumIaq = 0;
static int           bmeSumCount = 0;
static int           bmeLastPhase = -1; // 0=Nacht(18-06), 1=Tag(06-18)

// I2C Mutex für Thread-Safety (ESP32 Multi-Core)
#if defined(ARDUINO_ARCH_ESP32)
static SemaphoreHandle_t i2cMutex = nullptr;
#define I2C_LOCK()   if(i2cMutex) xSemaphoreTakeRecursive(i2cMutex, portMAX_DELAY)
#define I2C_UNLOCK() if(i2cMutex) xSemaphoreGiveRecursive(i2cMutex)
#else
#define I2C_LOCK()
#define I2C_UNLOCK()
#endif

// ============================================================
// Status-Variablen
// ============================================================
static String   activeNtp       = "";
static uint32_t lastNtpProbeMs  = 0;
static time_t   lastSyncEpoch   = 0;
static char     lastDispTime[9]  = "";  // für Invalidierung nach mx.clear()
static char     lastDispDate[11] = "";
static char     lastDispWeekday[24] = "";
static uint32_t bootMs          = 0;
static bool     vemlOk          = false;
static String   vemlDiag        = F("nicht gefunden");
static String   i2cDiag         = F("nicht gefunden");
static String   i2cDiagEnv      = F("nicht gefunden");
static bool     bmeOk           = false;
static uint8_t  bmeAddress      = 0;
static String   bmeDiag         = F("nicht gefunden");
static float    bmeTemp         = NAN;
static float    bmeHumi         = NAN;
static float    bmePres         = NAN;
static float    bmeGas          = NAN;
static float    bmeIaqScore     = -1.0f;
static float    bmeGasBaseline  = 0.0f;
static uint32_t lastBmeMs       = 0;
static bool     ds3231Present   = false;
static bool     ds3231TimeValid = false;
static String   timeSource      = F("keine");
static String   timeSourceDetail = F("");
static unsigned long lastLuxMs  = 0;
static unsigned long luxWarmupUntilMs = 0;
static unsigned long lastRtcSyncMs = 0;
static unsigned long vemlRetryAtMs = 0;
static uint8_t      vemlRetryCount = 0;
static float    luxFilt         = NAN;
static float    luxHold         = NAN;
static float    luxCurrent      = NAN;
static uint8_t  displayIntensityApplied = 255;

// Letzter manueller Test (für Web-UI)
static String   lastTestHost;
static String   lastTestResult;
static String   lastTestDetail;
static uint32_t lastTestMs   = 0;

// Asynchroner Test: wird in loop() ausgeführt, nicht im Web-Handler
static int8_t   pendingTestIndex = -1;   // -1=kein Test, 1/2/3=NTP#x
static bool     testRunning      = false;

// Factory-Reset Tracking
static uint32_t resetPressStartMs = 0;

// ============================================================
// Hilfsfunktionen
// ============================================================
static String ipOrLabel(const IPAddress& ip, const char* label = "(nicht gesetzt)") {
  return (ip == (IPAddress)0U) ? String(label) : ip.toString();
}

static inline void applyBrightness(uint8_t intensity) {
  if (intensity > 15) intensity = 15;
  if (displayIntensityApplied == intensity) return;
  displayIntensityApplied = intensity;
  for (uint8_t dev = 0; dev < MAX_DEVICES; dev++) {
    mx.control((uint8_t)dev, MD_MAX72XX::INTENSITY, intensity);
  }
}

static inline void applyConfiguredBrightness() {
  applyBrightness(cfg.brightness);
}

static uint8_t luxToBrightness(float lux) {
  constexpr float LUX_MIN = 0.2f;
  constexpr float LUX_MAX = 235.0f;
  constexpr float GAMMA_C  = 0.65f;

  if (!isfinite(lux)) return cfg.brightness;
  if (lux < LUX_MIN) lux = LUX_MIN;
  if (lux > LUX_MAX) lux = LUX_MAX;

  float x = (lux - LUX_MIN) / (LUX_MAX - LUX_MIN);
  float y = powf(x, GAMMA_C);
  int intensity = (int)lroundf(y * 15.0f);
  if (intensity < 1) intensity = 1;
  if (intensity > 15) intensity = 15;
  return (uint8_t)intensity;
}

static void updateBrightnessFromLux(bool force = false) {
  if (!vemlOk || !cfg.auto_brightness) {
    applyConfiguredBrightness();
    return;
  }

  unsigned long now = millis();
  if (!force && (long)(now - luxWarmupUntilMs) < 0) {
    applyConfiguredBrightness();
    return;
  }

  if (!force && now - lastLuxMs < 250) return;
  lastLuxMs = now;

  I2C_LOCK();
  float lux = veml.readLux();
  I2C_UNLOCK();

  if (!isfinite(lux) || lux < 0.0f) {
    applyConfiguredBrightness();
    return;
  }
  luxCurrent = lux;

  if (!isfinite(luxFilt)) luxFilt = lux;
  else {
    float a = (lux > luxFilt) ? 0.55f : 0.12f;
    luxFilt = (1.0f - a) * luxFilt + a * lux;
  }

  const float FAST_DELTA = 8.0f;
  if (!isfinite(luxHold)) {
    luxHold = luxFilt;
  } else {
    float d = luxFilt - luxHold;
    if (fabsf(d) >= FAST_DELTA) {
      luxHold = luxFilt;
    } else {
      if (luxFilt > luxHold + 1.2f) luxHold = luxFilt;
      else if (luxFilt < luxHold - 1.2f) luxHold = luxFilt;
    }
  }

  int autoBase = (int)luxToBrightness(luxHold);
  int relativeOffset = (int)cfg.brightness - 8; // 8 = neutral
  int finalIntensity = constrain(autoBase + relativeOffset, 0, 15);
  applyBrightness((uint8_t)finalIntensity);
}

static inline uint8_t binToBcd(uint8_t v) {
  return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static inline uint8_t bcdToBin(uint8_t v) {
  return (uint8_t)(((v >> 4) * 10) + (v & 0x0F));
}

static bool isLeapYear(int year) {
  return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static int daysInMonth(int year, int month) {
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month < 1 || month > 12) return 0;
  if (month == 2) return days[1] + (isLeapYear(year) ? 1 : 0);
  return days[month - 1];
}

static int64_t daysFromCivilUTC(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static bool rtcEpochToTime(time_t& epochOut) {
  uint8_t raw[7];
  I2C_LOCK();
  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) { I2C_UNLOCK(); return false; }
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)7, (uint8_t)true) != 7) { I2C_UNLOCK(); return false; }
  for (uint8_t i = 0; i < 7; i++) raw[i] = Wire.read();
  I2C_UNLOCK();

  int sec = bcdToBin(raw[0] & 0x7F);
  int min = bcdToBin(raw[1] & 0x7F);
  int hour = 0;
  if (raw[2] & 0x40) {
    int hr12 = bcdToBin(raw[2] & 0x1F) % 12;
    hour = hr12 + ((raw[2] & 0x20) ? 12 : 0);
  } else {
    hour = bcdToBin(raw[2] & 0x3F);
  }
  int day = bcdToBin(raw[4] & 0x3F);
  int month = bcdToBin(raw[5] & 0x1F);
  int year = 2000 + bcdToBin(raw[6]);
  if (raw[5] & 0x80) year += 100;

  if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23)
    return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > daysInMonth(year, month)) return false;
  if (year < 2000 || year > 2199) return false;

  epochOut = (time_t)(daysFromCivilUTC(year, (unsigned)month, (unsigned)day) * 86400LL
                      + hour * 3600L + min * 60L + sec);
  return epochOut >= 1700000000UL;
}

static bool rtcSetTimeFromEpoch(time_t epoch) {
  struct tm tmUtc {};
  gmtime_r(&epoch, &tmUtc);

  uint8_t buf[8] = {
    0x00,
    binToBcd((uint8_t)tmUtc.tm_sec),
    binToBcd((uint8_t)tmUtc.tm_min),
    binToBcd((uint8_t)tmUtc.tm_hour),
    (uint8_t)(tmUtc.tm_wday == 0 ? 1 : tmUtc.tm_wday),
    binToBcd((uint8_t)tmUtc.tm_mday),
    (uint8_t)(binToBcd((uint8_t)(tmUtc.tm_mon + 1)) & 0x1F),
    binToBcd((uint8_t)((tmUtc.tm_year + 1900) % 100))
  };

  I2C_LOCK();
  Wire.beginTransmission(0x68);
  for (uint8_t i = 0; i < sizeof(buf); i++) Wire.write(buf[i]);
  bool ok = (Wire.endTransmission(true) == 0);
  I2C_UNLOCK();
  return ok;
}

static void setTimeSource(const String& label, const String& detail = "");
static bool applySystemTimeEpoch(time_t epoch, bool writeRtc, const String& label, const String& detail) {
  if (writeRtc && ds3231Present && rtcSetTimeFromEpoch(epoch)) {
    ds3231TimeValid = true;
    lastRtcSyncMs = millis();
  } else if (writeRtc && ds3231Present) {
    return false;
  }

  setTimeSource(label, detail);
  return true;
}

static void setTimeSource(const String& label, const String& detail) {
  timeSource = label;
  timeSourceDetail = detail;
}

static void setupLightSensor() {
  uint8_t probe = 0;
  Wire.beginTransmission(VEML7700_I2CADDR_DEFAULT);
  probe = Wire.endTransmission(true);

  uint8_t dsProbe = 0;
  Wire.beginTransmission(0x68);
  dsProbe = Wire.endTransmission(true);
  ds3231Present = (dsProbe == 0);

  i2cDiag = String(F("VEML=")) + probe + F(" @0x10, DS3231=") + dsProbe + F(" @0x68");

  vemlOk = (probe == 0) && veml.begin();
  if (vemlOk) {
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_50MS);
    veml.interruptEnable(false);
    luxWarmupUntilMs = millis() + 1500;
    vemlRetryAtMs = 0;
    vemlRetryCount = 0;
    vemlDiag = i2cDiag + F(" | VEML OK");
  } else {
    luxWarmupUntilMs = 0;
    vemlRetryAtMs = millis() + 3000;
    vemlRetryCount++;
    vemlDiag = i2cDiag + F(" | VEML NACK");
  }
}

static void setupBme680() {
  uint8_t bme76Probe = 0, bme77Probe = 0;
  Wire.beginTransmission(0x76); bme76Probe = Wire.endTransmission(true);
  Wire.beginTransmission(0x77); bme77Probe = Wire.endTransmission(true);

  i2cDiagEnv = String(F("BME680@0x76=")) + bme76Probe + F(", BME680@0x77=") + bme77Probe;

  bmeOk = false;
  bmeAddress = 0;
  if (bme76Probe == 0) {
    bmeOk = bme.begin(0x76, false);
    if (bmeOk) bmeAddress = 0x76;
  } else if (bme77Probe == 0) {
    bmeOk = bme.begin(0x77, false);
    if (bmeOk) bmeAddress = 0x77;
  }

  if (bmeOk) {
    bme.setTemperatureOversampling(BME680_OS_2X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    bmeDiag = String(F("BME680 OK @0x")) + String(bmeAddress, HEX);
  } else {
    bmeDiag = i2cDiagEnv + F(" | BME680 nicht erkannt");
  }
}

static void timeSyncCallback(struct timeval *tv) {
  time_t now = tv->tv_sec;
  if (now > 1700000000UL) {
    rtcSetTimeFromEpoch(now);
    lastSyncEpoch = now;
    lastRtcSyncMs = millis();

    const char* server = sntp_getservername(0);
    activeNtp = server ? String(server) : cfg.ntp1;

    setTimeSource(F("ESP32 (NTP)"), activeNtp);
    // Display-Cache invalidieren
    invalidateDisplayCache();
    lastDispTime[0] = '\0';
    lastDispDate[0] = '\0';
    lastDispWeekday[0] = '\0';
  }
}

static bool isTimeSynced() {
  if (lastSyncEpoch == 0) return false;
  // Wenn mehr als das Doppelte des Intervalls vergangen ist (oder mind. 1h)
  uint32_t threshold = (uint32_t)cfg.ntp_interval_min * 60 * 2;
  if (threshold < 3600) threshold = 3600; 
  return (time(nullptr) - lastSyncEpoch < threshold);
}

static bool setupRtcTime() {
  if (!ds3231Present) {
    ds3231TimeValid = false;
    setTimeSource(F("ESP32 intern"), F("Kein DS3231 gefunden"));
    return false;
  }

  time_t rtcEpoch = 0;
  if (!rtcEpochToTime(rtcEpoch)) {
    ds3231TimeValid = false;
    setTimeSource(F("ESP32 intern"), F("DS3231 Zeit ungültig"));
    return false;
  }

  ds3231TimeValid = true;
  lastRtcSyncMs = millis();
  setTimeSource(F("ESP32 (RTC)"), F("Initialisiert aus DS3231"));

  // ESP32 Systemzeit initialisieren
  struct timeval tv = { .tv_sec = rtcEpoch, .tv_usec = 0 };
  settimeofday(&tv, NULL);

  return true;
}

static void saveHistory() {
  File f = LittleFS.open(BME_HISTORY_FILE, "w");
  if (!f) return;
  f.write((uint8_t*)&bmeHistIdx, sizeof(bmeHistIdx));
  f.write((uint8_t*)&bmeHistCount, sizeof(bmeHistCount));
  f.write((uint8_t*)bmeHistory, sizeof(bmeHistory));
  f.close();

  File f2 = LittleFS.open("/bme_base.bin", "w");
  if (f2) {
    f2.write((uint8_t*)&bmeGasBaseline, sizeof(bmeGasBaseline));
    f2.close();
  }

  File fMonth = LittleFS.open("/bme_month.bin", "w");
  if (fMonth) {
    fMonth.write((uint8_t*)&bmeMonthIdx, sizeof(bmeMonthIdx));
    fMonth.write((uint8_t*)&bmeMonthCount, sizeof(bmeMonthCount));
    fMonth.write((uint8_t*)bmeMonthHistory, sizeof(bmeMonthHistory));
    fMonth.close();
  }

  time_t t_now = time(nullptr);
  if (t_now > 1700000000UL) {
    struct tm tm_l;
    localtime_r(&t_now, &tm_l);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm_l.tm_hour, tm_l.tm_min, tm_l.tm_sec);
    lastBmeSaveTime = String(buf);
  } else {
    lastBmeSaveTime = String(millis() / 1000) + F("s (Uptime)");
  }
}

static void loadHistory() {
  if (LittleFS.exists("/bme_base.bin")) {
    File f2 = LittleFS.open("/bme_base.bin", "r");
    if (f2 && f2.size() == sizeof(bmeGasBaseline)) {
      f2.read((uint8_t*)&bmeGasBaseline, sizeof(bmeGasBaseline));
    }
    if (f2) f2.close();
  }

  if (LittleFS.exists("/bme_month.bin")) {
    File fMonth = LittleFS.open("/bme_month.bin", "r");
    if (fMonth) {
      if (fMonth.size() == sizeof(bmeMonthIdx) + sizeof(bmeMonthCount) + sizeof(bmeMonthHistory)) {
        fMonth.read((uint8_t*)&bmeMonthIdx, sizeof(bmeMonthIdx));
        fMonth.read((uint8_t*)&bmeMonthCount, sizeof(bmeMonthCount));
        fMonth.read((uint8_t*)bmeMonthHistory, sizeof(bmeMonthHistory));
      }
      fMonth.close();
    }
  }

  if (!LittleFS.exists(BME_HISTORY_FILE)) return;
  File f = LittleFS.open(BME_HISTORY_FILE, "r");
  if (!f) return;
  if (f.size() == (sizeof(bmeHistIdx) + sizeof(bmeHistCount) + sizeof(bmeHistory))) {
    f.read((uint8_t*)&bmeHistIdx, sizeof(bmeHistIdx));
    f.read((uint8_t*)&bmeHistCount, sizeof(bmeHistCount));
    f.read((uint8_t*)bmeHistory, sizeof(bmeHistory));
  }
  f.close();
}

static void updateHistory() {
  unsigned long now = millis();
  if (lastBmeHistMs != 0 && now - lastBmeHistMs < 300000) return; // Alle 5 Min
  lastBmeHistMs = now;

  if (isfinite(bmeTemp) && isfinite(bmeHumi) && isfinite(bmePres)) {
    bmeHistory[bmeHistIdx] = { bmeTemp, bmeHumi, bmePres, bmeIaqScore };
    bmeHistIdx = (bmeHistIdx + 1) % BME_HIST_POINTS;
    if (bmeHistCount < BME_HIST_POINTS) bmeHistCount++;

    time_t t_now = time(nullptr);
    if (t_now > 1700000000UL) {
      struct tm tm_l;
      localtime_r(&t_now, &tm_l);
      int currentPhase = (tm_l.tm_hour >= 6 && tm_l.tm_hour < 18) ? 1 : 0;
      
      if (bmeLastPhase == -1) bmeLastPhase = currentPhase;
      
      if (currentPhase != bmeLastPhase) {
        if (bmeSumCount > 0) {
          float avgTemp = bmeSumTemp / bmeSumCount;
          float avgHumi = bmeSumHumi / bmeSumCount;
          float avgPres = bmeSumPres / bmeSumCount;
          float avgIaq  = bmeSumIaq / bmeSumCount;
          
          if (bmeLastPhase == 1) { // War Tag
            bmeMonthHistory[bmeMonthIdx].temp_day = avgTemp;
            bmeMonthHistory[bmeMonthIdx].humi_day = avgHumi;
            bmeMonthHistory[bmeMonthIdx].pres_day = avgPres;
            bmeMonthHistory[bmeMonthIdx].iaq_day  = avgIaq;
          } else { // War Nacht
            bmeMonthHistory[bmeMonthIdx].temp_night = avgTemp;
            bmeMonthHistory[bmeMonthIdx].humi_night = avgHumi;
            bmeMonthHistory[bmeMonthIdx].pres_night = avgPres;
            bmeMonthHistory[bmeMonthIdx].iaq_night  = avgIaq;
            
            bmeMonthIdx = (bmeMonthIdx + 1) % 30;
            if (bmeMonthCount < 30) bmeMonthCount++;
          }
        }
        bmeSumTemp = 0; bmeSumHumi = 0; bmeSumPres = 0; bmeSumIaq = 0;
        bmeSumCount = 0;
        bmeLastPhase = currentPhase;
      }
      
      bmeSumTemp += bmeTemp;
      bmeSumHumi += bmeHumi;
      bmeSumPres += bmePres;
      if (bmeIaqScore >= 0) bmeSumIaq += bmeIaqScore;
      bmeSumCount++;
    }

    // Alle 60 Min wegspeichern
    if (now - lastBmeSaveMs >= 3600000) {
      lastBmeSaveMs = now;
      saveHistory();
    }
  }
}

static void updateBme680() {
  if (!bmeOk) return;
  unsigned long now = millis();
  if (lastBmeMs != 0 && now - lastBmeMs < 30000) return;
  lastBmeMs = now;

  I2C_LOCK();
  bool ok = bme.performReading();
  I2C_UNLOCK();

  if (ok) {
    bmeTemp = bme.temperature;
    bmeHumi = bme.humidity;
    bmePres = bme.pressure / 100.0f;
    float raw_gas = bme.gas_resistance / 1000.0f;
    
    // 1. Absolute Feuchtigkeit (AH) in g/m³ berechnen
    float ah = (6.112f * exp((17.67f * bmeTemp)/(bmeTemp + 243.5f)) * bmeHumi * 2.1674f) / (273.15f + bmeTemp);
    
    // 2. Gaswiderstand auf Referenz-Klima (AH_ref = 10.0 g/m³) kompensieren (k=0.04)
    float comp_gas = raw_gas * exp(0.04f * (ah - 10.0f));
    bmeGas = comp_gas;

    // 3. IAQ-Berechnung (Trend-basiert)
    // Erst nach 3 Minuten (180s) Aufwärmzeit ist der Sensor stabil genug
    if (millis() > 180000) {
      if (comp_gas > 0.1f) {
        if (bmeGasBaseline < 0.1f) {
          bmeGasBaseline = comp_gas;
        } else {
          // Baseline merkt sich den "saubersten" (höchsten) Widerstand.
          if (comp_gas > bmeGasBaseline) {
            bmeGasBaseline = comp_gas; // Sofort neuer Rekord!
          } else {
            // Nur sehr langsamer Verfall nach unten (falls der Sensor altert)
            bmeGasBaseline = (bmeGasBaseline * 0.999f) + (comp_gas * 0.001f);
          }
        }
      }

      if (bmeGasBaseline > 0.1f && comp_gas > 0.1f) {
        // 25% Feuchtigkeit, 75% Gas-Widerstand
        float hum_ref = 40.0f; // Ideale Feuchtigkeit
        float hum_score = 0;
        if (bmeHumi >= hum_ref) hum_score = (100.0f - bmeHumi) / (100.0f - hum_ref);
        else hum_score = (bmeHumi / hum_ref);
        hum_score *= 100.0f;
        if (hum_score > 100.0f) hum_score = 100.0f;

        float gas_score = (comp_gas / bmeGasBaseline) * 100.0f;
        if (gas_score > 100.0f) gas_score = 100.0f;
        
        float air_quality_score = (hum_score * 0.25f) + (gas_score * 0.75f);
        if (air_quality_score > 100.0f) air_quality_score = 100.0f;
        
        // Umrechnung auf Bosch-Skala 0-500 (invertiert: 100% Qualität = 0 IAQ)
        float currentIaq = (100.0f - air_quality_score) * 5.0f;
        if (currentIaq < 0.0f) currentIaq = 0.0f;
        if (currentIaq > 500.0f) currentIaq = 500.0f;
        
        // Glättungs-Filter (EMA), um extrem wilde Zacken durch thermisches Rauschen abzufangen
        if (bmeIaqScore < 0.0f) {
          bmeIaqScore = currentIaq; // Initialer Startwert nach der Aufwärmphase
        } else {
          // Dämpfung: 98% des alten Werts werden beibehalten, 2% des neuen kommen hinzu (sehr träge, ignoriert kurzes Rauschen).
          bmeIaqScore = (bmeIaqScore * 0.98f) + (currentIaq * 0.02f);
        }
      } else {
        bmeIaqScore = -1.0f; // Signalisiert "Wird ermittelt / Stabilisierung"
      }
    } else {
      bmeIaqScore = -1.0f; // Erzwinge Stabilisierungs-Anzeige während der Aufwärmphase
    }
  }
}

static void retryLightSensorIfNeeded() {
  if (vemlOk) return;
  if (!vemlRetryAtMs) return;
  if ((long)(millis() - vemlRetryAtMs) < 0) return;
  if (vemlRetryCount > 10) {
    vemlRetryAtMs = 0;
    return;
  }
  I2C_LOCK();
  setupLightSensor();
  I2C_UNLOCK();
}

// ============================================================
// Zeitzone → POSIX-String
// ============================================================
static String posixFromConfig() {
  int posixH = -(cfg.utc_offset_min / 60);
  int posixM =  abs(cfg.utc_offset_min % 60);

  auto fmtOff = [](int h, int m) -> String {
    return String(h) + (m ? String(F(":")) + String(m < 10 ? "0" : "") + m : "");
  };

  if (cfg.dst_mode == 0) return String(F("STD")) + fmtOff(posixH, posixM);

  uint8_t sm,sw,sd,sh, em,ew,ed,eh;
  if      (cfg.dst_mode == 1) { sm=3;sw=5;sd=0;sh=2; em=10;ew=5;ed=0;eh=3; }
  else if (cfg.dst_mode == 2) { sm=3;sw=2;sd=0;sh=2; em=11;ew=1;ed=0;eh=2; }
  else { sm=cfg.dst_sm;sw=cfg.dst_sw;sd=cfg.dst_sd;sh=cfg.dst_sh;
         em=cfg.dst_em;ew=cfg.dst_ew;ed=cfg.dst_ed;eh=cfg.dst_eh; }

  int dstTotal = cfg.utc_offset_min + cfg.dst_add_min;
  int dstH = -(dstTotal / 60), dstM = abs(dstTotal % 60);

  auto rule = [](uint8_t m,uint8_t w,uint8_t d,uint8_t h) -> String {
    return String(F("M"))+m+'.'+w+'.'+d+'/'+h;
  };

  return String(F("STD")) + fmtOff(posixH,posixM)
       + String(F("DST")) + fmtOff(dstH,dstM)
       + ',' + rule(sm,sw,sd,sh) + ',' + rule(em,ew,ed,eh);
}

static void applyTimezone() {
  String tz = posixFromConfig();
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

// ============================================================
// IP / DNS Setup
// ============================================================
static void applyIpAndDnsBeforeConnect() {
  WiFi.persistent(false);
  if (cfg.use_dhcp) return;

  IPAddress ip, gw, mask, d1, d2;
  if (!parseIP(cfg.static_ip, ip) || !parseIP(cfg.static_gw, gw) || !parseIP(cfg.static_mask, mask))
    return;

  bool has1 = cfg.dns1.length() && parseIP(cfg.dns1, d1);
  bool has2 = cfg.dns2.length() && parseIP(cfg.dns2, d2);
  if (has1 && !has2) d2 = d1;
  if (!has1 && has2) d1 = d2;

  WiFi.config(ip, gw, mask,
              has1 ? d1 : (IPAddress)0U,
              has2 ? d2 : (IPAddress)0U);
}

static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (wifiMgr.state != ClockWiFiState::CONNECTED) {
        wifiMgr.state = ClockWiFiState::CONNECTED;
        wifiMgr.reset();
        MDNS.begin(cfg.hostname.c_str());
        sntp_set_time_sync_notification_cb(timeSyncCallback);
        configTzTime(posixFromConfig().c_str(), cfg.ntp1.c_str(), cfg.ntp2.c_str(), cfg.ntp3.c_str());
        // Force redraw
        lastDispTime[0] = '\0';
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (wifiMgr.state == ClockWiFiState::CONNECTED) {
        wifiMgr.state = ClockWiFiState::DISCONNECTED;
        wifiMgr.lastCheckMs = millis();
        // Keine blockierenden Delays hier in der ISR/Event-Task!
        // Wir setzen nur den State, der Loop reagiert dann.
      }
      break;
    default:
      break;
  }
}

// ============================================================
// WiFi Connect (STA)
// ============================================================
static void startWiFiSTA() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(10);
  WiFi.setHostname(cfg.hostname.c_str());
  clockDisableWiFiSleep();
  applyIpAndDnsBeforeConnect();
  WiFi.begin(cfg.sta_ssid.c_str(), cfg.sta_pass.c_str());
}

static bool connectWiFiSTA(bool showUi = true) {
  startWiFiSTA();

  if (!showUi) return true; // Asynchron im Hintergrund

  uint32_t startMs = millis(), lastBlink = 0;
  bool visible = true;

  while (WiFi.status() != WL_CONNECTED) {
    uint32_t elapsed = (millis() - startMs) / 1000;
    if (elapsed >= cfg.wifi_timeout_sec) break;
    uint16_t left = cfg.wifi_timeout_sec - (uint16_t)elapsed;
    if (millis() - lastBlink >= 500) {
      lastBlink = millis(); visible = !visible;
      showInitWifi(left, visible);
    }
    delay(25); yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    mx.clear();
    invalidateDisplayCache();
    return true;
  }

  bool vis = true;
  for (uint8_t i = 0; i < 5; i++) {
    showWifiNotFound(vis); vis = !vis; delay(250); yield();
  }
  mx.clear();
  invalidateDisplayCache();
  return false;
}

// ============================================================
// Captive Portal (AP-Modus)
// ============================================================
ClockWebServer server(WEB_PORT);
DNSServer        dnsServer;
#if defined(ARDUINO_ARCH_ESP32)
static TaskHandle_t g_webTaskHandle = nullptr;
static TaskHandle_t g_sensorTaskHandle = nullptr;

static void webServerTask(void* arg) {
  (void)arg;
  for (;;) {
    if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5)); // Etwas mehr Luft für WiFi
  }
}

static void sensorTask(void* arg) {
  (void)arg;
  for (;;) {
    updateBme680();
    updateHistory();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Sensorik hat Zeit
  }
}

static void startBackgroundTasks() {
  if (!g_webTaskHandle) {
    xTaskCreatePinnedToCore(webServerTask, "web", 8192, nullptr, 1, &g_webTaskHandle, 0);
  }
  if (!g_sensorTaskHandle) {
    xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, nullptr, 1, &g_sensorTaskHandle, 0);
  }
}
#endif

static void startCaptivePortal() {
  WiFi.mode(WIFI_AP);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(50);
  clockDisableWiFiSleep();
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  String apName = String(FPSTR(AP_PREFIX)) + clockChipIdHex();
  apName.toUpperCase();
  WiFi.softAP(apName.c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.onNotFound([](){
    server.sendHeader(F("Location"), String(F("http://")) + WiFi.softAPIP().toString() + '/', true);
    server.send(302, F("text/plain"), "");
  });
}

// ============================================================
// OTA Setup
// ============================================================
static void setupOTA() {
  if (!cfg.ota_enabled) return;
  static bool otaReady = false;
  if (otaReady) return;

  ArduinoOTA.setHostname("dottie");
  if (cfg.ota_password.length())
    ArduinoOTA.setPassword(cfg.ota_password.c_str());

  ArduinoOTA.onStart([]() {
    drawTopCentered(F("OTA"));
    drawMiddleText5x7Centered(F("UPDATE"));
    clearRow(MAP_BOTTOM);
  });
  ArduinoOTA.onProgress([](unsigned int prog, unsigned int total) {
    static uint8_t lastPct = 255;
    uint8_t pct = (uint8_t)(prog * 100 / total);
    if (pct != lastPct) {
      lastPct = pct;
      drawTopCentered(String(pct) + '%');
    }
  });
  ArduinoOTA.onEnd([]() {
    drawTopCentered(F("DONE"));
    drawMiddleText5x7Centered(F("REBOOT"));
    clearRow(MAP_BOTTOM);
    lastDispTime[0] = '\0';
    lastDispDate[0] = '\0';
    lastDispWeekday[0] = '\0';
  });
  ArduinoOTA.onError([](ota_error_t err) {
    drawTopCentered(F("OTA ERR"));
    drawMiddleText5x7Centered(String(err));
    clearRow(MAP_BOTTOM);
  });
  ArduinoOTA.begin();
  otaReady = true;
}

// ============================================================
// Deferred SPI-Aktionen: Web-Task (Core 0) darf mx.*() nicht
// direkt aufrufen, weil der Haupt-Loop (Core 1) gleichzeitig
// Display-Updates via SPI sendet. Stattdessen setzt der
// Web-Handler nur Flags, die der Haupt-Loop abarbeitet.
// ============================================================
static volatile bool g_spiBrightnessForce = false;

void requestBrightnessFromLoop() {
  g_spiBrightnessForce = true;
}

#include "ClockWebUi.h"

// ============================================================
// Zeit-Strings
// ============================================================
static const char* weekdayLabelDe(uint8_t wday, bool mixedCase) {
  static const char* DAYS_UC[7] = {
    "SONNTAG",
    "MONTAG",
    "DIENSTAG",
    "MITTWOCH",
    "DONNERST.",
    "FREITAG",
    "SAMSTAG"
  };
  static const char* DAYS_MC[7] = {
    "Sonntag",
    "Montag",
    "Dienstag",
    "Mittwoch",
    "Donnersta.",
    "Freitag",
    "Samstag"
  };
  if (wday >= 7) return "--";
  return mixedCase ? DAYS_MC[wday] : DAYS_UC[wday];
}

static bool getLocalStrings(char timeStr[9], char dateStr[11], String& weekdayStr, bool hideColon = false) {
  time_t now = time(nullptr);
  if (now < 1700000000UL) return false;
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  // Zeitformat
  if (cfg.show_24h) {
    if (cfg.show_seconds) {
      write2Digits(timeStr, (uint8_t)tmNow.tm_hour);
      timeStr[2] = hideColon ? ' ' : ':';
      write2Digits(timeStr + 3, (uint8_t)tmNow.tm_min);
      timeStr[5] = hideColon ? ' ' : ':';
      write2Digits(timeStr + 6, (uint8_t)tmNow.tm_sec);
      timeStr[8] = '\0';
    } else {
      timeStr[0] = ' ';
      write2Digits(timeStr + 1, (uint8_t)tmNow.tm_hour);
      timeStr[3] = hideColon ? ' ' : ':';
      write2Digits(timeStr + 4, (uint8_t)tmNow.tm_min);
      timeStr[6] = ' ';
      timeStr[7] = ' ';
      timeStr[8] = '\0';
    }
  } else {
    uint8_t h = (uint8_t)(tmNow.tm_hour % 12);
    if (h == 0) h = 12;
    const char* ap = (tmNow.tm_hour < 12) ? "AM" : "PM";
    timeStr[0] = (h >= 10) ? (char)('0' + h / 10) : ' ';
    timeStr[1] = (char)('0' + h % 10);
    timeStr[2] = hideColon ? ' ' : ':';
    write2Digits(timeStr + 3, (uint8_t)tmNow.tm_min);
    timeStr[5] = ' ';
    timeStr[6] = ap[0];
    timeStr[7] = ap[1];
    timeStr[8] = '\0';
  }

  // Datumsformat
  uint16_t y4 = (uint16_t)(tmNow.tm_year + 1900);
  uint8_t y2 = (uint8_t)(y4 % 100);
  uint8_t mo = (uint8_t)(tmNow.tm_mon + 1);
  uint8_t dy = (uint8_t)tmNow.tm_mday;
  switch (cfg.date_fmt) {
    case 1:  // MM/DD/YYYY
      write2Digits(dateStr, mo); dateStr[2] = '/';
      write2Digits(dateStr + 3, dy); dateStr[5] = '/';
      write4Digits(dateStr + 6, y4); dateStr[10] = '\0';
      break;
    case 2:  // YYYY-MM-DD
      write4Digits(dateStr, y4); dateStr[4] = '-';
      write2Digits(dateStr + 5, mo); dateStr[7] = '-';
      write2Digits(dateStr + 8, dy); dateStr[10] = '\0';
      break;
    case 3:  // DD.MM.YY
      write2Digits(dateStr, dy); dateStr[2] = '.';
      write2Digits(dateStr + 3, mo); dateStr[5] = '.';
      write2Digits(dateStr + 6, y2); dateStr[8] = '\0';
      break;
    case 4:  // MM/DD/YY
      write2Digits(dateStr, mo); dateStr[2] = '/';
      write2Digits(dateStr + 3, dy); dateStr[5] = '/';
      write2Digits(dateStr + 6, y2); dateStr[8] = '\0';
      break;
    default: // DD.MM.YYYY
      formatDateDmy(dateStr, dy, mo, y4);
      break;
  }
  weekdayStr = weekdayLabelDe((uint8_t)tmNow.tm_wday, cfg.weekday_mixed_case);
  return true;
}

// ============================================================
// Factory-Reset-Button (Runtime, Release-Safe)
// ============================================================
static bool handleRuntimeResetHold() {
  static uint32_t releaseStartMs = 0;
  bool pressed = (digitalRead(FACTORY_RESET_PIN) == LOW);

  if (pressed) {
    releaseStartMs = 0;
    if (!resetPressStartMs) resetPressStartMs = millis();
    uint32_t held    = millis() - resetPressStartMs;
    uint32_t leftMs  = (held >= FACTORY_RESET_HOLD_MS) ? 0 : FACTORY_RESET_HOLD_MS - held;
    showResetCountdown((uint8_t)((leftMs + 999) / 1000));
    if (held >= FACTORY_RESET_HOLD_MS) { showFactoryScreen(); delay(600); factoryReset(); }
#if !defined(ARDUINO_ARCH_ESP32)
    server.handleClient();
    if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
#endif
    return true;
  }

  if (resetPressStartMs) {
    if (!releaseStartMs) releaseStartMs = millis();
    if (millis() - releaseStartMs >= RESET_DEBOUNCE_MS) {
      resetPressStartMs = 0; releaseStartMs = 0; mx.clear();
      invalidateDisplayCache();
      // Display-Cache invalidieren damit Zeit+Datum neu gezeichnet werden
      lastDispTime[0] = '\0';
      lastDispDate[0] = '\0';
    }
#if !defined(ARDUINO_ARCH_ESP32)
    server.handleClient();
    if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
#endif
    return true;
  }
  return false;
}

// ============================================================
// WiFi-Reconnect (State Machine)
// ============================================================
static void handleWiFiReconnect() {
  if (WiFi.getMode() & WIFI_AP) return;
  if (!cfg.sta_ssid.length()) return;

  static ClockWiFiState lastKnownState = ClockWiFiState::CONNECTED;
  ClockWiFiState currentState = wifiMgr.state;

  if (currentState == lastKnownState) return;

  if (currentState == ClockWiFiState::DISCONNECTED) {
    drawTopCentered(F("WLAN?"));
    drawMiddleText5x7Centered(F("LOST"));
    invalidateDisplayCache();
    lastDispTime[0] = '\0';
    lastDispDate[0] = '\0';
  } else if (currentState == ClockWiFiState::CONNECTED) {
    mx.clear();
    invalidateDisplayCache();
    lastDispTime[0] = '\0';
  }

  lastKnownState = currentState;
}

// ============================================================
// Setup
// ============================================================
void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  i2cMutex = xSemaphoreCreateRecursiveMutex();
#endif
  bootMs = millis();
  pinMode(FACTORY_RESET_PIN, INPUT);
  WiFi.onEvent(onWiFiEvent);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  mx.begin();
  mx.control(MD_MAX72XX::TEST, MD_MAX72XX::OFF);
  mx.control(MD_MAX72XX::INTENSITY, 1);
  mx.clear();

  // Factory-Reset beim Boot (Taste gehalten)
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    showFactoryScreen(); delay(900); factoryReset();
  }

  bool configOk = loadConfig();
  loadHistory();
  if (!configOk) {
    drawTopCentered(F("CFG"));
    drawMiddleText5x7Centered(F("ERROR"));
    delay(1200);
    mx.clear();
    invalidateDisplayCache();
  }
  I2C_LOCK();
  setupLightSensor();
  setupBme680();
  setupRtcTime();
  I2C_UNLOCK();
  {
    String tz = posixFromConfig();
    setenv("TZ", tz.c_str(), 1);
    tzset();
  }
  applyConfiguredBrightness();
  updateBrightnessFromLux(true);

  bool haveCreds = cfg.sta_ssid.length() > 0;

  if (haveCreds && connectWiFiSTA(true)) {
    wifiMgr.state = ClockWiFiState::CONNECTED;
    MDNS.begin(cfg.hostname.c_str());
    setupOTA();
    setupWeb();
#if defined(ARDUINO_ARCH_ESP32)
    startBackgroundTasks();
    sntp_set_sync_interval((uint32_t)cfg.ntp_interval_min * 60 * 1000);
#endif
    sntp_set_time_sync_notification_cb(timeSyncCallback);
    configTzTime(posixFromConfig().c_str(), cfg.ntp1.c_str(), cfg.ntp2.c_str(), cfg.ntp3.c_str());
    showTopMarquee(String(F("IP:")) + WiFi.localIP().toString(), cfg.ip_show_sec);
  } else {
    if (haveCreds) {
      // WLAN-Verbindung fehlgeschlagen — kurz anzeigen
      drawTopCentered(F("ERR"));
      drawMiddleText5x7Centered(F("WIFI"));
      delay(1500);
    }
    startCaptivePortal();
    setupWeb();
    setupOTA();
#if defined(ARDUINO_ARCH_ESP32)
    startBackgroundTasks();
#endif
    wifiMgr.state = ClockWiFiState::AP_MODE;
    // AP-IP als Laufschrift damit man weiß wohin verbinden
    showTopMarquee(String(F("AP ")) + WiFi.softAPIP().toString(), cfg.ip_show_sec);
    drawTopCentered(F("AP"));
    drawMiddleText5x7Centered(WiFi.softAPIP().toString());
  }
}

// ============================================================
// Loop
// ============================================================
void loop() {
  // ① Factory-Reset-Button prüfen
  if (handleRuntimeResetHold()) {
    return;
  }

#if !defined(ARDUINO_ARCH_ESP32)
  if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
  server.handleClient();
#endif

  // ② OTA verarbeiten
  if (cfg.ota_enabled) ArduinoOTA.handle();

  // ③ mDNS verarbeiten
  clockMdnsUpdate();

  // ⑤ WiFi-Reconnect State Machine
  handleWiFiReconnect();

  // ⑥b Umgebungslicht -> Displayhelligkeit
  retryLightSensorIfNeeded();
  if (g_spiBrightnessForce) {
    g_spiBrightnessForce = false;
    updateBrightnessFromLux(true);
  }
  updateBrightnessFromLux();

  // ⑧a Scroll-Animation tick (~50 Hz während Transition)
  {
    static uint32_t lastScrollMs = 0;
    uint32_t now = millis();
    if (lastScrollMs == 0) {
      lastScrollMs = now;
    }
    if (now - lastScrollMs >= 20) {
      lastScrollMs += 20;   // kein Drift: exakter 20ms-Takt
      tickScrollTop();
    }
  }

  // ⑧c Display-Update (max. 5 Hz reicht)
  // lastDispTime / lastDispDate sind global (werden von handleRuntimeResetHold invalidiert)
  char* lastTime = lastDispTime;
  char* lastDate = lastDispDate;
  char* lastWeekday = lastDispWeekday;
  static uint32_t lastMs   = 0;

  if (millis() - lastMs < 50) return;
  lastMs = millis();

  char t[9], d[11];
  String wd;
  bool blink = !isTimeSynced() && (millis() % 1000 < 500);
  if (!getLocalStrings(t, d, wd, blink)) {
    if (WiFi.getMode() & WIFI_AP) {
      if (strcmp(lastTime, "AP      ") != 0) {
        strcpy(lastTime, "AP      ");
        drawTopCentered(F("AP"));
        strcpy(lastDate, "");
        lastWeekday[0] = '\0';
        drawMiddleText5x7Centered(WiFi.softAPIP().toString());
        drawBottomTextU8g2Centered(F(""));
      }
    } else {
      const char* noTimeStr = cfg.show_24h
                              ? (cfg.show_seconds ? "--:--:--" : "  --:--  ")
                              : "--:-- --";
      if (strcmp(lastTime, noTimeStr) != 0) {
        strcpy(lastTime, noTimeStr);
        drawTimeHHMMSS(noTimeStr);
        strcpy(lastDate, "NO NTP");
        strcpy(lastWeekday, "NO NTP");
        drawMiddleText5x7Centered(F("NO NTP"));
        drawBottomTextU8g2Centered(F(""));
      }
    }
    return;
  }

  if (strcmp(t, lastTime) != 0) { strcpy(lastTime, t); drawTimeHHMMSS(t); }
  if (strcmp(d, lastDate) != 0) { strcpy(lastDate, d); drawMiddleText5x7Centered(String(d)); }

  // 3. Zeile (Wochentag / Umweltdaten)
  static uint32_t lastBottomSwitchMs = 0;
  static bool showEnv = false;
  static int16_t scrollPosX = 64;
  static uint32_t lastScrollBottomMs = 0;
  static String lastEnvStr = "";

  uint32_t now = millis();

  // Modus-Logik
  if (cfg.bottom_mode == 0) showEnv = false;
  else if (cfg.bottom_mode == 1) showEnv = true;
  else if (cfg.bottom_mode == 2) {
    if (!showEnv) {
      if (now - lastBottomSwitchMs >= (uint32_t)cfg.bottom_switch_sec * 1000) {
        showEnv = true;
        scrollPosX = 64; // Scroll am rechten Rand starten
      }
    }
  }

  if (!showEnv) {
    // Wochentag anzeigen
    if (wd != String(lastWeekday)) {
      strncpy(lastWeekday, wd.c_str(), sizeof(lastDispWeekday) - 1);
      lastWeekday[sizeof(lastDispWeekday) - 1] = '\0';
      drawBottomTextU8g2Centered(wd);
    }
  } else {
    // Umweltdaten scrollen
    if (now - lastScrollBottomMs >= cfg.bottom_scroll_speed) {
      lastScrollBottomMs = now;

      // String nur generieren wenn nötig oder Messwerte sich ändern könnten
      static String envStr = "";
      static int16_t fullWidth = 0;
      static uint32_t lastUpdate = 0;

      if (envStr == "" || now - lastUpdate > 10000) {
        lastUpdate = now;
        char envBuf[128];
        if (isfinite(bmeTemp) && isfinite(bmeHumi) && isfinite(bmePres)) {
          // UTF-8 Grad-Symbol: \xC2\xB0
          snprintf(envBuf, sizeof(envBuf), "%.1f\xC2\xB0" "C - %.1f %% - %.1f hPa ", 
                   bmeTemp, bmeHumi, bmePres);
        } else {
          strcpy(envBuf, "Warte auf Sensordaten... ");
        }
        envStr = String(envBuf);
        initU8g2Bottom(cfg.bottom_env_font);
        fullWidth = u8g2_GetUTF8Width(&g_u8g2_bottom, envStr.c_str());
      }
      
      u8g2_ClearBuffer(&g_u8g2_bottom);
      u8g2_DrawUTF8(&g_u8g2_bottom, scrollPosX, 8, envStr.c_str());
      
      // Wenn das Ende erreicht ist, wieder von vorne (oder zurück zum Wochentag)
      if (scrollPosX < -fullWidth) {
        if (cfg.bottom_mode == 2) {
          showEnv = false;
          lastBottomSwitchMs = now; // Timer für Wochentag neu starten
        }
        scrollPosX = 64;
        envStr = ""; // String beim nächsten Mal neu berechnen
      } else {
        scrollPosX--;
      }

      // Buffered Update um Flackern zu vermeiden
      mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
      clearRow(MAP_BOTTOM);
      uint8_t *buf = u8g2_GetBufferPtr(&g_u8g2_bottom);
      for (uint8_t col = 0; col < 64; col++) {
        uint8_t tile = col / 8;
        for (uint8_t row = 0; row < 8; row++) {
          if (buf[row * 8 + tile] & (0x80 >> (col & 7))) {
            rowSet(MAP_BOTTOM, col, row, true);
          }
        }
      }
      mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      lastDispWeekday[0] = '\0';
    }
  }
}
