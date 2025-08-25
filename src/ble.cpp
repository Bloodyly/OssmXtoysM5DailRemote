#include "ble.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

static const char* kTargetName = "OSSM";
static const BLEUUID NUS_SERVICE("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const BLEUUID NUS_RX     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
static const BLEUUID NUS_TX     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

static NimBLEClient* g_client = nullptr;
static NimBLERemoteCharacteristic* g_charWrite = nullptr;
static NimBLERemoteCharacteristic* g_charNotify = nullptr;

bool g_connected = false;

void connectToOSSM() {
  static bool s_inited = false;
  if (g_connected) return;

  if (!s_inited) {
    NimBLEDevice::init("M5Dial-OSSM-Remote");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    s_inited = true;
  }

  // --- Scan ohne Callbacks ---
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setInterval(45);
  scan->setWindow(30);
  scan->setActiveScan(true);

  // In deiner Version liefert start(...) bool → Ergebnisse separat holen
  scan->start(5, false);
  NimBLEScanResults results = scan->getResults();

  // getDevice(i) → const NimBLEAdvertisedDevice*  ⇒ Kopie erzeugen
  NimBLEAdvertisedDevice* found = nullptr;
  for (int i = 0; i < results.getCount(); ++i) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    if (!dev) continue;
    std::string name = dev->getName();
    if (!name.empty() && name.find(kTargetName) != std::string::npos) {
      found = new NimBLEAdvertisedDevice(*dev); // Kopie, nicht der interne Pointer
      break;
    }
  }
  scan->stop();
  if (!found) return;

  // --- Connect ---
  g_client = NimBLEDevice::createClient();
  if (!g_client->connect(found)) {
    delete found;
    return;
  }
  delete found; // Kopie nicht mehr nötig

  // Bevorzugt Nordic UART Service
  NimBLERemoteService* service = g_client->getService(NUS_SERVICE);
  if (service) {
    g_charWrite  = service->getCharacteristic(NUS_RX);
    g_charNotify = service->getCharacteristic(NUS_TX);
  }

  // Fallback: erste schreibbare Characteristic im gesamten GATT
  if (!g_charWrite) {
    // In deiner Version: Rückgabewert ist std::vector<NimBLERemoteService*> (by value)
    std::vector<NimBLERemoteService*> svcs = g_client->getServices(true);
    for (auto* s : svcs) {
      if (!s) continue;
      std::vector<NimBLERemoteCharacteristic*> chs = s->getCharacteristics(true);
      for (auto* c : chs) {
        if (!c) continue;
        if (!g_charWrite  && (c->canWrite() || c->canWriteNoResponse())) g_charWrite  = c;
        if (!g_charNotify && c->canNotify())                              g_charNotify = c;
      }
      if (g_charWrite) break;
    }
  }

  if (g_charNotify && g_charNotify->canNotify()) {
    g_charNotify->subscribe(true,
      [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
        Serial.print("OSSM→ ");
        for (size_t i = 0; i < len; ++i) Serial.print((char)data[i]);
        Serial.println();
      }
    );
  }

  g_connected = (g_charWrite != nullptr);
  if (g_connected) {
    const char* payload = "[{\"action\":\"connected\"}]";
    if (g_charWrite->canWriteNoResponse())
      g_charWrite->writeValue((uint8_t*)payload, strlen(payload), false);
    else
      g_charWrite->writeValue((uint8_t*)payload, strlen(payload), true);
    Serial.println(payload);
  }
}

void disconnectOSSM() {
  if (g_client && g_client->isConnected()) g_client->disconnect();
  g_client = nullptr;
  g_charWrite = nullptr;
  g_charNotify = nullptr;
  g_connected = false;
}

void sendJSON(const String& payload) {
  if (!g_connected || !g_charWrite) { Serial.println(payload); return; }
  if (g_charWrite->canWriteNoResponse())
    g_charWrite->writeValue((uint8_t*)payload.c_str(), payload.length(), false);
  else
    g_charWrite->writeValue((uint8_t*)payload.c_str(), payload.length(), true);
  Serial.println(payload);
}

void sendConnected(){ sendJSON(F("[{\"action\":\"connected\"}]")); }
void sendHome()     { sendJSON(F("[{\"action\":\"home\",\"type\":\"sensorless\"}]")); }
void sendDisable()  { sendJSON(F("[{\"action\":\"disable\"}]")); }

void sendSpeed(int v){
  v = constrain(v, 0, 100);
  if (v == 0) sendJSON(F("[{\"action\":\"stop\"}]"));
  else {
    String s = String("[{\"action\":\"setSpeed\",\"speed\":") + v + "}]";
    sendJSON(s);
  }
}
void sendStroke(int v){ v=constrain(v,0,100); String s=String("[{\"action\":\"setStroke\",\"stroke\":")+v+"}]"; sendJSON(s); }
void sendDepth (int v){ v=constrain(v,0,100); String s=String("[{\"action\":\"setDepth\",\"depth\":")+v+"}]";   sendJSON(s); }

void sendStartStreaming(){ sendJSON(F("[{\"action\":\"startStreaming\"}]")); }
void sendMove(int pos,int ms,bool replace){
  pos = constrain(pos,0,100);
  ms  = constrain(ms,50,2000);
  String s = String("[{\"action\":\"move\",\"position\":") + pos +
             ",\"time\":" + ms + ",\"replace\":" + (replace?"true":"false") + "}]";
  sendJSON(s);
}

void sendRetract(){ sendJSON(F("[{\"action\":\"retract\"}]")); }
void sendExtend() { sendJSON(F("[{\"action\":\"extend\"}]")); }
void sendAirIn()  { sendJSON(F("[{\"action\":\"airIn\"}]")); }
void sendAirOut() { sendJSON(F("[{\"action\":\"airOut\"}]")); }
