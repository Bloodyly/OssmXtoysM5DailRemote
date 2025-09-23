#include "input.h"
#include <M5Dial.h>
#include "app_state.h"
#include "geometry.h"
#include "utils.h"
#include "ble.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---- globals ----
volatile int32_t g_enc_accum = 0;
static portMUX_TYPE g_enc_mux = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_encTask = nullptr;

// Atomar abholen (read-and-reset)
int32_t takeEncoderDelta(){
  int32_t d;
  taskENTER_CRITICAL(&g_enc_mux);
  d = g_enc_accum;
  g_enc_accum = 0;
  taskEXIT_CRITICAL(&g_enc_mux);
  return d;
}

// 1 kHz Sampler: liest M5Dial.Encoder und sammelt Deltas
static void encoderSampler(void*){
  int32_t last = 0;

  // Wichtig: einmal initial update(), dann Ausgangswert holen
  M5Dial.update();
  last = M5Dial.Encoder.read();

  for(;;){
    // Alle 2 ms reicht meist locker, 1 ms geht auch – 2 ms schont I2C/Touch
    vTaskDelay(pdMS_TO_TICKS(8));

    // EINZIGE Stelle im Programm, die M5Dial.update() aufruft:
    M5Dial.update();

    int32_t cur = M5Dial.Encoder.read();
    int32_t d   = cur - last;
    if (d) {
      portENTER_CRITICAL(&g_enc_mux);
      g_enc_accum += d;          // verlustfrei puffer
      portEXIT_CRITICAL(&g_enc_mux);
      last = cur;
    }
  }
}

#ifndef APP_CPU_NUM
// Fallback: App-CPU ist i. d. R. 1 (PRO_CPU=0, APP_CPU=1) auf ESP32/S3
#define APP_CPU_NUM 1
#endif

void startEncoderSampler(){
  if (s_encTask) return;
  xTaskCreatePinnedToCore(
    encoderSampler,       // Task-Funktion
    "enc",                // Name
    2048,                 // Stack
    nullptr,              // Param
    3,                    // Priorität (höher als UI, aber nicht zu hoch)
    &s_encTask,           // Handle out
    APP_CPU_NUM           // Core-Pinning
  );
}

void stopEncoderSampler(){
  if (!s_encTask) return;
  vTaskDelete(s_encTask);
  s_encTask = nullptr;
}

// Drag-Status nur lokal
static bool draggingStroke=false, draggingDepth=false, draggingPosition=false, draggingSensation=false;

// ---------- Buttons oben (− ⏯ +) ----------
static int hitTopButtons(int x,int y){ // − ⏯ +
  int cy=BUTTONS_Y; if (abs(y-cy)>24) return -1;
  if ((x-(CX-CTRL_SPACING))*(x-(CX-CTRL_SPACING))+(y-cy)*(y-cy) <= 18*18) return 0; // minus
  if ((x-CX)*(x-CX)+(y-cy)*(y-cy)                                   <= 24*24) return 1; // play
  if ((x-(CX+CTRL_SPACING))*(x-(CX+CTRL_SPACING))+(y-cy)*(y-cy) <= 18*18) return 2; // plus
  return -1;
}

// ---------- Lokale Treffer-Tests mit verlagertem Nullpunkt ----------
static bool hitTopBandRel(int x,int y,int r_in,int r_out){
  if (!inAnnulus(x,y,r_in,r_out)) return false;
  float a = relTopDeg(x,y);             // −90..+90 gültig
  return (a >= -90.0f && a <= +90.0f);
}

static bool hitBottomBandRel(int x,int y,int r_in,int r_out){
  if (!inAnnulus(x,y,r_in,r_out)) return false;
  float a = relBottomDeg(x,y);          // −75..+75 gültig
  return (a >= -75.0f && a <= +75.0f);
}

// ---------- Tap-Handling ----------
static void onTap(int x,int y){
  if (g_showSettings){
    struct Row{int y;}; Row rows[4]={{CY-34},{CY-2},{CY+30},{CY+62}};
    for(int i=0;i<4;i++){
      int rx=CX-70, ry=rows[i].y-12;
      if (x>=rx && x<=rx+140 && y>=ry && y<=ry+24){
        if (i==0){ 
          //if (ble_is_connected()) bleDisconnect(); 
          //else bleConnectAuto(); closeSettings(); 
        }
        else if (i==1){ g_running=!g_running; closeSettings(); }
        else if (i==2){ 
          if (ble_is_connected()) {
            bleSendHome(); 
            bleSendSetPhysicalTravel(kPhysicalTravelMm);
            closeSettings(); 
          }
        }
        else if (i==3){ if (ble_is_connected()) bleSendDisable(); closeSettings(); }
        return;
      }
    }
    closeSettings(); return;
  }

  if (g_showPatternPicker){
    int listTop = CY-60 - g_pickerScroll;
    for (int i=0;i<(int)g_patterns.size();++i){
      int y0=listTop+i*34; int y1=y0+28;
      if (x>=CX-96 && x<=CX+96 && y>=y0 && y<=y1){ g_patternIndex=i; closePicker(); bleSendPattern(g_patternIndex); return; }
    }
    closePicker(); 
    bleSendPattern(g_patternIndex);
    return;
  }

  // obere Buttons
  int cc = hitTopButtons(x,y);
  if (cc>=0){
    if (cc==1){ toggleMode(); return; }                 // Play/Pause toggelt Mode
    if (cc==0){ if (ble_is_connected()) { bleSendRetract(); bleSendAirIn(); } }
    if (cc==2){ if (ble_is_connected()) { bleSendExtend();  bleSendAirOut(); } }
    return;
  }

  // Pattern-Pill unten
  if (x>=CX-60 && x<=CX+60 && y>=CTRL_Y-12 && y<=CTRL_Y+12){ openPicker(); return; }

  // Sens/Pos Band (unterer 150°-Bogen)
  if (hitBottomBandRel(x,y,R_SENS_IN,R_SENS_OUT)){
    float a = relBottomDeg(x,y);                // [-75..+75]
    a = clampf(a, -75.0f, +75.0f);
    float t = (a + 75.0f) / 150.0f;             // -> [0..1] links..rechts

    if (g_mode==Mode::POSITION){
      draggingPosition=true;
      int np = (int)roundf(t*100.0f);
      if(np!=g_position){ 
        g_position=np; 
        needsRedraw=true; 
        if(ble_is_connected()) bleSendMove(g_position,g_moveTime,true);
      }    
    } else {
      draggingSensation=true;
      // Mapping: links = +100 → Mitte = 0 → rechts = −100
      int ns = (int)roundf(((0.5f - t) / 0.5f) * 100.0f);
      ns = clampi(ns,-100,100);
      if (ns!=g_sensation){ g_sensation=ns; needsRedraw=true; }
    }
    return;
  }

  // Speed-Ring: Touch deaktiviert

  // Stroke/Depth Band → Griff wählen (oberer Halbring)
  if (hitTopBandRel(x,y,R_RANGE_IN,R_RANGE_OUT)){
    float aRel = relTopDeg(x,y);                 // [-90..+90]
    // Zielwinkel beider Griffe ebenfalls in relTop-Space:
    float aS = -90.0f + (g_stroke/100.0f)*180.0f;
    float aD = -90.0f + (g_depth /100.0f)*180.0f;
    if (fabsf(aRel-aS) < fabsf(aRel-aD)) draggingStroke=true; else draggingDepth=true;
    return;
  }
}

// ---------- Drag-Handling ----------
static void onDrag(int x,int y){
  if (draggingStroke){
    float a = relTopDeg(x,y);
    a = clampf(a, -90.0f, +90.0f);
    float t = (a + 90.0f) / 180.0f;
    int nv = (int)roundf(t * 100.0f);
    nv = clampi(nv, 0, g_depth - MIN_GAP);
    if (nv != g_stroke) { 
      g_stroke = nv;
      needsRedraw = true;
      if (ble_is_connected()) bleSendStroke(g_stroke); 
    }
  }
  else if (draggingDepth){
    float a = relTopDeg(x,y);
    a = clampf(a, -90.0f, +90.0f);
    float t = (a + 90.0f) / 180.0f;
    int nv = (int)roundf(t * 100.0f);
    nv = clampi(nv, g_stroke + MIN_GAP, 100);
    if (nv != g_depth) { 
      g_depth = nv;
      needsRedraw = true;
      if (ble_is_connected()) bleSendDepth(g_depth); 
    }
  }
  else if (draggingSensation){
    float a = relBottomDeg(x,y);
    a = clampf(a, -75.0f, +75.0f);
    float t = (a + 75.0f) / 150.0f;
    int ns = (int)roundf(((0.5f - t) / 0.5f) * 100.0f);
    ns = clampi(ns, -100, +100);
    if (ns != g_sensation) { 
      g_sensation = ns; 
      needsRedraw = true;
      if (g_mode == Mode::SPEED) {
        bleSendSensation(g_sensation);
      } 
    }
  }
  else if (draggingPosition){
    float a = relBottomDeg(x,y);
    a = clampf(a, -75.0f, +75.0f);
    float t = (a + 75.0f) / 150.0f;
    int nv = (int)roundf(t * 100.0f);
    if (nv != g_position) {
       g_position = nv; 
       needsRedraw = true;
        if (ble_is_connected()) bleSendMove(g_position, g_moveTime, true); 
    }
  }
}

static void onRelease(){ draggingStroke=draggingDepth=draggingSensation=draggingPosition=false; }

// ---------- Eingabe-Update ----------
void inputUpdate(){
  //M5Dial.update(); // <- RAUS! Update macht jetzt der Sampler-Task

  // BtnA: Long = Settings; Short = Mode toggle oder Auswahl bestätigen im Picker
  if (M5Dial.BtnA.wasHold()) {
    if (g_showPatternPicker) { closePicker(); } else { g_showSettings = !g_showSettings; }
    needsRedraw = true;
  } else if (M5Dial.BtnA.wasPressed()) {
    if (g_showPatternPicker) { closePicker(); bleSendPattern(g_patternIndex);needsRedraw = true; }
    else if (!g_showSettings) { toggleMode(); }
  }

  // ---------- Encoder — präzise langsam, beschleunigt schnell ----------
// ---------- Encoder: verlustfrei + sanfte Beschleunigung ----------
{
  //static int32_t lastEnc = 0;
  static uint32_t lastMs = 0;
  static float emaVel = 0.0f;       // counts/s (geglättet)
  static float accSpeed = 0.0f;     // Akkumulator für SPEED/POSITION Schritte (float)
  //static int lastSign = 0;

  const int   DETENT = 4;           // Counts pro Raster beim Picker
  const float EMA_A  = 0.18f;       // Glättung der Geschwindigkeit
  const float V0     = 15.0f;       // bis hier keine Beschleunigung
  const float KGAIN  = 0.012f;      // Steigung für den Multiplikator jenseits V0
  const float MULT_MAX = 10.0f;     // maximale Beschleunigung
  const int   MAX_STEPS_PER_FRAME = 60; // Sicherheitscap (Rest bleibt im Accu)
  const uint32_t MIN_DT_MS = 1;     // dt-Schutz

int32_t dRaw = takeEncoderDelta();   // <- vom 1 kHz Sampler (verlustfrei)
if (dRaw != 0) {
  uint32_t now = millis();
  uint32_t dt  = (lastMs==0) ? 16 : (now - lastMs);
  if (dt < MIN_DT_MS) dt = MIN_DT_MS;
  lastMs = now;
    // Geschwindigkeit in counts/s
    float vInst = (abs(dRaw) * 1000.0f) / (float)dt;
    emaVel = (1.0f - EMA_A) * emaVel + EMA_A * vInst;

    // kontinuierlicher Multiplikator: 1 + K * max(0, vel - V0)
    float mult = 1.0f + KGAIN * std::max(0.0f, emaVel - V0);
    if (mult > MULT_MAX) mult = MULT_MAX;

    //lastMs  = now;
    //lastEnc = enc;

    // ---- Pattern-Picker: in Detents, verlustfrei mit eigenem Accu ----
    if (g_showPatternPicker) {
      static int pickAcc = 0;              // akkumuliert rohe Counts
      pickAcc += dRaw;

      int step = 0;
      while (pickAcc >= DETENT)   { step++; pickAcc -= DETENT; }
      while (pickAcc <= -DETENT)  { step--; pickAcc += DETENT; }

      if (step != 0) {
        int ni = clampi(g_patternIndex + step, 0, (int)g_patterns.size()-1);
        if (ni != g_patternIndex) {
          g_patternIndex = ni;

          // Auto-Scroll in Sichtbereich
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
      return; // Picker hat Vorrang
    }

    // ---- Speed/Position: verlustfrei über Float-Accumulator ----
    //int sign = (dRaw>0) - (dRaw<0);
    //lastSign = sign;

    // Zieländerung in "Wert-Schritten": rohe Counts * mult
    float wantDelta = (float)dRaw * mult;
    accSpeed += wantDelta;

    // Ganze Schritte extrahieren, Rest bleibt im Accu (kein Skip!)
    int steps = (accSpeed > 0) ? (int)floorf(accSpeed) : (int)ceilf(accSpeed);
    // pro Frame kappen, Rest verbleibt
    if (steps >  MAX_STEPS_PER_FRAME) steps =  MAX_STEPS_PER_FRAME;
    if (steps < -MAX_STEPS_PER_FRAME) steps = -MAX_STEPS_PER_FRAME;
    accSpeed -= (float)steps;

    if (steps != 0) {
      if (g_mode == Mode::SPEED) {
        int ns = clampi(g_speed + steps, 0, 100);
        if (ns != g_speed) {
          g_speed = ns; needsRedraw = true;
          if (ble_is_connected()) bleSendSpeed(g_speed);
        }
      } else { // POSITION
        int np = clampi(g_position + steps, 0, 100);
        if (np != g_position) {
          g_position = np; needsRedraw = true;
          if (ble_is_connected()) bleSendMove(g_position, g_moveTime, true);
        }
      }
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
