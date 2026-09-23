#include "PopCounter.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../util/MathUtils.h"
#include <stdio.h>

namespace {
  constexpr int REST_X_MARGIN = 2; // отступ от правого края экрана / margin from the right edge
  constexpr int REST_Y = 7;        // базовая линия цифры в покое (верх экрана) / baseline at rest (top of screen)
  constexpr int GLYPH_H = 7;       // высота знака шрифта 5x7 / glyph height of the 5x7 font
}

void PopCounter::begin(const char* key) {
  key_ = key;
  count_ = SettingsStore::loadCounter(key_);
  lastSave_ = millis(); // первую страховочную запись сделаем не раньше чем через минуту
}

void PopCounter::pop(uint32_t now, int originX, int originY) {
  count_++;
  dirty_ = true;
  // Страховка: раз в минуту активных кликов фиксируем счёт в NVS, чтобы при
  // внезапном отключении питания терялось не больше ~минуты кликов. Основная
  // запись — по-прежнему flush() при сне/выходе из скина/входе в меню.
  // Safety net: once a minute of active clicking the count is stored to NVS, so a
  // sudden power loss costs at most ~1 minute of clicks. The main write is still
  // flush() on idle/leaving the skin/entering the menu.
  if (now - lastSave_ >= SAVE_INTERVAL_MS) {
    SettingsStore::saveCounter(key_, count_);
    lastSave_ = now;
    dirty_ = false;
  }
  active_ = true;
  startAt_ = now;
  startX_ = originX;
  startY_ = originY;
}

void PopCounter::flush() {
  if (!dirty_) return;
  SettingsStore::saveCounter(key_, count_);
  lastSave_ = millis();
  dirty_ = false;
}

void PopCounter::formatCount(char* buf, size_t bufSize) const {
  if (count_ < 1000UL) {
    snprintf(buf, bufSize, "%lu", (unsigned long)count_);
  } else if (count_ < 1000000UL) {
    float k = count_ / 1000.0f;
    snprintf(buf, bufSize, (k < 10.0f) ? "%.1fk" : "%.0fk", k);
  } else if (count_ < 1000000000UL) {
    float m = count_ / 1000000.0f;
    snprintf(buf, bufSize, (m < 10.0f) ? "%.1fM" : "%.0fM", m);
  } else {
    float g = count_ / 1000000000.0f;
    snprintf(buf, bufSize, (g < 10.0f) ? "%.1fB" : "%.0fB", g);
  }
}

void PopCounter::draw(uint32_t now) {
  if (!active_) return;

  uint32_t elapsed = now - startAt_;
  if (elapsed >= RISE_MS + HOLD_MS + FADE_MS) {
    active_ = false;
    return;
  }

  char buf[8]; // с запасом: "999k"/"9.9M" и т.п. занимают максимум 4 символа
  formatCount(buf, sizeof(buf));
  display.setFont(u8g2_font_5x7_tf);
  int w = display.getStrWidth(buf);
  int restX = SCREEN_WIDTH - REST_X_MARGIN - w;

  int curX, curY;
  uint8_t eraseDots = 0;

  if (elapsed < RISE_MS) {
    // Всплытие: линейно едем от точки хлопка к "месту отдыха" в углу
    // Rise: linear travel from the pop point to the resting spot in the corner
    float t = (float)elapsed / (float)RISE_MS;
    curX = startX_ + (int)((restX - startX_) * t);
    curY = startY_ + (int)((REST_Y - startY_) * t);
  } else if (elapsed < RISE_MS + HOLD_MS) {
    curX = restX;
    curY = REST_Y;
  } else {
    // Растворение: цифра чуть доплывает вверх и точечно "рассыпается"
    // Dissolve: the number drifts up a little and crumbles away dot by dot
    uint32_t fadeElapsed = elapsed - RISE_MS - HOLD_MS;
    float t = (float)fadeElapsed / (float)FADE_MS;
    curX = restX;
    curY = REST_Y - (int)(t * 3.0f);
    eraseDots = (uint8_t)(t * MAX_ERASE_DOTS);
  }

  display.setDrawColor(1);
  display.drawStr(curX, curY, buf);

  if (eraseDots > 0) {
    // Не претендуем на точное совпадение с формой глифа — точечное затирание
    // внутри его прямоугольника на коротком отрезке времени читается как
    // "рассыпание", а не как ошибка отрисовки.
    // No attempt to match the glyph shape exactly: erasing dots inside its box for
    // a short moment reads as "crumbling", not as a drawing glitch.
    display.setDrawColor(0);
    for (uint8_t i = 0; i < eraseDots; i++) {
      int ex = curX + getRnd(0, w);
      int ey = curY - getRnd(0, GLYPH_H);
      display.drawPixel(ex, ey);
    }
  }
}
