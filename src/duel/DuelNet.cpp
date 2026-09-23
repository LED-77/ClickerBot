#include "DuelNet.h"
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <WiFi.h>

namespace {
  constexpr int RX_SIZE = 8;
  constexpr uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  struct RxEntry {
    Duel::Packet pkt;
    uint8_t mac[6];
  };

  RxEntry rxBuf_[RX_SIZE];
  volatile int rxHead_ = 0;
  volatile int rxTail_ = 0;
  portMUX_TYPE rxMux_ = portMUX_INITIALIZER_UNLOCKED;

  bool running_ = false;

  void onSend(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status; // доставку пока не отслеживаем / delivery is not tracked yet
  }

  // Сигнатура колбэка приёма различается между ядром Arduino-ESP32 2.x и 3.x.
  // The receive callback signature differs between Arduino-ESP32 core 2.x and 3.x.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    const uint8_t* mac = info->src_addr;
#else
  void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
#endif
    if (len != (int)sizeof(Duel::Packet)) return;
    Duel::Packet p;
    memcpy(&p, data, sizeof(p));
    if (p.proto != Duel::PROTO) return;

    portENTER_CRITICAL(&rxMux_);
    int next = (rxHead_ + 1) % RX_SIZE;
    if (next != rxTail_) { // есть свободный слот / a free slot exists
      rxBuf_[rxHead_].pkt = p;
      memcpy(rxBuf_[rxHead_].mac, mac, 6);
      rxHead_ = next;
    }
    portEXIT_CRITICAL(&rxMux_);
  }

  void addPeer(const uint8_t* mac) {
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, mac, 6);
    pi.channel = 0;
    pi.encrypt = false;
    esp_now_add_peer(&pi); // ESP_ERR_ESPNOW_EXIST — уже добавлен, это нормально / already added, that is fine
  }
}

bool DuelNet::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // без modem-sleep ESP-NOW надёжнее / ESP-NOW is more reliable without modem sleep
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return false;

  esp_now_register_send_cb(onSend);
  esp_now_register_recv_cb(onRecv);

  rxHead_ = 0;
  rxTail_ = 0;
  addPeer(BROADCAST); // чтобы работала широковещательная отправка / so that broadcast sending works
  running_ = true;
  return true;
}

void DuelNet::end() {
  running_ = false;
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  esp_now_deinit();
  WiFi.setSleep(true);
  WiFi.mode(WIFI_OFF);
}

bool DuelNet::isRunning() {
  return running_;
}

bool DuelNet::send(const uint8_t* mac, const Duel::Packet& pkt) {
  addPeer(mac);
  return esp_now_send(mac, (const uint8_t*)&pkt, sizeof(pkt)) == ESP_OK;
}

bool DuelNet::sendBroadcast(const Duel::Packet& pkt) {
  return esp_now_send(BROADCAST, (const uint8_t*)&pkt, sizeof(pkt)) == ESP_OK;
}

bool DuelNet::poll(Duel::Packet& out, uint8_t outMac[6]) {
  if (rxHead_ == rxTail_) return false;
  portENTER_CRITICAL(&rxMux_);
  if (rxHead_ == rxTail_) {
    portEXIT_CRITICAL(&rxMux_);
    return false;
  }
  out = rxBuf_[rxTail_].pkt;
  memcpy(outMac, rxBuf_[rxTail_].mac, 6);
  rxTail_ = (rxTail_ + 1) % RX_SIZE;
  portEXIT_CRITICAL(&rxMux_);
  return true;
}
