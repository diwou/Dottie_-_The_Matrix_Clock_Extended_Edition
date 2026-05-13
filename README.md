# Dottie - The Matrix Clock 'Extended Version'

Eine Dot-Matrix-Uhr auf Basis einer **ESP32-S3-WROOM-1-N16R8-Platine** mit 16 MAX7219-LED-Matrix-Modulen, DS3231-RTC mit CR2032-Pufferung und VEML7700-Lichtsensor. Die Uhr zeigt Zeit und Datum mit Scroll-Animation, regelt die Helligkeit automatisch nach Umgebungslicht, synchronisiert die hochgenaue RTC per NTP und bietet eine Weboberfläche für Konfiguration, Diagnose, manuelle RTC-Einstellung und Firmware-Upload.

<img src="images/Dottie%20%E2%80%93%20The%20Matrix%20Clock.jpg" alt="Dottie - The Matrix Clock 'Extended Version'" width="100%">

<p>
  <a href="https://diwou.codeberg.page/Dottie_-_The_Matrix_Clock/">
    <img src="https://img.shields.io/badge/Browser%20Installer-Open%20In%20Browser-5aa9ff?style=for-the-badge" alt="Browser Installer im Browser oeffnen">
  </a>
</p>

- Aktuelle Version: `1.5.0` (Development-Builds zeigen zusätzlich den Codeberg-Kurz-Hash)
- Repository: `https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock`
- Browser-Installer: [https://diwou.codeberg.page/Dottie_-_The_Matrix_Clock/](https://diwou.codeberg.page/Dottie_-_The_Matrix_Clock/)
- Web-UI im Gerät: [http://dottie.local/](http://dottie.local/) im lokalen Netz
- Lizenz: `CC BY-NC-SA 4.0`

---

## Features

- **Zeitbasis**: DS3231-RTC als Laufzeit-Uhr, NTP zur regelmäßigen Korrektur und manuelle RTC-Einstellung im Webinterface
- **NTP-Synchronisation** mit 3 konfigurierbaren Fallback-Servern
- **Zeitzone & DST**: UTC-Offset, EU/US/Custom-Sommerzeit
- **Display**: 24h/12h-Modus, 5 Datumsformate, einstellbare Helligkeit, Scroll-Animation, Software-Dimmen und VEML7700-Autohelligkeit
- **Web-UI**: Responsive HTML5-Oberfläche mit Live-Display-Vorschau, RTC-Tab, NTP-Status, Diagnose, Firmware-Installer und umschaltbarem Light-/Dark-Mode
- **OTA-/Firmware-Updates**: Kabellose Updates über `dottie.local` und Web-Upload von `.bin`-Firmwaredateien
- **WiFi-Resilienz**: State-Machine mit exponentiellem Backoff, AP-Fallback (Captive Portal)
- **Factory-Reset**: konfigurierbarer GPIO 5 Sekunden halten

---

## Hardware

| Komponente | Details |
|---|---|
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Display | 16× MAX7219 LED-Matrix-Module (FC16_HW) |
| Display-Pins | DIN = GPIO11, CLK = GPIO12, CS/LOAD = GPIO10 |
| I2C | SDA = GPIO38, SCL = GPIO39 für VEML7700 und DS3231S |
| RTC | DS3231S als Laufzeit-Uhr, CR2032-batteriegepuffert, NTP schreibt Korrekturen zurück in die RTC |
| Anordnung | 2 Reihen: TOP (8 Module, 8×8 px) + BOTTOM (8 Module, 5×7 px) |
| Reset-Button | GPIO9 gegen GND, 5 Sekunden halten |

## KiCad

Der KiCad-Projektspiegel liegt unter [kicad/README.md](kicad/README.md).

| Pfad | Inhalt |
|---|---|
| `kicad/source/` | KiCad-Quellen des Boards |
| `kicad/fab/rev0.4/` | Fertigungsdaten für die aktuelle Hardwarefreigabe `0.4` |

Die vollständige Stückliste liegt in `kicad/fab/rev0.4/bom.csv`, die zugehörigen Bestückungsdaten in `positions.csv` und die Freigabe als Paket in `Dottie_-_The_Matrix_Clock_0.4.zip`.

### Verdrahtung

```
ESP32-S3 → MAX7219-Kette
GPIO11 → DIN
GPIO12 → CLK
GPIO10 → CS/LOAD
```

Die Pinwerte werden in `platformio.ini` über `DISPLAY_DATA_PIN`, `DISPLAY_CLK_PIN`, `DISPLAY_CS_PIN`, `FACTORY_RESET_PIN`, `I2C_SDA_PIN` und `I2C_SCL_PIN` gesetzt.

Die 16 Module bilden eine SPI-Kette: TOP (Module 0–7, rechts nach links) und BOTTOM (Module 8–15, rechts nach links). Das physikalische Mapping wird über `MAP_TOP[]` und `MAP_BOTTOM[]` in `display.h` konfiguriert.

---

## Software / Build

**Werkzeug:** PlatformIO (VSCode/VSCodium Extension oder CLI)

```bash
# Kompilieren
pio run -e dottie_esp32s3

# OTA-Upload (IP per CLI angeben)
pio run -t upload -e dottie_esp32s3_ota --upload-port dottie.local

# Wenn das Board nur im AP-Modus laeuft:
# pio run -t upload -e dottie_esp32s3_ota --upload-port 192.168.4.1

# VS Codium: Task "PIO OTA Upload (ESP32-S3)"
# (nutzt das OTA-Env direkt und umgeht den fehlerhaften Upload-Dialog)

# Native USB am ESP32-S3
pio run -t upload -e dottie_esp32s3 --upload-port /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_94-A9-90-2F-91-A0-if00

# Firmware-Build-Artefakte
# `Firmware/<version>/` und `Firmware/latest/` werden beim Build automatisch erzeugt

# Firmware-Installer im Webinterface
# Im Web-UI unter `System` -> `Firmware-Installer` die `.bin` aus `Firmware/latest/` hochladen.

# Serieller Monitor (USB-Fallback, 115200 Baud)
pio device monitor
```

### Abhängigkeiten

| Bibliothek | Version |
|---|---|
| `majicdesigns/MD_MAX72XX` | ^3.5.1 |
| `adafruit/Adafruit VEML7700 Library` | ^2.1.6 |
| ESP32 Arduino Core (PlatformIO `espressif32`) | – |

---

## Inbetriebnahme

1. Firmware per USB oder OTA flashen; bei OTA die Geräte-IP über `--upload-port` angeben
2. Firmware flashen: `pio run -t upload -e dottie_esp32s3`
3. Beim ersten Start ohne gespeicherte WLAN-Konfiguration öffnet Dottie einen **AP** mit SSID `DOTTIE-{ChipID}`
4. Mit dem AP verbinden (IP: `192.168.4.1`) und WLAN-Zugangsdaten eingeben
5. Nach dem Verbinden ist die Web-UI unter der zugewiesenen IP oder `http://dottie.local` erreichbar

---

## Web-Oberfläche

### Tabs

| Tab | Inhalt |
|---|---|
| **Status** | WiFi-Status, IP, NTP-Log (letzte 5 Syncs), manueller NTP-Test |
| **WiFi** | SSID, Passwort, Timeout, DHCP/Static-IP, DNS |
| **NTP** | 3 Server-URLs, Sync-Intervall, Test-Buttons |
| **RTC** | DS3231-Status, I2C-Diagnose, letzter NTP-Sync, manuelle Uhrzeit/Datum-Eingabe |
| **Zeitzone** | UTC-Offset, DST-Modus (Aus/EU/US/Custom) |
| **Display** | 24h/12h, Sekunden, Datumsformat, Helligkeit, Scroll-Effekt, VEML7700-Autohelligkeit |
| **System** | OTA, Firmware-Installer, Dark-Mode, Chip-Info, Neustart, Factory-Reset, Diagnose |

### REST-API

| Endpoint | Methode | Beschreibung |
|---|---|---|
| `/api/status` | GET | JSON: Zeit, Datum, Uptime, next_sync, Heap, RSSI |
| `/api/ntptest` | GET | JSON: running, host, result, detail |
| `/api/update` | POST | Firmware per Web-Upload einspielen |
| `/rtcset` | POST | RTC manuell auf Datum und Uhrzeit setzen |
| `/save` | POST | Konfiguration speichern |
| `/test?i=1..3` | POST | NTP-Server testen (asynchron) |
| `/reboot` | POST | Neustart |
| `/factory` | POST | Factory-Reset |
| `/diag` | GET | Diagnose-Seite (Debug-Info) |

---

## Konfiguration

Die Konfiguration wird als `Key=Value`-Datei auf LittleFS unter `/cfg.txt` gespeichert. Kommentare mit `#` sind erlaubt.

### Wichtige Parameter

| Schlüssel | Standardwert | Beschreibung |
|---|---|---|
| `ntp1..3` | pool.ntp.org, time.google.com, time.cloudflare.com | NTP-Server |
| `ntp_interval_min` | 15 | Sync-Intervall in Minuten (0 = 15) |
| `utc_offset_min` | 60 | UTC-Offset in Minuten (UTC+1 = 60) |
| `dst_mode` | 1 | 0=Aus, 1=EU, 2=US, 3=Custom |
| `date_fmt` | 0 | Datumsformat (0–4, s.u.) |
| `show_24h` | 1 | 24h-Modus (0 = 12h mit AM/PM) |
| `show_seconds` | 1 | Sekunden anzeigen (nur 24h) |
| `brightness` | 1 | Display-Helligkeit (0–15) |
| `auto_brightness` | 0 | Helligkeit automatisch per VEML7700 regeln |
| `scroll_enabled` | 1 | Scroll-Animation beim Ziffernwechsel |
| `pwm_duty` | 10 | Software-Dimmen (10 = aus, 1–5 = 10–50 % Abdunklung) |

### Datumsformate

| Wert | Format | Beispiel |
|---|---|---|
| 0 | `DD.MM.YYYY` | 07.03.2026 |
| 1 | `MM/DD/YYYY` | 03/07/2026 |
| 2 | `YYYY-MM-DD` | 2026-03-07 |
| 3 | `DD.MM.YY` | 07.03.26 |
| 4 | `MM/DD/YY` | 03/07/26 |

---

## Architektur

### Dateistruktur

| Datei | Rolle |
|---|---|
| `src/main.cpp` | Kernlogik: Boot, WiFi-State-Machine, NTP-Sync, OTA, Loop |
| `src/config.h` | Config-Struct, LittleFS-Load/Save (`/cfg.txt`), Makro-Parser, `APP_VERSION_BASE` |
| `src/display.h` | Bitmap-Glyphen (8×8 + 5×7), Scroll-Animation, Display-Mapping |
| `src/webserver.h` | HTML5-WebUI (Tabs), REST-APIs, JavaScript-Glyphen-Renderer |
| `platformio.ini` | Build-Konfiguration, Bibliotheksabhängigkeiten |

### WiFi-State-Machine

```
CONNECTED
  ↓ (Verbindungsverlust)
DISCONNECTED
  ↓ (nach Backoff: 30s → 60s → 120s → 300s max)
RECONNECTING
  ↓ (max. Fehlversuche erreicht)
AP_MODE (Captive Portal: SSID DOTTIE-{ChipID}, IP 192.168.4.1)
```

### NTP-Synchronisation

1. DNS-Auflösung (3 Versuche, lwIP-Direktfallback)
2. IP-Cache (3 Einträge) für schnelle Re-Syncs
3. UDP-Probe nach RFC 5905, Timeout 900 ms
4. 3 Server werden getestet, erster erfolgreicher wird verwendet
5. Log-Ringpuffer (5 Einträge) in der Web-UI sichtbar

### Display-Mapping

Physikalische Modul-IDs werden über `MAP_TOP[8]` und `MAP_BOTTOM[8]` von logischen Positionen entkoppelt. Flip/Mirror/Rotate für das Bottom-Display sind über Defines konfigurierbar (`BOTTOM_FLIP_VERTICAL_8`).

### OTA

- Hostname: `dottie.local` (mDNS)
- Progress-Anzeige auf dem Matrix-Display (Prozentbalken)
- OTA-Passwort optional konfigurierbar

---

## Factory-Reset

Den in `FACTORY_RESET_PIN` konfigurierten GPIO für **5 Sekunden** gegen GND halten:
- Countdown wird auf dem Display angezeigt
- `/cfg.txt` wird gelöscht
- Gerät startet neu und öffnet AP

---

## Lizenz

Dieses Projekt steht unter der Lizenz `Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)`.

Lizenztext: `LICENSE`
