#include "ble.h"
#include <NimBLEDevice.h>
#include <vector>

// ---------- Konfiguration ----------
static const char* kNameNeedle = "OSSM";
// Aus deinem Scan-Log:
static const NimBLEUUID kSvcOSSM("e5560000-6a2d-436f-a43d-82eab88dcefd");
static const NimBLEUUID kCtrlChar("e5560001-6a2d-436f-a43d-82eab88dcefd");
// ---------- interner State ----------
enum class BleState { Idle, Scanning, Connecting, Connected, Backoff };
static BleState    s_state      = BleState::Idle;
static bool        s_inited     = false;
static bool        s_scanRun    = false;
static String      s_peerAddr   = "";
static uint32_t    s_nextActionMs = 0;

// Treffer aus Scan-Callback
static volatile bool   s_hitPending = false;
static String          s_hitAddrStr = "";

// Verbindung + Chars
static NimBLEClient*               s_client     = nullptr;
static NimBLERemoteCharacteristic* s_charWrite  = nullptr;
static NimBLERemoteCharacteristic* s_charNotify = nullptr;

// ---------- Forward ----------
static void startPassiveScanForever();
static void stopScan();
static void goState(BleState st, uint32_t delayMs = 0);

// ---------- Client-Callbacks: bei Disconnect wieder scannen ----------
class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* c) {
    Serial.println("[BLE] disconnected");
    s_charWrite = nullptr;
    s_charNotify = nullptr;
    s_client = nullptr;
    s_peerAddr = "";
    goState(BleState::Backoff, 300);  // kurzer Backoff, dann Scan neu
  }
} s_clientCB;

// ---------- Scan-Callback: PASSIV, endlos, früh abbrechen bei Hit ----------
class LiveScanCB : public NimBLEScanCallbacks {
 public:
  void onResult(NimBLEAdvertisedDevice* d) { handle(d); }
  void onResult(const NimBLEAdvertisedDevice* d)    { handle(const_cast<NimBLEAdvertisedDevice*>(d)); }
 private:
  static void handle(NimBLEAdvertisedDevice* d) {
    if (!d) return;

    // Log (kompakt)
    const std::string& n = d->getName();
    Serial.print("[ADV] RSSI="); Serial.print(d->getRSSI());
    Serial.print(" Addr=");      Serial.print(d->getAddress().toString().c_str());
    Serial.print(" Name=");      Serial.println(n.empty() ? "<none>" : n.c_str());

    // Prüfen: Name oder Service-UUID passt?
    bool nameHit = false, uuidHit = false;

    if (!n.empty() && n.find(kNameNeedle) != std::string::npos) nameHit = true;

    if (d->haveServiceUUID()) {
      for (int u = 0; u < d->getServiceUUIDCount(); ++u) {
        if (d->getServiceUUID(u).equals(NimBLEUUID(kSvcOSSM))) { uuidHit = true; break; }
      }
    }

    if ((nameHit || uuidHit) && !s_hitPending) {
      s_hitAddrStr = d->getAddress().toString().c_str();
      s_hitPending = true;
      Serial.printf("[HIT] OSSM @ %s via %s\n", s_hitAddrStr.c_str(),
                    nameHit && uuidHit ? "name+uuid" : (nameHit ? "name" : "uuid"));
    }
  }
} s_scanCB;

// ---------- Utilities ----------
static void goState(BleState st, uint32_t delayMs) {
  s_state = st;
  s_nextActionMs = millis() + delayMs;
}

static bool due() {
  return (int32_t)(millis() - s_nextActionMs) >= 0;
}

static void startPassiveScanForever() {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (!s) return;

  if (s->isScanning()) s->stop();
  s->clearResults();
  s->setScanCallbacks(&s_scanCB, /*wantDuplicates=*/false);
  s->setActiveScan(false);        // PASSIVE only
  s->setDuplicateFilter(false);   // nichts wegfiltern
  s->setMaxResults(0);
  // volle Lauscherzeit (<= interval!)
  s->setInterval(160);  // ~100 ms
  s->setWindow(160);    // volle Zeit

  // 0 Sekunden = endlos (non-blocking)
  if (!s->start(0, /*is_continue=*/true)) {
    Serial.println("[BLE] scan start failed");
    s_scanRun = false;
    return;
  }
  s_scanRun = true;
  Serial.println("[BLE] PASSIVE scan started (forever)");
}

static void stopScan() {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (!s) return;
  if (s->isScanning()) {
    s->stop();
    Serial.println("[BLE] scan stopped");
  }
  s->clearResults();
  s_scanRun = false;
}

// ---------- Public API ----------
void ble_init() {
  if (s_inited) return;
  NimBLEDevice::init("M5Dial");
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);   // stabil fürs Scannen
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  s_inited = true;
  Serial.println("[BLE] init done");
}

void ble_auto_start() {
  if (!s_inited) ble_init();
  s_hitPending = false;
  s_hitAddrStr = "";
  startPassiveScanForever();
  goState(BleState::Scanning);
}

bool ble_is_connected() { return s_state == BleState::Connected; }
bool ble_is_scanning()  { return s_state == BleState::Scanning && s_scanRun; }
const char* ble_peer_addr() { return s_peerAddr.c_str(); }

// ---------- Connect-Flow ----------
static bool connectToAddr(const String& addrStr) {
    NimBLEAddress addr(std::string(addrStr.c_str()), BLE_ADDR_PUBLIC); 

  s_client = NimBLEDevice::createClient();
  if (!s_client) { Serial.println("[BLE] createClient failed"); return false; }
  s_client->setClientCallbacks(&s_clientCB);

  Serial.printf("[BLE] connecting to %s ...\n", addrStr.c_str());
  if (!s_client->connect(addr)) {
    Serial.println("[BLE] connect failed");
    s_client = nullptr;
    return false;
  }
  Serial.println("[BLE] connected");

  s_peerAddr = addrStr;

    // 1) Versuche: gezielt Service + Control-Char
  s_charWrite  = nullptr;
  s_charNotify = nullptr;

  // Optional: bevorzugten Service suchen, sonst alles durchsuchen
  NimBLERemoteService* svc = s_client->getService(NimBLEUUID(kSvcOSSM));
  if (svc) {
    NimBLERemoteCharacteristic* ctrl = svc->getCharacteristic(kCtrlChar);
  if (ctrl) {
    // Diese Char ist für COMMANDS
    s_charWrite = ctrl;

    // Wenn dieselbe Char auch notifyt, abonnieren
    if (ctrl->canNotify()) {
      if (ctrl->subscribe(true,
        [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool){
          Serial.print("[NTFY] ");
          for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
          Serial.println();
        })) {
        s_charNotify = ctrl;
        Serial.println("[BLE] subscribed to CONTROL characteristic notifications");
      }
    }
  }
}

  if (s_charNotify && s_charNotify->canNotify()) {
    bool ok = s_charNotify->subscribe(true,
      [](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool){
        Serial.print("[NTFY] ");
        for (size_t i=0;i<len;i++) Serial.print((char)data[i]);
        Serial.println();
      }
    );
    if (ok) Serial.printf("[BLE] subscribed to %s\n", s_charNotify->getUUID().toString().c_str());
  }

  if (s_charWrite) {
    Serial.printf("[BLE] write char: %s\n", s_charWrite->getUUID().toString().c_str());
    // TODO: Hier später deine JSON-Kommandos benutzen.
  } else {
    Serial.println("[BLE] no writable characteristic found (noch ok fürs Erste)");
  }

  return true;
}

// clamp helper (wie gehabt)
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// interner, sicherer Sender (No-Response only) – nutzt deine aktuelle Pipe
static bool send_text_noresp(const char* s) {
  if (!s_client || !s_client->isConnected() || !s_charWrite) {
    Serial.println("[BLE] send skipped: not ready");
    return false;
  }
  if (!s_charWrite->canWriteNoResponse()) {
    Serial.println("[BLE] send skipped: char has NO write-without-response (would block)");
    return false;
  }
  size_t len = strlen(s);
  bool ok = s_charWrite->writeValue((uint8_t*)s, len, /*response=*/false);
  if (!ok) Serial.println("[BLE] write(noRsp) failed");
  return ok;
}

// interner, robuster Sender: bevorzugt No-Response, fällt aber auf With-Response zurück
static bool send_text_auto(const char* s) {
  if (!s_client || !s_client->isConnected() || !s_charWrite) {
    Serial.println("[BLE] send skipped: not ready");
    return false;
  }
  size_t len = strlen(s);

  // 1) Wenn möglich: Write Without Response (schneller)
  if (s_charWrite->canWriteNoResponse()) {
    bool ok = s_charWrite->writeValue((uint8_t*)s, len, /*response=*/false);
    if (ok) return true;
    Serial.println("[BLE] write(noRsp) failed, trying with response...");
  }

  // 2) Fallback: Write With Response
  if (s_charWrite->canWrite()) {
    bool ok = s_charWrite->writeValue((uint8_t*)s, len, /*response=*/true);
    if (!ok) Serial.println("[BLE] write(withResp) failed");
    return ok;
  }

  Serial.println("[BLE] send skipped: char not writable");
  return false;
}
// ---------- Tick (in loop() aufrufen) ----------
void ble_tick() {
  if (!s_inited) return;

  switch (s_state) {
    case BleState::Idle:
      // nichts
      break;

    case BleState::Scanning:
      // auf Treffer warten; wenn da: Scan stoppen und verbinden
      if (s_hitPending) {
        s_hitPending = false;
        String target = s_hitAddrStr;
        s_hitAddrStr = "";
        stopScan();
        goState(BleState::Connecting, 50); // kleine Atempause
        // Adresse für nächsten Schritt merken:
        s_peerAddr = target;  // temporär als "Ziel"
      }
      break;

    case BleState::Connecting:
      if (!due()) break;
      if (s_peerAddr.length() == 0) {
        Serial.println("[BLE] no target addr?");
        goState(BleState::Backoff, 200);
        break;
      }
      if (connectToAddr(s_peerAddr)) {
        goState(BleState::Connected);
      } else {
        s_peerAddr = "";
        goState(BleState::Backoff, 300);
      }
      break;

    case BleState::Connected:
      // nichts; Disconnect wird via Callback gehandhabt
      break;

    case BleState::Backoff:
      if (!due()) break;
      // zurück in Scan
      s_hitPending = false;
      s_hitAddrStr = "";
      startPassiveScanForever();
      goState(BleState::Scanning);
      break;
  }
}
// --- JSON/Command API --------------------------------------------------------

bool bleSendJSON(const String& payload) {
  // optional: kurze Konsolen-Ausgabe
  //Serial.print("[BLE→OSSM] "); Serial.println(payload);
  String wire = payload + "\n";
  return send_text_auto(wire.c_str());
}

void bleSendConnected() {
  if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"connected\"}]"));
}

void bleSendHome() {
  if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"home\",\"type\":\"sensor\"}]"));
}

void bleSendDisable() {
  if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"disable\"}]"));
}

void bleSendStartStreaming() {
  if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"startStreaming\"}]"));
}

void bleSendSpeed(int v) {
  if (!ble_is_connected()) return;
  v = clampi(v, 0, 100);
  if (v == 0) {
    bleSendJSON(F("[{\"action\":\"stop\"}]"));
  } else {
    String s = String(F("[{\"action\":\"setSpeed\",\"speed\":")) + v + F("}]");
    bleSendJSON(s);
  }
}

void bleSendStroke(int v) {
  if (!ble_is_connected()) return;
  v = clampi(v, 0, 100);
  String s = String(F("[{\"action\":\"setStroke\",\"stroke\":")) + v + F("}]");
  bleSendJSON(s);
}

void bleSendDepth(int v) {
  if (!ble_is_connected()) return;
  v = clampi(v, 0, 100);
  String s = String(F("[{\"action\":\"setDepth\",\"depth\":")) + v + F("}]");
  bleSendJSON(s);
}

void bleSendMove(int pos, int ms, bool replace) {
  if (!ble_is_connected()) return;
  pos = clampi(pos, 0, 100);
  ms  = clampi(ms, 50, 2000);
  String s = String(F("[{\"action\":\"move\",\"position\":")) + pos +
             F(",\"time\":") + ms +
             F(",\"replace\":") + (replace ? F("true") : F("false")) + F("}]");
  bleSendJSON(s);
}

void bleSendSensation(int v) {
  if (!ble_is_connected()) return;
  if (v < -100) v = -100; else if (v > 100) v = 100;
  String s = String(F("[{\"action\":\"setSensation\",\"sensation\":")) + v + F("}]");
  bleSendJSON(s);
}

void bleSendPattern(int patternIndex) {
  if (!ble_is_connected()) return;
  String s = String(F("[{\"action\":\"setPattern\",\"pattern\":")) + patternIndex + F("}]");
  bleSendJSON(s);
}

void bleSendSetPhysicalTravel(int mm) {
  if (!ble_is_connected()) return;
  if (mm < 1) mm = 1;
  String s = String(F("[{\"action\":\"setPhysicalTravel\",\"travel\":")) + mm + F("}]");
  bleSendJSON(s);
}

void bleSendRetract() { if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"retract\"}]")); }
void bleSendExtend()  { if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"extend\"}]"));  }
void bleSendAirIn()   { if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"airIn\"}]"));   }
void bleSendAirOut()  { if (ble_is_connected()) bleSendJSON(F("[{\"action\":\"airOut\"}]"));  }

