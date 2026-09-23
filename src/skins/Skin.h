#pragma once
#include <Arduino.h>

// Общий интерфейс "скина" устройства (глаз, шарик и т.д.). main.cpp работает
// только через этот интерфейс и не знает, какой конкретно скин активен.
// Common interface of a device "skin" (eye, bubble wrap, ...). main.cpp talks
// only to this interface and never knows which skin is currently active.
class Skin {
public:
  virtual ~Skin() {}

  virtual void onEnter() = 0; // активация скина / skin becomes active

  // Каждый кадр, пока скин активен и не в простое; clickPressed — фронт клика
  // Every frame while the skin is active and not idle; clickPressed = click edge
  virtual void update(uint32_t now, bool clickPressed) = 0;

  // рисование в буфер; clear/send делает main.cpp / draw into the buffer; main.cpp clears and sends it
  virtual void draw() = 0;

  // --- Опциональное / --- Optional ---

  // Доп. вход для скинов со сложной логикой: фронт кнопки меню, фронт
  // отпускания клик-кнопки и сколько её уже держат (0 — не нажата).
  // Extra input for skins with complex controls: menu-button edge, click-button
  // release edge and how long it has been held (0 = not pressed).
  virtual void pollInput(uint32_t now, bool menuPressed, bool clickReleased,
                         uint32_t clickHeldMs) {}

  // скин больше не активен — гасим радио/периферию / skin is inactive: power down radio/peripherals
  virtual void onExit() {}
  // сохранить "ленивый" счётчик перед меню/статистикой / persist the lazy counter before menu/stats
  virtual void flushCounters() {}

  // --- Простой / --- Idle (driven by SleepManager in main.cpp) ---

  // начало простоя — зафиксировать старт анимации / idle starts: latch the animation start
  virtual void onIdleEnter() {}
  // кадры простоя; по умолчанию "замирает" на draw() / idle frames; by default freezes on draw()
  virtual void drawIdle() { draw(); }
  virtual void onWake(uint32_t now) {} // выход из простоя по клику / leaving idle on a click

  // false — запретить простой (например, посреди анимации взрыва)
  // false — forbid idling (e.g. in the middle of an explosion animation)
  virtual bool canIdle() const { return true; }

  virtual const char* name() const = 0;
  virtual const char* counterKey() const = 0; // ключ счётчика в NVS ("cntEye") / NVS counter key
  virtual const char* wireKey() const = 0;    // стабильный короткий id ("eye") / stable short id
  // доп. счётчик (победы), 0 — нет / secondary counter (wins), 0 = none
  virtual uint32_t statsSubCounter() const { return 0; }
  // командный скин — вне Random / multiplayer skin — excluded from Random
  virtual bool isMultiplayer() const { return false; }
};
