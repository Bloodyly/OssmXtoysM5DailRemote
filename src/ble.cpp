#include "ble.h"
#include <NimBLEDevice.h>
#include <vector>

static NimBLEClient*               s_client     = nullptr;
static NimBLERemoteCharacteristic* s_charWrite  = nullptr;
static NimBLERemoteCharacteristic* s_charNotify = nullptr;

// OSSM-Identifikation
static const char*  kNameNeedle = "OSSM";
static const char*  kSvcOSSM    = "e5560000-6a2d-436f-a43d-82eab88dcefd"; // aus deinem Scan

void ble_disconnect() {
  if (s_client && s_client->isConnected()) s_client->disconnect();
  s_client = nullptr; s_charWrite = nullptr; s_charNotify = nullptr;
  Serial.println("[BLE] disconnected");
}

// interner Helper: einmal scannen (active/passive konfigurierbar) und bei Treffer Adresse zurückgeben
static bool scan_once_for_ossm_addr(int seconds, bool active, NimBLEAddress& outAddr) {
  NimBLEScan* s = NimBLEDevice::getScan();
  s->stop(); s->clearResults();
  s->setActiveScan(active);
  s->setDuplicateFilter(false);  // wichtig: nichts wegfiltern
  s->setMaxResults(0);
  if (active) { s->setInterval(96);  s->setWindow(48);  }
  else        { s->setInterval(160); s->setWindow(160); }

  Serial.printf("[BLE] scan_for_ossm %s %ds ...\n", active ? "ACTIVE" : "PASSIVE", seconds);
  if (!s->start(seconds, false)) { Serial.println("[BLE] scan start failed"); return false; }

  auto res = s->getResults();
  for (int i = 0; i < res.getCount(); ++i) {
    const NimBLEAdvertisedDevice* d = res.getDevice(i);
    if (!d) continue;

    bool nameHit = false, uuidHit = false;

    // Name enthält "OSSM"?
    const std::string& n = d->getName();
    if (!n.empty() && n.find(kNameNeedle) != std::string::npos) nameHit = true;

    // Service-UUID match?
    if (d->haveServiceUUID()) {
      for (int u = 0; u < d->getServiceUUIDCount(); ++u) {
        if (d->getServiceUUID(u).equals(NimBLEUUID(kSvcOSSM))) { uuidHit = true; break; }
      }
    }

    if (nameHit || uuidHit) {
      outAddr = d->getAddress(); // **Adresse kopieren**, kein Pointer zurückgeben!
      Serial.printf("[BLE] OSSM candidate: %s  RSSI=%d  Name=%s  (%s%s)\n",
                    outAddr.toString().c_str(), d->getRSSI(),
                    n.empty() ? "<none>" : n.c_str(),
                    nameHit ? "name" : "", (nameHit && uuidHit) ? "+" : (uuidHit ? "uuid" : ""));
      s->stop();
      s->clearResults();
      return true;
    }
  }

  s->stop();
  s->clearResults();
  return false;
}

// versucht ACTIVE → PASSIVE; liefert bei Erfolg true & setzt outAddr
static bool scan_for_ossm_addr(int seconds, NimBLEAddress& outAddr) {
  if (scan_once_for_ossm_addr(seconds, /*active=*/true, outAddr))  return true;
  Serial.println("[BLE] no hit in ACTIVE; try PASSIVE …");
  if (scan_once_for_ossm_addr(seconds, /*active=*/false, outAddr)) return true;
  return false;
}

static void subscribe_if_possible(NimBLERemoteCharacteristic* c) {
  if (!c || !c->canNotify()) return;
  bool ok = c->subscribe(true, [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool){
    Serial.print("[NTFY] ");
    for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
    Serial.println();
  });
  if (ok) Serial.printf("[BLE] subscribed to %s\n", c->getUUID().toString().c_str());
}

bool ble_find_and_connect_ossm(int scan_seconds) {
  if (scan_seconds <= 0) scan_seconds = 10;

  NimBLEAddress addr;
  if (!scan_for_ossm_addr(scan_seconds, addr)) {
    Serial.println("[BLE] no OSSM found");
    return false;
  }

  Serial.printf("[BLE] connecting to %s ...\n", addr.toString().c_str());

  s_client = NimBLEDevice::createClient();
  if (!s_client) { Serial.println("[BLE] createClient failed"); return false; }

  if (!s_client->connect(addr)) {
    Serial.println("[BLE] connect failed");
    s_client = nullptr;
    return false;
  }
  Serial.println("[BLE] connected");

  // Versuch: direkt den OSSM-Service holen, sonst alle durchsuchen
  NimBLERemoteService* svc = s_client->getService(NimBLEUUID(kSvcOSSM));
  if (!svc) {
    Serial.println("[BLE] OSSM service not found directly; discovering all services...");
    auto svcs = s_client->getServices(true);
    for (auto* s : svcs) {
      if (!s) continue;
      auto chs = s->getCharacteristics(true);
      for (auto* c : chs) {
        if (!s_charWrite && (c->canWrite() || c->canWriteNoResponse())) s_charWrite = c;
        if (!s_charNotify && c->canNotify())                            s_charNotify = c;
      }
    }
  } else {
    auto chs = svc->getCharacteristics(true);
    for (auto* c : chs) {
      if (!s_charWrite && (c->canWrite() || c->canWriteNoResponse())) s_charWrite = c;
      if (!s_charNotify && c->canNotify())                            s_charNotify = c;
    }
  }

  if (s_charNotify) subscribe_if_possible(s_charNotify);

  if (s_charWrite) {
    Serial.printf("[BLE] write char: %s\n", s_charWrite->getUUID().toString().c_str());
    const char* msg = "hello\n";
    bool noRsp = s_charWrite->canWriteNoResponse();
    bool ok = s_charWrite->writeValue((uint8_t*)msg, strlen(msg), !noRsp);
    Serial.printf("[BLE] write test %s\n", ok ? "ok" : "FAILED");
  } else {
    Serial.println("[BLE] no writable characteristic found");
  }

  return true;
}


class LiveScanCB : public NimBLEScanCallbacks {
 public:
  void onResult(NimBLEAdvertisedDevice* d)  { logOne(d); }
  void onResult(const NimBLEAdvertisedDevice* d)    { logOne(const_cast<NimBLEAdvertisedDevice*>(d)); }
 private:
  static void logOne(NimBLEAdvertisedDevice* d) {
    if (!d) return;
    Serial.print("[ADV] RSSI="); Serial.print(d->getRSSI());
    Serial.print("  Addr=");     Serial.print(d->getAddress().toString().c_str());
    const std::string& n = d->getName();
    Serial.print("  Name=");     Serial.println(n.empty() ? "<none>" : n.c_str());
    if (d->haveServiceUUID()) {
      Serial.print("      UUIDs: ");
      for (int i = 0; i < d->getServiceUUIDCount(); ++i) {
        Serial.print(d->getServiceUUID(i).toString().c_str());
        if (i < d->getServiceUUIDCount()-1) Serial.print(", ");
      }
      Serial.println();
    }
  }
};

static bool       s_inited = false;
static LiveScanCB s_liveCB;

void ble_init() {
  if (s_inited) return;
  NimBLEDevice::init("scan-live");
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  s_inited = true;
  Serial.println("[BLE] init done");
}

// interner Helper: startet Scan *blocking* mit Callbacks, ohne Re-Init
static bool start_scan_blocking(int seconds, bool active) {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (!s) { Serial.println("[BLE] getScan() NULL"); return false; }

  if (s->isScanning()) { s->stop(); delay(50); }
  s->clearResults(); delay(10);

  s->setScanCallbacks(&s_liveCB, /*wantDuplicates=*/false);
  s->setActiveScan(active);
  s->setDuplicateFilter(true);
  s->setMaxResults(0);
  if (active) { s->setInterval(96);  s->setWindow(48);  }   // ~60/30 ms
  else        { s->setInterval(160); s->setWindow(160); }   // volle Lauscherzeit

  Serial.print("[BLE] "); Serial.print(active ? "ACTIVE" : "PASSIVE");
  Serial.print(" live-scan "); Serial.print(seconds); Serial.println(" s ...");

  // Non-blocking starten → selber warten → sauber stoppen
  if (!s->start(seconds, /*is_continue=*/true)) {
    Serial.println("[BLE] scan start failed");
    return false;
  }

  uint32_t tEnd = millis() + (uint32_t)seconds * 1000UL;
  while (millis() < tEnd) delay(50);

  s->stop(); delay(50);
  Serial.println("[BLE] scan stop");
  s->clearResults(); delay(10);
  return true;
}

void ble_scan_live(int seconds) {
  if (!s_inited) ble_init();
  if (seconds <= 0) seconds = 15;

  // 1) Active
  start_scan_blocking(seconds, /*active=*/true);

  // 2) Passive (ein zweiter Blick findet manchmal Geräte ohne Scan-Response)
  start_scan_blocking(seconds, /*active=*/false);
}
