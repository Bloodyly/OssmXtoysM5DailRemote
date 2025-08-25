#pragma once
#include <NimBLEDevice.h>

// BLE-Status
extern bool g_connected;

// Init/Connect
void connectToOSSM();
void disconnectOSSM();

// JSON Kommandos
void sendJSON(const String& payload);
void sendConnected();
void sendHome();
void sendDisable();
void sendSpeed(int v);
void sendStroke(int v);
void sendDepth(int v);
void sendStartStreaming();
void sendMove(int pos,int ms,bool replace);
void sendRetract();
void sendExtend();
void sendAirIn();
void sendAirOut();
