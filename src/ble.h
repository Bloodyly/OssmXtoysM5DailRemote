#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// Public API
void bleInit();
bool bleConnectAuto();     // scan + connect to device named "OSSM"
void bleDisconnect();
bool bleIsConnected();
String blePeerName();

void bleSendJSON(const String& payload);

// Convenience wrappers matching the OSSM README commands
void bleSendConnected();
void bleSendHome();                  // sensorless
void bleSendDisable();
void bleSendStartStreaming();
void bleSendSpeed(int v01_100);
void bleSendStroke(int v01_100);
void bleSendDepth(int v01_100);
void bleSendMove(int pos01_100, int ms, bool replace);
void bleSendRetract();
void bleSendExtend();
void bleSendAirIn();
void bleSendAirOut();

inline void sendSpeed(int v)            { bleSendSpeed(v); }
inline void sendStartStreaming()        { bleSendStartStreaming(); }