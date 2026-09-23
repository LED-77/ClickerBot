#pragma once
#include <Arduino.h>

// Экран статистики: бегущая строка снизу вверх с названиями скинов и
// количеством "хлопков" каждого. Читает значения напрямую из NVS — скинам
// быть активными не обязательно.
// Statistics screen: a marquee scrolling upwards with skin names and their pop
// counts. Reads straight from NVS, so the skins do not have to be active.
namespace StatsMode {
  void onEnter();
  void update(uint32_t now);
  void draw();
  bool isDone();
}
