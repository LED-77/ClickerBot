#pragma once
#include <Arduino.h>

// Режимы WiFi: точка доступа или клиент
// WiFi sub-modes: access point or client
enum class WifiSubMode { ACCESS_POINT, CLIENT };

// Экран настроек WiFi. Два режима, переключаются клик-кнопкой.
// Выход — по кнопке меню (обрабатывается в main.cpp).
// WiFi settings screen with two modes, switched by the click button. Leaving it
// is done with the menu button (handled in main.cpp).
namespace WifiMode {
  void onEnter();
  void tick();
  void nextMode();
  void stop();
  void draw();

  WifiSubMode currentMode();
}
