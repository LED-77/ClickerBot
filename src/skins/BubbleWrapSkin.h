#pragma once
#include "Skin.h"
#include "../Config.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

// Сетка пузырьков, как на упаковочной плёнке. Каждый клик "вдавливает", а
// затем лопает следующий пузырёк по порядку (слева направо, сверху вниз) —
// с коротким искристым всплеском (SparkleFX) в момент хлопка. Когда весь
// лист лопнут — короткая пауза, затем новый целый лист.
// A grid of bubbles, like bubble wrap. Every click presses in and then bursts the
// next bubble in order (left to right, top to bottom) with a short sparkle burst
// (SparkleFX). Once the whole sheet is popped: a short pause, then a fresh sheet.
class BubbleWrapSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void draw() override;

  void onIdleEnter() override;
  void drawIdle() override;
  bool canIdle() const override;

  const char* name() const override { return "Bubbles"; }
  const char* counterKey() const override { return "cntBubbles"; }
  const char* wireKey() const override { return "bubble"; }

private:
  static constexpr uint8_t COLS  = 6;
  static constexpr uint8_t ROWS  = 3;
  static constexpr uint8_t COUNT = COLS * ROWS;

  static constexpr int MARGIN_X  = 6;
  static constexpr int MARGIN_Y  = 4;
  static constexpr int SPACING_X = (SCREEN_WIDTH  - 2 * MARGIN_X) / COLS;
  static constexpr int SPACING_Y = (SCREEN_HEIGHT - 2 * MARGIN_Y) / ROWS;

  static constexpr int BUBBLE_R = 4;  // радиус целого пузырька
  static constexpr int DIMPLE_R = 1;  // радиус "вмятины" на месте лопнутого

  static constexpr uint32_t PRESS_MS = 90;  // "вдавливание" перед хлопком
  static constexpr uint32_t PAUSE_MS = 500; // пауза после лопанья последнего пузырька

  static constexpr float IDLE_SHRINK_SPEED = 0.06f;

  bool popped_[COUNT] = {};
  uint8_t nextIndex_ = 0; // индекс следующего пузырька для лопанья

  bool pressing_ = false;
  uint32_t pressStart_ = 0;
  uint8_t pressingIndex_ = 0;
  float pressProgress_ = 0.0f; // 0..1, кэшируется в update() для отрисовки в draw()

  bool allPoppedPause_ = false;
  uint32_t allPoppedAt_ = 0;

  float idleScale_ = 1.0f;

  uint32_t now_ = 0; // кэш времени из update() — draw() своего 'now' не получает

  SparkleFX sparkles_;
  PopCounter counter_;

  int bx(uint8_t idx) const;
  int by(uint8_t idx) const;
};
