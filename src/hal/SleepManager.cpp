#include "SleepManager.h"
#include "../Config.h"
#include "DisplayManager.h"

void SleepManager::reset(uint32_t now) {
  lastActivity_ = now;
  idleShown_ = false;
}

bool SleepManager::wantsIdle(uint32_t now) const {
  return (now - lastActivity_) >= TO_SLEEP_MS;
}

void SleepManager::onIdleShown(uint32_t now) {
  if (!idleShown_) {
    idleShown_ = true;
    idleShownSince_ = now;
  }
}

void SleepManager::tick(uint32_t now) {
  if (idleShown_ && (now - idleShownSince_) >= TO_DEEP_MS) {
    goToDeepSleep(WAKE_PIN_MASK);
  }
}
