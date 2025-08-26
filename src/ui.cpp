#include "ui.h"
#include <M5Dial.h>
#include <math.h>
#include <algorithm>

#include "app_state.h"   // extern g_spr, g_mode, g_running, g_speed, ...
#include "geometry.h"    // W,H,CX,CY, R_SPEED_IN/OUT, R_RANGE_IN/OUT, SENS/TOP, CTRL_Y, CTRL_SPACING
#include "utils.h"       // clampi/clampf, map01/invMap01/lerp, etc.

// -------------------- lokale Zeichen-Helper --------------------
static void drawArcBandAA(int cx,int cy,int r_in,int r_out,float a0,float a1,uint32_t col){
  // robust gegen vertauschte Winkel:
  if (a1 < a0) std::swap(a0, a1);
  int steps = std::max(12, (int)ceilf(fabs(a1 - a0) / 3.0f));
  for (int i=0;i<steps;i++) {
    float t0 = lerp(a0, a1, (float)i/steps);
    float t1 = lerp(a0, a1, (float)(i+1)/steps);
    float r0 = t0 * (float)M_PI / 180.0f;
    float r1 = t1 * (float)M_PI / 180.0f;

    int x0i = cx + (int)roundf(r_in * cosf(r0));
    int y0i = cy + (int)roundf(r_in * sinf(r0));
    int x0o = cx + (int)roundf(r_out* cosf(r0));
    int y0o = cy + (int)roundf(r_out* sinf(r0));
    int x1o = cx + (int)roundf(r_out* cosf(r1));
    int y1o = cy + (int)roundf(r_out* sinf(r1));
    int x1i = cx + (int)roundf(r_in * cosf(r1));
    int y1i = cy + (int)roundf(r_in * sinf(r1));

    g_spr.fillTriangle(x0i,y0i,x0o,y0o,x1o,y1o,col);
    g_spr.fillTriangle(x0i,y0i,x1o,y1o,x1i,y1i,col);
  }
}

static void drawHandle(int cx,int cy,int r,float ang,uint32_t col,int rad=6){
  float a = ang * (float)M_PI / 180.0f;
  int x = cx + (int)roundf(r * cosf(a));
  int y = cy + (int)roundf(r * sinf(a));
  g_spr.fillCircle(x,y,rad,col);
}

// -------------------- UI-Teilbereiche --------------------
static void drawLabels(){
  auto& d = g_spr;
  d.setTextDatum(textdatum_t::middle_center);
  d.setFont(&fonts::Font2);

  // Speed-Label
  d.setTextColor(d.color888(200,220,255));
  d.drawString(String("Speed ")+g_speed+"%", CX, CY - 54);

  // Stroke/Depth %-Werte an den ENDEN des Gesamt-Sliders (nicht mitlaufend)
  d.setTextColor(d.color888(180,255,180));
  d.drawString(String(g_stroke)+"%", CX - 70, CY - 10);
  d.drawString(String(g_depth) +"%", CX + 70, CY - 10);

  // Sensation/Position Anzeige
  d.setTextColor(TFT_WHITE);
  const int sv = (g_mode==Mode::POSITION) ? g_position : g_sensation;
  d.drawString(String((g_mode==Mode::POSITION) ? "Pos " : "Sens ") + String(sv), CX, CY + 80);
}

static void drawControls(){
  auto& d = g_spr;
  int y = CTRL_Y;
  int cx_play  = CX;
  int cx_minus = CX - CTRL_SPACING;
  int cx_plus  = CX + CTRL_SPACING;

  uint32_t colBg = d.color888(30,30,30);

  // minus
  d.fillCircle(cx_minus, y, 18, colBg);
  d.fillRect  (cx_minus - 10, y - 3, 20, 6, TFT_WHITE);

  // play/pause
  d.fillCircle(cx_play, y, 22, colBg);
  if (g_running) {
    d.fillRect(cx_play - 8, y - 12, 6, 24, TFT_WHITE);
    d.fillRect(cx_play + 2, y - 12, 6, 24, TFT_WHITE);
  } else {
    d.fillTriangle(cx_play - 8, y - 14, cx_play - 8, y + 14, cx_play + 14, y, TFT_WHITE);
  }

  // plus
  d.fillCircle(cx_plus, y, 18, colBg);
  d.fillRect  (cx_plus - 10, y - 3, 20, 6, TFT_WHITE);
  d.fillRect  (cx_plus - 3,  y - 10, 6, 20, TFT_WHITE);
}

static void drawPatternPill(){
  auto& d = g_spr;
  d.fillRoundRect(CX - 60, CY - 36, 120, 22, 10, d.color888(40,40,40));
  d.setTextDatum(textdatum_t::middle_center);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE);
  d.drawString(g_patterns[g_patternIndex], CX, CY - 25);
}

// -------------------- Haupt-Draw --------------------
void drawUI(){
  auto& d = g_spr;

  // Hintergrund (Sprite ist in setup bereits erstellt/konfiguriert)
  d.fillSprite(TFT_BLACK);

  // Speed (oberer Halbkreis – dünner Ring)
  drawArcBandAA(CX, CY, R_SPEED_IN, R_SPEED_OUT, TOP_START, TOP_END, d.color888(30,30,30));
  float speed_end = map01(g_speed/100.0f, TOP_START, TOP_END);
  drawArcBandAA(CX, CY, R_SPEED_IN, R_SPEED_OUT, TOP_START, speed_end,
                (g_mode==Mode::POSITION) ? d.color888(60,80,100) : d.color888(0,180,255));
  drawHandle(CX, CY, (R_SPEED_IN + R_SPEED_OUT)/2, speed_end,
             (g_mode==Mode::POSITION) ? d.color888(120,140,160) : d.color888(0,180,255),
             5);

  // Stroke/Depth (oberer Halbkreis – dicker Ring, direkt anschließend)
  drawArcBandAA(CX, CY, R_RANGE_IN, R_RANGE_OUT, TOP_START, TOP_END, d.color888(28,28,28));
  float a0 = map01(g_stroke/100.0f, TOP_START, TOP_END);
  float a1 = map01(g_depth /100.0f, TOP_START, TOP_END);
  if (a1 < a0) std::swap(a0, a1);
  drawArcBandAA(CX, CY, R_RANGE_IN, R_RANGE_OUT, a0, a1, d.color888(120,255,120));
  drawHandle(CX, CY, (R_RANGE_IN + R_RANGE_OUT)/2, a0, d.color888(120,255,120), 7);
  drawHandle(CX, CY, (R_RANGE_IN + R_RANGE_OUT)/2, a1, d.color888(120,255,120), 7);

  // Sensation/Position (unterer 150°-Bogen, etwas nach innen gesetzt)
  const int SENS_IN  = R_SPEED_IN  - 8;
  const int SENS_OUT = R_SPEED_OUT - 8;
  drawArcBandAA(CX, CY, SENS_IN, SENS_OUT, SENS_START, SENS_END, d.color888(30,30,30));
  float mid = 90.0f;
  if (g_mode==Mode::POSITION) {
    float ang = map01(g_position/100.0f, SENS_START, SENS_END);
    drawArcBandAA(CX, CY, SENS_IN, SENS_OUT, (ang>=mid? mid : ang), (ang>=mid? ang : mid), TFT_WHITE);
    drawHandle(CX, CY, (SENS_IN + SENS_OUT)/2, ang, TFT_WHITE, 9);
  } else {
    if (g_sensation >= 0) {
      float ang = map01((100 - g_sensation)/100.0f, SENS_START, mid);
      drawArcBandAA(CX, CY, SENS_IN, SENS_OUT, ang, mid, d.color888(255,200,0));
      drawHandle(CX, CY, (SENS_IN + SENS_OUT)/2, ang, d.color888(255,230,150), 9);
    } else {
      float ang = map01(fabsf(g_sensation)/100.0f, mid, SENS_END);
      drawArcBandAA(CX, CY, SENS_IN, SENS_OUT, mid, ang, d.color888(255,120,0));
      drawHandle(CX, CY, (SENS_IN + SENS_OUT)/2, ang, d.color888(255,230,150), 9);
    }
  }

  // Labels / Controls / Pattern-Pill
  drawLabels();
  drawControls();
  drawPatternPill();

  // Ausgabe
  d.pushSprite(0, 0);
}
