#include <M5Dial.h>
#include "app_state.h"
#include "ui.h"
#include "input.h"

void setup(){
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);
  Serial.begin(115200);

  g_spr.setColorDepth(16);
  g_spr.createSprite(240,240);
  g_spr.setTextWrap(false);
  g_spr.setTextDatum(textdatum_t::middle_center);
  g_spr.setFont(&fonts::Font4);

  needsRedraw = true;
}

void loop(){
  inputUpdate();      // Buttons, Encoder, Touch, BLE-Actions auslösen
  if (needsRedraw) {  // nur neu zeichnen wenn notwendig → flackerfrei
    drawUI();
    needsRedraw = false;
  }
}
