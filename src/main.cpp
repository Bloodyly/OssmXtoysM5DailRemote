#include <M5Dial.h>
#include "app_state.h"
#include "ui.h"
#include "input.h"
#include "ble.h"

#define SERIAL_PORT_MONITOR true
void setup(){
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);
  Serial.begin(115200);
  Serial.println("Serial Started, initialising hardware"); 
  startEncoderSampler();       // … dann Sampler starten

  g_spr.setColorDepth(16);
  g_spr.createSprite(240,240);
  g_spr.setTextWrap(false);
  g_spr.setTextDatum(textdatum_t::middle_center);
  g_spr.setFont(&fonts::Font4);
  delay(300);
  ble_init();
  ble_auto_start();
  initUI();
}

void loop(){
  //M5Dial.update();
  ble_tick();
  inputUpdate();      // Buttons, Encoder, Touch, BLE-Actions auslösen
  drawUI();
}
