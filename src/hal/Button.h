#pragma once
#include <Arduino.h>

// Универсальная кнопка с дебаунсом по времени и опциональным long-press.
// Используется и для штатной кликательной кнопки, и для кнопки меню —
// вся логика антидребезга в одном месте, не дублируется по скинам.
// Generic button with time-based debounce and optional long-press. Used for both
// the click button and the menu button: all debounce logic lives in one place.
class Button {
public:
  // longPressMs = 0 отключает распознавание long-press / 0 disables long-press
  Button(uint8_t pin, bool activeLow, uint32_t debounceMs, uint32_t longPressMs);

  void begin();
  // Вызывать каждый кадр (loop). now — millis() / call every frame from loop()
  void poll(uint32_t now);

  bool isPressed() const { return stableState_; }
  bool justPressed() const { return justPressed_; }
  bool justReleased() const { return justReleased_; }

  // true один раз за удержание — в момент пересечения порога longPressMs
  // true once per hold, at the moment the longPressMs threshold is crossed
  bool longPressFired() const { return longPressFired_; }

  uint32_t heldMs(uint32_t now) const;

private:
  bool readRaw() const;

  uint8_t pin_;
  bool activeLow_;
  uint32_t debounceMs_;
  uint32_t longPressMs_;

  bool rawState_ = false;
  bool stableState_ = false;
  uint32_t lastChangeTime_ = 0;
  uint32_t pressStartTime_ = 0;

  bool justPressed_ = false;
  bool justReleased_ = false;
  bool longPressFired_ = false;
  bool longPressConsumed_ = false; // одно срабатывание за удержание / fire once per hold
};
