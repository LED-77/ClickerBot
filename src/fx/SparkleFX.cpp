#include "SparkleFX.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"
#include <math.h>

void SparkleFX::spawn(int originX, int originY, uint8_t count, float minSpeed, float maxSpeed) {
  if (count > MAX_PARTICLES) count = MAX_PARTICLES;
  uint32_t now = millis();

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (i >= count) { particles_[i].alive = false; continue; }

    float ang = getRnd(0, 360) * PI / 180.0f;
    int spdLo = (int)(minSpeed * 10);
    int spdHi = (int)(maxSpeed * 10);
    float spd = getRnd(spdLo, spdHi + 1) / 10.0f;

    particles_[i].x = originX;
    particles_[i].y = originY;
    particles_[i].vx = cosf(ang) * spd;
    particles_[i].vy = sinf(ang) * spd;
    particles_[i].blinkPeriodMs = getRnd(80, 260);
    particles_[i].bornAt = now;
    particles_[i].lifeMs = getRnd(500, 1100);
    particles_[i].alive = true;
  }
}

void SparkleFX::update() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles_[i].alive) continue;

    particles_[i].x += particles_[i].vx;
    particles_[i].y += particles_[i].vy;
    particles_[i].vx *= 0.94f; particles_[i].vy *= 0.94f; // затухание разлёта / drag
    particles_[i].vy += 0.03f; // лёгкая гравитация — частицы оседают, как конфетти / slight gravity

    uint32_t age = millis() - particles_[i].bornAt;
    if (age >= particles_[i].lifeMs ||
        particles_[i].x < 0 || particles_[i].x >= SCREEN_WIDTH ||
        particles_[i].y < 0 || particles_[i].y >= SCREEN_HEIGHT) {
      particles_[i].alive = false;
    }
  }
}

void SparkleFX::draw() {
  display.setDrawColor(1);
  uint32_t now = millis();
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles_[i].alive) continue;
    // Мигание: точка видна половину своего периода
    // Blinking: the dot is visible for half of its period
    uint32_t phase = (now - particles_[i].bornAt) % particles_[i].blinkPeriodMs;
    if (phase < particles_[i].blinkPeriodMs / 2) {
      display.drawPixel((int)particles_[i].x, (int)particles_[i].y);
    }
  }
}

bool SparkleFX::isDone() const {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles_[i].alive) return false;
  }
  return true;
}
