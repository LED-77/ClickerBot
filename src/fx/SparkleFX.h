#pragma once
#include <Arduino.h>

// Всплеск мигающих частиц от точки — "конфетти"/"снежинки", в отличие от
// ShatterFX (жёсткие осколки). Частица мигает в своём ритме и живёт своё время.
// A burst of blinking particles from a point — "confetti"/"snowflakes", unlike
// ShatterFX (rigid fragments). Each particle blinks at its own rate and has its
// own lifetime.
class SparkleFX {
public:
  static constexpr uint8_t MAX_PARTICLES = 28;

  // count обрезается по MAX_PARTICLES; скорость — в диапазоне [minSpeed, maxSpeed]
  // count is clamped to MAX_PARTICLES; speed lies in [minSpeed, maxSpeed]
  void spawn(int originX, int originY, uint8_t count, float minSpeed, float maxSpeed);
  void update();
  void draw();
  bool isDone() const;

private:
  struct Particle {
    float x, y, vx, vy;
    uint32_t blinkPeriodMs;
    uint32_t bornAt;
    uint32_t lifeMs;
    bool alive;
  };
  Particle particles_[MAX_PARTICLES];
};
