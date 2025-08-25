#pragma once

// Display
static const int W = 240;
static const int H = 240;
static const int CX = W / 2;
static const int CY = H / 2;

// Abstände/Größen
static const int MIN_GAP = 10;  // Stroke <-> Depth Mindestabstand
static const int HIT_PAD = 8;   // Touch-Aufweitung

// Ringe
// Speed dünn (oben)
static const int R_SPEED_IN   = 100;
static const int R_SPEED_OUT  = 112;

// Stroke/Depth dick, schließt an Speed-IN an (außen = 100)
static const int R_RANGE_OUT  = R_SPEED_IN; // 100
static const int R_RANGE_IN   = 66;

// Sens/Pos dicker Ring unten
static const int R_SENS_IN    = 92;
static const int R_SENS_OUT   = 116;

// Winkel
static const float TOP_START  = 180.0f; // links oben
static const float TOP_END    = 360.0f; // rechts oben
static const float SENS_START = 15.0f;  // unten links
static const float SENS_END   = 165.0f; // unten rechts

// Controls
static const int CTRL_SPACING = 32;
static const int BUTTONS_Y    = CY - 16; // Minus/Play/Plus oben
static const int CTRL_Y       = CY + 36; // Pattern-Pill unten
