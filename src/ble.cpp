#include "ble.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>

// --- Prefer NUS (Nordic UART) ---
static const BLEUUID UUID_NUS_SERVICE("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const BLEUUID UUID_NUS_RX     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // central writes here
static const BLEUUID UUID_NUS_TX     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // notifications

// Connection state
static bool                       s_bleInited   = false;
static bool                       s_connected   = false;
static NimBLEClient*              s_client      = nullptr;
static NimBLERemoteCharacteristic*s_charWrite   = nullptr;   // where we write JSON
static NimBLERemoteCharacteristic*s_charNotify  = nullptr;   // optional notify
static String                     s_peerName    = "";

// --- Small helpers -----------------------------------------------------------
static inline int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// Write helper
static void writeJSON(const String& s) {
  if (!s_connected || !s_charWrite) return;
  const uint8_t* data = (const uint8_t*)s.c_str();
  size_t len = s.length();
  if (s_charWrite->canWriteNoResponse()) s_charWrite->writeValue(data, len, false);
  else                                   s_charWrite->writeValue(data, len, true);
#if defined(SERIAL_PORT_MONITOR)
  Serial.print("[BLE→OSSM] "); Serial.println(s);
#endif
}

// Try to bind NUS first
static bool bindNUS() {
  NimBLERemoteService* svc = s_client->getService(UUID_NUS_SERVICE);
  if (!svc) return false;

  s_charWrite  = svc->getCharacteristic(UUID_NUS_RX);
  s_charNotify = svc->getCharacteristic(UUID_NUS_TX);

  if (s_charNotify && s_charNotify->canNotify()) {
    s_charNotify->subscribe(true,
      [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
#if defined(SERIAL_PORT_MONITOR)
        Serial.print("[OSSM→BLE] ");
        for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
        Serial.println();
#endif
      }
    );
  }
  return (s_charWrite != nullptr);
}

// Fallback: iterate advertised services, pick first writeable char
static bool bindFirstWritable(const NimBLEAdvertisedDevice* advDev) {
  // Try advertised UUIDs to limit discovery work
  if (advDev && advDev->haveServiceUUID()) {
    int count = advDev->getServiceUUIDCount();
    for (int i=0; i<count; ++i) {
      NimBLEUUID su = advDev->getServiceUUID(i);
      NimBLERemoteService* s = s_client->getService(su);
      if (!s) continue;

      std::vector<NimBLERemoteCharacteristic*> chs = s->getCharacteristics(true);
      for (auto* c : chs) {
        if (!s_charWrite && (c->canWrite() || c->canWriteNoResponse())) s_charWrite = c;
        if (!s_charNotify && c->canNotify())                            s_charNotify = c;
      }
      if (s_charNotify && s_charNotify->canNotify()) {
        s_charNotify->subscribe(true,
          [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool){
#if defined(SERIAL_PORT_MONITOR)
            Serial.print("[OSSM→BLE] ");
            for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
            Serial.println();
#endif
          }
        );
      }
      if (s_charWrite) return true;
    }
  }

  // As a last resort, discover all services & their chars
  std::vector<NimBLERemoteService*> svcs = s_client->getServices(true);
  for (auto* s : svcs) {
    if (!s) continue;
    std::vector<NimBLERemoteCharacteristic*> chs = s->getCharacteristics(true);
    for (auto* c : chs) {
      if (!s_charWrite && (c->canWrite() || c->canWriteNoResponse())) s_charWrite = c;
      if (!s_charNotify && c->canNotify())                            s_charNotify = c;
    }
    if (s_charNotify && s_charNotify->canNotify()) {
      s_charNotify->subscribe(true,
        [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool){
#if defined(SERIAL_PORT_MONITOR)
          Serial.print("[OSSM→BLE] ");
          for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
          Serial.println();
#endif
        }
      );
    }
    if (s_charWrite) return true;
  }
  return false;
}

// --- Public API --------------------------------------------------------------
void bleInit() {
  if (s_bleInited) return;
  NimBLEDevice::init("M5Dial-OSSM-Remote");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  s_bleInited = true;
}

bool bleConnectAuto() {
  if (!s_bleInited) bleInit();
  if (s_connected) return true;

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(30);

  scan->start(5, false);
  NimBLEScanResults results = scan->getResults();

  const NimBLEAdvertisedDevice* choice = nullptr;
  for (int i=0; i<results.getCount(); ++i) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    std::string n = dev->getName();
    if (!n.empty() && n.find("OSSM") != std::string::npos) { choice = dev; break; }
  }
  scan->stop();

  if (!choice) {
#if defined(SERIAL_PORT_MONITOR)
    Serial.println("[BLE] Kein Device mit Namen 'OSSM' gefunden.");
#endif
    return false;
  }

  NimBLEAddress addr = choice->getAddress();
  s_peerName = choice->getName().c_str();

  s_client = NimBLEDevice::createClient();
  if (!s_client) return false;

  if (!s_client->connect(addr)) {
#if defined(SERIAL_PORT_MONITOR)
    Serial.println("[BLE] Connect fehlgeschlagen.");
#endif
    s_client = nullptr;
    return false;
  }

  // Prefer NUS; fallback to first writeable
  s_charWrite = nullptr; s_charNotify = nullptr;
  bool ok = bindNUS();
  if (!ok) ok = bindFirstWritable(choice);

  s_connected = ok;
  if (!s_connected) {
#if defined(SERIAL_PORT_MONITOR)
    Serial.println("[BLE] Keine passende Characteristic gefunden.");
#endif
    bleDisconnect();
    return false;
  }

#if defined(SERIAL_PORT_MONITOR)
  Serial.print("[BLE] Verbunden mit "); Serial.println(s_peerName);
#endif

  // initial event
  writeJSON(F("[{\"action\":\"connected\"}]"));
  return true;
}

void bleDisconnect() {
  if (s_client && s_client->isConnected()) s_client->disconnect();
  s_client = nullptr; s_charWrite=nullptr; s_charNotify=nullptr;
  s_connected = false; s_peerName = "";
}

bool   bleIsConnected() { return s_connected; }
String blePeerName()    { return s_peerName;  }

void bleSendJSON(const String& payload) { writeJSON(payload); }

// --- Command wrappers --------------------------------------------------------
void bleSendConnected()     { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"connected\"}]")); 
  }
}
void bleSendHome()          { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"home\",\"type\":\"sensor\"}]")); 
  }
}
void bleSendDisable()       { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"disable\"}]")); 
  }
}
void bleSendStartStreaming(){ 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"startStreaming\"}]")); 
  }
}

void bleSendSpeed(int v)  {
  if (bleIsConnected()) {
    v = clampi(v,0,100);
    if (v==0) writeJSON(F("[{\"action\":\"stop\"}]"));
    else {
      String s = String(F("[{\"action\":\"setSpeed\",\"speed\":")) + v + F("}]");
      writeJSON(s);
    }
  }
}
void bleSendStroke(int v) {
  if (bleIsConnected()) {
    v = clampi(v,0,100);
    String s = String(F("[{\"action\":\"setStroke\",\"stroke\":")) + v + F("}]");
    writeJSON(s);
  }
}
void bleSendDepth(int v)  {
  if (bleIsConnected()) {
    v = clampi(v,0,100);
    String s = String(F("[{\"action\":\"setDepth\",\"depth\":")) + v + F("}]");
    writeJSON(s);
  }
}
void bleSendMove(int pos, int ms, bool replace) {
  if (bleIsConnected()) {
    pos = clampi(pos,0,100); ms = clampi(ms, 50, 2000);
    String s = String(F("[{\"action\":\"move\",\"position\":")) + pos +
              F(",\"time\":") + ms +
              F(",\"replace\":") + (replace?F("true"):F("false")) + F("}]");
    writeJSON(s);
  }
}

void bleSendSensation(int v) {
  if (bleIsConnected()) {
    // clamp -100..+100
    if (v < -100) v = -100; else if (v > 100) v = 100;
    String s = String("[{\"action\":\"setSensation\",\"value\":") + v + "}]";
    writeJSON(s);
  }
}

void bleSendSetPhysicalTravel(int mm) {
  if (bleIsConnected()) {
    if (mm < 1) mm = 1;             // simple sanity
    String s = String("[{\"action\":\"setPhysicalTravel\",\"value\":") + mm + "}]";
    writeJSON(s);
  }
}
void bleSendRetract() { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"retract\"}]")); 
  }
}
void bleSendExtend()  { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"extend\"}]"));  
  }
}
void bleSendAirIn()   { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"airIn\"}]"));
  }   
}
void bleSendAirOut()  { 
  if (bleIsConnected()) {
    writeJSON(F("[{\"action\":\"airOut\"}]"));  
  }
}