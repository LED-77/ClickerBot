#include "SettingsStore.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  constexpr const char* NAMESPACE = "clicker";
  constexpr const char* KEY_SKIN  = "skin";
  // WiFi-список: счётчик "wn", записи "ws0..ws4"/"wp0..wp4"
  // WiFi list: counter "wn", entries "ws0..ws4"/"wp0..wp4"
  constexpr const char* KEY_WIFI_COUNT    = "wn";
  constexpr const char* KEY_WIFI_SSID_OLD = "wifi_ssid";
  constexpr const char* KEY_WIFI_PASS_OLD = "wifi_pass";

  // Записывает список сетей (индекс 0 — самая свежая)
  // Writes the network list (index 0 = most recent)
  void saveWifiList(const SettingsStore::WifiNetwork* nets, uint8_t count) {
    if (count > SettingsStore::MAX_WIFI_NETWORKS) count = SettingsStore::MAX_WIFI_NETWORKS;
    prefs.putUChar(KEY_WIFI_COUNT, count);
    for (uint8_t i = 0; i < SettingsStore::MAX_WIFI_NETWORKS; i++) {
      char k[8];
      snprintf(k, sizeof(k), "ws%u", i);
      if (i < count) prefs.putString(k, nets[i].ssid);
      else prefs.remove(k);
      snprintf(k, sizeof(k), "wp%u", i);
      if (i < count) prefs.putString(k, nets[i].pass);
      else prefs.remove(k);
    }
    // Если список опустел — чистим и старые одиночные ключи, иначе миграция
    // при следующем чтении «воскресит» удалённую сеть.
    // If the list became empty, drop the legacy single keys too — otherwise the
    // migration below would resurrect the deleted network on the next read.
    if (count == 0) {
      prefs.remove(KEY_WIFI_SSID_OLD);
      prefs.remove(KEY_WIFI_PASS_OLD);
    }
  }
}

void SettingsStore::begin() {
  prefs.begin(NAMESPACE, /*readOnly=*/false);
}

uint8_t SettingsStore::loadSkinIndex() {
  return prefs.getUChar(KEY_SKIN, 0); // скин по умолчанию / default skin
}

void SettingsStore::saveSkinIndex(uint8_t index) {
  prefs.putUChar(KEY_SKIN, index);
}

uint32_t SettingsStore::loadCounter(const char* key) {
  return prefs.getULong(key, 0);
}

void SettingsStore::saveCounter(const char* key, uint32_t value) {
  prefs.putULong(key, value);
}

String SettingsStore::loadWiFiSSID() {
  WifiNetwork net;
  if (loadWiFiNetworks(&net, 1) > 0) return String(net.ssid);
  return "";
}

String SettingsStore::loadWiFiPass() {
  WifiNetwork net;
  if (loadWiFiNetworks(&net, 1) > 0) return String(net.pass);
  return "";
}

uint8_t SettingsStore::loadWiFiNetworks(WifiNetwork* out, uint8_t max) {
  uint8_t n = prefs.getUChar(KEY_WIFI_COUNT, 0);
  if (n == 0) {
    // Миграция со старого одиночного ключа (было до списка сетей)
    // Migration from the legacy single-network key (pre-dates the list)
    String ssid = prefs.getString(KEY_WIFI_SSID_OLD, "");
    if (ssid.length() > 0 && max >= 1) {
      WifiNetwork net;
      memset(&net, 0, sizeof(net));
      strncpy(net.ssid, ssid.c_str(), 32);
      String pass = prefs.getString(KEY_WIFI_PASS_OLD, "");
      strncpy(net.pass, pass.c_str(), 64);
      saveWifiList(&net, 1);
      n = 1;
    }
  }
  uint8_t cnt = (n < max) ? n : max;
  for (uint8_t i = 0; i < cnt; i++) {
    char k[8];
    snprintf(k, sizeof(k), "ws%u", i);
    String s = prefs.getString(k, "");
    snprintf(k, sizeof(k), "wp%u", i);
    String p = prefs.getString(k, "");
    memset(&out[i], 0, sizeof(WifiNetwork));
    strncpy(out[i].ssid, s.c_str(), 32);
    strncpy(out[i].pass, p.c_str(), 64);
  }
  return cnt;
}

uint8_t SettingsStore::upsertWiFiNetwork(const char* ssid, const char* pass) {
  WifiNetwork nets[MAX_WIFI_NETWORKS];
  uint8_t n = loadWiFiNetworks(nets, MAX_WIFI_NETWORKS);

  // Если пароль пустой — оставляем старый (для уже известной сети)
  // An empty password keeps the stored one for an already known network
  String usePass = pass;
  for (uint8_t i = 0; i < n; i++) {
    if (strcmp(nets[i].ssid, ssid) == 0) {
      if (usePass.length() == 0) usePass = nets[i].pass;
      break;
    }
  }

  // Пересобираем список без этой сети (оставляем место под новую наверху)
  // Rebuild the list without this network, freeing a slot at the top
  WifiNetwork tmp[MAX_WIFI_NETWORKS];
  uint8_t m = 0;
  for (uint8_t i = 0; i < n && m < MAX_WIFI_NETWORKS - 1; i++) {
    if (strcmp(nets[i].ssid, ssid) == 0) continue;
    tmp[m++] = nets[i];
  }

  // Новая/обновлённая сеть — на первое место
  // The new/updated network goes first
  WifiNetwork fresh;
  memset(&fresh, 0, sizeof(fresh));
  strncpy(fresh.ssid, ssid, 32);
  strncpy(fresh.pass, usePass.c_str(), 64);

  WifiNetwork out[MAX_WIFI_NETWORKS];
  out[0] = fresh;
  for (uint8_t i = 0; i < m; i++) out[i + 1] = tmp[i];
  m++;

  saveWifiList(out, m);

  // Старые одиночные ключи держим в синхроне (обратная совместимость)
  // The legacy single-network keys are kept in sync (backward compatibility)
  prefs.putString(KEY_WIFI_SSID_OLD, fresh.ssid);
  prefs.putString(KEY_WIFI_PASS_OLD, fresh.pass);
  return m;
}

uint8_t SettingsStore::removeWiFiNetwork(const char* ssid) {
  WifiNetwork nets[MAX_WIFI_NETWORKS];
  uint8_t n = loadWiFiNetworks(nets, MAX_WIFI_NETWORKS);
  uint8_t m = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (strcmp(nets[i].ssid, ssid) == 0) continue;
    nets[m++] = nets[i];
  }
  saveWifiList(nets, m);
  return m;
}

void SettingsStore::saveWiFiSSID(const String& ssid) {
  prefs.putString(KEY_WIFI_SSID_OLD, ssid);
}

void SettingsStore::saveWiFiPass(const String& pass) {
  prefs.putString(KEY_WIFI_PASS_OLD, pass);
}

String SettingsStore::loadNick() {
  return prefs.getString("nick", "");
}

void SettingsStore::saveNick(const String& nick) {
  prefs.putString("nick", nick);
}
