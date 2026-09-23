#pragma once
#include <Arduino.h>

// Протокол дуэли двух кликеров по ESP-NOW.
// Все устройства прошиты одинаково, поэтому протокол живёт в одном заголовке.
//
// Пакеты:
//   - широковещательные (поиск оппонентов по имени);
//   - направленные по MAC (вызов, ответ, обмен счётом в дуэли).
// ESP-NOW duel protocol between two clickers. All devices run the same firmware,
// so the protocol lives in a single header.
//
// Packets:
//   - broadcast (looking for opponents by name);
//   - addressed by MAC (challenge, answer, score exchange during a duel).
namespace Duel {
  // Версия протокола — отсекает случайные ESP-NOW пакеты других проектов
  // Protocol version — filters out stray ESP-NOW packets from other projects
  constexpr uint16_t PROTO = 0x0D01;

  constexpr uint8_t  MAX_NAME = 15;               // длина имени (как ник в NVS)
  constexpr uint8_t  MAX_OPPONENTS = 4;           // сколько держим в списке

  constexpr uint32_t ANNOUNCE_INTERVAL_MS = 600;  // период широковещательного маячка
  constexpr uint32_t ANNOUNCE_IDLE_MS     = 2000; // маячок реже, пока оппонентов не видно
  constexpr uint32_t OPPONENT_TIMEOUT_MS  = 3000; // без сигнала столько — оппонента убираем
  constexpr uint32_t CHALLENGE_TIMEOUT_MS = 10000;// сколько ждём ответа на вызов
  constexpr uint32_t SEARCH_ACTIVE_MS     = 40000;// столько активно ищем оппонентов, затем "ожидание" и сон
  constexpr uint32_t DUEL_DURATION_MS     = 15000;// длительность дуэли (перетягивание каната)
  constexpr uint32_t STATE_INTERVAL_MS    = 200;  // период обмена счётом в дуэли
  constexpr uint32_t DUEL_DISCONNECT_MS   = 2000; // столько без пакетов соперника — связь потеряна, прерываем дуэль
  constexpr uint32_t END_GRACE_MS         = 700;  // пауза после времени, ждём финальный счёт
  constexpr uint32_t RESULT_SHOW_MS       = 5000; // минимум столько висит итог (клики игнорируются)
  constexpr uint32_t CHALLENGE_HOLD_MS    = 1000; // удержание клика = вызов оппонента

  // Опкоды пакетов
  // Packet opcodes
  enum Op : uint8_t {
    OP_ANNOUNCE  = 1, // широковещательный: "я в дуэли, вот моё имя"
    OP_CHALLENGE = 2, // направленный: вызываю тебя на дуэль
    OP_ACCEPT    = 3, // направленный: согласен (flags=1) / отказ (flags=0)
    OP_STATE     = 4, // направленный: мой текущий счёт кликов в дуэли
    OP_RESULT    = 5, // направленный: финальный счёт после 15 с
  };

  // Пакет ESP-NOW (32 байта — с большим запасом до лимита 250)
  // ESP-NOW packet (32 bytes — far below the 250-byte limit)
  struct Packet {
    uint16_t proto;      // PROTO — проверка совместимости
    uint8_t  op;         // один из Op
    uint8_t  flags;      // для OP_ACCEPT: 1 = принять, 0 = отклонить
    uint32_t duelId;     // идентификатор дуэли (уникален на стороне вызывающего)
    uint32_t clicks;     // счёт кликов (OP_STATE / OP_RESULT)
    uint32_t stamp;      // millis() отправителя (информационно)
    char     name[MAX_NAME + 1]; // имя отправителя
  };
}

// --- Режим "Король холма" (King of the Hill, любое число игроков) ---
// Переиспользует Duel::Packet и Duel::PROTO; опкоды 10+ не пересекаются
// с дуэлью (1..5), поэтому скины просто игнорируют чужие пакеты.
// --- King of the Hill mode (any number of players) ---
// Reuses Duel::Packet and Duel::PROTO; opcodes 10+ do not clash with the duel
// (1..5), so skins simply ignore packets that are not theirs.
namespace Koth {
  enum Op : uint8_t {
    OP_ANNOUNCE = 10, // "я в игре" — для счётчика игроков
    OP_READY    = 11, // широковещательный: раунд скоро (duelId=id, clicks=пауза)
    OP_GO       = 12, // широковещательный: старт раунда — держи холм!
    OP_STATE    = 13, // широковещательный: мощь/время короля (clicks=мощь, stamp=время)
  };

  constexpr uint32_t ANNOUNCE_INTERVAL_MS = 800;   // период маячка "я в игре"
  constexpr uint32_t ANNOUNCE_IDLE_MS     = 2000;  // маячок реже, пока игроков не видно
  constexpr uint32_t PLAYER_TIMEOUT_MS    = 3000;  // без маячка столько — игрока убираем
  constexpr uint32_t READY_DELAY_MIN_MS   = 1500;  // пауза "get ready" (min)
  constexpr uint32_t READY_DELAY_MAX_MS   = 4000;  // ... (max)
  constexpr uint32_t READY_TIMEOUT_MS     = 6000;  // если GO не пришёл — назад в ожидание
  constexpr uint32_t KOTH_ROUND_MS        = 60000; // длительность раунда (60 с)
  constexpr uint32_t KOTH_TICK_MS         = 250;   // такт: обновление мощи и обмен состояниями
  constexpr uint32_t POWER_WINDOW_MS      = 2000;  // окно мощи (клики за последние 2 с)
  constexpr uint8_t  POWER_SLOTS          = POWER_WINDOW_MS / KOTH_TICK_MS; // 8
  constexpr uint32_t END_GRACE_MS         = 700;   // пауза для финальных состояний
  constexpr uint32_t RESULT_SHOW_MS       = 4000;  // сколько висит победитель
  constexpr uint8_t  MAX_PLAYERS          = 12;    // максимум игроков в таблице
  constexpr uint32_t START_HOLD_MS        = 1000;  // удержание клика — начать раунд
  // Флаг в поле flags пакета OP_STATE: отправитель сейчас держит корону
  // Flag in the flags field of OP_STATE: the sender holds the crown right now
  constexpr uint8_t  FLAG_IS_KING         = 0x01;
}
