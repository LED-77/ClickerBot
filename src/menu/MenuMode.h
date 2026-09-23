#pragma once
#include <Arduino.h>

// Меню выбора скина: клик — следующий пункт, удержание кнопки платы —
// подтверждение и выход (обрабатывает main.cpp).
// Skin selection menu: a click moves to the next entry, holding the board button
// confirms and exits (handled in main.cpp).
namespace MenuMode {
  void onEnter(uint8_t currentIndex); // с какого скина начинаем / start from the active skin
  void next();                        // следующий пункт по кругу / next entry, wrapping

  void draw();

  uint8_t selectedIndex();
}
