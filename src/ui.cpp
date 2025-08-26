#include "ui.h"
#include "app_state.h"
#include "geometry.h"
#include "utils.h"
#include "ble.h"
#include <M5GFX.h>

void drawBattery(int cx,int cy,int pct){
  int w=26,h=12; int x0=cx-w/2,y0=cy-h/2;
  g_spr.drawRect(x0,y0,w,h,TFT_SILVER);
  g_spr.fillRect(x0+w, y0+3, 3, h-6, TFT_SILVER);
  int bars = (pct+12)/25; // 0..4
  int bar_w = (w-6)/4; int by=y0+2; int bh=h-4;
  for(int i=0;i<4;i++){
    int bx = x0+3+i*bar_w;
    uint32_t col = (i<bars)? TFT_GREEN : g_spr.color888(50,50,50);
    g_spr.fillRect(bx,by,bar_w-2,bh,col);
  }
}

void drawLabels(){
  auto& d = g_spr;
  d.setTextDatum(textdatum_t::middle_center);
  d.setFont(&fonts::Font2);

  // Speed
  d.setTextColor(d.color888(180,220,255));
  if (g_mode==Mode::POSITION) d.setTextColor(d.color888(120,140,160));
  d.drawString(String("Speed ")+g_speed+"%", CX, CY-52);

  // Stroke/Depth: Prozentwerte fix am Ende des Halbkreis-Sliders
  d.setTextColor(d.color888(180,255,180));
  float aL = TOP_START * M_PI/180.0f;
  float aR = TOP_END   * M_PI/180.0f;
  int r_text = R_RANGE_OUT + 12;       // radial nach außen
  int xL = CX + (int)roundf(r_text * cosf(aL)) -10;
  int yL = CY + (int)roundf(r_text * sinf(aL)) + 8;
  int xR = CX + (int)roundf(r_text * cosf(aR)) +10;
  int yR = CY + (int)roundf(r_text * sinf(aR)) + 8;
  d.drawString(String(g_stroke)+"%", xL, yL);
  d.drawString(String(g_depth )+"%", xR, yR);

  // Pos/Sens Anzeige etwas tiefer
  if (g_mode==Mode::POSITION) d.setTextColor(TFT_WHITE); else d.setTextColor(g_spr.color888(255,230,140));
  int sensShown = (g_mode==Mode::POSITION)? g_position : g_sensation;
  d.drawString(String((g_mode==Mode::POSITION)?"Pos ":"Sens ") + String(sensShown), CX, CY+78);
}

void drawTopButtons(){
  auto& d=g_spr;
  int y=BUTTONS_Y;
  int cx_play = CX;
  int cx_minus= CX-CTRL_SPACING;
  int cx_plus = CX+CTRL_SPACING;

  // minus (in SPEED grau)
  uint32_t circleFill = d.color888(30,30,30);
  uint32_t iconCol = (g_mode==Mode::SPEED)? d.color888(120,120,120): TFT_WHITE;
  int rMinus=18, rPlay=24, rPlus=18;

  d.fillCircle(cx_minus, y, rMinus, circleFill);
  d.fillRect(cx_minus-(rMinus-5), y-3, (rMinus-5)*2, 6, iconCol);

  d.fillCircle(cx_play, y, rPlay, circleFill);
  if (g_running) {
    d.fillRect(cx_play-9, y-12, 7, 24, TFT_WHITE);
    d.fillRect(cx_play+2, y-12, 7, 24, TFT_WHITE);
  } else {
    d.fillTriangle(cx_play-8, y-12, cx_play-8, y+12, cx_play+12, y, TFT_WHITE);
  }

  d.fillCircle(cx_plus, y, rPlus, circleFill);
  d.fillRect(cx_plus-(rPlus-5), y-3, (rPlus-5)*2, 6, iconCol);
  d.fillRect(cx_plus-3, y-(rPlus-5), 6, (rPlus-5)*2, iconCol);
}

void drawBottomPill(){
  auto& d=g_spr;
  d.fillRoundRect(CX-60, CTRL_Y-12, 120, 24, 12, d.color888(40,40,40));
  d.setTextDatum(textdatum_t::middle_center);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE);
  d.drawString(g_patterns[g_patternIndex], CX, CTRL_Y);
}

void drawSettingsOverlay(){
  if (!g_showSettings) return;
  auto& d=g_spr;
  d.fillRoundRect(CX-86, CY-64, 172, 128, 16, d.color888(25,25,25));
  d.drawRoundRect(CX-86, CY-64, 172, 128, 16, d.color888(80,80,80));
  d.setTextDatum(textdatum_t::middle_center); d.setFont(&fonts::Font2);
  struct Btn{const char* label; uint32_t col; int y;};
  Btn btns[4]={{ g_connected?"Trennen":"Verbinden", d.color888(0,180,255), CY-34},
               { g_running?"Stop":"Start", d.color888(255,120,120), CY-2},
               { "Home", d.color888(180,255,180), CY+30},
               { "Disable", d.color888(220,220,220), CY+62}};
  for (auto &b:btns){
    d.drawRoundRect(CX-70,b.y-12,140,24,10,d.color888(90,90,90));
    d.setTextColor(b.col); d.drawString(b.label, CX, b.y);
  }
}

void drawPatternPicker(){
  if (!g_showPatternPicker) return;
  auto& d=g_spr;
  d.fillRoundRect(CX-104, CY-78, 208, 156, 16, d.color888(25,25,25));
  d.drawRoundRect (CX-104, CY-78, 208, 156, 16, d.color888(80,80,80));
  d.setTextDatum(textdatum_t::middle_center); d.setTextColor(TFT_WHITE); d.setFont(&fonts::Font2);

  int listTop = CY-60 - g_pickerScroll;
  for (int i=0;i<(int)g_patterns.size();++i){
    int y = listTop + i*34;
    uint32_t fill = (i==g_patternIndex)? d.color888(40,40,70) : d.color888(40,40,40);
    d.fillRoundRect(CX-96, y, 192, 28, 8, fill);
    d.drawRoundRect(CX-96, y, 192, 28, 8, d.color888(80,80,80));
    // Preview
    int px0=CX-90, py0=y+5, px1=px0+80, py1=y+23;
    int prevN = 60; int lastx=px0, lasty=py0+(py1-py0)/2;
    for (int k=0;k<prevN;k++){
      float t=(float)k/(prevN-1);
      float v=0.5f;
      const String &nm=g_patterns[i];
      if      (nm.indexOf("Simple")>=0) v = 0.5f + 0.45f * sinf(2*M_PI*t);
      else if (nm.indexOf("Teasing")>=0) v = 0.5f + 0.45f * powf(sinf(2*M_PI*t),3);
      else if (nm.indexOf("Robo")>=0)    v = 1.0f - fabsf(fmodf(t*2.0f,2.0f)-1.0f);
      else if (nm.indexOf("Half")>=0)    v = 0.5f + (((int)floorf(t*2)%2)? 0.25f:0.45f) * sinf(2*M_PI*fmodf(t*2,1.0f));
      else if (nm.indexOf("Deeper")>=0)  { float a = (floorf(t*4)+1)/4.0f; v = 0.5f + 0.45f*a*sinf(2*M_PI*fmodf(t*4,1.0f)); }
      else if (nm.indexOf("Stop")>=0)    { float loc=fmodf(t*3,1.0f); v = (loc<0.7f)? 0.5f + 0.45f*sinf(2*M_PI*loc/0.7f) : 0.5f; }
      else if (nm.indexOf("Insist")>=0)  v = 0.5f + 0.15f*sinf(2*M_PI*t) + 0.25f*sinf(4*M_PI*t);
      else if (nm.indexOf("Jack")>=0)    v = (t<0.6f)? 0.5f + 0.35f*sinf(20*M_PI*t) : 1.0f - (t-0.6f)/0.4f;
      else if (nm.indexOf("Nibbler")>=0) v = 0.5f + 0.2f*sinf(10*M_PI*t);
      int x=px0 + (int)roundf(t*(px1-px0));
      int yv=py1 - (int)roundf(v*(py1-py0));
      d.drawLine(lastx,lasty,x,yv, d.color888(180,200,255)); lastx=x; lasty=yv;
    }
    d.setTextColor(TFT_WHITE);
    d.drawString(g_patterns[i], CX-96+96, y+14);
  }
}

void drawUI(){
  auto& d = g_spr;
  d.fillSprite(TFT_BLACK);
  d.drawCircle(CX,CY,110,TFT_DARKGREY);

  // Speed (oben)
  drawArcBandAA(CX,CY,R_SPEED_IN,R_SPEED_OUT,TOP_START,TOP_END,d.color888(30,30,30));
  float speed_end = map01(g_speed/100.0f, TOP_START, TOP_END);
  drawArcBandAA(CX,CY,R_SPEED_IN,R_SPEED_OUT,TOP_START,speed_end, (g_mode==Mode::POSITION)? d.color888(60,80,100): d.color888(0,180,255));
  drawHandleR(CX,CY,(R_SPEED_IN+R_SPEED_OUT)/2, speed_end, (g_mode==Mode::POSITION)? d.color888(120,140,160): d.color888(0,180,255), 4);

  // Stroke/Depth (oben, dick, nahtlos an Speed)
  drawArcBandAA(CX,CY,R_RANGE_IN,R_RANGE_OUT,TOP_START,TOP_END,d.color888(28,28,28));
  float a0 = map01(g_stroke/100.0f, TOP_START, TOP_END);
  float a1 = map01(g_depth /100.0f, TOP_START, TOP_END);
  if (a1<a0) { float t=a0;a0=a1;a1=t; }
  drawArcBandAA(CX,CY,R_RANGE_IN,R_RANGE_OUT,a0,a1,d.color888(120,255,120));
  drawHandleR(CX,CY,(R_RANGE_IN+R_RANGE_OUT)/2, map01(g_stroke/100.0f, TOP_START, TOP_END), d.color888(120,255,120), 6);
  drawHandleR(CX,CY,(R_RANGE_IN+R_RANGE_OUT)/2, map01(g_depth /100.0f, TOP_START, TOP_END), d.color888(120,255,120), 6);

  // Sens/Pos (unten)
  drawArcBandAA(CX,CY,R_SENS_IN,R_SENS_OUT,SENS_START,SENS_END,d.color888(30,30,30));
  float mid = 90.0f;
  if (g_mode==Mode::POSITION) {
    float ang = map01(g_position/100.0f, SENS_START, SENS_END);
    uint32_t col = TFT_WHITE;
    if (ang >= mid) drawArcBandAA(CX,CY,R_SENS_IN,R_SENS_OUT,mid,ang,col); else drawArcBandAA(CX,CY,R_SENS_IN,R_SENS_OUT,ang,mid,col);
    drawHandleR(CX,CY,(R_SENS_IN+R_SENS_OUT)/2, ang, TFT_WHITE, 7);
  } else {
    if (g_sensation >= 0) {
      float ang = map01((100.0f - g_sensation)/100.0f, SENS_START, mid);
      drawArcBandAA(CX,CY,R_SENS_IN,R_SENS_OUT,ang,mid,d.color888(255,200,0));
      drawHandleR(CX,CY,(R_SENS_IN+R_SENS_OUT)/2, ang, d.color888(255,230,150), 7);
    } else {
      float ang = map01(fabsf(g_sensation)/100.0f, mid, SENS_END);
      drawArcBandAA(CX,CY,R_SENS_IN,R_SENS_OUT,mid,ang,d.color888(255,120,0));
      drawHandleR(CX,CY,(R_SENS_IN+R_SENS_OUT)/2, ang, d.color888(255,230,150), 7);
    }
  }

  drawLabels();
  drawTopButtons();
  drawBottomPill();
  drawSettingsOverlay();
  drawPatternPicker();

  d.pushSprite(0,0);
}
