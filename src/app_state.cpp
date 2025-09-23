#include "app_state.h"
#include "ble.h"

// Sprite an den Display-Owner binden
LGFX_Sprite g_spr(&M5Dial.Display);

// ======= State-Variablen =======
Mode g_mode = Mode::SPEED;
bool g_running = true;

int g_speed = 0;
int g_stroke = 25;
int g_depth  = 75;
int g_sensation = 0;
int g_position  = 50;
int g_moveTime  = 350;
int g_batteryPct = 72;

std::vector<String> g_patterns = {
  "Simple Stroke","Teasing or Pounding","Robo Stroke","Half'n'Half",
  "Deeper","Stop'n'Go","Insist","Jack Hammer","Stroke Nibbler"
};
int g_patternIndex = 0;

bool g_showSettings = false;
bool g_showPatternPicker = false;
int  g_pickerScroll = 0;

int32_t lastEncoder = 0;
bool needsRedraw = true;

int lastDeltaSign = 0;
uint32_t lastDeltaMs = 0;


// ======= API-Implementierung =======
void toggleMode() {
  if (g_mode == Mode::SPEED) {
    g_mode = Mode::POSITION;
    g_running = false;
    if (g_speed != 0) { g_speed = 0; bleSendSpeed(0); }
    bleSendStartStreaming();
  } else {
    g_mode = Mode::SPEED;
    g_running = true;
  }
  needsRedraw = true;
}

void openSettings(){ g_showSettings = true;  needsRedraw = true; }
void closeSettings(){ g_showSettings = false; needsRedraw = true; }
void openPicker(){ g_showPatternPicker = true; needsRedraw = true; }
void closePicker(){ g_showPatternPicker = false; needsRedraw = true; }
