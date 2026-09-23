#include "ClickIntensity.h"
#include "../util/MathUtils.h"

void ClickIntensity::reset() {
  level_ = 0;
  lastClickTime_ = 0;
  lastTickTime_ = millis();
}

void ClickIntensity::onClick(uint32_t now) {
  uint32_t interval = lastClickTime_ ? (now - lastClickTime_) : slowMs;
  if (interval < slowMs) {
    uint32_t clamped = constrain(interval, fastMs, slowMs);
    float inc = mapf((float)clamped, (float)fastMs, (float)slowMs, incAtFast, incAtSlow);
    level_ += inc;
  } else {
    level_ -= slowClickPenalty;
  }
  level_ = constrain(level_, 0.0f, 100.0f);
  lastClickTime_ = now;
}

void ClickIntensity::tick(uint32_t now) {
  float dt = (now - lastTickTime_) / 1000.0f;
  lastTickTime_ = now;
  if (dt <= 0 || dt > 1.0f) return; // защита от переполнения millis() / первого вызова / guard against millis() wrap and the first call
  level_ -= passiveDecayPerSec * dt;
  if (level_ < 0) level_ = 0;
}
