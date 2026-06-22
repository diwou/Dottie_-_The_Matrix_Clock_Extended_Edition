<!-- markdownlint-disable MD022 MD024 MD032 -->

# Changelog – Dottie - The Matrix Clock 'Extended Version'

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/). 

---

## [Unreleased]

### Hinzugefügt
- **Wetter und Umweltdaten:** Berechnung von Taupunkt, Hitzeindex und der absoluten Luftfeuchtigkeit (in g/m³) aus den BME680 Messwerten hinzugefügt. Implementierung einer barometrischen Wettervorhersage (Zambretti-Ansatz). (Ref: fdf7d5d, d2ebceb)
- **WebUI Erweiterung:** Anzeige von absoluter Feuchtigkeit (inkl. qualitativem Indikator), Taupunkt, Gefühlter Temperatur (Hitzeindex) und einer Text-Wettervorhersage in den Umweltdaten. (Ref: ff421f8, d2ebceb)
- **Druckkorrektur & Schimmelwarnung:** Der Luftdruck wird nun anhand einer konfigurierbaren Höhe über NN (im UI unter System > Standort einstellbar) auf Meereshöhe umgerechnet. Bei einer relativen Luftfeuchtigkeit über 65% wird nun eine Schimmel-Warnung (Lüftungsempfehlung) angezeigt. (Ref: d2ebceb)
- **Matrix-Display Scrolltext:** Das Laufband auf der Matrix zeigt nun zusätzlich zur relativen Luftfeuchtigkeit den qualitativen Status der absoluten Feuchte (z.B. "OPTIMAL") sowie zum Luftdruck den Wettertrend (z.B. "REGNERISCH") in Großbuchstaben an. (Ref: 6a82ef4, 9063743, 7bd622d)
- **Erweiterte Historie:** Die 24h-Diagramme und 30-Tage-Trends zeigen nun auch Graphen für Taupunkt und Hitzeindex. (Achtung: Dies erforderte eine Änderung der Speicherstruktur, alte Historien-Daten wurden beim Boot zurückgesetzt). (Ref: fdf7d5d)
- **Umweltsensoren UI:** Die Web-Oberfläche zeigt nun zusätzlich zur Luftfeuchtigkeit in Prozent einen qualitativen Statustext mit Farbkodierung (z.B. "Optimal", "Zu trocken") sowie neben dem Luftdruck einen Trendpfeil (steigend, fallend, gleichbleibend) basierend auf dem Verlauf der letzten 3 Stunden an. (Ref: e0116df)
- **IAQ-Glättung (Filter):** Für das 24h-Diagramm wird nun ein komplett isolierter, extremer EMA-Filter (99% Trägheit) verwendet, um das Live-Rauschen vor der Speicherung perfekt weichzuzeichnen. Aufwärmwerte (-1.0) werden abgefangen, um die Y-Achse nicht zu ruinieren. Die Web-Anzeige reagiert weiterhin flott (80/20). (Ref: 54fb449)
- **Historie der Umweltdaten (30 Tage Trend):** Neue Diagramme unterhalb der 24h-Historie zeigen nun zusätzlich einen Trend über 30 Tage. Pro Diagramm werden eine Tages- und eine Nacht-Kurve (gemittelt) eingezeichnet, um langfristige Tendenzen (z.B. Heizverhalten oder Lüftungstrends über Wochen) optimal sichtbar zu machen. Die Datensicherung erfolgt 1x stündlich ausfallsicher ins LittleFS (`/bme_month.bin`). (Ref: a91db9c)

### Behoben
- **Verspätete NTP-Fehleranzeige:** Die Doppelpunkte (Zifferntrenner) begannen bei einem Netzwerkausfall fälschlicherweise erst nach frühestens einer Stunde zu blinken, unabhängig vom eingestellten Sync-Intervall. Die Logik wurde angepasst: Das Blinken beginnt nun exakt 5 Minuten nach Ablauf des konfigurierten NTP-Intervalls (Karenzzeit für Router-Reboots etc.).
- **Netzwerk-Absturz (LwIP Heap Fragmentation):** Die ständige dynamische Speicher-Neuzuweisung (`String`) bei der Berechnung des langen Matrix-Scrolltextes (Wetter und Umwelt) führte zu Heap-Fragmentierung, wodurch der LwIP-Stack abstürzte (Uhr offline, Webserver tot, Ping erfolglos). Der Matrix-Puffer wurde auf statische Speicherblöcke (`char array`) umgerüstet, was den Fehler dauerhaft eliminiert.
- **I2C Diagnose:** Die I2C Diagnose-Anzeige für RTC und Lichtsensor im WebUI zeigt nun ordnungsgemäß die gefundenen Adressen statt eines Platzhalters an. (Ref: d2ebceb, 4fbf1b0)
- **Fehlende Höhenspeicherung:** Ein Fehler wurde behoben, durch den die konfigurierte Höhe über NN nach einem Neustart nicht wiederhergestellt wurde. (Ref: b75b509)
- **Leere 30-Tage Diagramme am ersten Tag (UI):** Das Web-Interface zeichnet nun kleine Punkte auf den Datenlinien der 30-Tage Historie. Dadurch ist ein einzelner gespeicherter Tag (der keine Verbindungslinie zu einem anderen Punkt aufbauen kann) nicht mehr unsichtbar.
- **Fehlerhafte IAQ-Stabilisierungs-Anzeige:** Bei extrem sauberer Luft (besser als die historische Baseline) konnte die IAQ-Formel rechnerisch in den negativen Bereich kippen. Das Webinterface interpretierte dies fälschlicherweise als anhaltende Aufwärmphase ("Sensoren stabilisieren..."). Der berechnete IAQ-Score wird nun konsequent auf 0-500 begrenzt. (Ref: bd8af96)
- **IAQ-Werte fallen fälschlicherweise auf 0 (Bugfix):** Die interne Referenz-Baseline für saubere Luft speichert nun wie vorgesehen immer den historisch besten (höchsten) Widerstandswert des BME680-Sensors und verfällt nur noch extrem langsam, anstatt lediglich den Durchschnitt zu bilden. Zudem wurden harte rechnerische Limits (Caps) bei exakt 100% eingeführt, um zu verhindern, dass exzellente Luftqualität rechnerisch zu einem negativen IAQ-Score führt (was im UI zu der falschen Dauer-Anzeige von `0` führte). (Ref: f88c025)
- **Leere 30-Tage Diagramme (UI):** Das Web-Interface zeichnet das Diagramm-Raster und die Achsenbeschriftung nun korrekt, selbst wenn am allerersten Tag nach dem Update noch gar keine Langzeit-Messwerte vorliegen. (Ref: 9cd1d99)

### Geändert
- **IAQ Algorithmus robuster im Dauerbetrieb:** Der BME680-Algorithmus wurde auf Industrie-Standards umgestellt. Der Gaswiderstand wird nun anhand der *absoluten Luftfeuchtigkeit* physikalisch kompensiert, wodurch abendliche Feuchtigkeitsschwankungen (z.B. am offenen Fenster) nicht mehr fälschlicherweise als Verschmutzung gewertet werden. Zudem wird anstelle eines starr gespeicherten Höchstwerts nun eine träge, exponentiell gleitende Baseline (EMA) berechnet. Das System kalibriert sich dadurch im 24/7-Dauerbetrieb fließend selbst und speichert den Stand im LittleFS. (Ref: 56f8fb6)

## [1.7.0] - 2026-06-09

### Behoben
- **I2C-Deadlock in setup():** Ein System-Freeze während des Boot-Vorgangs wurde durch die Umstellung auf einen rekursiven Mutex für den I2C-Bus behoben. Dies erlaubt verschachtelte Zugriffe auf den Bus während der Initialisierung. (Ref: ff7aa4e)
- **BME680 Gasmessung & IAQ-Stabilität:** Korrektur der Heizelement-Konfiguration und Einführung einer 5-minütigen Stabilisierungsphase. Die IAQ-Baseline (Referenz für saubere Luft) wird nun dauerhaft im LittleFS gespeichert und über Neustarts hinweg beibehalten. (Ref: c2f1d9b, 93547eb)
- **Netzwerk-Instabilität:** Das Problem mit dem Aufhängen des Netzwerkstacks wurde durch zwei Maßnahmen behoben: 1. Einführung eines I2C-Mutex für thread-sicheren Multi-Core-Zugriff auf die Sensoren. 2. Optimierung des Webserver-Tasks zur besseren Koexistenz mit dem WLAN-Stack. (Ref: f019a1a)
- **Verschwindende Zifferntrenner:** Die ":" Trenner auf dem Display blieben nach einem `mx.clear()` (z. B. bei WLAN-Reconnect oder kurzem Reset-Tastendruck) leer, da der Scroll-Cache sie fälschlicherweise als unverändert und damit bereits gezeichnet betrachtete. Eine neue Funktion `invalidateDisplayCache()` stellt nun sicher, dass nach jedem Leeren des Displays alle Zeichen inklusive der Trenner zwingend neu gezeichnet werden. (Ref: 8f8fe44)

### Neu
- **NTP-Synchronisations-Anzeige:** Der Sekundentrenner ":" blinkt nun dezent, falls die Uhr seit längerer Zeit keine NTP-Synchronisation mehr erfolgreich durchführen konnte. Im Web-UI wird zudem die verbleibende Zeit bis zum nächsten Sync-Versuch angezeigt. (Ref: f019a1a)
- **Konfigurierbarer Hostname:** Der Gerätename (mDNS & DHCP) kann nun im Web-UI unter "WLAN" angepasst werden (Standard: "dottie"). (Ref: c505ded)
- **SVG-Visualisierung:** Interaktive 24h-Kurvendiagramme im Web-UI (Reiter "Umweltdaten"). Die Grafiken werden rein clientseitig per SVG gerendert, ohne externe Bibliotheken oder Internetabhängigkeit. (Ref: 84aa3b9)
- **BME680 Umweltdaten:** Anzeige von Temperatur, Luftfeuchtigkeit, Luftdruck und Gaswiderstand im Web-UI-Reiter "Umweltdaten". Die Werte werden alle 30 Sekunden aktualisiert. (Ref: bfee853)
- **Umweltdaten-Anzeige auf Display:** Die 3. Modulreihe kann nun optional Umweltdaten (Temp, Feuchte, Luftdruck) als Scrolltext anzeigen. Konfigurierbar im Web-UI (nur Wochentag, nur Umweltdaten oder wechselnd). (Ref: 17808c5)
- **IAQ-Index (Luftqualität):** Implementierung eines vereinfachten IAQ-Index (0-500) basierend auf Gaswiderstand und Luftfeuchtigkeit, inklusive farbiger Statusanzeige im Web-UI. (Ref: 682a1d3)
- **Architektur-Refactor:** Umstellung auf asynchrones SNTP und event-basiertes WLAN für maximale Systemstabilität auf dem ESP32-S3. (Ref: 565d8c5)
- Native Unterstützung der ESP32-Systemzeit (`time.h`): Die interne Uhr wird beim Booten aus dem DS3231 initialisiert und danach autonom per SNTP synchronisiert. (Ref: 565d8c5)
- SNTP-Synchronisations-Callback: Der DS3231 RTC-Baustein wird nun automatisch im Hintergrund aktualisiert, sobald eine NTP-Synchronisation erfolgt. (Ref: 565d8c5)
- **BME680 Baseline-Anzeige:** Im Webinterface (Reiter "Umweltdaten") wird nun zusätzlich die berechnete Referenz für "saubere Luft" (Baseline) in kOhm angezeigt. (Ref: 30383a9)

### Geändert
- **Projektregeln:** `AGENTS.md` um die verbindliche Regel zur Ausgabe der Session-ID am Ende jeder Aufgabe erweitert. (Ref: 30383a9)
- **WLAN-Handling:** Die blockierenden Verbindungs-Loops wurden durch asynchrone WiFi-Events ersetzt. Das Display friert bei Verbindungsabbrüchen oder Reconnects nicht mehr ein. (Ref: 565d8c5)
- **Effizienz-Optimierung:** Die Abfrage der Uhrzeit erfolgt nun über die interne Systemzeit statt über sekündliche I2C-Abfragen des DS3231. Dies entlastet den I2C-Bus und reduziert Latenzen im Haupt-Loop. (Ref: 565d8c5)
- **Loop-Bereinigung:** Entfernung aller signifikanten `delay()`-Aufrufe und blockierenden Netzwerk-Routinen im Haupt-Loop zur Vermeidung von Watchdog-Triggern (WDT). (Ref: 565d8c5)

### Entfernt
- **Software-PWM:** Die jitter-anfällige Software-PWM-Lösung über das SHUTDOWN-Register wurde entfernt. Die Helligkeitssteuerung erfolgt nun ausschließlich über die native Hardware-Intensität des MAX7219 in Kombination mit dem Lichtsensor. (Ref: 565d8c5)
- Manuelle UDP-NTP-Implementierung und DNS-Resolver-Logik zugunsten der robusten ESP32-Framework-Funktionen. (Ref: 565d8c5)
- Veraltete Web-UI-Elemente für Software-PWM und manuellen NTP-Test. (Ref: 565d8c5)

### Behoben
- Flackern und Hänger des Displays während Netzwerkaktivitäten durch konsequente Trennung von Anzeige-Logik und Hintergrund-Aufgaben. (Ref: 565d8c5)
- Watchdog-Resets bei schlechter WLAN-Verbindung durch Eliminierung blockierender Reconnect-Logik. (Ref: 565d8c5)

### Neu (vorherige Änderungen)
- `.gitattributes` mit `* text=auto` angelegt, um plattformübergreifend konsistente Zeilenenden sicherzustellen. (Ref: 1cc6d42)
- Dritte Modulreihe (Wochentag) auf U8g2-Proportionalschrift umgestellt (ncenR08, helvR08, timR08, courR08, lubR08, helvB08 über Web-UI wählbar); Rendering erfolgt in einen Offscreen-Buffer, dann kopiert auf die MAX7219-Module via bestehendem `rowSet()`. (Ref: a5af151, ba83ac7)
- Wochentagsanzeige: Umschaltung Großbuchstaben / gemischte Schreibweise im Webinterface ergänzt. (Ref: ba83ac7)
- Projekt als eigenständige Extended-Edition vom Basisprojekt `Dottie_-_The_Matrix_Clock` abgeleitet und auf eigenes Repository umgestellt: https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition (Basis: https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock). (Ref: 930b941)
- Display-Layout auf 24 Dot-Matrix-Module erweitert (3 Reihen × 8 Module): obere Reihe Uhrzeit, mittlere Reihe Datum, untere Reihe Wochentag. (Ref: 930b941)
- BME680 am bestehenden I2C-Bus ergänzt (Adressprüfung 0x76/0x77, Initialisierung und Diagnose-Status). (Ref: 930b941)
- Neuer Web-UI-Reiter `Umweltdaten` mit BME680-Status und separater I2C-Diagnoseausgabe für Umweltsensoren. (Ref: 930b941)

### Geändert
- Sichtbare Projektbezeichnung in Firmware/Web-UI/Dokumentation auf `Dottie - The Matrix Clock 'Extended Version'` vereinheitlicht. (Ref: f496800)

### Behoben
- Eigene Datei `src/webserver.h` nach `src/ClockWebUi.h` umbenannt, um Namenskollision mit dem ESP32-Framework-Header `<WebServer.h>` unter Windows zu beheben (case-insensitive Dateisystem). (Ref: 8e687ff)
- NTP-Zeitsynchronisation korrigiert: Subsekunden-Anteil der NTP-Transmit-Timestamp und halbe Round-Trip-Zeit werden jetzt ausgewertet und auf die nächste ganze Sekunde gerundet, bevor die RTC gesetzt wird. Behebt den ~1-Sekunden-Nachlauf der Uhrzeit. (Ref: 6bbb247)
- Race-Condition auf dem SPI-Bus zwischen Web-Server-Task (Core 0) und Haupt-Loop (Core 1) behoben: manuelle Helligkeits- und PWM-Änderungen aus dem Webinterface werden nicht mehr direkt im Web-Task per `mx.control()` auf die MAX7219 geschrieben, sondern über Deferred-Flags sicher im Haupt-Loop verarbeitet. Behebt zufällige Pixelfehler und ungleichmäßige Helligkeit besonders bei Sprung von dunkel (0) auf hell (10–15). (Ref: 66e8020)
- NTP-Subsekunden-Genauigkeit verbessert: Vor dem RTC-Schreiben wird jetzt gezielt auf die nächste volle Sekundengrenze gewartet, damit die RTC exakt zur Sekundenmitte losläuft. Eliminiert den variablen Subsekunden-Versatz zwischen mehreren unabhängig synchronisierten Uhren. (1f5f27e)
- Dritte Modulreihe (Wochentag) wird beim OTA-Firmware-Update gelöscht, damit keine Restanzeige aus dem vorherigen Anzeigezyklus stehen bleibt. Zusätzlich Anzeige-Cache nach OTA-Ende invalidiert. (Ref: 674eaba)
- Status- und Boot-Texte wieder auf die 2. Zeile (mittlere Modulreihe) gelegt; die 3. Zeile ist für den Wochentag reserviert. (Ref: e8b8731)
- Wochentage in der 3. Zeile auf lesbare 5x7-Glyphen korrigiert (inklusive `M` und `G`) und Donnerstag als `DONNERST.` umgesetzt. (Ref: e8b8731)
- 5x7-Zeichensatz für die Wochentagsanzeige vervollständigt (u. a. fehlendes `C` für `MITTWOCH`) und auf fehlende Großbuchstaben geprüft. (Ref: 0f209dd)
- Zeile für den Wochentag auf echtes 6x7-Raster umgestellt und sofortige Anzeigeaktualisierung nach Web-Änderungen korrigiert, damit keine fehlerhaften Restpixel stehen bleiben. (Ref: c2241e0)
- Helligkeitsänderungen aus dem Webinterface greifen wieder sofort (auch über `Sofort anwenden`) und der Regler wirkt im Auto-Modus als relative Korrektur zur VEML7700-Regelung. (Ref: c2241e0)
- Darstellung des Wochentags auf 6x7 pro Modul korrigiert (ein Zeichen je 8x8-Modul, ohne Zwischenraum), inklusive sauberer 8-Zeichen-Abbildung und Donnerstag-Sonderfall. (Ref: 1f1db3e)
- Helligkeitsanwendung auf alle 24 Module vereinheitlicht, damit Web-Änderungen ohne Neustart sofort auf das gesamte Display wirken. (Ref: 1f1db3e)
- Wochentagsanzeige auf einen festen 8x8-Zeichensatz umgestellt und exakt 1:1 aus den hinterlegten Glyphen gerendert. (Ref: d487aed)
- Projektbild aus `images/Dottie – The Matrix Clock.jpg` in die README und die öffentliche Installer-Seite eingebunden. (Ref: board-2026-05-11-project-image)
- Öffentliche Codeberg-Pages-Seite für den Browser-Installer angelegt, analog zum HUB75-Projekt. (Ref: board-2026-05-11-pages-installer)
- Web-UI um einen umschaltbaren Light-/Dark-Mode ergänzt und als Konfigurationsoption gespeichert. (Ref: board-2026-05-11-dark-mode)
- Die Uhr oben rechts bleibt im Web-UI unabhängig vom Theme im bisherigen dunklen Stil. (Ref: board-2026-05-11-dark-mode-clock)

## [1.5.0] - 2026-05-11

### Neu
- Firmware und Build-Konfiguration auf die neue ESP32-S3-WROOM-1-N16R8-Platine vorbereitet; Display-, Reset- und I2C-Pins werden zentral per Build-Flags gesetzt. (Ref: board-2026-05-10-esp32s3)
- Factory-Reset und I2C auf die gemeldete Verdrahtung angepasst: Reset auf `IO9`, SDA auf `IO38`, SCL auf `IO39`. (Ref: board-2026-05-10-pinfix)
- S3-Upload auf native USB-CDC umgestellt, damit PlatformIO nicht mehr an RTS/DTR-Reset auf `ttyACM0` scheitert. (Ref: board-2026-05-10-usb-upload)
- Upload-Port auf den stabilen Espressif-by-id-Pfad umgestellt, damit die USB-Enumeration nicht mehr an wechselnden `ttyACM`-Namen hängt. (Ref: board-2026-05-10-usb-byid)
- VEML7700-Lichtsensor angebunden und als optionale automatische Helligkeitsregelung im Webinterface eingebaut. (Ref: board-2026-05-10-veml7700)
- I2C-Initialisierung des VEML7700 meldet jetzt eine konkrete Diagnose, wenn der Sensor nicht antwortet. (Ref: board-2026-05-10-veml7700-diag)
- Eigener `RTC`-Reiter im Webinterface ergänzt, mit Status, manueller Datum-/Zeiteingabe und I2C-Diagnose für den DS3231-Pfad. (Ref: board-2026-05-11-rtc-tab)
- Firmware-Installer im `System`-Tab ergänzt; `.bin`-Dateien können direkt per Webinterface eingespielt und danach neu gestartet werden. (Ref: board-2026-05-11-firmware-installer)

### Geändert
- KiCad-Projektspiegel mit `source/`, `fab/rev0.4/` und Stücklisten-/Fertigungsdaten in die Repository-Doku aufgenommen. (Ref: board-2026-05-11-kicad-mirror)
- README um die aktuelle Funktionsbeschreibung mit DS3231-RTC, CR2032-Pufferung, VEML7700-Lichtsensor, RTC-Tab und NTP-Korrektur erweitert. (Ref: board-2026-05-11-readme)
- README um den direkten Verweis auf den Firmware-Installer im `System`-Tab und die exportierten Firmware-Artefakte ergänzt. (Ref: board-2026-05-11-readme-installer)
- README um einen direkten Link auf die Web-UI mit Firmware-Installer unter `http://dottie.local/` im lokalen Netz erweitert. (Ref: board-2026-05-11-readme-web-updater)
- Build exportiert jetzt automatisch `Firmware/<version>/` und `Firmware/latest/` mit Manifest und Flash-Binaries für den Firmware-Installer. (Ref: board-2026-05-11-firmware-export)
- Projektregeln, Release-Dokumentation und `.gitignore` auf dieses Repository abgestimmt; `AGENTS.md` ist nicht mehr ignoriert und kann versioniert werden. (Ref: review-2026-04-28-regeln)
- Versionsquelle auf `APP_VERSION_BASE` in `src/config.h` umgestellt und Build-Erzeugung von `include/version_generated.h` mit Codeberg-Kurz-Hash ergänzt. (Ref: review-2026-04-28-version)
- Reproduzierbarkeit des Builds verbessert: PlatformIO-Plattform und `MD_MAX72XX` sind fest gepinnt, lokale OTA-IP aus der Projektkonfiguration entfernt. (Ref: review-2026-04-28-build)
- Projektspezifischen `npm run spell` mit `cspell`-Konfiguration ergänzt. (Ref: review-2026-04-28-spell)
- CSpell-Dictionary `@cspell/dict-de-de` lokal als Projektabhängigkeit erfasst und `node_modules/` ignoriert, damit die Konfiguration `cspell-ext.json` sauber auflösen kann. (Ref: fix-2026-04-28-cspell)
- PlatformIO-Default-Env auf `nodemcuv2_ota` festgelegt, Serial-Upload in `nodemcuv2` explizit gemacht und störende `#warning`-/Python-Warnungen aus dem Build unterdrückt. (Ref: fix-2026-04-28-platformio)
- `delay(0)` in `chunk()` durch `yield()` ersetzt – verhindert Watchdog-Timeout bei großen HTML-Seiten. (Ref: ea26b7a)
- `NtpLogEntry` von `String` auf `char[]`-Buffer umgestellt – reduziert Heap-Fragmentierung im NTP-Log-Ringpuffer. (Ref: ea26b7a)

### Behoben
- DS3231 ist jetzt die Laufzeit-Zeitquelle; NTP korrigiert nur noch die RTC und die ESP-Systemzeit wird nicht mehr als Zeitbasis verwendet. Der Status zeigt dazu die aktuelle Zeitquelle in der Web-UI und per API an. (Ref: board-2026-05-11-ds3231-time)
- Wenn beim Start noch keine gueltige Zeit vorliegt, versucht die Firmware NTP erneut, aber erst wenn DNS im WLAN bereit ist, damit kein fruehes "DNS fehlgeschlagen" entsteht. (Ref: board-2026-05-11-ntp-retry)
- Software-PWM wird jetzt in der Hauptschleife getaktet statt per asynchronem Ticker; das verhindert korrupt gezeichnete Ziffern und reduziert Flackern beim Sekundenwechsel. (Ref: board-2026-05-11-pwm-loop)
- I2C-Diagnose zeigt jetzt zusätzlich den DS3231-Pfad an, damit ein einzelnes VEML-NACK nicht den Eindruck erweckt, der ganze Bus wäre tot. (Ref: board-2026-05-10-i2c-scan)
- OTA startet jetzt auch im AP-Modus, und das OTA-Target zeigt per Default auf `dottie.local` statt auf eine harte LAN-IP. (Ref: board-2026-05-10-ota-target)
- VEML7700 wird bei einem anfänglichen I2C-NACK jetzt später erneut angefragt, damit die Helligkeitsregelung nicht an einem einzelnen Startfehler hängen bleibt. (Ref: board-2026-05-10-veml7700-retry)
- Webserver läuft auf dem ESP32-S3 jetzt in einem eigenen Task, damit das Laden der Web-UI den Display-Loop nicht mehr ausbremst. (Ref: board-2026-05-10-webtask)
- Für VS Codium gibt es jetzt eine direkte OTA-Task, die den PlatformIO-Upload ohne den fehleranfälligen Upload-Dialog ausführt. (Ref: board-2026-05-10-vscodium-ota)
- Die obere Zeile behält jetzt die Zeichenreihenfolge `HH:MM:SS`, während die Module weiterhin um 180° gedreht gerendert werden. (Ref: board-2026-05-10-top-order)
- Die oberen 8 Matrix-Module werden jetzt vor dem Zeichnen um 180° gedreht, damit die gemeldete Einbaulage zur Firmware passt. (Ref: board-2026-05-10-top-rotate180)
- Erfolgreiche NTP-Antworten setzen die Systemzeit jetzt sofort, statt nur den späteren SNTP-Sync abzuwarten. (Ref: board-2026-05-10-ntp-fast-sync)
- Matrix-Ansteuerung auf direkten GPIO-Modus umgestellt, damit `CLK`, `DIN` und `CS` ohne ESP32-S3-Hardware-SPI-Host direkt über die gemeldeten GPIOs laufen. (Ref: board-2026-05-10-direct-pin)
- Matrix-Ansteuerung auf den HSPI-Host des ESP32-S3 umgestellt, damit der externe Clock-Pin nicht mehr über den FSPI-Host läuft. (Ref: board-2026-05-10-hspi)
- Matrix-Ansteuerung auf einen eigenen ESP32-S3-SPI-Bus umgestellt, damit `CLK` sauber auf `GPIO12` statt auf den geteilten Default-Bus läuft. (Ref: board-2026-05-10-displayspi)
- LittleFS auf dem ESP32-S3 wird jetzt beim ersten Mount automatisch formatiert, damit neue Boards die WLAN-Konfiguration speichern können. (Ref: board-2026-05-10-littlefs)
- Web-UI zeigt den letzten NTP-Sync nicht mehr als laufend aktualisierte aktuelle Uhrzeit an. (Ref: review-2026-04-28-ntp-sync)
- Fehler beim Laden oder Speichern der LittleFS-Konfiguration werden nicht mehr stillschweigend als Erfolg behandelt. (Ref: review-2026-04-28-config-io)
- JSON-Ausgabe des manuellen NTP-Tests escaped Strings jetzt JSON-konform statt per HTML-Escape. (Ref: review-2026-04-28-json)

[Unreleased]: https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition/compare/v1.6.0...HEAD
[1.6.0]: https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition/compare/v1.5.0...v1.6.0
[1.5.0]: https://codeberg.org/diwou/Dottie_-_The_Matrix_Clock/compare/v1.4.7...v1.5.0

---

## [1.4.7] – 2026-04-09

### Geändert
- Projekt für die Veröffentlichung auf Codeberg vorbereitet: `README.md` auf Versionsstand, Repository-URL und Lizenzangaben aktualisiert. (Ref: 31b90bb)
- `.gitignore` für ein sauberes öffentliches Repository geschärft, damit lokale KI-/IDE-Dateien, Build-Artefakte und temporäre Dateien nicht mit ins Git gelangen. (Ref: 31b90bb)
- Firmware-Versionsstring auf `1.4.7` angehoben, damit Code, Dokumentation und Release-Stand konsistent sind. (Ref: 31b90bb)

---

## [1.4.6] – 2026-03-28

### Verbessert
- **Subsekunden-Kompensation für synchrone Scroll-Animationen:** Zwei Uhren am gleichen NTP-Server animieren den Sekundenwechsel jetzt auf ≤ 1 Frame (~20ms) genau synchron. (Ref: 31b90bb)
  - `drawTimeHHMMSS()`: beim Scroll-Start wird per `gettimeofday()` die aktuelle Position innerhalb der Sekunde ermittelt; bereits vergangene Animation-Steps werden übersprungen, sodass die Uhr sofort im richtigen Frame einsteigt.
  - Scroll-Tick-Timer: `lastScrollMs += 20` statt `= millis()` verhindert akkumulierten Drift. Beim ersten Tick wird der Timer auf das Sub-Sekunden-20ms-Raster ausgerichtet.
  - Display-Update-Intervall von 200 ms auf 50 ms reduziert: Sekundenwechsel werden bis zu 4× früher erkannt (Erkennungsjitter ±50 ms statt ±200 ms).

---

## [1.4.5] – 2026-03-19

### Behoben
- `LOST`-Anzeige blieb dauerhaft im unteren Display stehen, wenn die WLAN-Verbindung kurz unterbrochen und sofort wiederhergestellt wurde. Ursache: `lastDispDate` wurde nach dem `drawBottomText5x7Centered("LOST")`-Aufruf nicht invalidiert, sodass der Loop-Vergleich keinen Unterschied erkannte und das Display nicht neu zeichnete. Fix: `lastDispTime` und `lastDispDate` werden nach der LOST-Anzeige geleert, um einen erzwungenen Redraw auszulösen. (Ref: 31b90bb)

---

## [1.4.4]

### Geändert
- PWM auf `Ticker`-Bibliothek umgestellt: Die Phasenwechsel werden jetzt unabhängig vom Haupt-Loop über `yield()`-Callbacks getriggert. Der Ticker feuert auch dann, wenn der Loop durch `server.handleClient()`, `ArduinoOTA.handle()` oder `MDNS.update()` belegt ist – Loop-Jitter hat damit keinen Einfluss mehr auf die PWM-Zeiten. (Ref: 31b90bb)

### Behoben
- OTA: `pwmStop()` wird jetzt im `onStart`-Callback aufgerufen, stoppt den Ticker und schaltet das Display sicher ein. (Ref: 31b90bb)

---

## [1.4.3]

### Behoben
- OTA: Display war während des Uploads dunkel, weil `ArduinoOTA.handle()` den Loop blockiert und die PWM-Dunkelphase nicht beendet werden konnte. Im `onStart`-Callback wird SHUTDOWN nun explizit aufgehoben. (Ref: 31b90bb)
- PWM-Periode von 40 ms (25 Hz) auf 20 ms (50 Hz) korrigiert: 25 Hz lag im klar sichtbaren Flicker-Bereich und verursachte starkes Flimmern nach OTA-Neustart. (Ref: 31b90bb)
- PWM-State-Machine: `phaseStartMs` wird beim ersten Aufruf auf `millis()` initialisiert, verhindert sofortigen Dark-Burst beim Systemstart. (Ref: 31b90bb)

---

## [1.4.2]

### Behoben
- PWM-Pulsrichtung korrigiert: kurze Pulse liegen jetzt auf der Dunkelseite, nicht der Hellseite → keine hellen Blitze mehr bei niedrigen Abdunklungsstufen; robuster gegenüber ungleichmäßigen Loop-Durchlaufzeiten. (Ref: 31b90bb)
- `g_pwmOn`-Initialzustand korrigiert (war `true` statt `false`) → Restore auf „Kein PWM" schaltet Display nun zuverlässig wieder ein. (Ref: 31b90bb)
- Label „Einschaltzeit" → „Abdunklungsstärke" (entspricht jetzt dem tatsächlichen Effekt: 10% = 10% der Zeit dunkel). (Ref: 31b90bb)

---

## [1.4.0]

### Neu
- **Software-PWM-Dimmer**: Helligkeit unterhalb der Hardware-Stufe 0 durch 50-Hz-SHUTDOWN-PWM des MAX7219. (Ref: 31b90bb)
  - Schieberegler im Display-Tab (1–5, entspricht 10–50 % Einschaltzeit) + Checkbox zum Aktivieren
  - Nützlicher Bereich: 10–50 % (ab ~60 % wird Flackern wahrnehmbar)
  - Neue Config-Option `pwm_duty` (Standard: 10 = deaktiviert)
- **Neustart-Seite**: Nach Reboot aus dem Webinterface wartet der Browser automatisch auf den ESP und lädt die Startseite neu. (Ref: 31b90bb)

### Behoben
- SHUTDOWN-Register-Logik korrigiert (ON/OFF waren invertiert → Display blieb nach Deaktivieren des PWM dauerhaft dunkel). (Ref: 31b90bb)

---

## [1.3.1]

### Neu
- Umbenennung: "NTP Uhr" → **"Dottie – The Matrix Clock"**. (Ref: 31b90bb)
- mDNS/OTA-Hostname geändert: `ntpclock` → `dottie` (`dottie.local`). (Ref: 31b90bb)
- AP-SSID-Prefix geändert: `NTP-UHR-` → `DOTTIE-`. (Ref: 31b90bb)

### Behoben
- `GLYPH_M` (8×8) korrigiert: 2-Pixel-breite Striche passend zum restlichen Font. (Ref: 31b90bb)

---

## [1.3.0]

### Neu
- **12h-Modus** mit AM/PM-Anzeige (im 12h-Modus entfallen die Sekunden, stattdessen AM/PM). (Ref: 31b90bb)
- Sekunden im 24h-Modus nun **deaktivierbar** (Display-Tab). (Ref: 31b90bb)
- **5 Datumsformate** wählbar. (Ref: 31b90bb)
  - `DD.MM.YYYY` (Standard)
  - `MM/DD/YYYY`
  - `YYYY-MM-DD` (ISO 8601)
  - `DD.MM.YY`
  - `MM/DD/YY`
- Neue Glyphen: `M` (8×8), `/` und `-` (5×7) für die neuen Formate. (Ref: 31b90bb)

---

## [1.2.5]

### Behoben
- Fehlende Glyphen ergänzt. (Ref: 31b90bb)
  - 8×8: `%`, `?`
  - 5×7: `A`, `B`, `H`, `L`

---

## [1.1.0] – Major Refactor

### Neu
- **OTA (ArduinoOTA)**: Kabellose Firmware-Updates, Progress-Anzeige auf dem Matrix-Display. (Ref: 31b90bb)
- **NTP-Sync-Protokoll**: Ringpuffer im RAM (letzte 5 Einträge), in Web-UI sichtbar. (Ref: 31b90bb)
- **Automatischer WiFi-Reconnect** mit State-Machine und exponentiellem Backoff (30 s → 300 s). (Ref: 31b90bb)
- **Uptime-Anzeige** in der Web-UI. (Ref: 31b90bb)
- **mDNS**: Gerät unter `dottie.local` erreichbar. (Ref: 31b90bb)

### Verbessert
- **Web-UI** komplett neu gestaltet: modernes Design, Tabs (Status/WiFi/NTP/Zeitzone/Display/System), responsive Layout, Live-Display-Vorschau (Canvas). (Ref: 31b90bb)
- **Speicher**: PROGMEM für HTML-Strings, `F()`-Makros konsequent überall. (Ref: 31b90bb)
- **Stabilität**: vollständige WiFi-Reconnect-Logik, AP-Fallback (Captive Portal). (Ref: 31b90bb)
- **NTP-Probe**: robustere Implementierung mit DNS-Fallback, IP-Cache, Fehlerklassifizierung. (Ref: 31b90bb)
- **Code-Qualität**: Konstanten statt Magic-Numbers, klare Benennung, kein duplizierter Code. (Ref: 31b90bb)

---

## [1.0.x] – Initiale Versionen

Grundlegende Funktionalität:
- NTP-Synchronisation (einzelner Server). (Ref: 31b90bb)
- Zeitanzeige auf TOP-Matrix (8×8-Glyphen). (Ref: 31b90bb)
- Datumsanzeige auf BOTTOM-Matrix (5×7-Glyphen). (Ref: 31b90bb)
- Einfache Web-UI zur WLAN-Konfiguration. (Ref: 31b90bb)
- LittleFS-Konfigurationsdatei (`/cfg.txt`). (Ref: 31b90bb)
- Factory-Reset via GPIO16. (Ref: 31b90bb)
