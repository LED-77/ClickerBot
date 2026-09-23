#include "MenuMode.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../skins/SkinRegistry.h"
#include <math.h>

namespace {
  uint8_t selected_ = 0;

  // --- Бегущая строка подсказки ---
  // --- Scrolling hint ---
  float scrollOffset_ = 0;
  uint32_t scrollLast_ = 0;
  bool scrollInit_ = false;

  void drawScrollHint(int y, const char* text) {
    int w = display.getStrWidth(text);
    if (w <= SCREEN_WIDTH) {
      display.drawStr((SCREEN_WIDTH - w) / 2, y, text);
      return;
    }
    uint32_t now = millis();
    if (!scrollInit_) { scrollInit_ = true; scrollLast_ = now; }
    float dt = (now - scrollLast_) / 1000.0f;
    scrollLast_ = now;
    if (dt > 0 && dt < 0.5f) scrollOffset_ += 18.0f * dt;

    int cycle = SCREEN_WIDTH + w;
    scrollOffset_ = fmodf(scrollOffset_, (float)cycle);
    if (scrollOffset_ < 0) scrollOffset_ += cycle;

    int x = SCREEN_WIDTH - (int)scrollOffset_;
    display.drawStr(x, y, text);
  }
}

void MenuMode::onEnter(uint8_t currentIndex) {
  selected_ = currentIndex;
  scrollOffset_ = 0;
  scrollInit_ = false;
}

void MenuMode::next() {
  // Пунктов на один больше, чем скинов — последний пункт "Random"
  // There is one entry more than skins: the last one is "Random"
  selected_ = (selected_ + 1) % (SkinRegistry::COUNT + 1);
}

uint8_t MenuMode::selectedIndex() {
  return selected_;
}

void MenuMode::draw() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);

  display.drawStr(2, 9, "SKIN");

  const char* name;
  if (selected_ >= SkinRegistry::COUNT) {
    name = "Random"; // виртуальный пункт
  } else {
    name = SkinRegistry::nameOf(selected_);
  }
  int w = display.getStrWidth(name);
  int x = (SCREEN_WIDTH - w) / 2;
  if (x < 9) x = 9; // оставляем место под стрелки слева
  display.drawStr(x, 21, name);

  // Стрелки-подсказки листания по бокам от названия
  // Paging arrows on the sides of the name
  display.drawTriangle(2, 17, 2, 23, 6, 20);
  display.drawTriangle(SCREEN_WIDTH - 3, 17, SCREEN_WIDTH - 3, 23, SCREEN_WIDTH - 7, 20);

  // Бегущая подсказка внизу
  // Scrolling hint at the bottom
  drawScrollHint(39, "Click - swap skin. Clic hold - Wifi menu. M - exit. M hold & R - flash mode.");
}
