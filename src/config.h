#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "pins.h"

// ============================================================
// Konstanten
// ============================================================
#define APP_VERSION_BASE      "1.5.0"

#define FACTORY_RESET_HOLD_MS 5000UL  // ms gehalten → Factory Reset
#define RESET_DEBOUNCE_MS     80UL

#define DNS_PORT              53
#define WEB_PORT              80
#define NTP_UDP_PORT_BASE     2390
#define NTP_PROBE_TIMEOUT_MS  900

#define WIFI_RECONNECT_INTERVAL_S 30   // Erster Reconnect-Versuch nach
#define WIFI_RECONNECT_MAX_S      300  // Maximales Backoff-Intervall

#define NTP_LOG_ENTRIES       5        // Anzahl der gespeicherten Sync-Logs

static const char CFG_FILE[]  PROGMEM = "/cfg.txt";
static const char AP_PREFIX[] PROGMEM = "DOTTIE-";
static const char MDNS_NAME[] PROGMEM = "dottie";

// ============================================================
// Konfigurationsstruktur
// ============================================================
struct Config {
  // WiFi
  String sta_ssid        = "";
  String sta_pass        = "";

  // IP
  bool   use_dhcp        = true;
  String static_ip       = "192.168.1.50";
  String static_gw       = "192.168.1.1";
  String static_mask     = "255.255.255.0";
  String dns1            = "9.9.9.9";      // Quad9 Primary
  String dns2            = "149.112.112.112"; // Quad9 Secondary

  // NTP
  String ntp1            = "pool.ntp.org";
  String ntp2            = "time.google.com";
  String ntp3            = "time.cloudflare.com";
  uint16_t ntp_interval_min = 15;

  // Zeitzone
  int16_t  utc_offset_min = 60;
  uint8_t  dst_mode       = 1;   // 0=off,1=EU,2=US,3=custom
  int16_t  dst_add_min    = 60;

  // Custom-DST-Regeln: M.w.d/h  (w=1..4 nth, w=5=last, d=0=Sun)
  uint8_t dst_sm=3, dst_sw=5, dst_sd=0, dst_sh=2;
  uint8_t dst_em=10,dst_ew=5, dst_ed=0, dst_eh=3;

  // Anzeigeformat
  bool     show_24h         = true; // false = 12h mit AM/PM (keine Sekunden)
  bool     show_seconds     = true; // Sekunden anzeigen (nur 24h-Modus)
  uint8_t  date_fmt         = 0;   // 0=DD.MM.YYYY 1=MM/DD/YYYY 2=YYYY-MM-DD 3=DD.MM.YY 4=MM/DD/YY

  // Sonstiges
  uint16_t wifi_timeout_sec = 30;
  uint16_t ip_show_sec      = 3;
  uint8_t  brightness       = 1;  // 0..15
  uint8_t  pwm_duty         = 10; // 1..10 (10 = kein PWM, 1 = 10% Einschaltzeit)
  bool     auto_brightness  = false;
  bool     scroll_enabled   = true; // Scroll-Effekt auf Ziffern
  bool     dark_mode        = false;

  // OTA
  bool     ota_enabled   = true;
  String   ota_password  = "";    // leer = kein Passwort
};

// ============================================================
// WiFi-Reconnect State Machine
// ============================================================
enum class ClockWiFiState {
  CONNECTED,
  DISCONNECTED,
  RECONNECTING,
  AP_MODE
};

struct WiFiManager {
  ClockWiFiState state      = ClockWiFiState::DISCONNECTED;
  uint32_t  lastCheckMs     = 0;
  uint32_t  reconnectIntervalS = WIFI_RECONNECT_INTERVAL_S;
  uint8_t   failCount       = 0;

  void reset() {
    failCount = 0;
    reconnectIntervalS = WIFI_RECONNECT_INTERVAL_S;
  }

  // Exponentielles Backoff (max. WIFI_RECONNECT_MAX_S)
  void backoff() {
    failCount++;
    reconnectIntervalS = min((uint32_t)reconnectIntervalS * 2,
                             (uint32_t)WIFI_RECONNECT_MAX_S);
  }
};

// ============================================================
// NTP-Sync-Protokoll (Ringpuffer)
// ============================================================
#define NTP_LOG_HOST_LEN   64
#define NTP_LOG_DETAIL_LEN 256

struct NtpLogEntry {
  uint32_t timestampEpoch; // Unix-Zeit des Sync-Versuchs
  char     host[NTP_LOG_HOST_LEN];
  bool     success;
  char     detail[NTP_LOG_DETAIL_LEN];
};

extern Config     cfg;
extern WiFiManager wifiMgr;

// ============================================================
// Hilfsfunktionen (config-intern)
// ============================================================
static String trimStr(const String& s) { String r = s; r.trim(); return r; }
static bool   parseIP(const String& s, IPAddress& out) { return out.fromString(s); }

static bool mountLittleFS() {
#if defined(ARDUINO_ARCH_ESP32)
  return LittleFS.begin(true);
#else
  return LittleFS.begin();
#endif
}

static bool loadConfig() {
  if (!mountLittleFS()) return false;
  if (!LittleFS.exists(CFG_FILE)) return true;

  File f = LittleFS.open(CFG_FILE, "r");
  if (!f) return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);
    k.trim(); v.trim();

    // Übersichtliche Macro-Variante zum Parsen
    #define CFG_STR(KEY, FIELD)   if (k == F(KEY)) cfg.FIELD = v;
    #define CFG_INT(KEY, FIELD, T) if (k == F(KEY)) cfg.FIELD = (T)v.toInt();
    #define CFG_BOOL(KEY, FIELD)  if (k == F(KEY)) cfg.FIELD = (v == "1");

    CFG_STR("sta_ssid",  sta_ssid)
    CFG_STR("sta_pass",  sta_pass)
    CFG_BOOL("use_dhcp", use_dhcp)
    CFG_STR("static_ip",   static_ip)
    CFG_STR("static_gw",   static_gw)
    CFG_STR("static_mask", static_mask)
    CFG_STR("dns1", dns1)  CFG_STR("dns2", dns2)
    CFG_STR("ntp1", ntp1)  CFG_STR("ntp2", ntp2)  CFG_STR("ntp3", ntp3)
    CFG_INT("ntp_interval_min", ntp_interval_min, uint16_t)
    CFG_INT("utc_offset_min",   utc_offset_min,   int16_t)
    CFG_INT("dst_mode",    dst_mode,    uint8_t)
    CFG_INT("dst_add_min", dst_add_min, int16_t)
    CFG_INT("dst_sm", dst_sm, uint8_t)  CFG_INT("dst_sw", dst_sw, uint8_t)
    CFG_INT("dst_sd", dst_sd, uint8_t)  CFG_INT("dst_sh", dst_sh, uint8_t)
    CFG_INT("dst_em", dst_em, uint8_t)  CFG_INT("dst_ew", dst_ew, uint8_t)
    CFG_INT("dst_ed", dst_ed, uint8_t)  CFG_INT("dst_eh", dst_eh, uint8_t)
    CFG_BOOL("show_24h",     show_24h)
    CFG_BOOL("show_seconds", show_seconds)
    CFG_INT("date_fmt",      date_fmt,  uint8_t)
    CFG_INT("wifi_timeout_sec", wifi_timeout_sec, uint16_t)
    CFG_INT("ip_show_sec",      ip_show_sec,      uint16_t)
    CFG_INT("brightness",       brightness,       uint8_t)
    CFG_INT("pwm_duty",         pwm_duty,         uint8_t)
    CFG_BOOL("auto_brightness", auto_brightness)
    CFG_BOOL("scroll_enabled",  scroll_enabled)
    CFG_BOOL("dark_mode",       dark_mode)
    CFG_BOOL("ota_enabled",  ota_enabled)
    CFG_STR("ota_password",  ota_password)

    #undef CFG_STR
    #undef CFG_INT
    #undef CFG_BOOL
  }
  f.close();
  if (cfg.brightness > 15) cfg.brightness = 15;
  if (cfg.pwm_duty < 1 || cfg.pwm_duty > 10) cfg.pwm_duty = 10; // 10 = aus, 1-5 = aktive PWM
  return true;
}

static bool saveConfig() {
  if (!mountLittleFS()) return false;
  File f = LittleFS.open(CFG_FILE, "w");
  if (!f) return false;

  f.println(F("# Dottie Konfiguration"));
  #define W(K,V) f.println(String(F(K "=")) + String(V))
  W("sta_ssid",   cfg.sta_ssid);
  W("sta_pass",   cfg.sta_pass);
  W("use_dhcp",   cfg.use_dhcp ? "1" : "0");
  W("static_ip",   cfg.static_ip);
  W("static_gw",   cfg.static_gw);
  W("static_mask",  cfg.static_mask);
  W("dns1", cfg.dns1);   W("dns2", cfg.dns2);
  W("ntp1", cfg.ntp1);   W("ntp2", cfg.ntp2);   W("ntp3", cfg.ntp3);
  W("ntp_interval_min", cfg.ntp_interval_min);
  W("utc_offset_min",   cfg.utc_offset_min);
  W("dst_mode",    cfg.dst_mode);
  W("dst_add_min", cfg.dst_add_min);
  W("dst_sm", cfg.dst_sm); W("dst_sw", cfg.dst_sw);
  W("dst_sd", cfg.dst_sd); W("dst_sh", cfg.dst_sh);
  W("dst_em", cfg.dst_em); W("dst_ew", cfg.dst_ew);
  W("dst_ed", cfg.dst_ed); W("dst_eh", cfg.dst_eh);
  W("wifi_timeout_sec", cfg.wifi_timeout_sec);
  W("show_24h",         cfg.show_24h     ? "1" : "0");
  W("show_seconds",     cfg.show_seconds ? "1" : "0");
  W("date_fmt",         cfg.date_fmt);
  W("ip_show_sec",      cfg.ip_show_sec);
  W("brightness",       cfg.brightness);
  W("pwm_duty",         cfg.pwm_duty);
  W("auto_brightness",  cfg.auto_brightness ? "1" : "0");
  W("scroll_enabled",   cfg.scroll_enabled ? 1 : 0);
  W("dark_mode",        cfg.dark_mode ? "1" : "0");
  W("ota_enabled",  cfg.ota_enabled ? "1" : "0");
  W("ota_password", cfg.ota_password);
  #undef W

  f.close();
  return true;
}

static void factoryReset() {
  if (mountLittleFS() && LittleFS.exists(CFG_FILE))
    LittleFS.remove(CFG_FILE);
  delay(150);
  ESP.restart();
}
