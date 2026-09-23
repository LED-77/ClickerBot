#include "KothSkin.h"
#include "../Config.h"
#include "../duel/DuelNet.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../util/MathUtils.h"
#include <esp_system.h>
#include <math.h>

namespace {
  // Ключ счётчика побед KOTH (основной "cntKoth" — клики)
  // NVS key of the KOTH win counter (the main "cntKoth" keeps clicks)
  constexpr const char* KOTH_WINS_KEY = "kothWins";
}

void KothSkin::onEnter() {
  buildMyName();
  esp_efuse_mac_get_default(myMac_);
  for (auto& pl : players_) { pl.valid = false; pl.hasState = false; pl.isKing = false; }
  for (uint8_t i = 0; i < Koth::POWER_SLOTS; i++) powerSlots_[i] = 0;
  powerSlotIndex_ = 0;
  myPower_ = 0;
  myClicks_ = 0;
  myKingTime_ = 0;
  iAmKing_ = false;
  holdFired_ = false;
  lastNetRetry_ = 0;
  now_ = millis();
  DuelNet::begin();
  counter_.begin(counterKey()); // клики — персистентный счётчик, как в других скинах
  setPhase(WAIT, now_);
  nextAnnounce_ = 0;
  announce();
}

void KothSkin::onExit() {
  counter_.flush(); // сохраним несохранённые клики
  DuelNet::end();
}

void KothSkin::flushCounters() {
  counter_.flush();
}

uint32_t KothSkin::statsSubCounter() const {
  return SettingsStore::loadCounter(KOTH_WINS_KEY);
}

void KothSkin::setPhase(Phase p, uint32_t now) {
  phase_ = p;
  phaseStart_ = now;
  scrollInit_ = false;
  scrollOffset_ = 0;
}

void KothSkin::buildMyName() {
  String nick = SettingsStore::loadNick();
  if (nick.length() > 0) {
    strncpy(myName_, nick.c_str(), Duel::MAX_NAME);
    myName_[Duel::MAX_NAME] = '\0';
  } else {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(myName_, Duel::MAX_NAME + 1, "Clicker-%02X%02X", mac[4], mac[5]);
  }
}

void KothSkin::announce() {
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Koth::OP_ANNOUNCE;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::sendBroadcast(p);
}

void KothSkin::sendReady() {
  roundId_ = (uint32_t)random(1, 0x7FFFFFFF);
  readyDelay_ = (uint32_t)getRnd(Koth::READY_DELAY_MIN_MS, Koth::READY_DELAY_MAX_MS + 1);
  startedByMe_ = true;
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Koth::OP_READY;
  p.duelId = roundId_;
  p.clicks = readyDelay_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::sendBroadcast(p);
  setPhase(READY, now_);
}

void KothSkin::sendGo() {
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Koth::OP_GO;
  p.duelId = roundId_;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::sendBroadcast(p);
}

void KothSkin::sendState() {
  Duel::Packet p;
  memset(&p, 0, sizeof(p));
  p.proto = Duel::PROTO;
  p.op = Koth::OP_STATE;
  p.duelId = roundId_;
  p.clicks = myPower_;
  p.stamp = myKingTime_;
  p.flags = iAmKing_ ? Koth::FLAG_IS_KING : 0;
  strncpy(p.name, myName_, Duel::MAX_NAME);
  DuelNet::sendBroadcast(p);
}

void KothSkin::handlePacket(const Duel::Packet& p, const uint8_t* mac) {
  if (memcmp(mac, myMac_, 6) == 0) return;

  switch (p.op) {
    case Koth::OP_ANNOUNCE: {
      addPlayer(mac, p.name, now_);
      break;
    }
    case Koth::OP_READY: {
      if (phase_ == WAIT) {
        roundId_ = p.duelId;
        readyDelay_ = p.clicks;
        startedByMe_ = false;
        setPhase(READY, now_);
      }
      break;
    }
    case Koth::OP_GO: {
      if (phase_ == READY && p.duelId == roundId_) {
        startRound(now_);
      }
      break;
    }
    case Koth::OP_STATE: {
      if (p.duelId != roundId_) break; // только текущий раунд
      int idx = findPlayer(mac);
      if (idx < 0) idx = addPlayer(mac, p.name, now_);
      if (idx >= 0) {
        players_[idx].lastSeen = now_;
        players_[idx].power = p.clicks;
        players_[idx].kingTime = p.stamp;
        players_[idx].isKing = (p.flags & Koth::FLAG_IS_KING) != 0;
        players_[idx].hasState = true;
        strncpy(players_[idx].name, p.name, Duel::MAX_NAME);
        players_[idx].name[Duel::MAX_NAME] = '\0';
      }
      break;
    }
  }
}

int KothSkin::findPlayer(const uint8_t* mac) const {
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (players_[i].valid && memcmp(players_[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

int KothSkin::addPlayer(const uint8_t* mac, const char* name, uint32_t now) {
  int idx = findPlayer(mac);
  if (idx >= 0) {
    players_[idx].lastSeen = now;
    strncpy(players_[idx].name, name, Duel::MAX_NAME);
    players_[idx].name[Duel::MAX_NAME] = '\0';
    return idx;
  }
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (!players_[i].valid) {
      memcpy(players_[i].mac, mac, 6);
      strncpy(players_[i].name, name, Duel::MAX_NAME);
      players_[i].name[Duel::MAX_NAME] = '\0';
      players_[i].lastSeen = now;
      players_[i].power = 0;
      players_[i].kingTime = 0;
      players_[i].isKing = false;
      players_[i].valid = true;
      players_[i].hasState = false;
      return i;
    }
  }
  // Всё занято — заменяем самого старого
  // All slots taken — replace the oldest entry
  int oldest = 0;
  for (int i = 1; i < Koth::MAX_PLAYERS; i++) {
    if (players_[i].lastSeen < players_[oldest].lastSeen) oldest = i;
  }
  memcpy(players_[oldest].mac, mac, 6);
  strncpy(players_[oldest].name, name, Duel::MAX_NAME);
  players_[oldest].name[Duel::MAX_NAME] = '\0';
  players_[oldest].lastSeen = now;
  players_[oldest].power = 0;
  players_[oldest].kingTime = 0;
  players_[oldest].isKing = false;
  players_[oldest].valid = true;
  players_[oldest].hasState = false;
  return oldest;
}

void KothSkin::prunePlayers(uint32_t now) {
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (players_[i].valid && now - players_[i].lastSeen >= Koth::PLAYER_TIMEOUT_MS) {
      players_[i].valid = false;
      players_[i].hasState = false;
    }
  }
}

void KothSkin::startRound(uint32_t now) {
  for (uint8_t i = 0; i < Koth::POWER_SLOTS; i++) powerSlots_[i] = 0;
  powerSlotIndex_ = 0;
  myPower_ = 0;
  myClicks_ = 0;
  myKingTime_ = 0;
  iAmKing_ = false;
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) { players_[i].hasState = false; players_[i].isKing = false; }
  roundStart_ = now;
  lastTick_ = now;
  setPhase(ROUND, now);
  nextTick_ = now; // первый такт — сразу
}

void KothSkin::endRound(uint32_t now) {
  sendState(); // финальное состояние
  setPhase(END_WAIT, now);
}

void KothSkin::finishRound(uint32_t now) {
  // Победитель = максимум времени короля; при равенстве — меньший MAC
  // The winner has the longest crown time; ties go to the smaller MAC
  uint32_t bestTime = myKingTime_;
  const uint8_t* bestMac = myMac_;
  const char* bestName = myName_;
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (!players_[i].valid || !players_[i].hasState) continue;
    if (players_[i].kingTime > bestTime ||
        (players_[i].kingTime == bestTime && memcmp(players_[i].mac, bestMac, 6) < 0)) {
      bestTime = players_[i].kingTime;
      bestMac = players_[i].mac;
      bestName = players_[i].name;
    }
  }
  iWon_ = (memcmp(bestMac, myMac_, 6) == 0);
  winnerTime_ = bestTime;
  strncpy(winnerName_, bestName, Duel::MAX_NAME);
  winnerName_[Duel::MAX_NAME] = '\0';
  haveWinner_ = true;

  // Победы копим в отдельный счётчик — статистика покажет "клики/победы"
  // Wins go into a separate counter; the stats show "clicks/wins"
  if (iWon_) {
    uint32_t wins = SettingsStore::loadCounter(KOTH_WINS_KEY) + 1;
    SettingsStore::saveCounter(KOTH_WINS_KEY, wins);
    spawnFirework(); // стартовый залп салюта
    fireworkNext_ = now + 350 + getRnd(0, 250);
  }
  setPhase(RESULT, now);
}

void KothSkin::updateKing(uint32_t dt) {
  // Угроза короне — только от того, кто сам кликает (p > 0): большая мощь
  // или равная при меньшем MAC. Затухание собственного окна мощи корону НЕ
  // отнимает, иначе король «слетал» сам, когда его никто не скидывал.
  // Only a player who clicks (p > 0) is a threat to the crown: higher power, or
  // equal power with a smaller MAC. Fading of our own power window does NOT cost
  // us the crown, otherwise the king fell on its own without being dethroned.
  bool threat = false;
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (!players_[i].valid || !players_[i].hasState) continue;
    uint32_t p = players_[i].power;
    if (p > myPower_ || (p == myPower_ && p > 0 &&
                         memcmp(players_[i].mac, myMac_, 6) < 0)) {
      threat = true;
      break;
    }
  }

  if (iAmKing_) {
    // Король остаётся королём, пока его не скинут реальным перекликом
    // The king stays king until somebody really out-clicks them
    if (threat) iAmKing_ = false;
  } else {
    // Не король: влезаем на гору только кликами и только если никто не бьёт
    // Not the king: we climb the hill only by clicking, and only while unchallenged
    if (myPower_ > 0 && !threat) iAmKing_ = true;
  }
  if (iAmKing_) myKingTime_ += dt;
}

void KothSkin::pollInput(uint32_t now, bool menuPressed, bool clickReleased,
                         uint32_t clickHeldMs) {
  now_ = now;

  // Удержание клика в ожидании — начать раунд (только если есть соперники)
  // Holding the click while waiting starts a round (only if others are present)
  if (clickHeldMs >= Koth::START_HOLD_MS && !holdFired_) {
    holdFired_ = true;
    if (phase_ == WAIT) {
      int others = 0;
      for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
        if (players_[i].valid) others++;
      }
      if (others > 0) {
        sendReady();
      } else {
        needPlayersHint_ = now; // в одиночку не играем — покажем подсказку
      }
    }
  }
  if (clickHeldMs == 0) holdFired_ = false;
}

void KothSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;

  if (!DuelNet::isRunning() && now >= lastNetRetry_) {
    DuelNet::begin();
    lastNetRetry_ = now + 1000;
  }

  Duel::Packet p;
  uint8_t mac[6];
  while (DuelNet::poll(p, mac)) {
    handlePacket(p, mac);
  }

  // Маячок "я в игре" — во время раунда состояния и так идентифицируют игроков.
  // Игроков рядом нет — маячим реже, чтобы не жечь батарею.
  // The "I am playing" beacon — during a round the state packets identify players
  // anyway. With nobody around we beacon less often to save battery.
  if (phase_ != ROUND && phase_ != END_WAIT && now >= nextAnnounce_) {
    announce();
    bool found = false;
    for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
      if (players_[i].valid) { found = true; break; }
    }
    nextAnnounce_ = now + (found ? Koth::ANNOUNCE_INTERVAL_MS : Koth::ANNOUNCE_IDLE_MS);
  }

  switch (phase_) {
    case WAIT: {
      prunePlayers(now);
      break;
    }
    case READY: {
      // Ведущий: по истечении случайной паузы шлёт GO и стартует раунд
      // The host sends GO after a random delay and starts the round
      if (startedByMe_ && now - phaseStart_ >= readyDelay_) {
        sendGo();
        startRound(now);
      }
      // GO не пришёл (потерялся) — возврат в ожидание
      // GO never arrived (lost) — fall back to waiting
      if (now - phaseStart_ >= Koth::READY_TIMEOUT_MS) {
        setPhase(WAIT, now);
        nextAnnounce_ = 0;
      }
      break;
    }
    case ROUND: {
      if (clickPressed) {
        powerSlots_[powerSlotIndex_]++;
        myClicks_++;
        counter_.pop(now, SCREEN_WIDTH / 2, 30);
      }
      if (now >= nextTick_) {
        uint32_t dt = now - lastTick_;
        lastTick_ = now;
        // Продвигаем окно мощи и пересчитываем её
        // Advance the power window and recompute it
        powerSlotIndex_ = (powerSlotIndex_ + 1) % Koth::POWER_SLOTS;
        powerSlots_[powerSlotIndex_] = 0;
        uint32_t sum = 0;
        for (uint8_t i = 0; i < Koth::POWER_SLOTS; i++) sum += powerSlots_[i];
        myPower_ = sum;

        updateKing(dt); // король = макс мощь; если я — коплю время
        sendState();    // разошлём мощь и время короля
        nextTick_ = now + Koth::KOTH_TICK_MS;
      }
      if (now - roundStart_ >= Koth::KOTH_ROUND_MS) {
        endRound(now);
      }
      break;
    }
    case END_WAIT: {
      if (now - phaseStart_ >= Koth::END_GRACE_MS) {
        finishRound(now);
      }
      break;
    }
    case RESULT: {
      // Салют победителю — живёт и выпускает залпы, пока висит экран
      // Fireworks for the winner keep firing while the screen is up
      if (iWon_) {
        fireworks_.update();
        if (now >= fireworkNext_) {
          spawnFirework();
          fireworkNext_ = now + 350 + getRnd(0, 250);
        }
      }
      if (now - phaseStart_ >= Koth::RESULT_SHOW_MS) {
        setPhase(WAIT, now);
        nextAnnounce_ = 0;
      }
      break;
    }
  }
}

// ---------- Сон (20 с + 20 с, как у других скинов) / Idle (20 s + 20 s, like other skins) ----------

bool KothSkin::canIdle() const {
  // Не спим во время раунда; в ожидании — стандартный сон 20 с + 20 с
  // No idling during a round; while waiting the usual 20 s + 20 s applies
  return phase_ == WAIT;
}

void KothSkin::onIdleEnter() {
  counter_.flush();
  // Радио на idle-экране не нужно — выключаем ESP-NOW и WiFi целиком.
  // The radio is not needed on the idle screen — shut ESP-NOW and WiFi down.
  DuelNet::end();
}

void KothSkin::drawIdle() {
  drawFlag(SCREEN_WIDTH / 2, 18); // тот же вымпел — минимальная картинка
}

void KothSkin::onWake(uint32_t now) {
  // Поднимаем радио снова и сразу сообщаем о себе.
  // Bring the radio back up and announce ourselves right away.
  if (!DuelNet::isRunning()) DuelNet::begin();
  nextAnnounce_ = 0; // сразу "я в игре"
}

// ---------- Графика / Graphics ----------

void KothSkin::drawBigNumber(int n, int y) {
  display.setFont(u8g2_font_10x20_tf);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", n);
  int w = display.getStrWidth(buf);
  display.drawStr((SCREEN_WIDTH - w) / 2, y, buf);
  display.setFont(u8g2_font_5x7_tf);
}

void KothSkin::drawFlag(int cx, int cy) {
  // Вымпел (большой треугольный): древко + полотнище + основание
  // A big triangular pennant: pole + flag + base
  display.drawVLine(cx, cy - 9, 19);            // древко (cy-9 .. cy+9)
  const int lenT[7] = {2, 4, 6, 8, 6, 4, 2};    // треугольное полотнище вправо
  for (int i = 0; i < 7; i++) {
    display.drawHLine(cx + 1, cy - 9 + i, lenT[i]);
  }
  display.drawHLine(cx - 5, cy + 9, 11);        // основание
}

// Место по времени удержания короны (по нему же определяется победитель):
// 0 — корону ещё никто не держал, иначе 1..N (1 — лидер).
// Rank by crown time (the winner is decided by it as well):
// 0 — nobody has held the crown yet, otherwise 1..N (1 = leader).
int KothSkin::myRank() const {
  bool anyoneHeld = (myKingTime_ > 0);
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (!players_[i].valid) continue;
    uint32_t t = players_[i].hasState ? players_[i].kingTime : 0;
    if (t > 0) anyoneHeld = true;
  }
  if (!anyoneHeld) return 0; // ещё никто не держал корону

  // Номер = 1 + сколько игроков впереди (больше время короля; при равенстве — меньший MAC)
  // Rank = 1 + how many players are ahead (longer crown time; ties: smaller MAC)
  int ahead = 0;
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (!players_[i].valid) continue;
    uint32_t t = players_[i].hasState ? players_[i].kingTime : 0;
    if (t > myKingTime_) {
      ahead++;
    } else if (t == myKingTime_ && memcmp(players_[i].mac, myMac_, 6) < 0) {
      ahead++;
    }
  }
  return ahead + 1;
}

void KothSkin::spawnFirework() {
  int fx = getRnd(10, SCREEN_WIDTH - 10);
  int fy = getRnd(6, 18);
  uint8_t n = (uint8_t)getRnd(10, 18);
  fireworks_.spawn(fx, fy, n, 0.6f, 2.5f);
}

void KothSkin::drawTimerBar(uint32_t remainMs) {
  // Время справа: широкая полоса — оставшееся (уменьшается сверху вниз),
  // на освободившемся месте остаётся тонкая 1px линия-"след" прошедшего
  // Time on the right: a wide bar is what is left (shrinking top to bottom), and
  // a thin 1 px "trail" marks the freed-up part of the elapsed time
  const int x = SCREEN_WIDTH - 4; // правый край (полоска 3px: x..x+2)
  const int y0 = 13;              // верх — ниже всплывающей цифры кликов (y≈7)
  const int y1 = 35;              // низ
  const int H = y1 - y0;

  float f = (float)remainMs / (float)Koth::KOTH_ROUND_MS;
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;

  int remainH = (int)(f * H); // высота широкой полосы (оставшееся время)
  int trailH = H - remainH;   // высота "следа" (прошедшее время)

  // След — тонкая 1px линия в освободившейся (верхней) части
  // The trail is a thin 1 px line in the freed-up upper part
  if (trailH > 0) display.drawVLine(x + 1, y0, trailH);
  // Широкая полоса оставшегося времени — снизу, уменьшается сверху вниз
  // The wide remaining-time bar sits at the bottom and shrinks top to bottom
  if (remainH > 0) display.drawBox(x, y1 - remainH, 3, remainH);
}

void KothSkin::drawPowerBar() {
  // Мощь кликов: вертикальная полоска слева, растёт на всю высоту экрана.
  // Click power: a vertical bar on the left, growing over the full screen height.
  const int x = 0;
  const int y0 = 5;    // верх полоски
  const int y1 = 35;   // низ (выше нижней подписи)
  const int maxH = y1 - y0;
  const float maxPower = 12.0f; // кликов за 2 с — полная высота (макс. кликание ~13)
  int h = (int)(myPower_ * maxH / maxPower);
  if (h > maxH) h = maxH;
  if (h < 0) h = 0;
  if (h > 0) display.drawBox(x, y1 - h, 3, h);
}

void KothSkin::drawCentered(int y, const char* s) {
  int w = display.getStrWidth(s);
  display.drawStr((SCREEN_WIDTH - w) / 2, y, s);
}

// Бегущая строка для длинных имён (с обрезкой по зоне)
// Marquee for long names (clipped to a zone)
void KothSkin::drawMarquee(int y, const char* s, int x0, int x1) {
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
  display.setClipWindow(x0, 0, x1 - 1, SCREEN_HEIGHT);
  int x = x1 - (int)scrollOffset_;
  display.drawStr(x, y, s);
  display.setClipWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void KothSkin::draw() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);
  switch (phase_) {
    case WAIT:     drawWait(); break;
    case READY:    drawReady(); break;
    case ROUND:    drawRound(); break;
    case END_WAIT: drawRound(); break;
    case RESULT:   drawResult(); break;
  }
}

void KothSkin::drawWait() {
  drawFlag(SCREEN_WIDTH / 2, 18); // флаг — символ скина
  char buf[24];
  int others = 0;
  for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
    if (players_[i].valid) others++;
  }
  snprintf(buf, sizeof(buf), "players %d", others + 1);
  drawCentered(6, buf);
  if (now_ - needPlayersHint_ < 2000) {
    drawCentered(38, "need 2+ players");
  } else {
    drawCentered(38, "hold - start");
  }
}

void KothSkin::drawReady() {
  drawFlag(SCREEN_WIDTH / 2, 18);
  // Вертикальный прогресс до старта справа (заполняется сверху вниз)
  // Vertical countdown to the start on the right (filled top to bottom)
  uint32_t elapsed = now_ - phaseStart_;
  const int x = SCREEN_WIDTH - 4;
  const int y0 = 6;
  const int h = SCREEN_HEIGHT - 16;
  float f = (float)elapsed / (float)readyDelay_;
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;
  int fh = (int)(f * h);
  if (fh > 0) display.drawBox(x - 1, y0, 3, fh);
  drawCentered(6, "get ready");
}

void KothSkin::drawRound() {
  uint32_t remain = Koth::KOTH_ROUND_MS - (now_ - roundStart_);
  if (now_ - roundStart_ >= Koth::KOTH_ROUND_MS) remain = 0;
  drawTimerBar(remain);
  drawBigNumber(myRank()); // 0 = корону никто не держал, иначе 1..N (1 — лидер)
  drawPowerBar();
  counter_.draw(now_); // всплывающая цифра кликов поверх графики

  // Кто сейчас король — короткой строкой внизу. Ищем по флагу "я король"
  // (король держит корону, даже пока не кликает), с проверкой свежести.
  // Who is king right now, as a short line at the bottom. Found by the "I am
  // king" flag (the king keeps the crown even while not clicking), checked fresh.
  if (iAmKing_) {
    drawCentered(38, "YOU KING");
  } else {
    int ki = -1;
    for (int i = 0; i < Koth::MAX_PLAYERS; i++) {
      if (players_[i].valid && players_[i].isKing &&
          now_ - players_[i].lastSeen < Koth::PLAYER_TIMEOUT_MS) {
        ki = i;
        break;
      }
    }
    if (ki >= 0) {
      drawMarquee(38, players_[ki].name, 0, SCREEN_WIDTH);
    }
  }
}

void KothSkin::drawResult() {
  if (iWon_) {
    fireworks_.draw(); // салют позади флага
    drawFlag(SCREEN_WIDTH / 2, 18);
    drawCentered(6, "YOU WIN!");
    char buf[20];
    snprintf(buf, sizeof(buf), "hill %lus", (unsigned long)(winnerTime_ / 1000));
    drawCentered(38, buf);
  } else {
    // "win: имя" — победитель сверху (имя бегущей строкой, если длинное)
    // "win: name" — the winner on top (the name scrolls if it is long)
    char wbuf[24];
    snprintf(wbuf, sizeof(wbuf), "win: %s", winnerName_);
    drawMarquee(8, wbuf, 0, SCREEN_WIDTH);
    // "you" — сразу под "win"
    // "you" — right under "win"
    drawCentered(17, "you:");
    // Моё место — крупной цифрой ниже
    // My rank as a big digit below
    drawBigNumber(myRank(), 36);
  }
}
