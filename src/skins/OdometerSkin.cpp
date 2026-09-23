#include "OdometerSkin.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"

// --- Геометрия барабана / Drum geometry ---
int OdometerSkin::drumX(uint8_t pos) const {
  return DRUM_X0 + pos * (DRUM_W + DRUM_GAP);
}

// --- Инициализация / Init ---
void OdometerSkin::onEnter() {
  value_ = 0;
  for (uint8_t i = 0; i < DIGITS; i++) display_[i] = 0;
  rolling_ = false;
  exploding_ = false;
  counter_.begin(counterKey());
}

// --- Инкремент: все изменившиеся разряды крутятся одновременно ---
// --- Increment: all changed digits roll at the same time ---
void OdometerSkin::increment() {
  // Если анимация ещё играет — дорисовываем всё мгновенно
  // If an animation is still running, finish it instantly
  if (rolling_) {
    for (uint8_t i = 0; i < DIGITS; i++) display_[i] = animTo_[i];
    rolling_ = false;
  }

  bool was999 = (value_ == 999);
  value_ = (value_ + 1) % 1000;

  uint8_t newDigits[DIGITS];
  newDigits[0] = value_ / 100;
  newDigits[1] = (value_ / 10) % 10;
  newDigits[2] = value_ % 10;

  // Запоминаем старые и новые значения для каждого разряда
  // Remember the old and the new value of every digit
  for (uint8_t i = 0; i < DIGITS; i++) {
    animFrom_[i] = display_[i];
    animTo_[i] = newDigits[i];
  }

  // Если хоть один разряд изменился — запускаем общую анимацию
  // If at least one digit changed, start the shared animation
  if (animTo_[0] != animFrom_[0] || animTo_[1] != animFrom_[1] || animTo_[2] != animFrom_[2]) {
    animOffset_ = 0.0f;
    rollStart_ = now_;
    rolling_ = true;
  }

  // Взрыв на каждой сотне (100, 200…900) и при сбросе с 999
  // An explosion every hundred (100, 200 ... 900) and on the 999 reset
  bool isRound = (value_ % 100 == 0) && value_ > 0;
  if (was999 || isRound) {
    exploding_ = true;
    explodeStart_ = now_;
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2;
    uint8_t count = was999 ? 28 : 18;
    float maxSpeed = was999 ? 6.0f : 4.0f;
    sparkles_.spawn(cx, cy, count, 1.2f, maxSpeed);
  }
}

// --- Основной цикл / Main loop ---
void OdometerSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;

  if (clickPressed) {
    increment();
    counter_.pop(now, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 5);
  }

  if (rolling_) {
    float dt = (now - rollStart_) / (float)ROLL_MS;
    animOffset_ = constrain(dt * DRUM_H, 0.0f, (float)DRUM_H);

    if (animOffset_ >= DRUM_H - 0.5f) {
      // Анимация завершена — фиксируем новые значения во всех разрядах
      // Animation finished — commit the new values of all digits
      for (uint8_t i = 0; i < DIGITS; i++) display_[i] = animTo_[i];
      rolling_ = false;
    }
  }

  if (exploding_) {
    sparkles_.update();
    if (now - explodeStart_ >= EXPLODE_MS) {
      exploding_ = false;
    }
  }
}

// --- Отрисовка / Drawing ---
void OdometerSkin::drawDrumFrame(int x) const {
  // Рамка барабана
  // Drum frame
  display.setDrawColor(1);
  display.drawFrame(x, DRUM_Y, DRUM_W, DRUM_H);

  // Лёгкая тень сверху — чтобы читалось как объёмный барабан
  // A light shadow on top so it reads as a cylindrical drum
  display.drawHLine(x + 2, DRUM_Y + 1, DRUM_W - 4);
  display.drawPixel(x + 1, DRUM_Y + 1);
  display.drawPixel(x + DRUM_W - 2, DRUM_Y + 1);
}

void OdometerSkin::drawDigitInDrum(int x, uint8_t digit, int yOff) const {
  // Перед рисованием цифры устанавливаем clip-окно = область барабана,
  // чтобы выходящие за рамку части цифры не отображались
  // Before drawing a digit the clip window is set to the drum area, so parts of
  // the glyph outside the frame are not shown
  display.setClipWindow(x, DRUM_Y, x + DRUM_W, DRUM_Y + DRUM_H);

  char buf[2] = { (char)('0' + digit), 0 };
  int digitW = display.getStrWidth(buf);
  int dx = x + (DRUM_W - digitW) / 2;
  // Базовая линия: центр барабана + половина высоты шрифта (~10px для 7x13)
  // Baseline: drum centre plus half the font height (~10 px for 7x13)
  int baseY = DRUM_Y + DRUM_H / 2 + 5 + yOff;

  display.setDrawColor(1);
  display.drawStr(dx, baseY, buf);

  // Сброс clip-окна на весь экран
  // Reset the clip window back to the whole screen
  display.setClipWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void OdometerSkin::draw() {
  display.setFont(u8g2_font_7x13_tf);
  display.setDrawColor(1);

  // Взрыв — рисуем только искры поверх всего
  // Explosion — sparks only, drawn on top of everything
  if (exploding_) {
    sparkles_.draw();
    counter_.draw(now_);
    return;
  }

  for (uint8_t i = 0; i < DIGITS; i++) {
    int x = drumX(i);

    if (rolling_ && animFrom_[i] != animTo_[i]) {
      // Анимация: старая цифра едет вверх, новая въезжает снизу
      // Animation: the old digit moves up while the new one slides in from below
      drawDrumFrame(x);
      int shift = (int)animOffset_;
      drawDigitInDrum(x, animFrom_[i], -shift);
      drawDigitInDrum(x, animTo_[i], (DRUM_H - 7) - shift);
    } else {
      drawDrumFrame(x);
      drawDigitInDrum(x, display_[i], 0);
    }
  }

  counter_.draw(now_);
}

// --- Idle ---
void OdometerSkin::onExit() {
  counter_.flush();
}

void OdometerSkin::flushCounters() {
  counter_.flush();
}

void OdometerSkin::onIdleEnter() {
  counter_.flush();
}

void OdometerSkin::drawIdle() {
  // В простое — только рамки барабанов, без цифр (как обесточенный счётчик)
  // When idle: drum frames only, no digits (like a powered-down counter)
  display.setDrawColor(1);
  for (uint8_t i = 0; i < DIGITS; i++) {
    display.drawFrame(drumX(i), DRUM_Y, DRUM_W, DRUM_H);
  }
}

void OdometerSkin::onWake(uint32_t now) {
  // Значение счётчика не сбрасывается при пробуждении
  // The counter value is not reset on wake-up
}

bool OdometerSkin::canIdle() const {
  return !rolling_ && !exploding_;
}
