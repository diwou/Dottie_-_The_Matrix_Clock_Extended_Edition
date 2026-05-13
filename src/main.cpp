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

// ============================================================
// NTP-Sync-Protokoll (Ringpuffer)
// ============================================================
static NtpLogEntry ntpLog[NTP_LOG_ENTRIES];
static uint8_t     ntpLogHead = 0;
static uint8_t     ntpLogCount = 0;

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

  float lux = veml.readLux();
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
  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)7, (uint8_t)true) != 7) return false;
  for (uint8_t i = 0; i < 7; i++) raw[i] = Wire.read();

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

  Wire.beginTransmission(0x68);
  for (uint8_t i = 0; i < sizeof(buf); i++) Wire.write(buf[i]);
  return Wire.endTransmission(true) == 0;
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
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  uint8_t probe = 0;
  Wire.beginTransmission(VEML7700_I2CADDR_DEFAULT);
  probe = Wire.endTransmission(true);

  uint8_t dsProbe = 0;
  Wire.beginTransmission(0x68);
  dsProbe = Wire.endTransmission(true);
  ds3231Present = (dsProbe == 0);

  uint8_t bme76Probe = 0;
  Wire.beginTransmission(0x76);
  bme76Probe = Wire.endTransmission(true);

  uint8_t bme77Probe = 0;
  Wire.beginTransmission(0x77);
  bme77Probe = Wire.endTransmission(true);

  i2cDiag = String(F("VEML=")) + probe + F(" @0x10, DS3231=") + dsProbe + F(" @0x68");
  i2cDiagEnv = String(F("BME680@0x76=")) + bme76Probe + F(", BME680@0x77=") + bme77Probe;

  bmeOk = false;
  bmeAddress = 0;
  if (bme76Probe == 0) {
    bmeOk = bme.begin(0x76, &Wire);
    if (bmeOk) bmeAddress = 0x76;
  } else if (bme77Probe == 0) {
    bmeOk = bme.begin(0x77, &Wire);
    if (bmeOk) bmeAddress = 0x77;
  }
  if (bmeOk) {
    bme.setTemperatureOversampling(BME680_OS_2X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bmeDiag = String(F("BME680 OK @0x")) + String(bmeAddress, HEX);
  } else {
    bmeDiag = i2cDiagEnv + F(" | BME680 nicht erkannt");
  }

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

static bool setupRtcTime() {
  if (!ds3231Present) {
    ds3231TimeValid = false;
    setTimeSource(F("keine"), F("DS3231 nicht gefunden"));
    return false;
  }

  time_t rtcEpoch = 0;
  if (!rtcEpochToTime(rtcEpoch)) {
    ds3231TimeValid = false;
    setTimeSource(F("keine"), F("DS3231 Zeit ungueltig"));
    return false;
  }

  ds3231TimeValid = true;
  lastRtcSyncMs = millis();
  setTimeSource(F("DS3231 aktiv"), F("RTC hat Zeit geliefert"));
  return true;
}

static void refreshSystemTimeFromRtc(bool force = false) {
  if (!ds3231Present || !ds3231TimeValid) return;

  unsigned long nowMs = millis();
  if (!force && nowMs - lastRtcSyncMs < 300000UL) return;

  time_t rtcEpoch = 0;
  if (!rtcEpochToTime(rtcEpoch)) {
    ds3231TimeValid = false;
    setTimeSource(F("keine"), F("DS3231 Zeit nicht lesbar"));
    return;
  }
  lastRtcSyncMs = nowMs;
  setTimeSource(F("DS3231 aktiv"), F("RTC ist Zeitquelle"));
}

static void retryLightSensorIfNeeded() {
  if (vemlOk) return;
  if (!vemlRetryAtMs) return;
  if ((long)(millis() - vemlRetryAtMs) < 0) return;
  if (vemlRetryCount > 10) {
    vemlRetryAtMs = 0;
    return;
  }
  setupLightSensor();
}

// ============================================================
// Software-PWM via SHUTDOWN-Register
// Laufzeitgesteuert im Hauptloop, damit kein asynchroner Callback
// mitten in Display-Zeichnungen auf den MAX7219 eingreift.
// ============================================================
static bool   g_pwmDark = false;
static uint32_t g_pwmNextMs = 0;

static inline void pwmApply(bool dark) {
  g_pwmDark = dark;
  mx.control(MD_MAX72XX::SHUTDOWN, dark ? MD_MAX72XX::ON : MD_MAX72XX::OFF);
}

static void pwmStop() {
  g_pwmNextMs = 0;
  pwmApply(false);  // Display an
}

static void pwmStart() {
  if (cfg.pwm_duty >= 10) {
    pwmStop();
    return;
  }
  pwmApply(false);  // mit Hellphase starten
  g_pwmNextMs = millis() + (20 - (uint32_t)cfg.pwm_duty * 2);
}

static void pwmService() {
  if (cfg.pwm_duty >= 10 || !g_pwmNextMs) return;

  uint32_t now = millis();
  while ((int32_t)(now - g_pwmNextMs) >= 0) {
    if (g_pwmDark) {
      pwmApply(false);
      g_pwmNextMs += 20 - (uint32_t)cfg.pwm_duty * 2;
    } else {
      pwmApply(true);
      g_pwmNextMs += (uint32_t)cfg.pwm_duty * 2;
    }
  }
}

static inline uint16_t effectiveNtpIntervalMin() {
  return (cfg.ntp_interval_min == 0) ? 15 : cfg.ntp_interval_min;
}

static uint32_t nextSyncInSec() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  uint32_t intervalMs = (uint32_t)effectiveNtpIntervalMin() * 60000UL;
  if (!lastNtpProbeMs) return intervalMs / 1000UL;
  uint32_t el = millis() - lastNtpProbeMs;
  return (el >= intervalMs) ? 0 : (intervalMs - el) / 1000UL;
}

static bool dnsReadyForNtp() {
  return WiFi.status() == WL_CONNECTED && WiFi.dnsIP(0) != (IPAddress)0U;
}

// ============================================================
// lwIP DNS-Auflösung (zuverlässig, für Diagnose)
// ============================================================
struct DnsWaitState {
  volatile bool done, ok;
  ip_addr_t addr;
};

static void dnsCb(const char*, const ip_addr_t* ipaddr, void* arg) {
  DnsWaitState* st = (DnsWaitState*)arg;
  if (!st) return;
  if (ipaddr) { st->addr = *ipaddr; st->ok = true; }
  else        { st->ok = false; }
  st->done = true;
}

static bool dnsResolveA_lwip(const String& host, IPAddress& outIp,
                              String& dbg, uint32_t timeoutMs)
{
  outIp = (IPAddress)0U;
  if (!host.length())                    { dbg = F("lwip:LEER");    return false; }
  if (WiFi.status() != WL_CONNECTED)    { dbg = F("lwip:KEIN_WLAN"); return false; }

  DnsWaitState st { false, false, {} };
  ip_addr_t addr;
  err_t e = dns_gethostbyname(host.c_str(), &addr, dnsCb, &st);

  if (e == ERR_OK) {
    st.addr = addr; st.ok = st.done = true;
  } else if (e == ERR_INPROGRESS) {
    uint32_t t0 = millis();
    while (!st.done && (millis() - t0 < timeoutMs)) { delay(1); yield(); }
    if (!st.done) { dbg = F("lwip:TIMEOUT"); return false; }
    if (!st.ok)   { dbg = F("lwip:FAIL");    return false; }
  } else {
    dbg = String(F("lwip:ERR=")) + (int)e;
    return false;
  }

  const ip4_addr_t* a4 = ip_2_ip4(&st.addr);
  outIp = IPAddress(ip4_addr1(a4), ip4_addr2(a4), ip4_addr3(a4), ip4_addr4(a4));
  dbg   = String(F("lwip:OK ip=")) + outIp.toString();
  return true;
}

// ============================================================
// IP-Cache (3 Einträge)
// ============================================================
struct HostIpCache { String host; IPAddress ip; uint32_t ms; bool valid; };
static HostIpCache g_ipcache[3];

static uint8_t cacheSlotFor(const String& host) {
  for (uint8_t i = 0; i < 3; i++)
    if (g_ipcache[i].valid && g_ipcache[i].host == host) return i;
  for (uint8_t i = 0; i < 3; i++)
    if (!g_ipcache[i].valid) return i;
  return 0;
}

static void cachePut(const String& host, const IPAddress& ip) {
  uint8_t idx = cacheSlotFor(host);
  g_ipcache[idx] = { host, ip, millis(), true };
}

static bool cacheGet(const String& host, IPAddress& ip, uint32_t& ageSec) {
  for (uint8_t i = 0; i < 3; i++) {
    if (g_ipcache[i].valid && g_ipcache[i].host == host) {
      ip = g_ipcache[i].ip;
      ageSec = (millis() - g_ipcache[i].ms) / 1000UL;
      return true;
    }
  }
  return false;
}

static void ntpLogAdd(uint32_t epoch, const String& host, bool ok, const String& detail) {
  ntpLog[ntpLogHead].timestampEpoch = epoch;
  strlcpy(ntpLog[ntpLogHead].host, host.c_str(), NTP_LOG_HOST_LEN);
  ntpLog[ntpLogHead].success = ok;
  strlcpy(ntpLog[ntpLogHead].detail, detail.c_str(), NTP_LOG_DETAIL_LEN);
  ntpLogHead  = (ntpLogHead + 1) % NTP_LOG_ENTRIES;
  if (ntpLogCount < NTP_LOG_ENTRIES) ntpLogCount++;
}

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

// ============================================================
// NTP Probe
// ============================================================
static bool ntpProbeOnce(const String& host, uint16_t timeoutMs, String* detail = nullptr, uint32_t* txSecOut = nullptr) {
  if (!host.length())                 { if(detail) *detail=F("EMPTY_HOST"); return false; }
  if (WiFi.status() != WL_CONNECTED) { if(detail) *detail=F("NO_WIFI");    return false; }

  IPAddress ip;
  bool resolved = false;
  for (uint8_t attempt = 0; attempt < 3 && !resolved; attempt++) {
    if (WiFi.hostByName(host.c_str(), ip)) resolved = true;
    else { delay(80); yield(); }
  }

  String how;
  if (resolved) {
    cachePut(host, ip);
    how = String(F("RESOLVED ip=")) + ip.toString();
  } else {
    IPAddress lastIp; uint32_t ageSec;
    if (cacheGet(host, lastIp, ageSec)) {
      ip  = lastIp;
      how = String(F("CACHE ip=")) + ip.toString() + F(" age=") + ageSec + "s";
    } else {
      if (detail) *detail = String(F("RESOLVE_FAIL dns0="))
                          + ipOrLabel(WiFi.dnsIP(0)) + F(" dns1=")
                          + ipOrLabel(WiFi.dnsIP(1));
      return false;
    }
  }

  // UDP-Probe -----------------------------------------------
  static uint16_t localPort = 0;
  if (!localPort) localPort = NTP_UDP_PORT_BASE + (clockChipId() & 0x0FFF);

  static WiFiUDP udp;
  udp.stop();
  // Bei begin()-Fehler: Port wechseln und nochmal versuchen
  if (!udp.begin(localPort)) {
    localPort = (localPort < 2500) ? localPort + 1 : NTP_UDP_PORT_BASE;
    if (!udp.begin(localPort)) {
      if (detail) *detail = how + F(" UDP_BEGIN_FAIL port=") + localPort;
      return false;
    }
  }

  // Rx-Buffer leeren
  while (udp.parsePacket() > 0) { uint8_t d[64]; udp.read(d, 64); }

  uint8_t pkt[48] = {};
  pkt[0] = 0x1B;

  uint32_t t1 = millis();  // Startzeit vor Senden
  if (!udp.beginPacket(ip, 123)) { udp.stop(); if(detail) *detail=how+F(" BEGINPKT_FAIL"); return false; }
  if (udp.write(pkt, 48) != 48) { udp.stop(); if(detail) *detail=how+F(" WRITE_FAIL");    return false; }
  if (!udp.endPacket())          { udp.stop(); if(detail) *detail=how+F(" ENDPKT_FAIL");   return false; }

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    int len = udp.parsePacket();
    if (len > 0) {
      uint32_t t4 = millis();  // Empfangszeitpunkt
      uint8_t buf[48]; int n = udp.read(buf, min(len, 48));
      udp.stop();
      if (n < 48)       { if(detail) *detail=how+F(" SHORT=") +n; return false; }
      if ((buf[0]&7)!=4){ if(detail) *detail=how+F(" BAD_MODE=")+(buf[0]&7); return false; }
      uint32_t txSec = ((uint32_t)buf[40]<<24)|((uint32_t)buf[41]<<16)|
                       ((uint32_t)buf[42]<<8 )|buf[43];
      uint32_t txFrac= ((uint32_t)buf[44]<<24)|((uint32_t)buf[45]<<16)|
                       ((uint32_t)buf[46]<<8 )|buf[47];
      if (!txSec)        { if(detail) *detail=how+F(" BAD_TS"); return false; }
      // Subsekunden-Anteil ms + halbe Round-Trip-Zeit → auf ganze Sekunden runden
      uint32_t extraMs = (uint32_t)(((uint64_t)txFrac * 1000ULL) >> 32) + (t4 - t1) / 2;
      txSec += (extraMs + 500) / 1000;
      if (txSecOut) *txSecOut = txSec;
      if (detail) *detail = how + F(" OK txSec=") + txSec + F(" extraMs=") + extraMs;
      return true;
    }
    delay(10); yield();
  }
  udp.stop();
  if (detail) *detail = how + F(" TIMEOUT");
  return false;
}

// NTP-Probe direkt per IP (kein DNS) — für Test & Fallback
static bool ntpProbeByIp(const IPAddress& ip, uint16_t timeoutMs, String* detail, uint32_t* txSecOut = nullptr) {
  if (ip == (IPAddress)0U) { if (detail) *detail = F("IP=0 ungueltig"); return false; }
  if (WiFi.status() != WL_CONNECTED) { if (detail) *detail = F("NO_WIFI"); return false; }

  static WiFiUDP udpDirect;
  udpDirect.stop();
  static uint16_t directPort = 2389;
  if (!udpDirect.begin(directPort)) {
    directPort = (directPort < 2450) ? directPort + 1 : 2389;
    if (!udpDirect.begin(directPort)) {
      if (detail) *detail = F("UDP_BEGIN_FAIL");
      return false;
    }
  }

  // Rx-Buffer leeren
  while (udpDirect.parsePacket() > 0) { uint8_t d[64]; udpDirect.read(d, 64); }

  uint8_t pkt[48] = {};
  pkt[0] = 0x1B;

  uint32_t t1 = millis();
  if (!udpDirect.beginPacket(ip, 123)) { udpDirect.stop(); if (detail) *detail=F("BEGINPKT_FAIL"); return false; }
  if (udpDirect.write(pkt, 48) != 48)  { udpDirect.stop(); if (detail) *detail=F("WRITE_FAIL");    return false; }
  if (!udpDirect.endPacket())           { udpDirect.stop(); if (detail) *detail=F("ENDPKT_FAIL");   return false; }

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    int len = udpDirect.parsePacket();
    if (len > 0) {
      uint32_t t4 = millis();
      uint8_t buf[48]; int n = udpDirect.read(buf, min(len, 48));
      udpDirect.stop();
      if (n < 48)        { if (detail) *detail=String(F("SHORT="))+n; return false; }
      if ((buf[0]&7)!=4) { if (detail) *detail=String(F("BAD_MODE="))+(buf[0]&7); return false; }
      uint32_t txSec = ((uint32_t)buf[40]<<24)|((uint32_t)buf[41]<<16)|
                       ((uint32_t)buf[42]<<8)|buf[43];
      uint32_t txFrac= ((uint32_t)buf[44]<<24)|((uint32_t)buf[45]<<16)|
                       ((uint32_t)buf[46]<<8)|buf[47];
      if (!txSec) { if (detail) *detail=F("BAD_TS=0"); return false; }
      uint32_t extraMs = (uint32_t)(((uint64_t)txFrac * 1000ULL) >> 32) + (t4 - t1) / 2;
      txSec += (extraMs + 500) / 1000;
      if (txSecOut) *txSecOut = txSec;
      if (detail) *detail = String(F("OK txSec=")) + txSec + F(" ip=") + ip.toString() + F(" extraMs=") + extraMs;
      return true;
    }
    delay(10); yield();
  }
  udpDirect.stop();
  if (detail) *detail = String(F("TIMEOUT ip=")) + ip.toString();
  return false;
}

static String chooseBestNtp() {
  if (ntpProbeOnce(cfg.ntp1, NTP_PROBE_TIMEOUT_MS)) return cfg.ntp1;
  if (ntpProbeOnce(cfg.ntp2, NTP_PROBE_TIMEOUT_MS)) return cfg.ntp2;
  if (ntpProbeOnce(cfg.ntp3, NTP_PROBE_TIMEOUT_MS)) return cfg.ntp3;
  return "";
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

static void applyTimeConfig(bool force) {
  if (WiFi.status() != WL_CONNECTED) return;

  String best = chooseBestNtp();
  lastNtpProbeMs = millis();

  String detail;
  uint32_t txSec = 0;
  ntpProbeOnce(best.length() ? best : cfg.ntp1, NTP_PROBE_TIMEOUT_MS, &detail, &txSec);
  if (txSec) {
    time_t rtcEpoch = (time_t)(txSec - 2208988800UL);
    lastSyncEpoch = rtcEpoch;
    if (ds3231Present) {
      applySystemTimeEpoch(lastSyncEpoch, true, F("DS3231 aktiv"), F("RTC von NTP korrigiert"));
    } else {
      setTimeSource(F("NTP aktiv"), best.length() ? best : cfg.ntp1);
    }
  }
  ntpLogAdd(lastSyncEpoch, best.length() ? best : cfg.ntp1, best.length() > 0, detail);

  String tz = posixFromConfig();
  setenv("TZ", tz.c_str(), 1);
  tzset();

  if (!force && best == activeNtp) return;
  activeNtp = best;
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

// ============================================================
// WiFi Connect (STA)
// ============================================================
static bool connectWiFiSTA(bool showUi = true) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  clockDisableWiFiSleep();
  applyIpAndDnsBeforeConnect();
  WiFi.begin(cfg.sta_ssid.c_str(), cfg.sta_pass.c_str());

  uint32_t startMs = millis(), lastBlink = 0;
  bool visible = true;

  while (WiFi.status() != WL_CONNECTED) {
    uint32_t elapsed = (millis() - startMs) / 1000;
    if (elapsed >= cfg.wifi_timeout_sec) break;
    uint16_t left = cfg.wifi_timeout_sec - (uint16_t)elapsed;
    if (showUi && millis() - lastBlink >= 500) {
      lastBlink = millis(); visible = !visible;
      showInitWifi(left, visible);
    }
    delay(25); yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Warten bis DNS verfügbar
    uint32_t tDns = millis();
    while (WiFi.dnsIP(0) == (IPAddress)0U && millis() - tDns < 2000) { delay(50); yield(); }
    mx.clear();
    return true;
  }

  if (showUi) {
    bool vis = true;
    for (uint8_t i = 0; i < 5; i++) {
      showWifiNotFound(vis); vis = !vis; delay(250); yield();
    }
  }
  mx.clear();
  return false;
}

// ============================================================
// Captive Portal (AP-Modus)
// ============================================================
ClockWebServer server(WEB_PORT);
DNSServer        dnsServer;
#if defined(ARDUINO_ARCH_ESP32)
static TaskHandle_t g_webTaskHandle = nullptr;
static void webServerTask(void* arg) {
  (void)arg;
  for (;;) {
    if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
static void startWebServerTask() {
  if (g_webTaskHandle) return;
  xTaskCreatePinnedToCore(webServerTask, "web", 8192, nullptr, 1, &g_webTaskHandle, 0);
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
    pwmStop();  // Dimmen anhalten, Display einschalten
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
static volatile bool g_spiPwmChanged = false;

void requestBrightnessFromLoop() {
  g_spiBrightnessForce = true;
}

void requestPwmFromLoop() {
  g_spiPwmChanged = true;
}

#include "webserver.h"

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
    "Donnerst.",
    "Freitag",
    "Samstag"
  };
  if (wday >= 7) return "--";
  return mixedCase ? DAYS_MC[wday] : DAYS_UC[wday];
}

static bool getLocalStrings(char timeStr[9], char dateStr[11], String& weekdayStr) {
  time_t now = 0;
  if (!rtcEpochToTime(now)) return false;
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  // Zeitformat
  if (cfg.show_24h) {
    if (cfg.show_seconds) {
      write2Digits(timeStr, (uint8_t)tmNow.tm_hour);
      timeStr[2] = ':';
      write2Digits(timeStr + 3, (uint8_t)tmNow.tm_min);
      timeStr[5] = ':';
      write2Digits(timeStr + 6, (uint8_t)tmNow.tm_sec);
      timeStr[8] = '\0';
    } else {
      timeStr[0] = ' ';
      write2Digits(timeStr + 1, (uint8_t)tmNow.tm_hour);
      timeStr[3] = ':';
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
    timeStr[2] = ':';
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
  if (WiFi.getMode() & WIFI_AP) return; // Im AP-Modus nicht reconnecten
  if (!cfg.sta_ssid.length()) return;

  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    if (wifiMgr.state != ClockWiFiState::CONNECTED) {
      wifiMgr.state = ClockWiFiState::CONNECTED;
      wifiMgr.reset();
      MDNS.begin("dottie");
      applyTimeConfig(true);
    }
    return;
  }

  // Getrennt — Reconnect planen
  if (wifiMgr.state == ClockWiFiState::CONNECTED) {
    wifiMgr.state = ClockWiFiState::DISCONNECTED;
    wifiMgr.lastCheckMs = millis();
    // Kurze Display-Anzeige
    drawTopCentered(F("WLAN?"));
    drawMiddleText5x7Centered(F("LOST"));
    lastDispTime[0] = '\0';  // Redraw erzwingen nach LOST-Anzeige
    lastDispDate[0] = '\0';
    delay(800);
  }

  uint32_t waitMs = (uint32_t)wifiMgr.reconnectIntervalS * 1000UL;
  if (millis() - wifiMgr.lastCheckMs < waitMs) return;

  wifiMgr.lastCheckMs = millis();
  wifiMgr.state = ClockWiFiState::RECONNECTING;

  bool ok = connectWiFiSTA(false); // ohne UI-Blink
  if (ok) {
    wifiMgr.state = ClockWiFiState::CONNECTED;
    wifiMgr.reset();
  } else {
    wifiMgr.state = ClockWiFiState::DISCONNECTED;
    wifiMgr.backoff();
    // Display: nächster Versuch in X Sekunden
    drawTopCentered(F("RETRY"));
    drawMiddleText5x7Centered(String(wifiMgr.reconnectIntervalS) + 'S');
    delay(600);
    mx.clear();
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  bootMs = millis();
  pinMode(FACTORY_RESET_PIN, INPUT);

  mx.begin();
  mx.control(MD_MAX72XX::TEST, MD_MAX72XX::OFF);
  mx.control(MD_MAX72XX::INTENSITY, 1);
  mx.clear();

  // Factory-Reset beim Boot (Taste gehalten)
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    showFactoryScreen(); delay(900); factoryReset();
  }

  bool configOk = loadConfig();
  if (!configOk) {
    drawTopCentered(F("CFG"));
    drawMiddleText5x7Centered(F("ERROR"));
    delay(1200);
    mx.clear();
  }
  setupLightSensor();
  setupRtcTime();
  {
    String tz = posixFromConfig();
    setenv("TZ", tz.c_str(), 1);
    tzset();
  }
  applyConfiguredBrightness();
  updateBrightnessFromLux(true);
  pwmStart();

  bool haveCreds = cfg.sta_ssid.length() > 0;

  if (haveCreds && connectWiFiSTA(true)) {
    wifiMgr.state = ClockWiFiState::CONNECTED;
    MDNS.begin("dottie");
    setupOTA();
    setupWeb();
#if defined(ARDUINO_ARCH_ESP32)
    startWebServerTask();
#endif
    applyTimeConfig(true);
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
    startWebServerTask();
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

  // ④ Asynchronen NTP-Test ausführen (muss in loop() sein, nicht im Web-Handler)
  runPendingNtpTest();

  // ⑤ WiFi-Reconnect State Machine
  handleWiFiReconnect();

  // ⑥ NTP-Intervall
  if (WiFi.status() == WL_CONNECTED) {
    uint32_t intervalMs = (uint32_t)effectiveNtpIntervalMin() * 60000UL;
    if (!lastNtpProbeMs || millis() - lastNtpProbeMs > intervalMs)
      applyTimeConfig(false);
  }

  // Solange die RTC noch keine gueltige Zeit liefert, deutlich schneller neu versuchen.
  if (!ds3231TimeValid && dnsReadyForNtp()) {
    static uint32_t lastInvalidNtpMs = 0;
    if (!lastInvalidNtpMs || millis() - lastInvalidNtpMs > 10000UL) {
      lastInvalidNtpMs = millis();
      applyTimeConfig(true);
    }
  }

  // RTC-Zeitbasis verifizieren
  refreshSystemTimeFromRtc();

  // ⑥b Umgebungslicht -> Displayhelligkeit
  retryLightSensorIfNeeded();
  if (g_spiBrightnessForce) {
    g_spiBrightnessForce = false;
    updateBrightnessFromLux(true);
  }
  if (g_spiPwmChanged) {
    g_spiPwmChanged = false;
    if (cfg.pwm_duty >= 10) pwmStop();
    else pwmStart();
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

  if (millis() - lastMs < 50) { pwmService(); delay(0); return; }
  lastMs = millis();

  char t[9], d[11];
  String wd;
  if (!getLocalStrings(t, d, wd)) {
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
  if (wd != String(lastWeekday)) {
    strncpy(lastWeekday, wd.c_str(), sizeof(lastDispWeekday) - 1);
    lastWeekday[sizeof(lastDispWeekday) - 1] = '\0';
    drawBottomTextU8g2Centered(wd);
  }
  pwmService();
}
