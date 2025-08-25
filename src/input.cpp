#include "input.h"
#include <M5Dial.h>
#include "app_state.h"
#include "geometry.h"
#include "utils.h"
#include "ble.h"

// Drag-Status nur lokal
static bool draggingStroke=false, draggingDepth=false, draggingPosition=false, draggingSensation=false;

// Helpers
static int hitTopButtons(int x,int y){ // − ⏯ +
  int cy=BUTTONS_Y; if (abs(y-cy)>24) return -1;
  if ((x-(CX-CTRL_SPACING))*(x-(CX-CTRL_SPACING))+(y-cy)*(y-cy) <= 18*18) return 0; // minus
  if ((x-CX)*(x-CX)+(y-cy)*(y-cy)                                   <= 24*24) return 1; // play
  if ((x-(CX+CTRL_SPACING))*(x-(CX+CTRL_SPACING))+(y-cy)*(y-cy) <= 18*18) return 2; // plus
  return -1;
}

static void onTap(int x,int y){
  if (g_showSettings){
    struct Row{int y;}; Row rows[4]={{CY-34},{CY-2},{CY+30},{CY+62}};
    for(int i=0;i<4;i++){
      int rx=CX-70, ry=rows[i].y-12;
      if (x>=rx && x<=rx+140 && y>=ry && y<=ry+24){
        if (i==0){ if (g_connected) disconnectOSSM(); else connectToOSSM(); closeSettings(); }
        else if (i==1){ g_running=!g_running; closeSettings(); }
        else if (i==2){ if (g_connected) sendHome(); closeSettings(); }
        else if (i==3){ if (g_connected) sendDisable(); closeSettings(); }
        return;
      }
    }
    closeSettings(); return;
  }

  if (g_showPatternPicker){
    int listTop = CY-60 - g_pickerScroll;
    for (int i=0;i<(int)g_patterns.size();++i){
      int y0=listTop+i*34; int y1=y0+28;
      if (x>=CX-96 && x<=CX+96 && y>=y0 && y<=y1){ g_patternIndex=i; closePicker(); return; }
    }
    closePicker(); return;
  }

  // obere Buttons
  int cc = hitTopButtons(x,y);
  if (cc>=0){
    if (cc==1){ toggleMode(); return; }                 // Play/Pause toggelt Mode
    if (g_mode==Mode::SPEED){ return; }                 // ± im Speed aus
    if (cc==0){ if (g_connected) { sendRetract(); /*sendAirIn();*/ } }
    if (cc==2){ if (g_connected) { sendExtend();  /*sendAirOut();*/ } }
    return;
  }

  // Pattern-Pill unten
  if (x>=CX-60 && x<=CX+60 && y>=CTRL_Y-12 && y<=CTRL_Y+12){ openPicker(); return; }

  // Sens/Pos Band
  if (hitBandPad(x,y,R_SENS_IN,R_SENS_OUT,SENS_START,SENS_END)){
    float ang = clampAngleToArc(pointAngle(x,y), SENS_START, SENS_END);
    if (g_mode==Mode::POSITION){
      draggingPosition=true;
      float t = invMap01(ang, SENS_START, SENS_END); t = clampf(t,0.0f,1.0f);
      int np = (int)roundf(t*100.0f); if(np!=g_position){ g_position=np; needsRedraw=true; if(g_connected) sendMove(g_position,g_moveTime,true);}    
    } else {
      draggingSensation=true;
      if (ang <= 90.0f) {
        float t = invMap01(ang, SENS_START, 90.0f); t = clampf(t,0.0f,1.0f);
        int ns = (int)roundf((1.0f - t) * 100.0f);
        if (ns!=g_sensation){ g_sensation=ns; needsRedraw=true; }
      } else {
        float t = invMap01(ang, 90.0f, SENS_END); t = clampf(t,0.0f,1.0f);
        int ns = -(int)roundf(t * 100.0f);
        if (ns!=g_sensation){ g_sensation=ns; needsRedraw=true; }
      }
    }
    return;
  }

  // Speed-Ring: Touch deaktiviert

  // Stroke/Depth Band → Griff wählen
  if (hitBandPad(x,y,R_RANGE_IN,R_RANGE_OUT,TOP_START,TOP_END)){
    float ang = clampAngleToArc(pointAngle(x,y), TOP_START, TOP_END);
    float aS = map01(g_stroke/100.0f, TOP_START, TOP_END);
    float aD = map01(g_depth /100.0f, TOP_START, TOP_END);
    if (fabsf(ang-aS) < fabsf(ang-aD)) draggingStroke=true; else draggingDepth=true;
    return;
  }
}

static void onDrag(int x,int y){
  float ang = pointAngle(x,y);
  if (draggingStroke){
    ang = clampAngleToArc(ang, TOP_START, TOP_END);
    float t=invMap01(ang,TOP_START,TOP_END); t=clampf(t,0.0f,1.0f);
    int nv=(int)roundf(t*100.0f);
    nv=clampi(nv, 0, g_depth - MIN_GAP);
    if(nv!=g_stroke){ g_stroke=nv; needsRedraw=true; if(g_connected) sendStroke(g_stroke);} 
  } else if (draggingDepth){
    ang = clampAngleToArc(ang, TOP_START, TOP_END);
    float t=invMap01(ang,TOP_START,TOP_END); t=clampf(t,0.0f,1.0f);
    int nv=(int)roundf(t*100.0f);
    nv=clampi(nv, g_stroke+MIN_GAP, 100);
    if(nv!=g_depth){ g_depth=nv; needsRedraw=true; if(g_connected) sendDepth(g_depth);} 
  } else if (draggingSensation){
    ang = clampAngleToArc(ang, SENS_START, SENS_END);
    if (ang <= 90.0f) {
      float t = invMap01(ang, SENS_START, 90.0f); t = clampf(t,0.0f,1.0f);
      int ns = (int)roundf((1.0f - t) * 100.0f);
      if (ns!=g_sensation){ g_sensation=ns; needsRedraw=true; }
    } else {
      float t = invMap01(ang, 90.0f, SENS_END); t = clampf(t,0.0f,1.0f);
      int ns = -(int)roundf(t * 100.0f);
      if (ns!=g_sensation){ g_sensation=ns; needsRedraw=true; }
    }
  } else if (draggingPosition){
    ang = clampAngleToArc(ang, SENS_START, SENS_END);
    float t=invMap01(ang,SENS_START,SENS_END); t=clampf(t,0.0f,1.0f);
    int nv=(int)roundf(t*100.0f);
    if(nv!=g_position){ g_position=nv; needsRedraw=true; if(g_connected) sendMove(g_position,g_moveTime,true);} 
  }
}

static void onRelease(){ draggingStroke=draggingDepth=draggingSensation=draggingPosition=false; }

void inputUpdate(){
  M5Dial.update();

  // BtnA: Long = Settings; Short = Mode toggle oder Auswahl bestätigen im Picker
  if (M5Dial.BtnA.wasHold()) {
    if (g_showPatternPicker) { closePicker(); } else { g_showSettings = !g_showSettings; }
    needsRedraw = true;
  } else if (M5Dial.BtnA.wasPressed()) {
    if (g_showPatternPicker) { closePicker(); needsRedraw = true; }
    else if (!g_showSettings) { toggleMode(); }
  }

  // Encoder — robust + sanfte Beschleunigung
  int32_t enc = M5Dial.Encoder.read();
  int32_t rawDelta = enc - lastEncoder;
  if (rawDelta != 0){
    lastEncoder = enc;
    uint32_t now = millis();
    int sign = (rawDelta>0) - (rawDelta<0);

    // sehr kurzer Gegenimpuls (< 10 ms, klein) ignorieren
    if (sign != 0 && sign != lastDeltaSign && (now - lastDeltaMs) < 10 && abs(rawDelta) <= 3) {
      // ignore tiny glitch
    } else {
      lastDeltaSign = sign;
      lastDeltaMs = now;

      int delta = (int)rawDelta;
      if (abs(delta) >= 4) delta += (delta>0 ? +abs(delta)/3 : -abs(delta)/3);

      if (g_showPatternPicker){
        // langsam / präzise: 1 Schritt je ~4 Ticks
        static int accum = 0;
        accum += delta;
        int step=0; while (accum >= 4){ step++; accum-=4; } while (accum <= -4){ step--; accum+=4; }
        if (step!=0){
          int ni = clampi(g_patternIndex + step, 0, (int)g_patterns.size()-1);
          if (ni != g_patternIndex){
            g_patternIndex = ni;
            // Sichtbarkeit sichern
            int itemY = (CY-60) + g_patternIndex*34 - g_pickerScroll;
            int topVisible = CY-78 + 12;
            int botVisible = CY+78 - 12 - 28;
            if (itemY < topVisible) {
              g_pickerScroll = clampi(g_pickerScroll - (topVisible - itemY), 0, (int)g_patterns.size()*34);
            } else if (itemY > botVisible) {
              g_pickerScroll = clampi(g_pickerScroll + (itemY - botVisible), 0, (int)g_patterns.size()*34);
            }
            needsRedraw = true;
          }
        }
      } else if (g_mode==Mode::SPEED){
        int ns = clampi(g_speed + delta, 0, 100);
        if (ns!=g_speed){ g_speed = ns; needsRedraw = true; if(g_connected) sendSpeed(g_speed);}    
      } else {
        int np = clampi(g_position + delta, 0, 100);
        if (np!=g_position){ g_position = np; needsRedraw = true; if(g_connected) sendMove(g_position, g_moveTime, true);}    
      }
    }
  }

  // Touch
  auto t = M5Dial.Touch.getDetail();
  if (t.isPressed()) {
    if (t.wasPressed()) onTap(t.x, t.y); else onDrag(t.x, t.y);
  } else if (draggingStroke || draggingDepth || draggingSensation || draggingPosition) {
    onRelease();
  }
}
