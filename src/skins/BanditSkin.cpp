#include "BanditSkin.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../util/MathUtils.h"
#include <math.h>

namespace {
  // Ключ счётчика совпадений (основной "cntBandit" — клики)
  // NVS key of the match counter (the main "cntBandit" keeps clicks)
  constexpr const char* BANDIT_MATCHES_KEY = "banditMatches";
}

// --- Геометрия барабана / Drum geometry ---
int BanditSkin::drumX(uint8_t i) const {
  return DRUM_X0 + i * (DRUM_W + DRUM_GAP);
}

// --- Инициализация / Init ---
void BanditSkin::onEnter() {
  for (uint8_t i = 0; i < REELS; i++) {
    pos_[i] = 0;
    speed_[i] = 0;
    spinning_[i] = false;
    stopping_[i] = false;
    target_[i] = 0;
  }
  allStopped_ = true;
  matches_ = SettingsStore::loadCounter(BANDIT_MATCHES_KEY);
  counter_.begin(counterKey());
  resultUntil_ = 0;
  now_ = millis();
  lastFrame_ = now_;
}

void BanditSkin::onExit() {
  counter_.flush(); // сохраним несохранённые клики
}

void BanditSkin::flushCounters() {
  counter_.flush();
}

uint32_t BanditSkin::statsSubCounter() const {
  // Из NVS напрямую: matches_ в памяти живёт только с onEnter()
  // Straight from NVS: matches_ in RAM only exists after onEnter()
  return SettingsStore::loadCounter(BANDIT_MATCHES_KEY);
}

// Новый раунд: с вероятностью MATCH_PROB все барабаны встанут на одну цифру,
// иначе гарантированно не совпадут.
// New round: with probability MATCH_PROB all reels land on the same digit,
// otherwise they are guaranteed not to match.
void BanditSkin::startRound(uint32_t now) {
  bool win = (getRnd(0, 1000) < (int)(MATCH_PROB * 1000.0f));
  // Цифры независимо случайные — иначе вылезали «зеркальные» сочетания
  // вроде 454/101/202 (первая и третья рельсы совпадали всегда).
  // The digits are independent random values: otherwise mirrored combos like
  // 454/101/202 showed up (the first and third reels always matched).
  for (uint8_t i = 0; i < REELS; i++) target_[i] = (uint8_t)getRnd(0, 10);
  if (win) {
    uint8_t d = target_[0]; // выигрыш: все три встают на одну цифру
    for (uint8_t i = 0; i < REELS; i++) target_[i] = d;
  } else if (target_[0] == target_[1] && target_[1] == target_[2]) {
    // Редкий случай: выпало случайное совпадение — уводим от него
    // Rare case: a random match came up — steer away from it
    target_[2] = (uint8_t)((target_[2] + 1 + getRnd(0, 9)) % 10);
  }
  lastClick_ = now;
  for (uint8_t i = 0; i < REELS; i++) {
    pos_[i] = (float)getRnd(0, 20); // стартуем с произвольной позиции
    speed_[i] = MAX_SPEED;
    spinning_[i] = true;
    stopping_[i] = false;
  }
  allStopped_ = false;
  resultUntil_ = 0; // новый раунд убирает салют прошлого
}

// Начало замедления i-го барабана к его цели
// Starts the deceleration of reel i towards its target
void BanditSkin::beginStop(int i) {
  stopping_[i] = true;
  decelStart_[i] = now_;
  decelFrom_[i] = pos_[i];
  // Докрутим до следующего десятка и встанем ровно на цель (модуль 10)
  // Round up to the next ten and land exactly on the target (modulo 10)
  decelTo_[i] = (floorf(pos_[i] / 10.0f) + 1.0f) * 10.0f + target_[i];
}

// --- Основной цикл / Main loop ---
void BanditSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;

  if (clickPressed) {
    counter_.pop(now, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 5);
    if (allStopped_) {
      startRound(now);
    } else {
      // Продолжаем вращение: клик отодвигает остановку и раскручивает снова
      // Keep spinning: a click pushes the stop further away and speeds it up again
      lastClick_ = now;
      for (uint8_t i = 0; i < REELS; i++) {
        if (spinning_[i] && !stopping_[i]) speed_[i] = MAX_SPEED;
      }
    }
  }

  if (allStopped_) {
    sparkles_.update(); // доигрываем салют
    return;
  }

  float dt = (float)(now - lastFrame_) / 1000.0f;
  lastFrame_ = now;
  if (dt < 0.0f || dt > 0.25f) dt = 0.25f;

  bool anySpinning = false;
  for (uint8_t i = 0; i < REELS; i++) {
    if (!spinning_[i]) continue;
    anySpinning = true;

    if (stopping_[i]) {
      // Плавное замедление к цели (ease-out)
      // Smooth deceleration towards the target (ease-out)
      float t = (float)(now - decelStart_[i]) / (float)DECEL_MS;
      if (t >= 1.0f) {
        pos_[i] = decelTo_[i];
        spinning_[i] = false;
      } else {
        float ease = 1.0f - (1.0f - t) * (1.0f - t);
        pos_[i] = decelFrom_[i] + (decelTo_[i] - decelFrom_[i]) * ease;
      }
    } else {
      pos_[i] += speed_[i] * dt;
      // Вращение понемногу затухает
      // The spin gradually dies down
      speed_[i] -= 1.5f * dt;
      if (speed_[i] < 1.0f) speed_[i] = 1.0f;

      // Барабаны останавливаются по одному (~5 секунд после последнего клика)
      // The reels stop one by one (~5 seconds after the last click)
      uint32_t delay = (i == 0) ? 1500 : (i == 1) ? 3000 : 4500;
      if (now >= lastClick_ + delay) {
        beginStop(i);
      }
    }
  }

  if (!anySpinning) {
    allStopped_ = true;
    // Проверяем совпадение — салют и счётчик совпадений
    // Check for a match — firework and the match counter
    if (target_[0] == target_[1] && target_[1] == target_[2]) {
      matches_++;
      SettingsStore::saveCounter(BANDIT_MATCHES_KEY, matches_);
      sparkles_.spawn(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 24, 1.2f, 3.0f);
      resultUntil_ = now + RESULT_MS;
    }
  }

  sparkles_.update();
}

// --- Отрисовка / Drawing ---
void BanditSkin::drawDrumFrame(int x) const {
  display.setDrawColor(1);
  display.drawFrame(x, DRUM_Y, DRUM_W, DRUM_H);

  // Лёгкая тень сверху — читается как объёмный барабан
  // A light shadow on top makes it read as a cylindrical drum
  display.drawHLine(x + 2, DRUM_Y + 1, DRUM_W - 4);
  display.drawPixel(x + 1, DRUM_Y + 1);
  display.drawPixel(x + DRUM_W - 2, DRUM_Y + 1);
}

void BanditSkin::drawDigitInDrum(int x, uint8_t digit, int yOff) const {
  // Клип по области барабана, чтобы цифры не вылезали за рамку
  // Clip to the drum area so the digits do not spill outside the frame
  display.setClipWindow(x, DRUM_Y, x + DRUM_W, DRUM_Y + DRUM_H);

  char buf[2] = { (char)('0' + digit), 0 };
  int digitW = display.getStrWidth(buf);
  int dx = x + (DRUM_W - digitW) / 2;
  int baseY = DRUM_Y + DRUM_H / 2 + 5 + yOff;

  display.setDrawColor(1);
  display.drawStr(dx, baseY, buf);

  display.setClipWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void BanditSkin::draw() {
  display.setFont(u8g2_font_7x13_tf);
  display.setDrawColor(1);

  bool showResult = (now_ < resultUntil_);
  if (showResult) {
    sparkles_.draw(); // салют позади барабанов
    display.setFont(u8g2_font_7x13_tf);
  }

  for (uint8_t i = 0; i < REELS; i++) {
    int x = drumX(i);
    drawDrumFrame(x);

    int cur = ((int)floorf(pos_[i])) % 10;
    if (spinning_[i]) {
      // Вращение: текущая цифра едет вверх, следующая въезжает снизу
      // Spinning: the current digit moves up while the next one slides in from below
      float frac = pos_[i] - floorf(pos_[i]);
      int next = (cur + 1) % 10;
      drawDigitInDrum(x, cur, -(int)(frac * 10.0f));
      drawDigitInDrum(x, next, (DRUM_H - 7) - (int)(frac * 10.0f));
    } else {
      drawDigitInDrum(x, cur, 0);
    }
  }

  counter_.draw(now_);

  if (showResult) {
    int w = display.getStrWidth("WIN!");
    display.drawStr((SCREEN_WIDTH - w) / 2, 7, "WIN!");
  }
}

// --- Idle ---
void BanditSkin::onIdleEnter() {
  counter_.flush(); // фиксируем клики перед сном
}

void BanditSkin::drawIdle() {
  // В простое — только рамки барабанов, без цифр (как обесточенный автомат)
  // When idle: reel frames only, no digits (like a powered-down machine)
  display.setDrawColor(1);
  for (uint8_t i = 0; i < REELS; i++) {
    display.drawFrame(drumX(i), DRUM_Y, DRUM_W, DRUM_H);
  }
}

void BanditSkin::onWake(uint32_t now) {
  // Значения не сбрасываются при пробуждении
  // The values are not reset on wake-up
}

bool BanditSkin::canIdle() const {
  // Не уходим в сон посреди вращения — даём барабанам доиграть
  // Do not sleep in the middle of a spin — let the reels finish
  return allStopped_;
}
