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
 ble_scan_live(20);   // 20 s Live-Scan
if (ble_find_and_connect_ossm(10)) {
  Serial.println("[APP] ready – verbunden mit OSSM");
} else {
  Serial.println("[APP] kein OSSM erreichbar");
}
  //bleConnectAuto();     // scan + connect to device named "OSSM"
  needsRedraw = true;
}

void loop(){
  //M5Dial.update();
/*
  inputUpdate();      // Buttons, Encoder, Touch, BLE-Actions auslösen
  if (needsRedraw) {  // nur neu zeichnen wenn notwendig → flackerfrei
    drawUI();
    needsRedraw = false;
  }
  */  
}
