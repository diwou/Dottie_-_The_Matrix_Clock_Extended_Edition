#pragma once

#include <MD_MAX72xx.h>
#include <U8g2lib.h>
#include "config.h"

extern MD_MAX72XX mx;
extern const char* BOTTOM_FONT_LABELS[6];

// Physikalisches Display-Mapping (obere / untere Zeile)
static const uint8_t MAP_TOP[8]    = {0,1,2,3,4,5,6,7};
static const uint8_t MAP_MIDDLE[8] = {8,9,10,11,12,13,14,15};
static const uint8_t MAP_BOTTOM[8] = {16,17,18,19,20,21,22,23};

// Obere 8 Module sind um 180° gedreht eingebaut.
static inline uint8_t reverseBits8(uint8_t b) {
  b = (uint8_t)((b & 0xF0) >> 4) | (uint8_t)((b & 0x0F) << 4);
  b = (uint8_t)((b & 0xCC) >> 2) | (uint8_t)((b & 0x33) << 2);
  b = (uint8_t)((b & 0xAA) >> 1) | (uint8_t)((b & 0x55) << 1);
  return b;
}

static inline uint8_t rotate180Byte(uint8_t b) {
  return reverseBits8(b);
}

// Orientierung der unteren Zeile
#define BOTTOM_MIRROR_WHOLE_ROW    0
#define BOTTOM_REVERSE_COLS_IN_DEV 0
#define BOTTOM_FLIP_VERTICAL_8     1
#define BOTTOM_ROTATE_90           0

static inline bool isPrintableAscii(char c) { return c >= 32 && c <= 126; }

// ============================================================
// Glyphen: obere Zeile 8×8 (unveränderter Code)
// ============================================================
static const uint8_t GLYPH_0[8]     = {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C};
static const uint8_t GLYPH_1[8]     = {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E};
static const uint8_t GLYPH_2[8]     = {0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E};
static const uint8_t GLYPH_3[8]     = {0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C};
static const uint8_t GLYPH_4[8]     = {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C};
static const uint8_t GLYPH_5[8]     = {0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C};
static const uint8_t GLYPH_6[8]     = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x66,0x3C};
static const uint8_t GLYPH_7[8]     = {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30};
static const uint8_t GLYPH_8[8]     = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C};
static const uint8_t GLYPH_9[8]     = {0x3C,0x66,0x66,0x66,0x3E,0x06,0x0C,0x38};
static const uint8_t GLYPH_COLON[8] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00};
static const uint8_t GLYPH_DASH[8]  = {0x00,0x00,0x00,0x7E,0x7E,0x00,0x00,0x00};
static const uint8_t GLYPH_SPACE[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t GLYPH_DOT[8]   = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00};
static const uint8_t GLYPH_A[8]     = {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x66};
static const uint8_t GLYPH_B[8]     = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x66,0x7C};
static const uint8_t GLYPH_C[8]     = {0x3C,0x66,0x60,0x60,0x60,0x60,0x66,0x3C};
static const uint8_t GLYPH_D[8]     = {0x78,0x6C,0x66,0x66,0x66,0x66,0x6C,0x78};
static const uint8_t GLYPH_E[8]     = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x7E};
static const uint8_t GLYPH_F[8]     = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x60};
static const uint8_t GLYPH_I[8]     = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x7E};
static const uint8_t GLYPH_L[8]     = {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7E};
static const uint8_t GLYPH_N[8]     = {0x66,0x76,0x7E,0x6E,0x66,0x66,0x66,0x66};
static const uint8_t GLYPH_O[8]     = {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C};
static const uint8_t GLYPH_P[8]     = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x60};
static const uint8_t GLYPH_R[8]     = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66};
static const uint8_t GLYPH_S[8]     = {0x3C,0x66,0x60,0x3C,0x06,0x06,0x66,0x3C};
static const uint8_t GLYPH_T[8]     = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18};
static const uint8_t GLYPH_W[8]     = {0x66,0x66,0x66,0x66,0x6E,0x7E,0x76,0x66};
static const uint8_t GLYPH_Y[8]     = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18};
static const uint8_t GLYPH_PERCENT[8]= {0x60,0x60,0x04,0x08,0x10,0x20,0x06,0x06};
static const uint8_t GLYPH_QUEST[8] = {0x3C,0x66,0x06,0x0C,0x18,0x18,0x00,0x18};
static const uint8_t GLYPH_M[8]     = {0xC3,0xE7,0xDB,0xC3,0xC3,0xC3,0xC3,0xC3};

static const uint8_t* glyph8x8(char c) {
  switch (c) {
    case '0': return GLYPH_0; case '1': return GLYPH_1; case '2': return GLYPH_2;
    case '3': return GLYPH_3; case '4': return GLYPH_4; case '5': return GLYPH_5;
    case '6': return GLYPH_6; case '7': return GLYPH_7; case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case ':': return GLYPH_COLON; case '-': return GLYPH_DASH;
    case ' ': return GLYPH_SPACE; case '.': return GLYPH_DOT;
    case 'A': return GLYPH_A; case 'B': return GLYPH_B; case 'C': return GLYPH_C;
    case 'D': return GLYPH_D; case 'E': return GLYPH_E; case 'F': return GLYPH_F;
    case 'I': return GLYPH_I; case 'L': return GLYPH_L; case 'N': return GLYPH_N;
    case 'O': return GLYPH_O; case 'P': return GLYPH_P; case 'R': return GLYPH_R;
    case 'S': return GLYPH_S; case 'T': return GLYPH_T; case 'W': return GLYPH_W;
    case 'Y': return GLYPH_Y;
    case '%': return GLYPH_PERCENT;
    case '?': return GLYPH_QUEST;
    case 'M': return GLYPH_M;
    default:  return GLYPH_SPACE;
  }
}

// ============================================================
// Display-Ausgabe (Top = 8×8)
// ============================================================
// ── Scroll-Zustand pro Zeichen (obere Zeile, 8 Positionen) ──
struct CharScroll {
  char    shown;          // aktuell sichtbare Ziffer
  char    target;         // Ziffer wohin gescrollt wird
  uint8_t step;           // aktueller Scroll-Schritt (0 = fertig)
  uint8_t totalSteps;     // Gesamtschritte für eine Transition
};
static CharScroll scrollState[8] = {};
static bool scrollInitDone = false;

// Einen einzelnen Frame einer scrollenden oder statischen Stelle zeichnen
static void drawTopCharScrollFrame(uint8_t pos, char from, char to, uint8_t step, uint8_t totalSteps) {
  const uint8_t* gFrom = glyph8x8(from);
  const uint8_t* gTo   = glyph8x8(to);
  uint8_t dev = MAP_TOP[pos];

  // step=0 → sofort To anzeigen (fertig)
  if (step == 0 || totalSteps == 0) {
    for (uint8_t row = 0; row < 8; row++) mx.setRow(dev, row, rotate180Byte(gTo[7 - row]));
    return;
  }

  // progress: wie viele Zeilen sind bereits gescrollt (0..8)
  // step läuft von totalSteps..1 (wird dekrementiert), also:
  // am Anfang step=totalSteps → progress≈0, am Ende step=1 → progress≈8
  uint8_t progress = (uint8_t)(((uint16_t)(totalSteps - step) * 8) / totalSteps);
  // progress = 0..7: "from" schiebt sich um progress nach oben,
  //                  "to"   kommt von unten

  uint8_t rowData[8];
  for (uint8_t row = 0; row < 8; row++) {
    uint8_t srcRow = row + progress;   // von welcher Zeile in "from"
    if (srcRow < 8) {
      rowData[row] = gFrom[srcRow];    // noch sichtbarer Teil von "from"
    } else {
      uint8_t toRow = srcRow - 8;      // passender Teil von "to"
      rowData[row] = (toRow < 8) ? gTo[toRow] : 0;
    }
  }
  for (uint8_t row = 0; row < 8; row++) mx.setRow(dev, row, rotate180Byte(rowData[7 - row]));
}

// Direkt zeichnen (ohne Scroll), z.B. für Init / AP-Modus
static void drawTopChar(uint8_t pos, char c) {
  drawTopCharScrollFrame(pos, c, c, 0, 0);
  if (pos < 8) { scrollState[pos].shown = c; scrollState[pos].target = c; scrollState[pos].step = 0; }
}

static void drawTopText8Raw(const char* s8) {
  for (uint8_t i = 0; i < 8; i++) drawTopChar(i, s8[i] ? s8[i] : ' ');
}

static void drawTopCentered(const String& text) {
  char buf[9]; memset(buf, ' ', 8); buf[8] = '\0';
  uint8_t len = (uint8_t)min((int)text.length(), 8);
  uint8_t start = (len < 8) ? (8 - len) / 2 : 0;
  for (uint8_t i = 0; i < len; i++) buf[start + i] = text[i];
  drawTopText8Raw(buf);
}

// Scroll-Tick: einen Animationsschritt für alle laufenden Scrolls ausführen
// Gibt true zurück wenn noch Animationen laufen
static bool tickScrollTop() {
  bool running = false;
  for (uint8_t i = 0; i < 8; i++) {
    if (scrollState[i].step > 0) {
      scrollState[i].step--;
      drawTopCharScrollFrame(i,
        scrollState[i].shown, scrollState[i].target,
        scrollState[i].step, scrollState[i].totalSteps);
      if (scrollState[i].step == 0) scrollState[i].shown = scrollState[i].target;
      else running = true;
    }
  }
  return running;
}

// Neue Zeit setzen — geänderte Ziffern bekommen einen Scroll-Auftrag
static void drawTimeHHMMSS(const char* t) {
  const uint8_t SCROLL_STEPS = cfg.scroll_enabled ? 12 : 0;
  if (!scrollInitDone) {
    // Erster Aufruf: sofort anzeigen
    for (uint8_t i = 0; i < 8; i++) {
      char c = t[i] ? t[i] : ' ';
      scrollState[i] = { c, c, 0, SCROLL_STEPS };
      drawTopChar(i, c);
    }
    scrollInitDone = true;
    return;
  }
  for (uint8_t i = 0; i < 8; i++) {
    char c = t[i] ? t[i] : ' ';
    if (c != scrollState[i].shown) {
      if (SCROLL_STEPS == 0) {
        // Kein Scroll: sofort zeichnen
        scrollState[i].shown  = c;
        scrollState[i].target = c;
        drawTopChar(i, c);
      } else if (scrollState[i].step == 0) {
        // Scroll starten: immer mit voller Schrittzahl
        scrollState[i].target     = c;
        scrollState[i].step       = SCROLL_STEPS;
        scrollState[i].totalSteps = SCROLL_STEPS;
      } else {
        // Scroll läuft bereits — Ziel aktualisieren ohne Neustart
        scrollState[i].target = c;
      }
    }
  }
}

// ============================================================
// Display-Ausgabe (Bottom = 5×7)
// ============================================================
static uint8_t glyphW(char c) {
  if (c == '.') return 2;
  if (c == ' ') return 3;
  return 5;
}

static uint8_t glyphCol5x7(char c, uint8_t i) {
  switch (c) {
    case '0': { static const uint8_t a[5]={0x3E,0x51,0x49,0x45,0x3E}; return i<5?a[i]:0; }
    case '1': { static const uint8_t a[5]={0x00,0x42,0x7F,0x40,0x00}; return i<5?a[i]:0; }
    case '2': { static const uint8_t a[5]={0x42,0x61,0x51,0x49,0x46}; return i<5?a[i]:0; }
    case '3': { static const uint8_t a[5]={0x21,0x41,0x45,0x4B,0x31}; return i<5?a[i]:0; }
    case '4': { static const uint8_t a[5]={0x18,0x14,0x12,0x7F,0x10}; return i<5?a[i]:0; }
    case '5': { static const uint8_t a[5]={0x27,0x45,0x45,0x45,0x39}; return i<5?a[i]:0; }
    case '6': { static const uint8_t a[5]={0x3C,0x4A,0x49,0x49,0x30}; return i<5?a[i]:0; }
    case '7': { static const uint8_t a[5]={0x01,0x71,0x09,0x05,0x03}; return i<5?a[i]:0; }
    case '8': { static const uint8_t a[5]={0x36,0x49,0x49,0x49,0x36}; return i<5?a[i]:0; }
    case '9': { static const uint8_t a[5]={0x06,0x49,0x49,0x29,0x1E}; return i<5?a[i]:0; }
    case '.': { return i < 2 ? 0x60 : 0; }
    case ' ': return 0;
    case 'A': { static const uint8_t a[5]={0x7E,0x11,0x11,0x11,0x7E}; return i<5?a[i]:0; }
    case 'B': { static const uint8_t a[5]={0x7F,0x49,0x49,0x49,0x36}; return i<5?a[i]:0; }
    case 'C': { static const uint8_t a[5]={0x3E,0x41,0x41,0x41,0x22}; return i<5?a[i]:0; }
    case 'D': { static const uint8_t a[5]={0x7F,0x41,0x41,0x22,0x1C}; return i<5?a[i]:0; }
    case 'E': { static const uint8_t a[5]={0x7F,0x49,0x49,0x49,0x41}; return i<5?a[i]:0; }
    case 'F': { static const uint8_t a[5]={0x7F,0x09,0x09,0x09,0x01}; return i<5?a[i]:0; }
    case 'G': { static const uint8_t a[5]={0x3E,0x41,0x49,0x49,0x7A}; return i<5?a[i]:0; }
    case 'H': { static const uint8_t a[5]={0x7F,0x08,0x08,0x08,0x7F}; return i<5?a[i]:0; }
    case 'I': { static const uint8_t a[5]={0x00,0x41,0x7F,0x41,0x00}; return i<5?a[i]:0; }
    case 'J': { static const uint8_t a[5]={0x20,0x40,0x41,0x3F,0x01}; return i<5?a[i]:0; }
    case 'K': { static const uint8_t a[5]={0x7F,0x08,0x14,0x22,0x41}; return i<5?a[i]:0; }
    case 'L': { static const uint8_t a[5]={0x7F,0x40,0x40,0x40,0x40}; return i<5?a[i]:0; }
    case 'M': { static const uint8_t a[5]={0x7F,0x02,0x04,0x02,0x7F}; return i<5?a[i]:0; }
    case 'N': { static const uint8_t a[5]={0x7F,0x02,0x0C,0x10,0x7F}; return i<5?a[i]:0; }
    case 'O': { static const uint8_t a[5]={0x3E,0x41,0x41,0x41,0x3E}; return i<5?a[i]:0; }
    case 'P': { static const uint8_t a[5]={0x7F,0x09,0x09,0x09,0x06}; return i<5?a[i]:0; }
    case 'Q': { static const uint8_t a[5]={0x3E,0x41,0x51,0x21,0x5E}; return i<5?a[i]:0; }
    case 'R': { static const uint8_t a[5]={0x7F,0x09,0x19,0x29,0x46}; return i<5?a[i]:0; }
    case 'S': { static const uint8_t a[5]={0x46,0x49,0x49,0x49,0x31}; return i<5?a[i]:0; }
    case 'T': { static const uint8_t a[5]={0x01,0x01,0x7F,0x01,0x01}; return i<5?a[i]:0; }
    case 'U': { static const uint8_t a[5]={0x3F,0x40,0x40,0x40,0x3F}; return i<5?a[i]:0; }
    case 'V': { static const uint8_t a[5]={0x1F,0x20,0x40,0x20,0x1F}; return i<5?a[i]:0; }
    case 'W': { static const uint8_t a[5]={0x7F,0x20,0x18,0x20,0x7F}; return i<5?a[i]:0; }
    case 'X': { static const uint8_t a[5]={0x63,0x14,0x08,0x14,0x63}; return i<5?a[i]:0; }
    case 'Y': { static const uint8_t a[5]={0x03,0x04,0x78,0x04,0x03}; return i<5?a[i]:0; }
    case 'Z': { static const uint8_t a[5]={0x61,0x51,0x49,0x45,0x43}; return i<5?a[i]:0; }
    case '/': { static const uint8_t a[5]={0x60,0x38,0x0C,0x06,0x03}; return i<5?a[i]:0; }
    case '-': { static const uint8_t a[5]={0x08,0x08,0x08,0x08,0x08}; return i<5?a[i]:0; }
    default:  return 0;
  }
}

static uint8_t glyphCol6x7(char c, uint8_t i) {
  if (i > 5) return 0;
  if (c == ' ') return 0;
  if (c == '.') return (i == 5) ? 0x60 : 0x00;
  if (c == '-') return (i >= 1 && i <= 4) ? 0x08 : 0x00;
  // 6x7: 5 Nutzspalten + 1 Spalte, um den Zeichenkörper breiter zu machen.
  if (i < 5) return glyphCol5x7(c, i);
  switch (c) {
    case 'A': return 0x7E;
    case 'B': return 0x36;
    case 'C': return 0x22;
    case 'D': return 0x1C;
    case 'E': return 0x41;
    case 'F': return 0x01;
    case 'G': return 0x7A;
    case 'H': return 0x7F;
    case 'I': return 0x00;
    case 'J': return 0x01;
    case 'K': return 0x41;
    case 'L': return 0x40;
    case 'M': return 0x7F;
    case 'N': return 0x7F;
    case 'O': return 0x3E;
    case 'P': return 0x06;
    case 'Q': return 0x5E;
    case 'R': return 0x46;
    case 'S': return 0x31;
    case 'T': return 0x01;
    case 'U': return 0x3F;
    case 'V': return 0x1F;
    case 'W': return 0x7F;
    case 'X': return 0x63;
    case 'Y': return 0x03;
    case 'Z': return 0x43;
    default: return 0x00;
  }
}

struct BottomFont8x8Glyph {
  char c;
  uint8_t row[8];
};

static const BottomFont8x8Glyph BOTTOM_FONT_8X8[] = {
  { ' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
  { 'A', {0x00,0x10,0x28,0x44,0x82,0xFE,0x82,0x82} },
  { 'B', {0x00,0xFC,0x82,0x82,0xFC,0x82,0x82,0xFC} },
  { 'C', {0x00,0x7C,0x82,0x80,0x80,0x80,0x82,0x7C} },
  { 'D', {0x00,0xF8,0x84,0x82,0x82,0x82,0x84,0xF8} },
  { 'E', {0x00,0xFE,0x80,0x80,0xFC,0x80,0x80,0xFE} },
  { 'F', {0x00,0xFE,0x80,0x80,0xFC,0x80,0x80,0x80} },
  { 'G', {0x00,0x7C,0x82,0x80,0x8E,0x82,0x82,0x7C} },
  { 'H', {0x00,0x82,0x82,0x82,0xFE,0x82,0x82,0x82} },
  { 'I', {0x00,0x7C,0x10,0x10,0x10,0x10,0x10,0x7C} },
  { 'J', {0x00,0x3E,0x04,0x04,0x04,0x84,0x84,0x78} },
  { 'K', {0x00,0x82,0x84,0x88,0xF0,0x88,0x84,0x82} },
  { 'L', {0x00,0x80,0x80,0x80,0x80,0x80,0x80,0xFE} },
  { 'M', {0x00,0x82,0xC6,0xAA,0x92,0x82,0x82,0x82} },
  { 'N', {0x00,0x82,0xC2,0xA2,0x92,0x8A,0x86,0x82} },
  { 'O', {0x00,0x7C,0x82,0x82,0x82,0x82,0x82,0x7C} },
  { 'P', {0x00,0xFC,0x82,0x82,0xFC,0x80,0x80,0x80} },
  { 'Q', {0x00,0x7C,0x82,0x82,0x82,0x92,0x8C,0x7A} },
  { 'R', {0x00,0xFC,0x82,0x82,0xFC,0x88,0x84,0x82} },
  { 'S', {0x00,0x7C,0x82,0x80,0x7C,0x02,0x82,0x7C} },
  { 'T', {0x00,0xFE,0x10,0x10,0x10,0x10,0x10,0x10} },
  { 'U', {0x00,0x82,0x82,0x82,0x82,0x82,0x82,0x7C} },
  { 'V', {0x00,0x82,0x82,0x82,0x44,0x44,0x28,0x10} },
  { 'W', {0x00,0x82,0x82,0x82,0x92,0xAA,0xC6,0x82} },
  { 'X', {0x00,0x82,0x44,0x28,0x10,0x28,0x44,0x82} },
  { 'Y', {0x00,0x82,0x44,0x28,0x10,0x10,0x10,0x10} },
  { 'Z', {0x00,0xFE,0x04,0x08,0x10,0x20,0x40,0xFE} },
  { '0', {0x00,0x7C,0x82,0x86,0x9A,0xA2,0x82,0x7C} },
  { '1', {0x00,0x10,0x30,0x50,0x10,0x10,0x10,0x7C} },
  { '2', {0x00,0x7C,0x82,0x02,0x0C,0x30,0x40,0xFE} },
  { '3', {0x00,0x7C,0x82,0x02,0x3C,0x02,0x82,0x7C} },
  { '4', {0x00,0x08,0x18,0x28,0x48,0xFE,0x08,0x08} },
  { '5', {0x00,0xFE,0x80,0xFC,0x02,0x02,0x82,0x7C} },
  { '6', {0x00,0x3C,0x40,0x80,0xFC,0x82,0x82,0x7C} },
  { '7', {0x00,0xFE,0x02,0x04,0x08,0x10,0x20,0x20} },
  { '8', {0x00,0x7C,0x82,0x82,0x7C,0x82,0x82,0x7C} },
  { '9', {0x00,0x7C,0x82,0x82,0x7E,0x02,0x04,0x78} },
  { '.', {0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x30} },
  { ',', {0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x20} },
  { ':', {0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x00} },
  { ';', {0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x20} },
  { '!', {0x00,0x10,0x10,0x10,0x10,0x10,0x00,0x10} },
  { '?', {0x00,0x7C,0x82,0x02,0x0C,0x10,0x00,0x10} },
  { '-', {0x00,0x00,0x00,0x00,0x7C,0x00,0x00,0x00} },
  { '/', {0x00,0x02,0x04,0x08,0x10,0x20,0x40,0x80} },
  { '+', {0x00,0x00,0x10,0x10,0x7C,0x10,0x10,0x00} },
  { '*', {0x00,0x00,0x44,0x28,0x7C,0x28,0x44,0x00} },
  { '=', {0x00,0x00,0x00,0x7C,0x00,0x7C,0x00,0x00} },
  { '_', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE} },
  { '(', {0x00,0x08,0x10,0x20,0x20,0x20,0x10,0x08} },
  { ')', {0x00,0x20,0x10,0x08,0x08,0x08,0x10,0x20} },
  { '[', {0x00,0x38,0x20,0x20,0x20,0x20,0x20,0x38} },
  { ']', {0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x38} },
  { '<', {0x00,0x04,0x08,0x10,0x20,0x10,0x08,0x04} },
  { '>', {0x00,0x40,0x20,0x10,0x08,0x10,0x20,0x40} }
};

static const uint8_t* bottomFont8x8Glyph(char c) {
  for (uint16_t i = 0; i < (sizeof(BOTTOM_FONT_8X8) / sizeof(BOTTOM_FONT_8X8[0])); i++) {
    if (BOTTOM_FONT_8X8[i].c == c) return BOTTOM_FONT_8X8[i].row;
  }
  return BOTTOM_FONT_8X8[0].row;
}

static inline void rowSet(const uint8_t rowMap[8], uint8_t x, uint8_t y, bool on) {
  if (BOTTOM_MIRROR_WHOLE_ROW) x = 63 - x;
  uint8_t devPos = x / 8, col = x % 8, row = y;
  uint8_t dev = rowMap[devPos];
  if (BOTTOM_ROTATE_90)           { uint8_t t=row; row=col; col=t; }
  if (BOTTOM_REVERSE_COLS_IN_DEV)  col = 7 - col;
  if (BOTTOM_FLIP_VERTICAL_8)      row = 7 - row;
  mx.setPoint(row, (uint16_t)dev * 8 + col, on);
}

static void clearRow(const uint8_t rowMap[8]) {
  for (uint8_t i = 0; i < 8; i++) mx.clear(rowMap[i]);
}

// ============================================================
// U8g2-Proportionalschrift für die untere Zeile (64×8 Pixel)
// Rendert in einen Offscreen-Buffer, kopiert dann die Pixel
// via rowSet() auf die MAX7219-Module – keine SPI-Konkurrenz.
// Buffer-Layout (horizontal_right_lsb): buf[row*8 + tile],
// Maske 0x80>>col = MSB→linkes Pixel, LSB→rechtes Pixel.
// ============================================================
static u8g2_t g_u8g2_bottom;
static int8_t g_u8g2_current_font = -1;  // Neuladen erzwingen bei Änderung

static const uint8_t* BOTTOM_FONTS[] = {
  u8g2_font_ncenR08_tr,   // 0: New Century Schoolbook
  u8g2_font_helvR08_tr,   // 1: Helvetica
  u8g2_font_timR08_tr,    // 2: Times
  u8g2_font_courR08_tr,   // 3: Courier
  u8g2_font_lubR08_tr,    // 4: Lubalin Graph
  u8g2_font_helvB08_tr,   // 5: Helvetica Bold
};

const char* BOTTOM_FONT_LABELS[6] = {
  "New Century Schoolbook",
  "Helvetica",
  "Times Roman",
  "Courier",
  "Lubalin Graph",
  "Helvetica Bold",
};

static uint8_t u8x8_byte_bottom_dummy(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
  return 1;
}

static uint8_t u8x8_gpio_bottom_dummy(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
  return 1;
}

static void initU8g2Bottom() {
  int8_t fid = (int8_t)constrain((int)cfg.bottom_font, 0, 5);
  if (g_u8g2_current_font == fid) return;
  g_u8g2_current_font = fid;
  u8g2_Setup_max7219_64x8_f(&g_u8g2_bottom, U8G2_R0,
    u8x8_byte_bottom_dummy, u8x8_gpio_bottom_dummy);
  u8g2_SetFont(&g_u8g2_bottom, BOTTOM_FONTS[fid]);
  u8g2_SetFontMode(&g_u8g2_bottom, 1);
}

static void drawBottomTextU8g2Centered(const String& s) {
  initU8g2Bottom();
  clearRow(MAP_BOTTOM);

  const char* str = s.c_str();
  u8g2_ClearBuffer(&g_u8g2_bottom);

  u8g2_uint_t tw = u8g2_GetStrWidth(&g_u8g2_bottom, str);
  u8g2_uint_t x = (tw < 64) ? (64 - tw) / 2 : 0;

  u8g2_DrawStr(&g_u8g2_bottom, x, 8, str);

  // Buffer (horizontal_right_lsb) auslesen → MAX7219 kopieren
  uint8_t *buf = u8g2_GetBufferPtr(&g_u8g2_bottom); // 8 Zeilen × 8 Tiles = 64 Byte
  for (uint8_t col = 0; col < 64; col++) {
    uint8_t tile = col / 8;
    for (uint8_t row = 0; row < 8; row++) {
      if (buf[row * 8 + tile] & (0x80 >> (col & 7))) {
        rowSet(MAP_BOTTOM, col, row, true);
      }
    }
  }
}

static void drawRowText5x7Centered(const String& s, const uint8_t rowMap[8]) {
  clearRow(rowMap);
  uint8_t wTotal = 0;
  for (uint16_t k = 0; k < s.length(); k++) { wTotal += glyphW(s[k]) + 1; }
  if (wTotal) wTotal--;
  if (wTotal > 64) wTotal = 64;
  uint8_t x = (wTotal < 64) ? (64 - wTotal) / 2 : 0;
  for (uint16_t k = 0; k < s.length() && x < 64; k++) {
    char ch = s[k];
    uint8_t w = glyphW(ch);
    for (uint8_t i = 0; i < w && x < 64; i++, x++) {
      uint8_t bits = glyphCol5x7(ch, i);
      for (uint8_t y = 0; y < 7; y++) if (bits & (1 << y)) rowSet(rowMap, x, y + 1, true);
    }
    if (k + 1U < s.length() && x < 64) x++;
  }
}

static void drawMiddleText5x7Centered(const String& s) {
  drawRowText5x7Centered(s, MAP_MIDDLE);
}

static void drawBottomText5x7Centered(const String& s) {
  clearRow(MAP_BOTTOM);
  char txt[9];
  memset(txt, ' ', 8);
  txt[8] = '\0';
  bool hasTrailingDot = false;

  if (s.length() == 9 && s.endsWith(".")) {
    for (uint8_t i = 0; i < 8; i++) txt[i] = s[i];
    hasTrailingDot = true;
  } else {
    uint8_t len = (uint8_t)min((int)s.length(), 8);
    for (uint8_t i = 0; i < len; i++) txt[i] = s[i];
  }

  // 8x8-Rendering 1:1 aus festem Font, ein Zeichen pro Modul.
  for (uint8_t m = 0; m < 8; m++) {
    const uint8_t* g = bottomFont8x8Glyph(txt[m]);
    const uint8_t x0 = (uint8_t)(m * 8);
    for (uint8_t row = 0; row < 8; row++) {
      uint8_t rb = g[row];
      for (uint8_t col = 0; col < 8; col++) {
        if (rb & (0x80 >> col)) rowSet(MAP_BOTTOM, (uint8_t)(x0 + col), row, true);
      }
    }
  }

  if (hasTrailingDot) {
    const uint8_t* dot = bottomFont8x8Glyph('.');
    const uint8_t x0 = 56;
    for (uint8_t row = 0; row < 8; row++) {
      uint8_t rb = dot[row];
      for (uint8_t col = 0; col < 8; col++) {
        if (rb & (0x80 >> col)) rowSet(MAP_BOTTOM, (uint8_t)(x0 + col), row, true);
      }
    }
  }
}

// ============================================================
// Display-Screens
// ============================================================
static void showInitWifi(uint16_t secLeft, bool visible) {
  if (!visible) { mx.clear(); return; }
  drawTopCentered(String(F("INIT ")) + secLeft);
  drawMiddleText5x7Centered(F("WIFI"));
}

static void showWifiNotFound(bool visible) {
  if (!visible) { mx.clear(); return; }
  drawTopText8Raw("WLAN NOT");
  drawMiddleText5x7Centered(F("FOUND"));
}

static void showFactoryScreen() {
  drawTopText8Raw("FACTORY");
  drawMiddleText5x7Centered(F("RESET"));
}

static void showResetCountdown(uint8_t secLeft) {
  char buf[9]; snprintf(buf, sizeof(buf), "RESET %u ", (unsigned)min<uint8_t>(secLeft, 9));
  drawTopText8Raw(buf);
  drawMiddleText5x7Centered(F("HOLD"));
}

static void showTopMarquee(const String& msg, uint16_t seconds) {
  if (!seconds) return;
  String m = String(F("   ")) + msg + String(F("   "));
  uint32_t t0 = millis();
  uint16_t idx = 0;
  while (millis() - t0 < (uint32_t)seconds * 1000UL) {
    char win[9];
    for (uint8_t i = 0; i < 8; i++) {
      char c = m[(idx + i) % m.length()];
      win[i] = isPrintableAscii(c) ? c : ' ';
    }
    win[8] = '\0';
    drawTopText8Raw(win);
    clearRow(MAP_MIDDLE);
    clearRow(MAP_BOTTOM);
    idx = (idx + 1) % m.length();
    delay(220); yield();
  }
  mx.clear();
}
