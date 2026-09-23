#pragma once
#include "Skin.h"
#include "../Config.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

// Однорукий бандит (слот-машина): три барабана с цифрами 0..9.
// Клик раскручивает барабаны и увеличивает счётчик кликов; пока кликаешь —
// они крутятся на полной скорости, перестал — по одному останавливаются в
// течение ~5 секунд. Если все три цифры совпали — салют и засчитываем
// совпадение (статистика показывает "клики/совпадения").
//
// Вероятность совпадения — настраиваемый параметр MATCH_PROB (по умолчанию
// 0.10 = 1 к 10): с такой вероятностью раунд «выигрышный» (все барабаны
// встают на одну цифру), иначе гарантированно не совпадают.
// Slot machine: three reels with digits 0..9. A click spins the reels and bumps
// the click counter; while you click they spin at full speed, and once you stop
// they stop one by one over ~5 seconds. Three equal digits means a firework and a
// counted match (statistics show "clicks/matches").
//
// The match chance is the MATCH_PROB parameter (default 0.10 = 1 in 10): with that
// probability the round is a winning one (all reels land on the same digit),
// otherwise they are guaranteed not to match.
class BanditSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void draw() override;

  void onIdleEnter() override;
  void drawIdle() override;
  void onWake(uint32_t now) override;
  bool canIdle() const override;

  const char* name() const override { return "Bandit"; }
  const char* counterKey() const override { return "cntBandit"; }
  const char* wireKey() const override { return "bandit"; }

  // Статистика: основной счётчик — клики (cntBandit), доп. — совпадения
  // Statistics: main counter = clicks (cntBandit), secondary = matches
  uint32_t statsSubCounter() const override;

private:
  static constexpr uint8_t REELS = 3;

  // === Настраиваемые параметры ===
  // === Tunable parameters ===
  static constexpr float MATCH_PROB = 0.10f;  // 1 к 10 — барабаны совпадут / 1 in 10 reels match
  static constexpr float MAX_SPEED = 24.0f;   // цифр/сек при вращении / digits per second
  // Задержки остановки барабанов (~1.5/3/4.5 с) заданы прямо в update():
  // массив constexpr дал бы odr-use при передаче в функцию
  // Reel stop delays (~1.5/3/4.5 s) live inside update(): a constexpr array would
  // be odr-used when passed to a function
  static constexpr uint32_t DECEL_MS = 500;   // мс плавного замедления / ease-out time
  static constexpr uint32_t RESULT_MS = 2500; // сколько висит салют / firework duration

  // Геометрия барабанов — как у одометра
  // Drum geometry — same as the odometer
  static constexpr int DRUM_W = 14;       // ширина окна барабана / drum window width
  static constexpr int DRUM_H = 24;       // высота окна барабана / drum window height
  static constexpr int DRUM_GAP = 3;      // зазор между барабанами / gap between drums
  static constexpr int DRUM_Y = (SCREEN_HEIGHT - DRUM_H) / 2;
  static constexpr int TOTAL_W = REELS * DRUM_W + (REELS - 1) * DRUM_GAP;
  static constexpr int DRUM_X0 = (SCREEN_WIDTH - TOTAL_W) / 2;

  float pos_[REELS];        // непрерывная позиция; цифра = ((int)pos) % 10
  float speed_[REELS];      // цифр/сек
  bool  spinning_[REELS];   // барабан в движении
  bool  stopping_[REELS];   // замедление к цели
  uint32_t decelStart_[REELS];
  float decelFrom_[REELS];
  float decelTo_[REELS];
  uint8_t target_[REELS];   // цифра, на которую встанет барабан

  bool allStopped_ = true;  // все барабаны стоят — ждём первый клик раунда
  uint32_t lastClick_ = 0;  // время последнего клика (база отсчёта остановки)
  uint32_t lastFrame_ = 0;  // время прошлого кадра (для dt)
  uint32_t now_ = 0;

  uint32_t matches_ = 0;    // совпадения (сохраняются в NVS при совпадении)
  uint32_t resultUntil_ = 0;// до скольки показывать салют после совпадения

  SparkleFX sparkles_;
  PopCounter counter_;

  int drumX(uint8_t i) const;
  void drawDrumFrame(int x) const;
  void drawDigitInDrum(int x, uint8_t digit, int yOff) const;
  void startRound(uint32_t now);
  void beginStop(int i);
};
