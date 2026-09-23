#pragma once
#include <Arduino.h>

// Обёртка над ESP32 Preferences (NVS) — переживает перезагрузку и deep sleep.
// Thin wrapper over ESP32 Preferences (NVS) — survives a reboot and deep sleep.
namespace SettingsStore {
  void begin();

  uint8_t loadSkinIndex();
  void saveSkinIndex(uint8_t index);

  // Счётчики по строковому ключу (например "cntEye"). Ключ — до 15 символов
  // (ограничение NVS) и уникальный для скина.
  // Counters by string key (e.g. "cntEye"). The key is at most 15 characters
  // (NVS limit) and unique per skin.
  uint32_t loadCounter(const char* key);
  void saveCounter(const char* key, uint32_t value);

  // Сохранённая сеть: SSID + пароль / Saved network: SSID + password
  struct WifiNetwork {
    char ssid[33];
    char pass[65];
  };
  // Сколько сетей помним (индекс 0 — последняя использованная) / How many networks we keep (index 0 = last used)
  static constexpr uint8_t MAX_WIFI_NETWORKS = 5;

  String loadWiFiSSID(); // SSID/пароль последней использованной сети (или "") / last used network, "" if none
  String loadWiFiPass();
  uint8_t loadWiFiNetworks(WifiNetwork* out, uint8_t max);       // вернёт число сетей / returns the count
  uint8_t upsertWiFiNetwork(const char* ssid, const char* pass); // сеть — на первое место / moves it to the top
  uint8_t removeWiFiNetwork(const char* ssid);

  // Устаревшие одиночные ключи — оставлены для совместимости
  // Legacy single-network keys, kept for backward compatibility
  void saveWiFiSSID(const String& ssid);
  void saveWiFiPass(const String& pass);

  // Ник для дуэлей по ESP-NOW (пустая строка — скин подставит Clicker-XXXX)
  // Nickname for ESP-NOW duels (empty = the skin falls back to Clicker-XXXX)
  String loadNick();
  void saveNick(const String& nick);
}
