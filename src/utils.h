#pragma once
#include <math.h>
#include "geometry.h"  // liefert CX, CY

// --- clamps ---
inline int   clampi(int v, int lo, int hi)      { return v<lo?lo:(v>hi?hi:v); }
inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }

// --- Mapping helpers ---
inline float map01(float v, float a, float b)    { return a + (b - a) * v; }
inline float invMap01(float v, float a, float b) { return (v - a) / (b - a); }
inline float lerp(float a, float b, float t)     { return a + (b - a) * t; }

// --- Winkel/Geometrie ---
inline float wrap180(float a) {
  while (a <= -180.0f) a += 360.0f;
  while (a >   180.0f) a -= 360.0f;
  return a;
}

// Rohwinkel (0° = rechts, 90° = oben) um Displayzentrum
inline float rawAngleDeg(int x, int y) {
  return atan2f((float)(y - CY), (float)(x - CX)) * 180.0f / M_PI;
}

// OBERER Halbring: Nullpunkt = unten (270°) -> [-90..+90]
inline float relTopDeg(int x, int y)    { return wrap180(rawAngleDeg(x,y) - 270.0f); }
// UNTERER Bogen: Nullpunkt = oben (90°) -> [-75..+75]
inline float relBottomDeg(int x, int y) { return wrap180(rawAngleDeg(x,y) -  90.0f); }

inline float normAngle(float a){ if(a<0) a+=360.0f; return a; }

// Punkt in Ring (Annulus)
inline bool inAnnulus(int x,int y,int r_in,int r_out){
  int dx = x - CX, dy = y - CY;
  int r2 = dx*dx + dy*dy;
  return (r2 >= r_in*r_in) && (r2 <= r_out*r_out);
}
