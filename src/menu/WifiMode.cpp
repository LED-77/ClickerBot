#include "WifiMode.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include "../hal/SettingsStore.h"
#include "../web/WebServer.h"
#include <WiFi.h>
#include <math.h>

namespace {
  WifiSubMode mode_ = WifiSubMode::ACCESS_POINT;
  char ssid_[24] = "ClickerBot_";
  // Пароль точки доступа (WPA2). Без пароля iPhone считает сеть
  // «небезопасной» и отказывается подключаться. Виден на экране AP-режима.
  // Access point password (WPA2). Without one an iPhone treats the network as
  // insecure and refuses to join. Shown on the AP-mode screen.
  const char* AP_PASS = "12345678";

  // --- Состояние клиентского подключения ---
  // --- Client connection state ---
  enum ClientState { IDLE, CONNECTING, CONNECTED, FAILED };
  ClientState clientState_ = IDLE;
  uint32_t connStart_ = 0;
  uint32_t retryLast_ = 0;
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
  static constexpr uint32_t RETRY_INTERVAL_MS = 3000;

  // Список сохранённых сетей и текущая попытка подключения
  // Saved networks and the current connection attempt
  SettingsStore::WifiNetwork nets_[SettingsStore::MAX_WIFI_NETWORKS];
  uint8_t netCount_ = 0;
  int8_t netIndex_ = -1;   // какую сеть пробуем сейчас
  bool tried_[SettingsStore::MAX_WIFI_NETWORKS] = {false};
  bool scanned_ = false;   // сканировали ли доступные сети
  char visible_[16][33];   // SSID видимых сетей (после скана)
  uint8_t visibleCount_ = 0;

  // --- Единая бегущая строка (синхронно для обеих строк) ---
  // --- One shared marquee (both lines scroll together) ---
  float scrollOffset_ = 0;
  uint32_t scrollLast_ = 0;
  bool scrollInit_ = false;

  void resetScroll() {
    scrollOffset_ = 0;
    scrollInit_ = false;
  }

  // Две строки прокручиваются синхронно — сдвиг считается по более длинной
  // Both lines scroll in sync — the offset is driven by the longer one
  void drawSyncLines(int y1, const char* t1, int y2, const char* t2) {
    int w1 = display.getStrWidth(t1);
    int w2 = display.getStrWidth(t2);
    int maxW = max(w1, w2);

    // Обе влезают — просто центрируем
    // Both fit — just centre them
    if (maxW <= SCREEN_WIDTH) {
      display.drawStr((SCREEN_WIDTH - w1) / 2, y1, t1);
      display.drawStr((SCREEN_WIDTH - w2) / 2, y2, t2);
      return;
    }

    // Прокрутка: полный проход = от правого края до полного ухода влево
    // Scrolling: one pass goes from the right edge until the text is fully out on the left
    uint32_t now = millis();
    if (!scrollInit_) { scrollInit_ = true; scrollLast_ = now; }
    float dt = (now - scrollLast_) / 1000.0f;
    scrollLast_ = now;
    if (dt > 0 && dt < 0.5f) scrollOffset_ += 18.0f * dt;

    int cycle = SCREEN_WIDTH + maxW;
    scrollOffset_ = fmodf(scrollOffset_, (float)cycle);
    if (scrollOffset_ < 0) scrollOffset_ += cycle;

    int x = SCREEN_WIDTH - (int)scrollOffset_;
    display.drawStr(x, y1, t1);
    display.drawStr(x, y2, t2);
  }

  // SSID точки доступа: ClickerBot_ + последние 2 байта MAC (AB:CD -> ABCD)
  // AP SSID: ClickerBot_ plus the last 2 bytes of the MAC (AB:CD -> ABCD)
  void buildSSID() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    strncpy(ssid_ + 11, suffix, 5);
    ssid_[15] = '\0';
  }

  // Запускает точку доступа
  // Starts the access point
  void startAP() {
    buildSSID();
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false); // радио всегда на связи — веб-морда отвечает стабильно
    // Явно задаём подсеть: DHCP всегда 192.168.4.x, и после AP<->CLIENT точка не «плывёт»
    // Subnet is set explicitly: DHCP always hands out 192.168.4.x and the AP does not drift after AP<->CLIENT
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid_, AP_PASS); // WPA2: без пароля iPhone не подключается
    WebSrv::begin();
    resetScroll();
  }

  // Начинает подключение к i-й сети из списка
  // Starts connecting to the i-th saved network
  void tryNet(int8_t i) {
    if (i < 0 || i >= (int8_t)netCount_) return;
    netIndex_ = i;
    tried_[i] = true;
    WiFi.disconnect(true);
    delay(50);
    WiFi.begin(nets_[i].ssid, nets_[i].pass);
  }

  // Сканирует доступные сети и запоминает их SSID
  // Scans for available networks and remembers their SSIDs
  void scanVisible() {
    visibleCount_ = 0;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && visibleCount_ < 16; i++) {
      String s = WiFi.SSID(i);
      if (s.length() == 0) continue; // скрытые сети пропускаем / skip hidden networks
      strncpy(visible_[visibleCount_], s.c_str(), 32);
      visible_[visibleCount_][32] = '\0';
      visibleCount_++;
    }
    WiFi.scanDelete();
  }

  bool isVisible(const char* ssid) {
    for (uint8_t i = 0; i < visibleCount_; i++) {
      if (strcmp(visible_[i], ssid) == 0) return true;
    }
    return false;
  }

  // Следующая сеть для попытки: самая свежая из видимых и ещё не пробованных
  // Next candidate: the most recent visible network that has not been tried yet
  int8_t nextCandidate() {
    if (netCount_ == 0) return -1;
    if (!scanned_) {
      scanned_ = true;
      scanVisible();
    }
    for (uint8_t i = 0; i < netCount_; i++) {
      if (tried_[i]) continue;
      if (isVisible(nets_[i].ssid)) return (int8_t)i;
    }
    return -1;
  }

  // Имя сети, к которой сейчас пытаемся/подключены
  // Name of the network we are currently connecting to or connected to
  const char* currentSsid() {
    if (netIndex_ >= 0 && netIndex_ < (int8_t)netCount_) return nets_[netIndex_].ssid;
    return "";
  }

  // Подключается к сохранённой WiFi сети (режим клиента). Пробуем самую
  // свежую сеть сразу, при неудаче — сканируем и выбираем из доступных.
  // Connects to a saved WiFi network (client mode). The most recent one is tried
  // first; on failure we scan and pick from the visible ones.
  void startClient() {
    netCount_ = SettingsStore::loadWiFiNetworks(nets_, SettingsStore::MAX_WIFI_NETWORKS);
    if (netCount_ == 0) {
      clientState_ = FAILED;
      return;
    }
    for (uint8_t i = 0; i < netCount_; i++) tried_[i] = false;
    scanned_ = false;
    visibleCount_ = 0;
    netIndex_ = -1;
    delay(100); // даём WiFi-модулю прийти в себя после WIFI_OFF / let the WiFi module recover after WIFI_OFF
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // без modem sleep: иначе входящие запросы к веб-морде теряются / without modem sleep incoming web requests are lost
    tryNet(0); // сначала самая свежая сеть / the most recent network first
    clientState_ = CONNECTING;
    connStart_ = millis();
    retryLast_ = millis();
    resetScroll();
    // Веб-морду поднимаем позже — на WL_CONNECTED, когда STA получила IP
    // (в updateClient), иначе веб-морда привяжется не к тому интерфейсу.
    // The web UI is started later, on WL_CONNECTED once the STA has an IP (in
    // updateClient); starting it earlier binds it to the wrong interface.
  }

  // Обновляет состояние клиентского подключения (вызывается в tick)
  // Updates the client state (called from tick)
  void updateClient() {
    if (mode_ != WifiSubMode::CLIENT) return;

    // Как только STA получила IP — поднимаем веб-морду, если ещё не запущена.
    // Ориентируемся на IP, а не на статус WL_CONNECTED: тот может не успеть
    // переключиться, а IP уже выдан — иначе сайт по локалке не открывается.
    // Start the web UI as soon as the STA has an IP. We key off the IP rather
    // than WL_CONNECTED: the status can lag behind while the IP is already there,
    // and without this the local page would not open.
    if (WiFi.localIP() != IPAddress(0, 0, 0, 0) && !WebSrv::isRunning()) {
      WebSrv::begin();
    }

    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      if (clientState_ != CONNECTED) {
        clientState_ = CONNECTED;
        WebSrv::begin();
        // Сеть стала последней использованной — поднимаем её вверх списка
        // This network is now the last used one — move it to the top of the list
        if (netIndex_ >= 0 && netIndex_ < (int8_t)netCount_) {
          SettingsStore::upsertWiFiNetwork(nets_[netIndex_].ssid, nets_[netIndex_].pass);
        }
      }
      return;
    }
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
      // Пароль не подошёл или сеть не найдена — окончательно провал;
      // WL_DISCONNECTED/WL_CONNECTING на старте значит "ещё пытаемся", решает таймаут
      // Wrong password or network not found — a definitive failure;
      // WL_DISCONNECTED/WL_CONNECTING early on means "still trying", the timeout decides
      clientState_ = FAILED;
    } else if (clientState_ == CONNECTED) {
      clientState_ = CONNECTING; // потеряли соединение
    }

    // Таймаут или ошибка → пробуем следующую сеть из списка
    // Timeout or error → try the next saved network
    if (clientState_ == FAILED ||
        (clientState_ == CONNECTING && millis() - connStart_ >= CONNECT_TIMEOUT_MS)) {
      if (millis() - retryLast_ >= RETRY_INTERVAL_MS) {
        int8_t next = nextCandidate();
        if (next >= 0) {
          tryNet(next);
          clientState_ = CONNECTING;
          connStart_ = millis();
          retryLast_ = millis();
        } else {
          // Все видимые сети перепробованы — сбрасываем цикл, повторим позже
          // All visible networks tried — reset the cycle and retry later
          clientState_ = FAILED;
          for (uint8_t i = 0; i < netCount_; i++) tried_[i] = false;
          scanned_ = false;
          retryLast_ = millis();
        }
      }
    }
  }

  // Останавливает любой режим WiFi
  // Stops whichever WiFi mode is running
  void stopWiFi() {
    WebSrv::stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    clientState_ = IDLE;
  }

  // Иконка WiFi (точка доступа): точка + концентрические дуги
  // WiFi icon (access point): dot plus concentric arcs
  void drawWifiSymbol(int cx, int cy) {
    display.drawDisc(cx, cy, 2);
    for (int r = 4; r <= 8; r += 2) {
      int halfW = r;
      for (int x = -halfW; x <= halfW; x++) {
        int y = cy - (int)(sqrtf((float)(r * r - x * x)) * 0.5f);
        if (y >= 0 && y < SCREEN_HEIGHT) display.drawPixel(cx + x, y);
      }
    }
  }

  // Иконка клиента: простой прямоугольник "экран" + маленькая антенна
  // Client icon: a simple "screen" rectangle plus a small antenna
  void drawClientIcon(int cx, int cy) {
    // Экранчик / screen
    display.drawFrame(cx - 5, cy - 4, 10, 7);
    // Подставка / stand
    display.drawPixel(cx, cy + 3);
    display.drawPixel(cx - 1, cy + 4);
    display.drawPixel(cx + 1, cy + 4);
    // Антенна / antenna
    display.drawVLine(cx, cy - 7, 3);
    display.drawPixel(cx, cy - 8);
  }
}

void WifiMode::onEnter() {
  mode_ = WifiSubMode::ACCESS_POINT;
  startAP();
}

void WifiMode::tick() {
  if (mode_ == WifiSubMode::CLIENT) {
    updateClient();
  }
}

void WifiMode::nextMode() {
  stopWiFi();
  mode_ = (mode_ == WifiSubMode::ACCESS_POINT)
    ? WifiSubMode::CLIENT
    : WifiSubMode::ACCESS_POINT;
  if (mode_ == WifiSubMode::ACCESS_POINT) {
    startAP();
  } else {
    startClient();
  }
}

void WifiMode::stop() {
  stopWiFi();
}

WifiSubMode WifiMode::currentMode() {
  return mode_;
}

void WifiMode::draw() {
  display.setFont(u8g2_font_5x7_tf);
  display.setDrawColor(1);

  char buf[32];
  if (mode_ == WifiSubMode::ACCESS_POINT) {
    // --- AP mode ---
    drawWifiSymbol(SCREEN_WIDTH / 2, 8);
    snprintf(buf, sizeof(buf), "%s %s", ssid_, WiFi.softAPIP().toString().c_str());
    char pbuf[24];
    snprintf(pbuf, sizeof(pbuf), "pass: %s | swap", AP_PASS);
    drawSyncLines(22, buf, 39, pbuf);
  } else {
    // --- Client mode ---
    drawClientIcon(14, 8);
    const char* ssid = currentSsid();
    String ip = WiFi.localIP().toString();

    if (clientState_ == CONNECTED && ip != "0.0.0.0") {
      snprintf(buf, sizeof(buf), "%s OK %s", ssid, ip.c_str());
      drawSyncLines(22, buf, 39, "Click - swap menu. M - exit.");
      return;
    } else if (clientState_ == FAILED) {
      snprintf(buf, sizeof(buf), "%s FAIL", ssid);
    } else if (ip != "0.0.0.0") {
      // IP уже выдан — показываем его, не дожидаясь перехода в CONNECTED
      // The IP is already assigned — show it without waiting for CONNECTED
      snprintf(buf, sizeof(buf), "%s %s", ssid, ip.c_str());
    } else {
      snprintf(buf, sizeof(buf), "%s CONN", ssid);
    }
    drawSyncLines(22, buf, 39, "Click - swap menu. M - exit.");
  }
}
