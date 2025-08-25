#include "utils.h"
#include "app_state.h"
#include "geometry.h"
#include <math.h>

float map01(float v, float a, float b){ return a + (b-a) * v; }
float invMap01(float v, float a, float b){ return (v - a) / (b - a); }

float normAngle(float a){ while(a<0) a+=360.0f; while(a>=360.0f) a-=360.0f; return a; }

bool angleBetween(float a,float a0,float a1){
  a=normAngle(a); a0=normAngle(a0); a1=normAngle(a1);
  if (a0<=a1) return a>=a0 && a<=a1;
  return (a>=a0 || a<=a1);
}

float clampAngleToArc(float ang,float a0,float a1){
  ang=normAngle(ang); a0=normAngle(a0); a1=normAngle(a1);
  if (angleBetween(ang,a0,a1)) return ang;
  auto dist=[&](float from,float to){ float d=fabsf(normAngle(to-from)); return d>180.0f? 360.0f-d:d; };
  float d0=dist(ang,a0), d1=dist(ang,a1); return (d0<d1)? a0:a1;
}

void drawArcBandAA(int cx,int cy,int r_in,int r_out,float a0,float a1,uint32_t color){
  int steps = max(12, (int)ceilf(fabs(a1 - a0) / 3.0f));
  for (int i=0;i<steps;i++) {
    float t0 = lerp(a0,a1,(float)i/steps);
    float t1 = lerp(a0,a1,(float)(i+1)/steps);
    float r0 = t0 * M_PI / 180.0f;
    float r1 = t1 * M_PI / 180.0f;
    int x0i = cx + (int)roundf(r_in * cosf(r0));
    int y0i = cy + (int)roundf(r_in * sinf(r0));
    int x0o = cx + (int)roundf(r_out* cosf(r0));
    int y0o = cy + (int)roundf(r_out* sinf(r0));
    int x1o = cx + (int)roundf(r_out* cosf(r1));
    int y1o = cy + (int)roundf(r_out* sinf(r1));
    int x1i = cx + (int)roundf(r_in * cosf(r1));
    int y1i = cy + (int)roundf(r_in * sinf(r1));
    g_spr.fillTriangle(x0i,y0i,x0o,y0o,x1o,y1o,color);
    g_spr.fillTriangle(x0i,y0i,x1o,y1o,x1i,y1i,color);
  }
}

void drawHandleR(int cx,int cy,int r_mid,float ang,uint32_t outline,int rr){
  float rad = ang * M_PI / 180.0f;
  int x = cx + (int)roundf(r_mid * cosf(rad));
  int y = cy + (int)roundf(r_mid * sinf(rad));
  g_spr.drawCircle(x,y,rr,outline);
}

bool hitBandPad(int x,int y,int r_in,int r_out,float a0,float a1){
  int dx=x-CX, dy=y-CY; int r2=dx*dx+dy*dy;
  int rin = max(0, r_in - HIT_PAD);
  int rout = r_out + HIT_PAD;
  if (r2<rin*rin||r2>rout*rout) return false;
  float ang=normAngle(pointAngle(x,y));
  return angleBetween(ang,a0,a1);
}

float pointAngle(int x,int y){
  return atan2f((float)(y-CY),(float)(x-CX))*180.0f/M_PI;
}
