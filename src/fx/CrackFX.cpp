#include "CrackFX.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"

void CrackFX::reset() {
  level_ = 0;
  generated_ = false;
  lastTime_ = millis();
}

void CrackFX::generate(int originX, int originY, int spreadX, int spreadY) {
  for (int c = 0; c < MAX_CRACKS; c++) {
    int sx = originX + getRnd(-spreadX, spreadX + 1);
    int sy = originY + getRnd(-spreadY, spreadY + 1);
    crackX_[c][0] = sx; crackY_[c][0] = sy;
    for (int p = 1; p < CRACK_PTS; p++) {
      sx += getRnd(-spreadX / 2, spreadX / 2 + 1);
      sy += getRnd(-spreadY / 2, spreadY / 2 + 1);
      crackX_[c][p] = sx; crackY_[c][p] = sy;
    }
  }
}

void CrackFX::update(uint32_t now, bool growing, float growRatePerSec, float healRatePerSec,
                      int originX, int originY, int spreadX, int spreadY) {
  float dt = (now - lastTime_) / 1000.0f;
  lastTime_ = now;
  if (dt <= 0 || dt > 1.0f) return; // защита от скачков millis() / первого вызова / guard against millis() jumps and the first call

  if (growing) {
    if (!generated_) {
      generate(originX, originY, spreadX, spreadY);
      generated_ = true;
    }
    level_ += growRatePerSec * dt;
  } else {
    level_ -= healRatePerSec * dt;
  }

  level_ = constrain(level_, 0.0f, (float)MAX_CRACKS);
  if (level_ <= 0.001f) {
    level_ = 0;
    generated_ = false; // при полном заживлении — при новом росте узор перегенерится / fully healed: a new grow regenerates the pattern
  }
}

void CrackFX::plot(int x, int y, const PixelTest& bgTest) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
  bool lit = bgTest(x, y);
  display.setDrawColor(lit ? 0 : 1); // рисуем инверсией фона — видно на любом цвете / inverted background: visible on any colour
  display.drawPixel(x, y);
}

void CrackFX::drawLine(int x0, int y0, int x1, int y1, const PixelTest& bgTest) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  while (true) {
    plot(x0, y0, bgTest);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void CrackFX::drawPartial(int idx, float progress, const PixelTest& bgTest) {
  if (progress <= 0.0f) return;
  float segFloat = progress * (CRACK_PTS - 1);
  int fullSegs = (int)segFloat;
  float frac = segFloat - fullSegs;
  for (int p = 0; p < fullSegs && p < CRACK_PTS - 1; p++) {
    drawLine(crackX_[idx][p], crackY_[idx][p], crackX_[idx][p + 1], crackY_[idx][p + 1], bgTest);
  }
  if (fullSegs < CRACK_PTS - 1 && frac > 0.0f) {
    int x0 = crackX_[idx][fullSegs], y0 = crackY_[idx][fullSegs];
    int x1 = crackX_[idx][fullSegs + 1], y1 = crackY_[idx][fullSegs + 1];
    int mx = x0 + (int)((x1 - x0) * frac);
    int my = y0 + (int)((y1 - y0) * frac);
    drawLine(x0, y0, mx, my, bgTest);
  }
}

void CrackFX::draw(const PixelTest& bgTest) {
  if (level_ <= 0.0f) return;
  int full = (int)level_;
  float frac = level_ - full;
  for (int c = 0; c < full && c < MAX_CRACKS; c++) {
    drawPartial(c, 1.0f, bgTest);
  }
  if (full < MAX_CRACKS) {
    drawPartial(full, frac, bgTest);
  }
}
