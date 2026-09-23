#include "Button.h"

Button::Button(uint8_t pin, bool activeLow, uint32_t debounceMs, uint32_t longPressMs)
  : pin_(pin), activeLow_(activeLow), debounceMs_(debounceMs), longPressMs_(longPressMs) {}

bool Button::readRaw() const {
  int v = digitalRead(pin_);
  return activeLow_ ? (v == LOW) : (v == HIGH);
}

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
  rawState_ = stableState_ = readRaw();
  lastChangeTime_ = millis();
}

void Button::poll(uint32_t now) {
  justPressed_ = false;
  justReleased_ = false;
  longPressFired_ = false;

  bool raw = readRaw();
  if (raw != rawState_) {
    rawState_ = raw;
    lastChangeTime_ = now;
  }

  // Принимаем новое состояние только если оно продержалось дольше debounceMs_
  // Accept the new state only if it held longer than debounceMs_
  if ((now - lastChangeTime_) >= debounceMs_ && rawState_ != stableState_) {
    stableState_ = rawState_;
    if (stableState_) {
      justPressed_ = true;
      pressStartTime_ = now;
      longPressConsumed_ = false;
    } else {
      justReleased_ = true;
    }
  }

  if (stableState_ && longPressMs_ > 0 && !longPressConsumed_) {
    if (now - pressStartTime_ >= longPressMs_) {
      longPressFired_ = true;
      longPressConsumed_ = true; // одно срабатывание за удержание / fire once per hold
    }
  }
}

uint32_t Button::heldMs(uint32_t now) const {
  return stableState_ ? (now - pressStartTime_) : 0;
}
