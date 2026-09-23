#include "StatsMode.h"
#include "../Config.h"
#include "../hal/BatteryManager.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../skins/SkinRegistry.h"

namespace {
  struct Line {
    const char* name;
    uint32_t count;  // основной счётчик (клики)
    uint32_t count2; // доп. счётчик (победы); 0 = нет
  };
  Line lines_[SkinRegistry::COUNT];
  uint32_t totalCount_ = 0; // сумма по всем скинам

  static constexpr float SCROLL_SPEED = 14.0f; // пикселей/сек
  static constexpr int LINE_H = 8;              // 7px font + 1px межстрочник
  static constexpr int EXTRA_LINES = 3;         // разделитель + "total" + отступ

  float scrollY_ = 0;
  int totalH_ = 0;
  uint32_t lastFrame_ = 0;
  bool done_ = false;

  // Когда "total" и заряд полностью на экране — короткая пауза, затем выход
  // Short pause once "total" and the charge are fully on screen, then exit
  bool holdTotal_ = false;
  uint32_t holdTotalAt_ = 0;
  static constexpr uint32_t TOTAL_HOLD_MS = 2000;
}

void StatsMode::onEnter() {
  totalCount_ = 0;
  for (uint8_t i = 0; i < SkinRegistry::COUNT; i++) {
    lines_[i].name = SkinRegistry::nameOf(i);
    // Ключ счётчика берём у самого скина — новый скин попадает в статистику сам
    // The counter key comes from the skin itself, so a new skin shows up here automatically
    lines_[i].count = SettingsStore::loadCounter(SkinRegistry::get(i)->counterKey());
    lines_[i].count2 = SkinRegistry::get(i)->statsSubCounter();
    totalCount_ += lines_[i].count;
  }
  totalH_ = (SkinRegistry::COUNT + EXTRA_LINES) * LINE_H + 4;
  scrollY_ = SCREEN_HEIGHT; // начинаем под экраном
  lastFrame_ = millis();
  done_ = false;
  holdTotal_ = false;
}

void StatsMode::update(uint32_t now) {
  float dt = (now - lastFrame_) / 1000.0f;
  lastFrame_ = now;
  if (dt < 0 || dt > 0.5f) return;

  if (!holdTotal_) {
    scrollY_ -= SCROLL_SPEED * dt;
    // Как только total и заряд полностью на экране — замораживаем прокрутку
    // и даём посмотреть итог (+2 = разделитель и "total", +1 = строка заряда)
    // Freeze the scroll once total and the charge are fully on screen, so the
    // summary can be read (+2 = separator and "total", +1 = charge line)
    int totalShownAt = SCREEN_HEIGHT - (int)(SkinRegistry::COUNT + 3) * LINE_H;
    if (scrollY_ <= totalShownAt) {
      holdTotal_ = true;
      holdTotalAt_ = now;
    }
  } else if (now - holdTotalAt_ >= TOTAL_HOLD_MS) {
    done_ = true; // main.cpp вернёт нас в скин / main.cpp returns us to the skin
  }
}

void StatsMode::draw() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);

  char buf[24];
  uint8_t n = SkinRegistry::COUNT;

  // Строки скинов
  // One line per skin
  for (uint8_t i = 0; i < n; i++) {
    int y = (int)(scrollY_ + i * LINE_H);
    if (y < -LINE_H || y >= SCREEN_HEIGHT) continue;

    if (lines_[i].count2 > 0) {
      snprintf(buf, sizeof(buf), "%s %lu/%lu", lines_[i].name,
               (unsigned long)lines_[i].count, (unsigned long)lines_[i].count2);
    } else {
      snprintf(buf, sizeof(buf), "%s %lu", lines_[i].name, (unsigned long)lines_[i].count);
    }
    int x = (SCREEN_WIDTH - display.getStrWidth(buf)) / 2;
    if (x < 0) x = 0;
    display.drawStr(x, y + LINE_H - 1, buf);
  }

  // Разделитель
  // Separator
  int sepY = (int)(scrollY_ + n * LINE_H);
  if (sepY >= 0 && sepY < SCREEN_HEIGHT) {
    display.drawHLine(8, sepY, SCREEN_WIDTH - 16);
  }

  // Общий итог
  // Grand total
  int totalY = (int)(scrollY_ + (n + 1) * LINE_H);
  if (totalY >= -LINE_H && totalY < SCREEN_HEIGHT) {
    snprintf(buf, sizeof(buf), "total %lu", (unsigned long)totalCount_);
    int x = (SCREEN_WIDTH - display.getStrWidth(buf)) / 2;
    if (x < 0) x = 0;
    display.drawStr(x, totalY + LINE_H - 1, buf);
  }

  // Заряд аккумулятора
  // Battery charge
  int battY = (int)(scrollY_ + (n + 2) * LINE_H);
  if (battY >= -LINE_H && battY < SCREEN_HEIGHT) {
    snprintf(buf, sizeof(buf), "batt %d%%", Battery::percent());
    int x = (SCREEN_WIDTH - display.getStrWidth(buf)) / 2;
    if (x < 0) x = 0;
    display.drawStr(x, battY + LINE_H - 1, buf);
  }
}

bool StatsMode::isDone() { return done_; }
