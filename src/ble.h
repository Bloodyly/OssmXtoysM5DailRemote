#ifndef BLE_HELPER_H
#define BLE_HELPER_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Einmalig initialisieren (idempotent)
void ble_init();

// State-Maschine starten: endloser PASSIVE-Scan, auto-connect bei Treffer, auto-restart bei Disconnect
void ble_auto_start();

// In loop() regelmäßig aufrufen
void ble_tick();
void bleSetMaxRateHz(int hz);   // z.B. 30
void blePump();                 // im loop() aufrufen

// Status-Helpers
bool        ble_is_connected();
bool        ble_is_scanning();
const char* ble_peer_addr();   // z.B. "58:8c:81:af:6a:96" oder "" wenn unbekannt

// --- JSON/Command API --------------------------------------------------------
// Direkter JSON-Write (falls du mal freie JSON-Strings senden willst)
bool bleSendJSON(const String& payload,bool critical = false);

// Komfort-Wrapper wie in deiner ersten Version
void bleSendConnected();
void bleSendHome();
void bleSendDisable();
void bleSendStartStreaming();

void bleSendSpeed(int v);            // 0..100 (0 → stop)
void bleSendStroke(int v);           // 0..100
void bleSendDepth(int v);            // 0..100
void bleSendMove(int pos, int ms, bool replace);  // pos 0..100, ms 50..2000
void bleSendPattern(int patternIndex); 

void bleSendSensation(int v);        // -100..+100
void bleSendSetPhysicalTravel(int mm);

void bleSendRetract();
void bleSendExtend();
void bleSendAirIn();
void bleSendAirOut();

#ifdef __cplusplus
}
#endif

#endif // BLE_HELPER_H
