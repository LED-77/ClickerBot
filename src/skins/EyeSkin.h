#pragma once
#include "Skin.h"
#include "../fx/ClickIntensity.h"
#include "../fx/CrackFX.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

class EyeSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void draw() override;

  void onIdleEnter() override;
  void drawIdle() override;
  void onWake(uint32_t now) override;
  bool canIdle() const override;

  const char* name() const override { return "Eye"; }
  const char* counterKey() const override { return "cntEye"; }
  const char* wireKey() const override { return "eye"; }

private:
  // Сон ведёт общий SleepManager — здесь только состояния активной жизни глаза
  // Idle is handled by the shared SleepManager; only active-life states live here
  enum Mode { ACTIVE, WAKE, HYSTERIA, SHAKING, EXPLODING, RESPAWN };

  int cX_, cY_;
  int pX_, pY_;
  int tX_, tY_;
  int tR_;
  float curR_ = 0;
  float curAmp_ = 0;
  float idleAmp_ = 0; // отдельная амплитуда для анимации закрытия глаза в простое

  bool blinking_ = false;
  uint32_t blStart_ = 0, blDur_ = 0, blNext_ = 0;
  bool squinting_ = false;
  uint32_t sqEnd_ = 0, sqNext_ = 0;

  Mode mode_ = WAKE;
  uint8_t wBlinks_ = 0;
  uint32_t wTimer_ = 0;

  int shX_ = 0, shY_ = 0;
  uint32_t mvNext_ = 0, frLast_ = 0;

  uint32_t shakeStart_ = 0;
  uint32_t explodeStart_ = 0;

  uint32_t now_ = 0; // кэш времени из update() — draw() своего 'now' не получает

  ClickIntensity intensity_;
  CrackFX cracks_;
  SparkleFX sparkles_;
  PopCounter counter_;

  void resetTarget();
  void startBlink();
  void startSquint();
  void triggerWake(uint32_t now);

  bool eyeBgLit(int x, int y, int pr, int px, int py, int amp) const;
  void drawClipped(int px, int py, int pr, int amp);
};
