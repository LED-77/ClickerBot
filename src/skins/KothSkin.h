#pragma once
#include "Skin.h"
#include "../duel/DuelProtocol.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

// "Король холма": кто дольше всех удерживает корону, тот побеждает. Игроков
// не ограничено: каждый вещает свою мощь (клики за последние 2 с) и время
// короля; король определяется локально (максимум мощи, при равенстве — MAC).
//
// Фазы: WAIT (удержание клика ~1 с — старт) -> READY -> ROUND (60 с, обмен
// раз в 250 мс) -> END_WAIT -> RESULT -> снова WAIT. Сон (20 с + 20 с) работает
// только в WAIT; во время раунда устройство не спит.
// "King of the Hill": whoever holds the crown the longest wins. The number of
// players is unlimited: each one broadcasts its power (clicks in the last 2 s) and
// its crown time; the king is decided locally (max power, ties broken by MAC).
//
// Phases: WAIT (~1 s click hold starts) -> READY -> ROUND (60 s, exchange every
// 250 ms) -> END_WAIT -> RESULT -> WAIT again. Idling (20 s + 20 s) only works in
// WAIT; during a round the device does not sleep.
class KothSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void pollInput(uint32_t now, bool menuPressed, bool clickReleased,
                 uint32_t clickHeldMs) override;
  void draw() override;

  void onIdleEnter() override;
  void drawIdle() override;
  void onWake(uint32_t now) override;
  bool canIdle() const override;

  const char* name() const override { return "KOTH"; }
  const char* counterKey() const override { return "cntKoth"; }
  const char* wireKey() const override { return "koth"; }

  // Командный скин — не попадает в режим Random
  // Multiplayer skin — never picked in Random mode
  bool isMultiplayer() const override { return true; }

  // Статистика: основной счётчик — клики (cntKoth), доп. — победы (kothWins)
  // Statistics: main counter = clicks (cntKoth), secondary = wins (kothWins)
  uint32_t statsSubCounter() const override;

private:
  enum Phase { WAIT, READY, ROUND, END_WAIT, RESULT };

  struct Player {
    uint8_t mac[6];
    char name[Duel::MAX_NAME + 1];
    uint32_t lastSeen;
    uint32_t power;     // последняя мощь (клики за окно)
    uint32_t kingTime;  // последнее время короля (мс)
    bool valid;
    bool hasState;
    bool isKing;        // сам игрок заявил, что держит корону
  };

  Phase phase_ = WAIT;
  Player players_[Koth::MAX_PLAYERS];

  uint32_t roundId_ = 0;
  uint32_t readyDelay_ = 0;
  bool startedByMe_ = false;
  uint32_t phaseStart_ = 0;
  uint32_t roundStart_ = 0;
  uint32_t nextAnnounce_ = 0;
  uint32_t nextTick_ = 0;
  uint32_t lastTick_ = 0;
  uint32_t lastNetRetry_ = 0;

  bool holdFired_ = false;
  uint32_t needPlayersHint_ = 0; // когда показали подсказку "нужен 2-й игрок"

  // Мощь кликов: кольцевой буфер слотов по 250 мс (окно 2 с)
  // Click power: a ring buffer of 250 ms slots (a 2 s window)
  uint8_t powerSlots_[Koth::POWER_SLOTS];
  uint8_t powerSlotIndex_ = 0;
  uint32_t myPower_ = 0;     // текущая мощь (сумма слотов)
  uint32_t myClicks_ = 0;    // всего кликов за раунд
  uint32_t myKingTime_ = 0;  // накопленное время короля (мс)
  bool iAmKing_ = false;     // король ли я прямо сейчас

  bool haveWinner_ = false;
  bool iWon_ = false;
  char winnerName_[Duel::MAX_NAME + 1];
  uint32_t winnerTime_ = 0;

  char myName_[Duel::MAX_NAME + 1];
  uint8_t myMac_[6];
  uint32_t now_ = 0;

  SparkleFX fireworks_;        // салют победителю
  uint32_t fireworkNext_ = 0;  // когда следующий залп

  PopCounter counter_;         // анимированный счётчик кликов (как в других скинах)

  // Прокрутка длинных имён
  // Marquee scrolling for long names
  float scrollOffset_ = 0;
  uint32_t scrollLast_ = 0;
  bool scrollInit_ = false;

  void setPhase(Phase p, uint32_t now);
  void buildMyName();
  void announce();
  void sendReady();
  void sendGo();
  void sendState();
  void handlePacket(const Duel::Packet& p, const uint8_t* mac);
  int findPlayer(const uint8_t* mac) const;
  int addPlayer(const uint8_t* mac, const char* name, uint32_t now);
  void prunePlayers(uint32_t now);
  void startRound(uint32_t now);
  void endRound(uint32_t now);
  void finishRound(uint32_t now);
  void updateKing(uint32_t dt);

  // Графика
  // Graphics
  // Крупный номер: 0 = ещё никто не держал корону (самое начало раунда),
  // иначе 1..N — место по времени удержания короны (1 — лидер).
  // Big number: 0 = nobody has held the crown yet (very start of a round),
  // otherwise 1..N = rank by crown time (1 = leader).
  void drawBigNumber(int n, int y = 24);
  // Флаг/вымпел (символ скина) — на стартовом экране и у победителя.
  // Flag/pennant (the skin symbol) — on the start screen and next to the winner.
  void drawFlag(int cx, int cy);
  void drawTimerBar(uint32_t remainMs);
  void drawPowerBar();
  void drawCentered(int y, const char* s);
  void drawMarquee(int y, const char* s, int x0, int x1);
  void drawWait();
  void drawReady();
  void drawRound();
  void drawResult();
  // Место по времени удержания короны: 0 = ещё никто не держал корону
  // (начало раунда), иначе 1..N (1 — лидер).
  // Rank by crown time: 0 = nobody has held the crown yet (start of a round),
  // otherwise 1..N (1 = leader).
  int myRank() const;
  void spawnFirework();
};
