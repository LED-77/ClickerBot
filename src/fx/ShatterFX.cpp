#include "ShatterFX.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"
#include <math.h>

void ShatterFX::spawn(int scanX0, int scanY0, int scanX1, int scanY1,
                       int originX, int originY, const PixelTest& test) {
  float sumX[NUM_FRAGMENTS]; float sumY[NUM_FRAGMENTS];
  int cnt[NUM_FRAGMENTS]; int seen[NUM_FRAGMENTS];
  for (int f = 0; f < NUM_FRAGMENTS; f++) {
    sumX[f] = 0; sumY[f] = 0; cnt[f] = 0; seen[f] = 0;
    fragments_[f].count = 0; fragments_[f].alive = true;
  }

  // Первый проход: суммы координат (для точного центроида) + резервуарная
  // выборка "сырых" точек на сектор (пока абсолютные x,y)
  // First pass: coordinate sums (for an exact centroid) plus reservoir sampling
  // of raw points per sector (still stored as absolute x,y)
  for (int y = scanY0; y < scanY1; y++) {
    for (int x = scanX0; x < scanX1; x++) {
      if (!test(x, y)) continue;
      float ang = atan2f((float)(y - originY), (float)(x - originX));
      if (ang < 0) ang += 2.0f * PI;
      int sector = (int)(ang / (2.0f * PI) * NUM_FRAGMENTS);
      if (sector >= NUM_FRAGMENTS) sector = NUM_FRAGMENTS - 1;

      sumX[sector] += x; sumY[sector] += y; cnt[sector]++;

      if (fragments_[sector].count < FRAG_MAX_PTS) {
        fragments_[sector].offX[fragments_[sector].count] = (int8_t)x; // временно абсолютные
        fragments_[sector].offY[fragments_[sector].count] = (int8_t)y;
        fragments_[sector].count++;
      } else {
        int j = getRnd(0, seen[sector] + 1);
        if (j < FRAG_MAX_PTS) {
          fragments_[sector].offX[j] = (int8_t)x;
          fragments_[sector].offY[j] = (int8_t)y;
        }
      }
      seen[sector]++;
    }
  }

  // Второй проход: переводим абсолютные координаты в смещения от центроида,
  // задаём начальную радиальную скорость и случайное вращение
  // Second pass: turn absolute coordinates into offsets from the centroid and set
  // the initial radial speed and a random spin
  for (int f = 0; f < NUM_FRAGMENTS; f++) {
    if (cnt[f] == 0) { fragments_[f].alive = false; continue; }
    float cx = sumX[f] / cnt[f];
    float cy = sumY[f] / cnt[f];
    fragments_[f].x = cx;
    fragments_[f].y = cy;

    for (int i = 0; i < fragments_[f].count; i++) {
      int rawX = fragments_[f].offX[i];
      int rawY = fragments_[f].offY[i];
      fragments_[f].offX[i] = (int8_t)constrain((int)roundf(rawX - cx), -127, 127);
      fragments_[f].offY[i] = (int8_t)constrain((int)roundf(rawY - cy), -127, 127);
    }

    float dx = cx - originX, dy = cy - originY;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { dx = 1; dy = 0; len = 1; }
    float spd = getRnd(8, 30) / 10.0f;
    fragments_[f].vx = dx / len * spd + getRnd(-5, 6) / 10.0f;
    fragments_[f].vy = dy / len * spd + getRnd(-5, 6) / 10.0f;
    fragments_[f].rot = 0;
    fragments_[f].angVel = getRnd(-30, 31) / 100.0f; // рад/кадр / rad per frame
  }
}

void ShatterFX::update() {
  for (int f = 0; f < NUM_FRAGMENTS; f++) {
    if (!fragments_[f].alive) continue;
    fragments_[f].x += fragments_[f].vx;
    fragments_[f].y += fragments_[f].vy;
    fragments_[f].vx *= 0.96f; fragments_[f].vy *= 0.96f;
    fragments_[f].rot += fragments_[f].angVel;
    fragments_[f].angVel *= 0.97f;
    if (fragments_[f].x < -15 || fragments_[f].x > SCREEN_WIDTH + 15 ||
        fragments_[f].y < -15 || fragments_[f].y > SCREEN_HEIGHT + 15) {
      fragments_[f].alive = false;
    }
  }
}

void ShatterFX::draw() {
  display.setDrawColor(1);
  for (int f = 0; f < NUM_FRAGMENTS; f++) {
    if (!fragments_[f].alive) continue;
    float c = cosf(fragments_[f].rot);
    float s = sinf(fragments_[f].rot);
    for (int i = 0; i < fragments_[f].count; i++) {
      float ox = fragments_[f].offX[i];
      float oy = fragments_[f].offY[i];
      float rx = ox * c - oy * s;
      float ry = ox * s + oy * c;
      int dpx = (int)roundf(fragments_[f].x + rx);
      int dpy = (int)roundf(fragments_[f].y + ry);
      if (dpx >= 0 && dpx < SCREEN_WIDTH && dpy >= 0 && dpy < SCREEN_HEIGHT) {
        display.drawPixel(dpx, dpy);
      }
    }
  }
}

bool ShatterFX::isDone() const {
  for (int f = 0; f < NUM_FRAGMENTS; f++) {
    if (fragments_[f].alive) return false;
  }
  return true;
}
