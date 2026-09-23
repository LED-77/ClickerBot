#include "BubbleWrapSkin.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"

int BubbleWrapSkin::bx(uint8_t idx) const {
  return MARGIN_X + SPACING_X / 2 + (idx % COLS) * SPACING_X;
}

int BubbleWrapSkin::by(uint8_t idx) const {
  return MARGIN_Y + SPACING_Y / 2 + (idx / COLS) * SPACING_Y;
}

void BubbleWrapSkin::onEnter() {
  for (uint8_t i = 0; i < COUNT; i++) popped_[i] = false;
  nextIndex_ = 0;
  pressing_ = false;
  pressProgress_ = 0.0f;
  allPoppedPause_ = false;
  idleScale_ = 1.0f;
  counter_.begin(counterKey());
}

void BubbleWrapSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;
  sparkles_.update(); // всплеск живёт своей жизнью независимо от состояния листа

  if (allPoppedPause_) {
    if (now - allPoppedAt_ >= PAUSE_MS) {
      for (uint8_t i = 0; i < COUNT; i++) popped_[i] = false;
      nextIndex_ = 0;
      allPoppedPause_ = false;
    }
    return;
  }

  if (clickPressed && !pressing_ && nextIndex_ < COUNT) {
    pressing_ = true;
    pressStart_ = now;
    pressingIndex_ = nextIndex_;
  }

  if (pressing_) {
    uint32_t elapsed = now - pressStart_;
    pressProgress_ = constrain((float)elapsed / (float)PRESS_MS, 0.0f, 1.0f);

    if (elapsed >= PRESS_MS) {
      popped_[pressingIndex_] = true;
      int px = bx(pressingIndex_), py = by(pressingIndex_);
      sparkles_.spawn(px, py, 6, 0.5f, 1.4f);
      counter_.pop(now, px, py);
      nextIndex_++;
      pressing_ = false;
      pressProgress_ = 0.0f;

      if (nextIndex_ >= COUNT) {
        allPoppedPause_ = true;
        allPoppedAt_ = now;
      }
    }
  }
}

void BubbleWrapSkin::draw() {
  display.setDrawColor(1);
  for (uint8_t i = 0; i < COUNT; i++) {
    if (popped_[i]) {
      display.drawDisc(bx(i), by(i), DIMPLE_R);
    } else if (pressing_ && i == pressingIndex_) {
      // "Вдавливание": пузырёк сжимается перед хлопком — контуром, а не
      // заливкой, чтобы читалось как нажатие
      // Pressing: the bubble shrinks before it pops — outline only, not filled,
      // so it reads as being pushed in
      float r = mapf(pressProgress_, 0.0f, 1.0f, (float)BUBBLE_R, (float)BUBBLE_R * 0.4f);
      int ir = (int)r;
      if (ir < 1) ir = 1;
      display.drawCircle(bx(i), by(i), ir);
    } else {
      display.drawDisc(bx(i), by(i), BUBBLE_R);
    }
  }
  sparkles_.draw();
  counter_.draw(now_);
}

void BubbleWrapSkin::onExit() {
  counter_.flush();
}

void BubbleWrapSkin::flushCounters() {
  counter_.flush();
}

void BubbleWrapSkin::onIdleEnter() {
  counter_.flush(); // фиксируем клики перед сном
  idleScale_ = 1.0f;
}

void BubbleWrapSkin::drawIdle() {
  idleScale_ += (0.0f - idleScale_) * IDLE_SHRINK_SPEED;

  display.setDrawColor(1);
  for (uint8_t i = 0; i < COUNT; i++) {
    if (popped_[i]) {
      display.drawDisc(bx(i), by(i), DIMPLE_R);
    } else {
      int r = (int)(BUBBLE_R * idleScale_);
      if (r > 0) display.drawDisc(bx(i), by(i), r);
    }
  }
}

bool BubbleWrapSkin::canIdle() const {
  // Не обрываем "вдавливание" перед хлопком и паузу перед новым листом
  // Do not interrupt the press before the pop or the pause before a fresh sheet
  return !pressing_ && !allPoppedPause_;
}
