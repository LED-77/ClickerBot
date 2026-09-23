#pragma once
#include "Skin.h"
#include "../Config.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

// Механический одометр (счётчик с барабанами). Три разряда: сотни, десятки,
// единицы. Каждый клик — барабан единиц проворачивается с анимацией
// прокрутки снизу вверх. При переносе (9→0) последовательно прокручивается
// следующий разряд. При 999 → сброс в 000.
// Mechanical odometer (a counter with drums). Three digits: hundreds, tens, ones.
// Each click rolls the ones drum with an upward scroll animation. On a carry (9→0)
// the next drum rolls too. At 999 it resets to 000.
class OdometerSkin : public Skin {
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

  const char* name() const override { return "Odometer"; }
  const char* counterKey() const override { return "cntOdo"; }
  const char* wireKey() const override { return "odometer"; }

private:
  static constexpr uint8_t DIGITS = 3;

  // Геометрия барабанов
  // Drum geometry
  static constexpr int DRUM_W = 14;       // ширина окна барабана
  static constexpr int DRUM_H = 24;       // высота окна барабана
  static constexpr int DRUM_GAP = 3;      // зазор между барабанами
  static constexpr int DRUM_Y = (SCREEN_HEIGHT - DRUM_H) / 2;
  static constexpr int TOTAL_W = DIGITS * DRUM_W + (DIGITS - 1) * DRUM_GAP;
  static constexpr int DRUM_X0 = (SCREEN_WIDTH - TOTAL_W) / 2;

  // Анимация прокрутки
  // Scroll animation
  static constexpr uint32_t ROLL_MS = 250;    // мс на прокрутку одного барабана
  static constexpr uint32_t EXPLODE_MS = 700; // мс взрыва при 999→0

  uint16_t value_ = 0;         // 0…999, текущее значение / current value
  uint8_t display_[DIGITS];   // отображаемые цифры (после анимации) / shown digits (after the animation)

  bool exploding_ = false;
  uint32_t explodeStart_ = 0;

  // Одновременная анимация всех изменившихся разрядов
  // All changed digits animate at the same time
  bool rolling_ = false;
  uint32_t rollStart_ = 0;
  float animOffset_;           // 0..DRUM_H — смещение крутящихся разрядов / offset of the rolling digits
  uint8_t animFrom_[DIGITS];  // старые значения (до клика) / previous values (before the click)
  uint8_t animTo_[DIGITS];    // новые значения (куда крутиться)

  SparkleFX sparkles_;
  PopCounter counter_;
  uint32_t now_ = 0;

  int drumX(uint8_t pos) const;
  void drawDrumFrame(int x) const;
  void drawDigitInDrum(int x, uint8_t digit, int yOff) const;
  void increment();
};
