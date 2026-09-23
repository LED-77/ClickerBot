#include "EyeSkin.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"
#include <math.h>

namespace {
  constexpr int AMP_MAX    = 16;
  constexpr int AMP_SQUINT = 6;

  constexpr float PANIC_HYSTERIA = 70.0f; // порог входа в истерику (вращение)
  constexpr float PANIC_EXIT     = 60.0f; // порог выхода обратно в ACTIVE

  constexpr float CRACK_ADR_THRESHOLD = 85.0f; // выше этого уровня трещины растут
  constexpr float CRACK_GROW_MIN = 0.5f;       // трещин/сек у порога
  constexpr float CRACK_GROW_MAX = 3.0f;       // трещин/сек на максимуме
  constexpr float CRACK_HEAL_RATE = 1.2f;      // трещин/сек, когда не растут (откат)

  constexpr uint32_t SHAKE_MS = 600;    // финальная тряска перед взрывом (без отката)
  constexpr uint32_t EXPLODE_MS = 1100;
  constexpr uint32_t RESPAWN_PAUSE_MS = 500;

  constexpr float IDLE_CLOSE_SPEED = 0.05f; // скорость закрытия глаза в простое
}

void EyeSkin::resetTarget() {
  if (mode_ == HYSTERIA || mode_ == SHAKING || mode_ == EXPLODING || mode_ == RESPAWN) {
    return; // этими режимами движение зрачка управляется отдельно
  }
  float adr = intensity_.value();
  if (adr > 40.0f) {
    tX_ = cX_ + getRnd(-13, 14);
    tY_ = cY_ + getRnd(-4, 5);
  } else if (squinting_) {
    tX_ = cX_ + getRnd(-14, 15);
    tY_ = cY_ + getRnd(-1, 2);
  } else {
    tX_ = cX_ + getRnd(-10, 11);
    tY_ = cY_ + getRnd(-3, 4);
  }
}

void EyeSkin::startBlink() {
  if (squinting_ || mode_ != ACTIVE || intensity_.value() > 50.0f) return;
  blinking_ = true;
  blStart_ = millis();
  blDur_ = getRnd(120, 200);
  blNext_ = millis() + getRnd(2000, 5000);
}

void EyeSkin::startSquint() {
  if (blinking_ || mode_ != ACTIVE || intensity_.value() > 30.0f) return;
  squinting_ = true;
  sqEnd_ = millis() + getRnd(1200, 2500);
  sqNext_ = millis() + getRnd(6000, 15000);
  resetTarget();
}

void EyeSkin::triggerWake(uint32_t now) {
  mode_ = WAKE;
  wBlinks_ = getRnd(3, 6);
  wTimer_ = now;
  intensity_.reset();
  cracks_.reset();
}

void EyeSkin::onEnter() {
  cX_ = SCREEN_WIDTH / 2;
  cY_ = SCREEN_HEIGHT / 2;
  pX_ = cX_; pY_ = cY_;
  tX_ = cX_; tY_ = cY_;
  blinking_ = false; squinting_ = false;
  shX_ = 0; shY_ = 0;
  curAmp_ = 0;
  uint32_t now = millis();
  frLast_ = now;
  mvNext_ = now + 400;
  triggerWake(now);
  counter_.begin(counterKey());
}

bool EyeSkin::eyeBgLit(int x, int y, int pr, int px, int py, int amp) const {
  int w = RIGHT_X - LEFT_X;
  int tx = x - shX_; int ty = y - shY_;
  if (tx < LEFT_X || tx > RIGHT_X) return false;
  float angle = ((float)(tx - LEFT_X) / w) * PI;
  int off = (int)(sin(angle) * amp);
  if (ty < (cY_ - off) || ty > (cY_ + off)) return false;
  bool inPupil = (tx - px) * (tx - px) + (ty - py) * (ty - py) <= pr * pr;
  bool isHighlight = pr > 3 && (tx >= px - 3 && tx <= px - 2) &&
                      (ty >= py - 3 && ty <= py - 2);
  return (!inPupil) || isHighlight;
}

void EyeSkin::drawClipped(int px, int py, int pr, int amp) {
  int w = RIGHT_X - LEFT_X;
  static int offTable[SCREEN_WIDTH];
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int tx = x - shX_;
    if (tx >= LEFT_X && tx <= RIGHT_X) {
      float angle = ((float)(tx - LEFT_X) / w) * PI;
      offTable[x] = (int)(sin(angle) * amp);
    }
  }
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      int tx = x - shX_; int ty = y - shY_;
      if (tx >= LEFT_X && tx <= RIGHT_X) {
        int off = offTable[x];
        if (ty >= (cY_ - off) && ty <= (cY_ + off)) {
          if ((tx - px) * (tx - px) + (ty - py) * (ty - py) <= pr * pr) {
            if (pr > 3 && (tx >= px - 3 && tx <= px - 2) &&
                (ty >= py - 3 && ty <= py - 2)) {
              display.setDrawColor(1);
            } else {
              display.setDrawColor(0);
            }
          } else {
            display.setDrawColor(1);
          }
          display.drawPixel(x, y);
        }
      }
    }
  }
}

void EyeSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;
  if (clickPressed) {
    if (mode_ == ACTIVE || mode_ == HYSTERIA) {
      intensity_.onClick(now);
      resetTarget();
      if (mode_ == ACTIVE && intensity_.value() >= PANIC_HYSTERIA) {
        mode_ = HYSTERIA;
      }
    }
    counter_.pop(now, cX_, cY_);
    squinting_ = false; blinking_ = false;
  }

  intensity_.tick(now); // пассивное затухание, если кликов нет

  if (mode_ == ACTIVE) {
    float adr = intensity_.value();
    tR_ = (adr > 40.0f) ? map((int)adr, 40, 100, 7, 2) : 9;

    if (blinking_) {
      if (now - blStart_ >= blDur_) blinking_ = false;
    } else if (now >= blNext_) {
      startBlink();
    }
    if (squinting_) {
      if (now >= sqEnd_) squinting_ = false;
    } else if (now >= sqNext_) {
      startSquint();
    }

  } else if (mode_ == WAKE) {
    tR_ = 10;
    if ((int)curAmp_ >= AMP_MAX - 1) {
      if (wBlinks_ > 0) {
        if (!blinking_ && (now - wTimer_ > 150)) {
          blinking_ = true; blStart_ = now;
          blDur_ = 80; wBlinks_--; wTimer_ = now;
        }
        if (blinking_ && (now - blStart_ >= blDur_)) blinking_ = false;
      } else {
        mode_ = ACTIVE;
        blNext_ = now + getRnd(2000, 4000);
        sqNext_ = now + getRnd(4000, 8000);
      }
    }

  } else if (mode_ == HYSTERIA) {
    tR_ = 8;
    if (intensity_.value() < PANIC_EXIT) {
      mode_ = ACTIVE;
      blNext_ = now + getRnd(2000, 4000);
      sqNext_ = now + getRnd(4000, 8000);
    }

  } else if (mode_ == SHAKING) {
    if (now - shakeStart_ >= SHAKE_MS) {
      mode_ = EXPLODING;
      explodeStart_ = now;
      sparkles_.spawn(cX_, cY_, 24, 0.8f, 4.0f);
      counter_.pop(now, cX_, cY_);
    }

  } else if (mode_ == EXPLODING) {
    sparkles_.update();
    if (now - explodeStart_ >= EXPLODE_MS) {
      mode_ = RESPAWN;
      wTimer_ = now;
    }

  } else if (mode_ == RESPAWN) {
    if (now - wTimer_ >= RESPAWN_PAUSE_MS) triggerWake(now);
  }

  // Трещины растут/затухают всегда (кроме взрыва/респавна/пробуждения) — обратимость
  // Cracks always grow or heal (except during explosion/respawn/waking) — reversible
  if (mode_ != EXPLODING && mode_ != RESPAWN && mode_ != WAKE) {
    bool growing = (mode_ == HYSTERIA && intensity_.value() >= CRACK_ADR_THRESHOLD);
    float rate = growing
      ? mapf(intensity_.value(), CRACK_ADR_THRESHOLD, 100.0f, CRACK_GROW_MIN, CRACK_GROW_MAX)
      : 0.0f;
    cracks_.update(now, growing, rate, CRACK_HEAL_RATE, cX_, cY_, 8, 6);
    if (mode_ == HYSTERIA && cracks_.isMaxed()) {
      mode_ = SHAKING;
      shakeStart_ = now;
    }
  }

  if (now >= mvNext_ && mode_ != HYSTERIA && mode_ != SHAKING &&
      mode_ != EXPLODING && mode_ != RESPAWN) {
    resetTarget();
    float adr = intensity_.value();
    int del = (adr > 40.0f) ? getRnd(150, 300) :
              (squinting_ ? getRnd(300, 700) : getRnd(600, 1800));
    mvNext_ = now + del;
  }

  if (now - frLast_ >= 20) {
    frLast_ = now;

    if (mode_ == HYSTERIA) {
      float adr = intensity_.value();
      float period = mapf(adr, PANIC_HYSTERIA, 100.0f, 1000.0f, 250.0f);
      float ang = fmodf((float)now, period) / period * 2.0f * PI;
      const int orbitR = 7;
      tX_ = cX_ + (int)(cosf(ang) * orbitR);
      tY_ = cY_ + (int)(sinf(ang) * orbitR * 0.4f);
      pX_ = tX_; pY_ = tY_;
      curR_ += (tR_ - curR_) * 0.3f;
      int sh = (int)mapf(adr, PANIC_HYSTERIA, 100.0f, 2.0f, 4.0f);
      shX_ = getRnd(-sh, sh + 1); shY_ = getRnd(-sh, sh + 1);

    } else if (mode_ == SHAKING) {
      pX_ = cX_; pY_ = cY_; // зрачок фиксирован в центре на время тряски
      curR_ += (tR_ - curR_) * 0.3f;
      shX_ = getRnd(-5, 6); shY_ = getRnd(-5, 6); // резкая, сильная тряска

    } else if (mode_ == EXPLODING) {
      sparkles_.update();

    } else if (mode_ != RESPAWN) {
      int div = (mode_ == WAKE) ? 6 : ((intensity_.value() > 40.0f) ? 2 : 4);
      pX_ += (tX_ - pX_) / div;
      pY_ += (tY_ - pY_) / div;
      curR_ += (tR_ - curR_) * 0.2f;
      pX_ = constrain(pX_, cX_ - 12, cX_ + 12);
      pY_ = constrain(pY_, cY_ - 3, cY_ + 3);

      float adr = intensity_.value();
      if (adr > 50.0f) {
        int sh = (int)mapf(adr, 50.0f, 100.0f, 1.0f, 3.0f);
        shX_ = getRnd(-sh, sh + 1); shY_ = getRnd(-sh, sh + 1);
      } else {
        shX_ = 0; shY_ = 0;
      }
    }
  }
}

void EyeSkin::draw() {
  if (mode_ == EXPLODING) {
    sparkles_.draw();
    counter_.draw(now_);
    return;
  }
  if (mode_ == RESPAWN) {
    counter_.draw(now_); // пустой экран, но всплывающая цифра ещё может доигрывать
    return;
  }

  int tAmp = squinting_ ? AMP_SQUINT : AMP_MAX;
  if (mode_ == HYSTERIA || mode_ == SHAKING) tAmp = AMP_MAX;

  float spd = (mode_ == WAKE) ? 0.03f : 0.2f;
  curAmp_ += (tAmp - curAmp_) * spd;
  int fAmp = (int)curAmp_;

  if (blinking_) {
    uint32_t prg = millis() - blStart_;
    if (prg < blDur_ / 2) fAmp = map(prg, 0, blDur_ / 2, fAmp, 0);
    else fAmp = map(prg, blDur_ / 2, blDur_, 0, fAmp);
  }

  if (fAmp > 1) {
    drawClipped(pX_, pY_, (int)curR_, fAmp);
    if (cracks_.level() > 0.0f) {
      int pr = (int)curR_, px = pX_, py = pY_;
      cracks_.draw([this, pr, px, py, fAmp](int x, int y) {
        return eyeBgLit(x, y, pr, px, py, fAmp);
      });
    }
  } else {
    display.setDrawColor(1);
    display.drawLine(LEFT_X + shX_, cY_ + shY_, RIGHT_X + shX_, cY_ + shY_);
  }
  counter_.draw(now_);
}

// --- Простой: закрытие глаза до полоски, удержание, пробуждение ---
// --- Idle: the eye closes to a slit, holds, then wakes up ---

void EyeSkin::onExit() {
  counter_.flush(); // сохраним несохранённые клики
}

void EyeSkin::flushCounters() {
  counter_.flush();
}

void EyeSkin::onIdleEnter() {
  counter_.flush(); // фиксируем клики перед сном
  idleAmp_ = curAmp_; // начинаем закрытие с текущей раскрытости глаза
}

void EyeSkin::drawIdle() {
  idleAmp_ += (0.0f - idleAmp_) * IDLE_CLOSE_SPEED;
  int fAmp = (int)idleAmp_;

  if (fAmp > 1) {
    drawClipped(pX_, pY_, (int)curR_, fAmp);
  } else {
    display.setDrawColor(1);
    display.drawLine(LEFT_X, cY_, RIGHT_X, cY_);
  }
}

void EyeSkin::onWake(uint32_t now) {
  triggerWake(now); // та же последовательность морганий, что при первом запуске
}

bool EyeSkin::canIdle() const {
  // Не обрываем неперебиваемую последовательность тряска -> взрыв -> респавн
  // Never cut off the uninterruptible shiver -> explosion -> respawn sequence
  return mode_ != SHAKING && mode_ != EXPLODING && mode_ != RESPAWN;
}
