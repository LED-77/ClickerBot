#pragma once
#include <Arduino.h>

// Переводит темп нажатий в уровень интенсивности 0..100: частишь — растёт,
// кликаешь редко — падает. Общий для любых скинов (глаз, надувание шарика).
// Turns the clicking rate into an intensity level 0..100: click fast and it
// rises, click rarely and it falls. Shared by all skins (eye, inflating bubble).
class ClickIntensity {
public:
  void reset();

  void onClick(uint32_t now); // на каждый клик (justPressed)
  void tick(uint32_t now);    // каждый кадр: затухание, если кликов нет

  float value() const { return level_; }
  void setValue(float v) { level_ = constrain(v, 0.0f, 100.0f); }

  // Параметры поведения — можно переопределить под конкретный скин
  // Behaviour parameters — override them per skin
  uint32_t slowMs = 1000;            // клик реже этого — считается "медленным"
  uint32_t fastMs = 150;             // почти предельная скорость клика
  float incAtFast = 35.0f;           // прирост при клике на грани fastMs
  float incAtSlow = 8.0f;            // прирост при клике на грани slowMs
  float slowClickPenalty = 20.0f;    // насколько один медленный клик снижает уровень
  float passiveDecayPerSec = 15.0f;  // затухание в секунду, если кликов вообще нет

private:
  float level_ = 0;
  uint32_t lastClickTime_ = 0;
  uint32_t lastTickTime_ = 0;
};
