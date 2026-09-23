#pragma once
#include <Arduino.h>

// Тайминги простоя. После TO_SLEEP_MS без активности wantsIdle() даёт true —
// main.cpp переключает скин на drawIdle(). Как только минимальная картинка
// реально показана (onIdleShown), стартует второй отсчёт: ещё TO_DEEP_MS —
// и tick() уводит контроллер в deep sleep.
// Idle timing. After TO_SLEEP_MS without activity wantsIdle() returns true and
// main.cpp switches the skin to drawIdle(). Once the minimal frame is actually on
// screen (onIdleShown) a second countdown starts: another TO_DEEP_MS and tick()
// puts the controller into deep sleep.
class SleepManager {
public:
  void reset(uint32_t now);           // любая активность — оба таймера с нуля / any activity resets both timers
  bool wantsIdle(uint32_t now) const; // пора показать drawIdle() / time to show drawIdle()
  void onIdleShown(uint32_t now);     // минимальная картинка показана, один раз / minimal frame shown
  void tick(uint32_t now);            // каждый кадр — deep sleep, если пора / every frame: sleep when due

private:
  uint32_t lastActivity_ = 0;
  uint32_t idleShownSince_ = 0;
  bool idleShown_ = false;
};
