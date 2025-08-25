#pragma once
#include <Arduino.h>

// Math
inline int   clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }
inline float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
inline float lerp(float a,float b,float t){ return a + (b-a)*t; }
float map01(float v, float a, float b);
float invMap01(float v, float a, float b);

// Winkel-Helfer
float normAngle(float a);
bool  angleBetween(float a,float a0,float a1);
float clampAngleToArc(float ang,float a0,float a1);

// Rendering
void drawArcBandAA(int cx,int cy,int r_in,int r_out,float a0,float a1,uint32_t color);
void drawHandleR(int cx,int cy,int r_mid,float ang,uint32_t outline,int rr);

// Hit-Tests
bool hitBandPad(int x,int y,int r_in,int r_out,float a0,float a1);
float pointAngle(int x,int y);
