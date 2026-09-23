#pragma once
#include "DuelProtocol.h"

// Обёртка над ESP-NOW для дуэли. Приём работает в фоне (задача WiFi),
// входящие пакеты складываются в кольцевой буфер, основной цикл вычитывает
// их через poll(). Вся игровая логика живёт в DuelSkin, здесь — только радио.
// ESP-NOW wrapper for the duel. Reception runs in the background (WiFi task) and
// pushes packets into a ring buffer that the main loop drains via poll(). All game
// logic lives in DuelSkin — this file is radio only.
namespace DuelNet {
  bool begin();     // WiFi (STA) + ESP-NOW; false — ошибка инициализации
  void end();       // выключить ESP-NOW и WiFi
  bool isRunning(); // ESP-NOW активен (WiFi могли выключить мимо нас)

  // Направленная отправка пиру (peer добавляется автоматически)
  // Directed send to a peer (the peer is added automatically)
  bool send(const uint8_t* mac, const Duel::Packet& pkt);

  // Широковещательная отправка (поиск оппонентов)
  // Broadcast send (looking for opponents)
  bool sendBroadcast(const Duel::Packet& pkt);

  // Достать один входящий пакет. Возвращает false, если очередь пуста.
  // При успехе в out — пакет, в outMac — MAC отправителя.
  // Take one incoming packet. Returns false when the queue is empty. On success
  // out holds the packet and outMac the sender MAC.
  bool poll(Duel::Packet& out, uint8_t outMac[6]);
}
