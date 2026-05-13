#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>

  using ClockWebServer = ESP8266WebServer;

  static inline uint32_t clockChipId() {
    return ESP.getChipId();
  }

  static inline String clockChipIdHex() {
    return String(clockChipId(), HEX);
  }

  static inline String clockChipLabel() {
    return F("ESP8266");
  }

  static inline void clockDisableWiFiSleep() {
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
  }

  static inline void clockMdnsUpdate() {
    MDNS.update();
  }
#elif defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>

  using ClockWebServer = WebServer;

  static inline uint32_t clockChipId() {
    return (uint32_t)(ESP.getEfuseMac() & 0x00FFFFFFUL);
  }

  static inline String clockChipIdHex() {
    char buf[13];
    snprintf(buf, sizeof(buf), "%04X%08X",
             (uint16_t)(ESP.getEfuseMac() >> 32),
             (uint32_t)ESP.getEfuseMac());
    return String(buf);
  }

  static inline String clockChipLabel() {
    return String(ESP.getChipModel()) + F(" rev ") + ESP.getChipRevision();
  }

  static inline void clockDisableWiFiSleep() {
    WiFi.setSleep(false);
  }

  static inline void clockMdnsUpdate() {
  }
#else
  #error "Nicht unterstuetzte Arduino-Architektur"
#endif
