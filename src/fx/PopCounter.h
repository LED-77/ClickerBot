#pragma once
#include <Arduino.h>

// Счётчик "хлопков" скина (лопнувший шарик, взорвавшийся глаз и т.п.) плюс
// всплывающая цифра над точкой события. Каждый скин заводит свой экземпляр
// со своим ключом NVS, значение переживает deep sleep и отключение питания.
// Анимация чисто визуальная — при перезапуске не проигрывается.
// A skin "pop" counter (burst bubble, exploded eye, ...) plus a floating number
// over the point of the event. Every skin owns an instance with its own NVS key;
// the value survives deep sleep and a power cut. The animation is purely visual
// and is not replayed after a restart.
class PopCounter {
public:
  void begin(const char* key); // из onEnter(); ключ — см. SettingsStore.h / call from onEnter(); key: see SettingsStore.h

  // Вызывать в момент "хлопка": увеличивает счётчик (только в RAM) и
  // запускает анимацию всплывающей цифры от точки (originX, originY).
  // В NVS сразу НЕ пишем — чтобы не изнашивать flash частыми записями;
  // значение фиксируется вызовом flush() при уходе в сон / выходе из скина.
  // Call on a "pop": bumps the counter (RAM only) and starts the floating-number
  // animation from (originX, originY). Nothing is written to NVS right away, to
  // spare the flash; flush() persists it when idling or leaving the skin.
  void pop(uint32_t now, int originX, int originY);

  // Сохранить текущее значение в NVS, если были несохранённые клики.
  // Звать при уходе в сон (onIdleEnter) и при выходе из скина (onExit).
  // Persist the value to NVS if there are unsaved clicks. Call when going idle
  // (onIdleEnter) and when leaving the skin (onExit).
  void flush();

  // Рисовать каждый кадр поверх остальной графики скина (после draw()).
  // Ничего не делает, если анимация сейчас не активна — вызывать безусловно.
  // Draw every frame on top of the skin graphics (after draw()). Does nothing
  // while no animation is running — safe to call unconditionally.
  void draw(uint32_t now);

  uint32_t value() const { return count_; }

private:
  static constexpr uint32_t RISE_MS = 300;  // всплытие от точки к уголку / rise from the point to the corner
  static constexpr uint32_t HOLD_MS = 900;  // пауза на виду / hold on screen
  static constexpr uint32_t FADE_MS = 500;  // растворение россыпью точек / dissolve into dots
  static constexpr uint8_t  MAX_ERASE_DOTS = 10;
  static constexpr uint32_t SAVE_INTERVAL_MS = 60000; // страховка: раз в минуту / safety write once a minute

  const char* key_ = nullptr;
  uint32_t count_ = 0;
  bool dirty_ = false;  // есть несохранённые клики / unsaved clicks
  uint32_t lastSave_ = 0; // когда последний раз писали в NVS / last NVS write

  bool active_ = false;
  uint32_t startAt_ = 0;
  int startX_ = 0, startY_ = 0;

  // Компактно форматирует count_ в buf: "42", "1.2k", "45k", "1.2M" и т.д. —
  // не более 4-5 символов независимо от величины счётчика, чтобы цифра
  // никогда не выходила за пределы крошечного экрана.
  // Formats count_ compactly into buf: "42", "1.2k", "45k", "1.2M", ... —
  // never more than 4-5 characters, so the number always fits the tiny screen.
  void formatCount(char* buf, size_t bufSize) const;
};
