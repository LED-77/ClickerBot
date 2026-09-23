#pragma once
#include "Skin.h"
#include "../duel/DuelProtocol.h"
#include "../fx/SparkleFX.h"
#include "../fx/PopCounter.h"

// "Дуэль": перетягивание каната между двумя кликерами по ESP-NOW.
// Фазы: SEARCH (маячок + список оппонентов; клик — листать, удержание ~1 с —
// вызов) -> WAIT_ACCEPT (ждём ответа, меню — отмена) -> CHALLENGE_IN (клик =
// принять, удержание = отказать) -> DUEL (15 с, кто чаще кликает) ->
// END_WAIT (пауза под финальный счёт) -> RESULT -> снова SEARCH.
// "Duel": a tug of war between two clickers over ESP-NOW.
// Phases: SEARCH (beacon + opponent list; click pages, ~1 s hold challenges) ->
// WAIT_ACCEPT (waiting for an answer, menu cancels) -> CHALLENGE_IN (click =
// accept, hold = decline) -> DUEL (15 s, whoever clicks more wins) ->
// END_WAIT (pause for the final score) -> RESULT -> SEARCH again.
class DuelSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void pollInput(uint32_t now, bool menuPressed, bool clickReleased,
                 uint32_t clickHeldMs) override;
  void draw() override;

  // Сон как у других скинов: 20 с бездействия -> минимальная картинка ->
  // ещё 20 с -> deep sleep. Во время активной дуэли сон блокируем.
  // Idle behaves like in other skins: 20 s of inactivity -> minimal frame ->
  // another 20 s -> deep sleep. An active duel blocks idling.
  void onIdleEnter() override;
  void drawIdle() override;
  void onWake(uint32_t now) override;
  bool canIdle() const override;

  const char* name() const override { return "Duel"; }
  const char* counterKey() const override { return "cntDuel"; }
  const char* wireKey() const override { return "duel"; }

  // Командный скин — не попадает в режим Random
  // Multiplayer skin — never picked in Random mode
  bool isMultiplayer() const override { return true; }

  // Статистика: основной счётчик — клики (cntDuel), доп. — победы (duelWins)
  // Statistics: main counter = clicks (cntDuel), secondary = wins (duelWins)
  uint32_t statsSubCounter() const override;

private:
  enum Phase {
    SEARCH,
    WAIT_ACCEPT,
    CHALLENGE_IN,
    DUEL,
    END_WAIT,
    RESULT
  };

  struct Opponent {
    uint8_t mac[6];
    char name[Duel::MAX_NAME + 1];
    uint32_t lastSeen;
    bool valid;
  };

  Phase phase_ = SEARCH;
  Opponent opps_[Duel::MAX_OPPONENTS];
  int highlight_ = -1; // подсвеченный оппонент в списке
  int peerIndex_ = -1; // текущий пир (вызов/дуэль/результат)

  uint32_t nextAnnounce_ = 0;
  uint32_t phaseStart_ = 0;
  uint32_t duelStart_ = 0;
  uint32_t lastStateSent_ = 0;
  uint32_t nextNetRetry_ = 0;
  uint32_t lastActivity_ = 0; // сброс окна активного поиска (клик/вход)

  uint32_t myClicks_ = 0;
  uint32_t foeClicks_ = 0;
  uint32_t duelId_ = 0;
  bool holdFired_ = false; // чтобы вызов сработал один раз за удержание
  bool won_ = false;
  bool draw_ = false;
  bool foeGone_ = false;        // соперник пропал из эфира — дуэль прервана
  uint32_t lastFoeState_ = 0;   // когда последний раз принимали пакеты соперника

  SparkleFX fireworks_;         // салют победителю
  uint32_t fireworkNext_ = 0;   // когда запускать следующий залп

  PopCounter counter_;          // анимированный счётчик кликов (как в других скинах)

  char myName_[Duel::MAX_NAME + 1];
  uint8_t myMac_[6];
  uint32_t now_ = 0;

  // Прокрутка длинных имён (бегущая строка)
  // Marquee scrolling for long names
  float scrollOffset_ = 0;
  uint32_t scrollLast_ = 0;
  bool scrollInit_ = false;

  void setPhase(Phase p, uint32_t now);
  void buildMyName();
  void announce();
  void sendState();
  void sendAccept(bool ok);
  void sendAcceptTo(const uint8_t* mac, uint32_t duelId, bool ok);
  void sendResult();
  void sendChallenge();
  void acceptChallenge();
  void startDuel(uint32_t now);
  void finalize(uint32_t now);
  void spawnFirework();

  void handlePacket(const Duel::Packet& p, const uint8_t* mac);
  int findOpponent(const uint8_t* mac) const;
  int addOpponent(const uint8_t* mac, const char* name, uint32_t now);
  void removeOpponent(int i);
  void pruneOpponents(uint32_t now);
  void selectNextOpponent();

  void drawCentered(int y, const char* s);
  // Бегущая строка для длинных имён: если текст не влезает в [x0, x1) —
  // прокручивается, иначе центрируется.
  // Marquee for long names: if the text does not fit into [x0, x1) it scrolls,
  // otherwise it is centred.
  void drawMarquee(int y, const char* s, int x0, int x1);
  void drawSearch();
  void drawWaiting();
  void drawChallengeIn();
  void drawDuel();
  void drawResult();
};
