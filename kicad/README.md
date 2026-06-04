# KiCad-Projektspiegel

Hier liegt der importierte KiCad-Stand von Dottie. Die Struktur folgt dem Muster aus den anderen Board-Projekten: `source/` enthält die KiCad-Quellen, `fab/rev0.4/` die Freigabe- und Fertigungsdaten.

## Struktur

| Pfad | Inhalt |
|---|---|
| `source/` | KiCad-Projektdateien, Schaltplan, Platine, Blockbibliothek und projektbezogene Hilfsdateien |
| `fab/rev0.5/` | Fertigungsdaten für die freigegebene Version `0.5` |

## Fertigungsdaten

Die aktuelle Freigabe liegt in `fab/rev0.5/`:

- `bom.csv` - Stückliste
- `designators.csv` - Zuordnung der Bauteilbezeichner
- `netlist.ipc` - IPC-1791-Netzliste
- `positions.csv` - Bestückungskoordinaten
- `Dottie_-_The_Matrix_Clock_0.5.zip` - kompletter Export der Fertigungslage

## Stückliste in Kurzform

Die vollständige Stückliste steht in `fab/rev0.5/bom.csv`. Die wichtigsten Funktionsbausteine sind:

| Bauteil | Menge | Rolle |
|---|---:|---|
| `MAX7219CWG` | 16 | LED-Matrix-Treiber für die 16 Module |
| `ESP32-S3-WROOM-1` | 1 | Haupt-MCU |
| `DS3231S` | 1 | hochgenaue RTC mit `CR2032`-Pufferung |
| `VEML7700-TT` | 1 | Umgebungslichtsensor |
| `TLV76133DCY` | 1 | 3.3-V-Versorgung |
| `74HCT245` | 1 | Pegel-/Bus-Treiber |
| `USB-C-Receptacle` | 1 | USB-Anschluss der Platine |
| `BatteryHolder_CR2032` | 1 | Pufferbatterie für die RTC |

## Hinweise

- Die KiCad-Quellen gehören nach `source/`.
- Fertigungsdaten gehören nach `fab/rev0.5/`.
- Änderungen an der Platine werden im Changelog des Firmware-Repos mit aufgeführt, damit Firmware- und Hardware-Stand zusammenbleiben.
