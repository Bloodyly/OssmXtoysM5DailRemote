#pragma once
#include <M5Dial.h>
#include <vector>
#include <WString.h>

constexpr int kPhysicalTravelMm = 150;

// Offscreen-Buffer global
extern LGFX_Sprite g_spr;

// Betriebsarten
enum class Mode { SPEED, POSITION };

// ======= Zustände (Definition in app_state.cpp) =======
extern Mode g_mode;
extern bool g_running;

extern int g_speed;       // 0..100
extern int g_stroke;      // 0..100
extern int g_depth;       // 0..100 (>= stroke)
extern int g_sensation;   // -100..+100
extern int g_position;    // 0..100
extern int g_moveTime;    // ms
extern int g_batteryPct;  // optional

extern std::vector<String> g_patterns;
extern int g_patternIndex;

extern bool g_showSettings;
extern bool g_showPatternPicker;
extern int  g_pickerScroll;

extern int32_t lastEncoder;
extern bool needsRedraw;

// Encoder-Filter
extern int lastDeltaSign;
extern uint32_t lastDeltaMs;

// ======= API =======
void toggleMode();        // Speed <-> Position (setzt Speed=0 beim Wechsel nach Position, sendet Streaming)
void openSettings();
void closeSettings();
void openPicker();
void closePicker();
