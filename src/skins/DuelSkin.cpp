#include "DuelSkin.h"
#include "../Config.h"
#include "../duel/DuelNet.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../util/MathUtils.h"
#include <esp_system.h>
#include <math.h>

namespace {
  // Геометрия каната на экране 72x40
  // Tug-of-war geometry on the 72x40 screen
  constexpr int ROPE_Y      = 18;
  constexpr int ROPE_X0     = 6;
  constexpr int ROPE_X1     = 66;
  constexpr int CENTER_X    = (ROPE_X0 + ROPE_X1) / 2;
  constexpr int MAX_OFFSET  = (ROPE_X1 - ROPE_X0) / 2 - 4; // запас под узел
  constexpr float PX_PER_CLICK = 0.45f; // пикселей смещения узла за клик перевеса

  // Ключ счётчика побед Duel (основной "cntDuel" — клики)
  // NVS key of the Duel win counter (the main "cntDuel" keeps clicks)
  constexpr const char* DUEL_WINS_KEY = "duelWins";
}

void DuelSkin::onEnter() {
  buildMyName();
  esp_efuse_mac_get_default(myMac_);
  for (auto& o : opps_) o.valid = false;
  highlight_ = -1;
  peerIndex_ = -1;
  duelId_ = 0;
  myClicks_ = 0;
  foeClicks_ = 0;
  holdFired_ = false;
  nextNetRetry_ = 0;
  now_ = millis();
  lastActivity_ = now_; // окно активного поиска стартует с входа в скин
  DuelNet::begin();
  counter_.begin(counterKey()); // клики — персистентный счётчик, как в других скинах
  setPhase(SEARCH, now_);
  nextAnnounce_ = 0;
  announce(); // сразу сообщаем, что мы в дуэли
}

void DuelSkin::onExit() {
  counter_.flush(); // сохраним несохранённые клики
  DuelNet::end();
}

void DuelSkin::flushCounters() {
  counter_.flush();
}

uint32_t DuelSkin::statsSubCounter() const {
  return SettingsStore::loadCounter(DUEL_WINS_KEY);
}

void DuelSkin::setPhase(Phase p, uint32_t now) {
  phase_ = p;
  phaseStart_ = now;
  // Прокрутка длинных имён начинается заново при смене экрана
  // Marquee scrolling of long names restarts whenever the screen changes
  scrollInit_ = false;
  scrollOffset_ = 0;
}

void DuelSkin::buildMyName() {
  String nick = SettingsStore::loadNick();
  if (nick.length() > 0) {
    strncpy(myName_, nick.c_str(), Duel::MAX_NAME);
    myName_[Duel::MAX_NAME] = '\0';
  } else {
    // Дефолт по MAC: Clicker-XXXX / MAC-based default: Clicker-XXXX
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(myName_, Duel::MAX_NAME + 1, "Clicker-%02X%02X", mac[4], mac[5]);
  }
}

void DuelSkin::announce() {
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Duel::OP_ANNOUNCE;
  p.stamp = now_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::sendBroadcast(p);
}

void DuelSkin::pollInput(uint32_t now, bool menuPressed, bool clickReleased,
                         uint32_t clickHeldMs) {
  now_ = now;

  // Удержание клика: вызов оппонента (SEARCH) или отказ (CHALLENGE_IN)
  // Click hold: challenge an opponent (SEARCH) or decline (CHALLENGE_IN)
  if (clickHeldMs >= Duel::CHALLENGE_HOLD_MS && !holdFired_) {
    holdFired_ = true;
    if (phase_ == SEARCH) {
      if (highlight_ >= 0 && opps_[highlight_].valid) {
        sendChallenge();
      }
    } else if (phase_ == CHALLENGE_IN) {
      sendAccept(false); // отказ на входящий вызов
      setPhase(SEARCH, now);
    }
  }
  if (clickHeldMs == 0) holdFired_ = false;

  // Листаем ПО ОТПУСКАНИЮ, а не по нажатию — иначе при удержании для вызова
  // оппонент успевал бы смениться до срабатывания вызова
  // Paging happens on RELEASE, not on press: otherwise holding for a challenge
  // would switch the opponent before the challenge fires
  if (clickReleased) {
    if (phase_ == SEARCH) {
      selectNextOpponent();
    } else if (phase_ == CHALLENGE_IN) {
      acceptChallenge();
    }
  }

  // Короткое нажатие кнопки меню — отмена текущего действия
  // A short menu-button press cancels the current action
  if (menuPressed) {
    lastActivity_ = now; // тоже активность
    if (phase_ == WAIT_ACCEPT || phase_ == CHALLENGE_IN) {
      setPhase(SEARCH, now);
    }
  }
}

void DuelSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;
  if (clickPressed) lastActivity_ = now; // любой клик сбрасывает окно поиска

  // Само-восстановление радио: если кто-то выключил WiFi мимо нас
  // (например, режим статистики), переинициализируем ESP-NOW.
  // Radio self-healing: if WiFi was switched off behind our back (the stats
  // screen does that), ESP-NOW is re-initialised.
  if (!DuelNet::isRunning() && now >= nextNetRetry_) {
    DuelNet::begin();
    nextNetRetry_ = now + 1000;
  }

  // Вычитываем всё, что пришло по ESP-NOW
  // Drain everything that arrived over ESP-NOW
  Duel::Packet p;
  uint8_t mac[6];
  while (DuelNet::poll(p, mac)) {
    handlePacket(p, mac);
  }

  // Маячок — только пока активно ищем: занятое устройство (ждёт ответа, решает
  // про вызов, в дуэли) молчит, чтобы не путаться с чужими вызовами.
  // The beacon runs only while actively searching: a busy device (waiting for an
  // answer, handling a challenge, duelling) stays silent to avoid confusion.
  if (phase_ == SEARCH) {
    if (now >= nextAnnounce_) {
      announce();
      // Оппонентов рядом нет — маячим реже, чтобы не жечь батарею.
      // Nobody around — beacon less often to save battery.
      bool found = false;
      for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
        if (opps_[i].valid) { found = true; break; }
      }
      nextAnnounce_ = now + (found ? Duel::ANNOUNCE_INTERVAL_MS : Duel::ANNOUNCE_IDLE_MS);
    }
  }

  switch (phase_) {
    case SEARCH: {
      pruneOpponents(now);
      break;
    }
    case WAIT_ACCEPT: {
      if (now - phaseStart_ >= Duel::CHALLENGE_TIMEOUT_MS) {
        setPhase(SEARCH, now); // не ответили — возвращаемся в поиск
      }
      break;
    }
    case CHALLENGE_IN: {
      if (now - phaseStart_ >= Duel::CHALLENGE_TIMEOUT_MS) {
        setPhase(SEARCH, now);
      }
      break;
    }
    case DUEL: {
      if (clickPressed) {
        myClicks_++;
        counter_.pop(now, SCREEN_WIDTH / 2, 30);
      }
      if (now - lastStateSent_ >= Duel::STATE_INTERVAL_MS) {
        sendState();
        lastStateSent_ = now;
      }
      // Соперник перестал отвечать — прерываем дуэль БЕЗ победителя,
      // чтобы оставшийся игрок не "накликал" нечестную победу.
      // The opponent went silent: abort the duel with NO winner, so the remaining
      // player cannot click themselves an unfair victory.
      if (now - lastFoeState_ >= Duel::DUEL_DISCONNECT_MS) {
        foeGone_ = true;
        myClicks_ = 0;
        foeClicks_ = 0;
        setPhase(RESULT, now);
      } else if (now - duelStart_ >= Duel::DUEL_DURATION_MS) {
        finalize(now);
      }
      break;
    }
    case END_WAIT: {
      if (now - phaseStart_ >= Duel::END_GRACE_MS) {
        // Победителя решаем по последним известным счётам
        // The winner is decided by the last known scores
        won_ = myClicks_ > foeClicks_;
        draw_ = (myClicks_ == foeClicks_);
        // Выигранные дуэли копим в персистентный счётчик побед (duelWins),
        // он идёт в статистику как "клики/победы"; ничьи и обрывы не считаем
        // Won duels go into the persistent win counter (duelWins), shown in the
        // stats as "clicks/wins"; draws and aborts are not counted
        if (won_ && !draw_ && !foeGone_) {
          uint32_t wins = SettingsStore::loadCounter(DUEL_WINS_KEY) + 1;
          SettingsStore::saveCounter(DUEL_WINS_KEY, wins);
          spawnFirework(); // стартовый залп салюта
          fireworkNext_ = now + 350 + getRnd(0, 250);
        }
        sendResult();
        setPhase(RESULT, now);
      }
      break;
    }
    case RESULT: {
      // Салют: залпы, пока висит экран результата
      // Fireworks: more salvos while the result screen is up
      if (won_ && !draw_ && !foeGone_) {
        fireworks_.update();
        if (now >= fireworkNext_) {
          spawnFirework();
          fireworkNext_ = now + 350 + getRnd(0, 250);
        }
      }
      if (now - phaseStart_ >= Duel::RESULT_SHOW_MS) {
        myClicks_ = 0;
        foeClicks_ = 0;
        foeGone_ = false;
        setPhase(SEARCH, now);
        nextAnnounce_ = 0; // сразу "я свободен"
      }
      break;
    }
  }
}

void DuelSkin::handlePacket(const Duel::Packet& p, const uint8_t* mac) {
  // Не слушаем сами себя
  // Do not listen to ourselves
  if (memcmp(mac, myMac_, 6) == 0) return;

  switch (p.op) {
    case Duel::OP_ANNOUNCE: {
      addOpponent(mac, p.name, now_);
      break;
    }
    case Duel::OP_CHALLENGE: {
      if (phase_ == SEARCH) {
        int idx = findOpponent(mac);
        if (idx < 0) idx = addOpponent(mac, p.name, now_);
        peerIndex_ = idx;
        duelId_ = p.duelId;
        setPhase(CHALLENGE_IN, now_);
      } else if (phase_ == WAIT_ACCEPT) {
        // Взаимный вызов: оба нажали "вызвать" одновременно.
        // Чтобы обе стороны использовали один duelId, "хозяином" считается
        // устройство с большим MAC — его duelId и принимаем для обмена.
        // Mutual challenge: both pressed "challenge" at the same time.
        // So that both sides use a single duelId, the device with the larger MAC
        // is the "owner" — its duelId is accepted for the exchange.
        int idx = findOpponent(mac);
        if (idx >= 0 && idx == peerIndex_) {
          if (memcmp(mac, myMac_, 6) > 0) {
            duelId_ = p.duelId; // соперник "хозяин" — берём его id
            sendAcceptTo(mac, p.duelId, true);
          } else {
            sendAcceptTo(mac, duelId_, true); // мы "хозяин" — свой id
          }
          startDuel(now_);
        }
      } else {
        // Мы заняты дуэлью — вежливо отказываем
        // Already duelling — politely decline
        sendAcceptTo(mac, p.duelId, false);
      }
      break;
    }
    case Duel::OP_ACCEPT: {
      if (phase_ == WAIT_ACCEPT && peerIndex_ >= 0 &&
          memcmp(mac, opps_[peerIndex_].mac, 6) == 0 && p.duelId == duelId_) {
        if (p.flags) startDuel(now_);
        else setPhase(SEARCH, now_);
      }
      break;
    }
    case Duel::OP_STATE: {
      if ((phase_ == DUEL || phase_ == END_WAIT) && peerIndex_ >= 0 &&
          memcmp(mac, opps_[peerIndex_].mac, 6) == 0 && p.duelId == duelId_) {
        foeClicks_ = p.clicks;
        lastFoeState_ = now_; // соперник на связи
      }
      break;
    }
    case Duel::OP_RESULT: {
      if (phase_ == END_WAIT && peerIndex_ >= 0 &&
          memcmp(mac, opps_[peerIndex_].mac, 6) == 0) {
        foeClicks_ = p.clicks; // финальный счёт соперника
      }
      break;
    }
  }
}

int DuelSkin::findOpponent(const uint8_t* mac) const {
  for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
    if (opps_[i].valid && memcmp(opps_[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

int DuelSkin::addOpponent(const uint8_t* mac, const char* name, uint32_t now) {
  int idx = findOpponent(mac);
  if (idx >= 0) {
    opps_[idx].lastSeen = now;
    strncpy(opps_[idx].name, name, Duel::MAX_NAME);
    opps_[idx].name[Duel::MAX_NAME] = '\0';
    return idx;
  }
  // Ищем свободный слот
  // Look for a free slot
  for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
    if (!opps_[i].valid) {
      memcpy(opps_[i].mac, mac, 6);
      strncpy(opps_[i].name, name, Duel::MAX_NAME);
      opps_[i].name[Duel::MAX_NAME] = '\0';
      opps_[i].lastSeen = now;
      opps_[i].valid = true;
      if (highlight_ < 0) highlight_ = i; // подсвечиваем первого найденного
      return i;
    }
  }
  // Всё занято — заменяем самого старого
  // All slots taken — replace the oldest entry
  int oldest = 0;
  for (int i = 1; i < Duel::MAX_OPPONENTS; i++) {
    if (opps_[i].lastSeen < opps_[oldest].lastSeen) oldest = i;
  }
  memcpy(opps_[oldest].mac, mac, 6);
  strncpy(opps_[oldest].name, name, Duel::MAX_NAME);
  opps_[oldest].name[Duel::MAX_NAME] = '\0';
  opps_[oldest].lastSeen = now;
  opps_[oldest].valid = true;
  return oldest;
}

void DuelSkin::removeOpponent(int i) {
  opps_[i].valid = false;
  if (highlight_ == i) highlight_ = -1;
  if (peerIndex_ == i) peerIndex_ = -1;
  if (highlight_ < 0) {
    for (int k = 0; k < Duel::MAX_OPPONENTS; k++) {
      if (opps_[k].valid) {
        highlight_ = k;
        break;
      }
    }
  }
}

void DuelSkin::pruneOpponents(uint32_t now) {
  for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
    if (opps_[i].valid && now - opps_[i].lastSeen >= Duel::OPPONENT_TIMEOUT_MS) {
      removeOpponent(i);
    }
  }
}

void DuelSkin::selectNextOpponent() {
  scrollInit_ = false; // имя сменилось — прокрутка заново
  scrollOffset_ = 0;
  if (highlight_ < 0) {
    for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
      if (opps_[i].valid) {
        highlight_ = i;
        return;
      }
    }
    return;
  }
  int start = highlight_;
  for (int k = 1; k <= Duel::MAX_OPPONENTS; k++) {
    int i = (start + k) % Duel::MAX_OPPONENTS;
    if (opps_[i].valid) {
      highlight_ = i;
      return;
    }
  }
}

void DuelSkin::sendChallenge() {
  peerIndex_ = highlight_;
  duelId_ = (uint32_t)random(1, 0x7FFFFFFF);
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Duel::OP_CHALLENGE;
  p.duelId = duelId_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::send(opps_[peerIndex_].mac, p);
  setPhase(WAIT_ACCEPT, now_);
}

void DuelSkin::acceptChallenge() {
  sendAccept(true);
  startDuel(now_);
}

void DuelSkin::sendAccept(bool ok) {
  if (peerIndex_ < 0) return;
  sendAcceptTo(opps_[peerIndex_].mac, duelId_, ok);
}

void DuelSkin::sendAcceptTo(const uint8_t* mac, uint32_t duelId, bool ok) {
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Duel::OP_ACCEPT;
  p.flags = ok ? 1 : 0;
  p.duelId = duelId;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::send(mac, p);
}

void DuelSkin::startDuel(uint32_t now) {
  myClicks_ = 0;
  foeClicks_ = 0;
  foeGone_ = false;      // считаем, что соперник на связи
  lastFoeState_ = now;   // стартовый запас до срабатывания "связь потеряна"
  duelStart_ = now;
  lastStateSent_ = 0;
  setPhase(DUEL, now);
  sendState(); // сразу стартовый счёт
}

void DuelSkin::sendState() {
  if (peerIndex_ < 0) return;
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Duel::OP_STATE;
  p.duelId = duelId_;
  p.clicks = myClicks_;
  p.stamp = now_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::send(opps_[peerIndex_].mac, p);
}

void DuelSkin::finalize(uint32_t now) {
  sendState(); // последний счёт перед финалом
  setPhase(END_WAIT, now);
}

void DuelSkin::sendResult() {
  if (peerIndex_ < 0) return;
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Duel::OP_RESULT;
  p.duelId = duelId_;
  p.clicks = myClicks_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::send(opps_[peerIndex_].mac, p);
}

// Один залп салюта: случайная точка в верхней части экрана
// One firework salvo: a random point in the upper part of the screen
void DuelSkin::spawnFirework() {
  int fx = getRnd(12, SCREEN_WIDTH - 12);
  int fy = getRnd(8, 18);
  uint8_t n = (uint8_t)getRnd(10, 18);
  fireworks_.spawn(fx, fy, n, 0.6f, 2.5f);
}

// ---------- Отрисовка / Drawing ----------

void DuelSkin::drawCentered(int y, const char* s) {
  int w = display.getStrWidth(s);
  display.drawStr((SCREEN_WIDTH - w) / 2, y, s);
}

// Если имя не влезает в [x0, x1) — гоняем его бегущей строкой (как в меню)
// If the name does not fit into [x0, x1) it is scrolled as a marquee (as in the menu)
void DuelSkin::drawMarquee(int y, const char* s, int x0, int x1) {
  if (x1 <= x0) x1 = x0 + 1;
  int w = display.getStrWidth(s);
  int width = x1 - x0;
  if (w <= width) {
    display.drawStr(x0 + (width - w) / 2, y, s);
    return;
  }

  uint32_t now = millis();
  if (!scrollInit_) {
    scrollInit_ = true;
    scrollLast_ = now;
  }
  float dt = (now - scrollLast_) / 1000.0f;
  scrollLast_ = now;
  if (dt > 0 && dt < 0.5f) scrollOffset_ += 18.0f * dt;

  int cycle = width + w;
  scrollOffset_ = fmodf(scrollOffset_, (float)cycle);
  if (scrollOffset_ < 0) scrollOffset_ += cycle;

  // Клип по зоне [x0, x1-1]: бегущая строка не должна выходить за свою
  // область и наезжать на стрелки/таймер за её пределами.
  // Clip to [x0, x1-1]: the marquee must not leave its area and overlap the
  // arrows or the timer outside it.
  display.setClipWindow(x0, 0, x1 - 1, SCREEN_HEIGHT);
  int x = x1 - (int)scrollOffset_;
  display.drawStr(x, y, s);
  display.setClipWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void DuelSkin::draw() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);
  switch (phase_) {
    case SEARCH:       drawSearch(); break;
    case WAIT_ACCEPT:  drawWaiting(); break;
    case CHALLENGE_IN: drawChallengeIn(); break;
    case DUEL:
    case END_WAIT:     drawDuel(); break;
    case RESULT:       drawResult(); break;
  }
}

// ---------- Сон (как у других скинов) / Idle (like other skins) ----------

bool DuelSkin::canIdle() const {
  // Активную дуэль сном не рвём.
  // An active duel is never interrupted by idling.
  if (phase_ == DUEL || phase_ == END_WAIT) return false;
  // В поиске/ожидании сначала SEARCH_ACTIVE_MS (40 с) активно ищем,
  // и лишь затем разрешаем сну показать "ожидание" и выключить устройство.
  // While searching/waiting we first search actively for SEARCH_ACTIVE_MS (40 s);
  // only then may idling show the "waiting" frame and power the device down.
  if (phase_ == SEARCH || phase_ == WAIT_ACCEPT || phase_ == CHALLENGE_IN) {
    return now_ - lastActivity_ >= Duel::SEARCH_ACTIVE_MS;
  }
  return true; // RESULT и пр. — как обычно
}

void DuelSkin::onIdleEnter() {
  counter_.flush(); // фиксируем клики перед сном
  // Радио на idle-экране не нужно — выключаем ESP-NOW и WiFi целиком.
  // The radio is not needed on the idle screen — shut ESP-NOW and WiFi down.
  DuelNet::end();
}

void DuelSkin::drawIdle() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);
  display.drawStr(2, 8, "DUEL");
  drawCentered(20, "waiting...");
  // Анимированные точки, как на экране поиска
  // Animated dots, same as on the search screen
  char buf[8];
  int dots = (millis() / 500) % 4;
  for (int i = 0; i < dots; i++) buf[i] = '.';
  buf[dots] = '\0';
  drawCentered(30, buf);
}

void DuelSkin::onWake(uint32_t now) {
  // Поднимаем радио снова и сразу сообщаем о себе.
  // Bring the radio back up and announce ourselves right away.
  if (!DuelNet::isRunning()) DuelNet::begin();
  nextAnnounce_ = 0;
  lastActivity_ = now;
}

void DuelSkin::drawSearch() {
  display.drawStr(2, 8, "DUEL");

  int found = 0;
  for (int i = 0; i < Duel::MAX_OPPONENTS; i++) {
    if (opps_[i].valid) found++;
  }

  if (found == 0) {
    drawCentered(20, "searching...");
    char buf[8];
    int dots = (now_ / 500) % 4;
    for (int i = 0; i < dots; i++) buf[i] = '.';
    buf[dots] = '\0';
    drawCentered(30, buf);
    return;
  }

  int hi = (highlight_ >= 0 && opps_[highlight_].valid) ? highlight_ : 0;
  // Длинное имя — бегущая строка между стрелками
  // A long name scrolls as a marquee between the arrows
  drawMarquee(21, opps_[hi].name, 8, SCREEN_WIDTH - 8);

  // Стрелки листания
  // Paging arrows
  display.drawTriangle(2, 17, 2, 23, 6, 20);
  display.drawTriangle(SCREEN_WIDTH - 3, 17, SCREEN_WIDTH - 3, 23, SCREEN_WIDTH - 7, 20);

  drawCentered(30, "click - next");
  drawCentered(39, "hold - fight");
}

void DuelSkin::drawWaiting() {
  display.drawStr(2, 8, "DUEL");
  drawCentered(17, "waiting...");
  if (peerIndex_ >= 0) {
    drawMarquee(27, opps_[peerIndex_].name, 0, SCREEN_WIDTH);
  }
  drawCentered(37, "M - cancel");
}

void DuelSkin::drawChallengeIn() {
  display.drawStr(2, 8, "DUEL");
  if (peerIndex_ >= 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s vs YOU", opps_[peerIndex_].name);
    drawMarquee(18, buf, 0, SCREEN_WIDTH);
  }
  drawCentered(28, "click - accept");
  drawCentered(38, "hold - refuse");
}

void DuelSkin::drawDuel() {
  // Верх: имя соперника (бегущая строка; правый верхний угол — под
  // всплывающую цифру кликов)
  // Top: opponent name (a marquee; the top-right corner stays free for the
  // floating click counter)
  const char* foeName = (peerIndex_ >= 0) ? opps_[peerIndex_].name : "?";
  drawMarquee(8, foeName, 0, SCREEN_WIDTH - 22);
  counter_.draw(now_); // всплывающая цифра кликов (как в других скинах)

  // Канат: узел едет в сторону того, кто впереди по кликам
  // The rope: the knot moves towards whoever is ahead in clicks
  int diff = (int)myClicks_ - (int)foeClicks_;
  int off = constrain((int)(-diff * PX_PER_CLICK), -MAX_OFFSET, MAX_OFFSET);
  int knotX = CENTER_X + off;

  display.drawHLine(ROPE_X0, ROPE_Y, ROPE_X1 - ROPE_X0); // верёвка
  display.drawDisc(knotX, ROPE_Y, 2);                    // узел
  // Флажки: слева мы, справа соперник
  // Flags: ours on the left, the opponent's on the right
  display.drawTriangle(ROPE_X0, ROPE_Y - 3, ROPE_X0, ROPE_Y + 3, ROPE_X0 + 5, ROPE_Y);
  display.drawTriangle(ROPE_X1, ROPE_Y - 3, ROPE_X1, ROPE_Y + 3, ROPE_X1 - 5, ROPE_Y);

  // Счёт внизу
  // The score at the bottom
  char buf[16];
  snprintf(buf, sizeof(buf), "you %lu", (unsigned long)myClicks_);
  display.drawStr(2, 29, buf);
  snprintf(buf, sizeof(buf), "foe %lu", (unsigned long)foeClicks_);
  display.drawStr(2, 38, buf);

  // Оставшееся время — внизу справа (сверху его место занято цифрой кликов)
  // Time left sits in the bottom-right corner (the top one is taken by the click counter)
  uint32_t remain = Duel::DUEL_DURATION_MS - (now_ - duelStart_);
  if (now_ - duelStart_ >= Duel::DUEL_DURATION_MS) remain = 0;
  int sec = (int)((remain + 999) / 1000);
  char tb[8];
  snprintf(tb, sizeof(tb), "%ds", sec);
  int tw = display.getStrWidth(tb);
  display.drawStr(SCREEN_WIDTH - tw - 2, 38, tb);
}

void DuelSkin::drawResult() {
  display.drawStr(2, 8, "DUEL");

  // Салют рисуем ПОД текстом результата
  // Fireworks are drawn UNDER the result text
  if (won_ && !draw_ && !foeGone_) {
    fireworks_.draw();
  }

  // Итог рисуем ниже, чтобы не наезжало на "DUEL" сверху
  // The result goes lower so it does not overlap "DUEL" at the top
  display.setFont(u8g2_font_7x13_tf);
  const char* line;
  if (foeGone_) {
    line = "CONN LOST";
  } else if (draw_) {
    line = "DRAW";
  } else {
    line = won_ ? "YOU WIN!" : "YOU LOSE";
  }
  drawCentered(21, line);

  display.setFont(u8g2_font_5x7_tf);
  if (foeGone_) {
    drawCentered(31, "opponent left");
  } else {
    char buf[16];
    snprintf(buf, sizeof(buf), "you %lu", (unsigned long)myClicks_);
    drawCentered(31, buf);
    snprintf(buf, sizeof(buf), "foe %lu", (unsigned long)foeClicks_);
    drawCentered(38, buf);
  }
}
