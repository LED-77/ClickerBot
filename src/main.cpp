#include <Arduino.h>
#include "Config.h"
#include "hal/BatteryManager.h"
#include "hal/DisplayManager.h"
#include "hal/Button.h"
#include "hal/SettingsStore.h"
#include "hal/SleepManager.h"
#include "skins/SkinRegistry.h"
#include "menu/MenuMode.h"
#include "menu/FlashReadyMode.h"
#include "menu/StatsMode.h"
#include "menu/WifiMode.h"
#include "web/WebServer.h"

// Метка версии в .bin — по строке CLICKERFW: версию видно прямо в собранном
// файле. extern "C" + флаг -Wl,--undefined=FW_BIN_MARKER нужны потому, что
// ссылок на метку в коде нет и иначе линковщик выбросит её (--gc-sections).
// Version marker inside the .bin: the string CLICKERFW: shows the version right
// in the built file. extern "C" and -Wl,--undefined=FW_BIN_MARKER are needed
// because nothing in the code references the symbol, so the linker drops it.
extern "C" const char FW_BIN_MARKER[] = "CLICKERFW:" FW_VERSION;

static Button clickBtn(PIN_CLICK_BUTTON, /*activeLow=*/true, BTN_DEBOUNCE_MS, STATS_LONG_PRESS_MS);
static Button menuBtn(PIN_MENU_BUTTON, /*activeLow=*/true, BTN_DEBOUNCE_MS, MENU_LONG_PRESS_MS);

enum class AppMode { SKIN, MENU, FLASH_READY, STATS, WIFI };
static AppMode appMode = AppMode::SKIN;
static uint32_t menuLastActivity = 0; // авто-выход из меню по бездействию / menu auto-exit
static uint32_t wifiLastActivity = 0; // авто-выход из WiFi-меню / WiFi menu auto-exit

static SleepManager sleepManager;
static bool wasIdle = false; // в прошлом кадре показана "минимальная картинка" / minimal frame on screen

static uint8_t currentSkinIndex = 0;
static Skin* currentSkin = nullptr;
static bool randomSkinMode = false; // клик → случайный скин / click -> random skin

// Применить выбор меню, сохранить его в NVS и вернуться в скин. Общая точка
// для обоих путей выхода из меню — по удержанию кнопки и по таймауту.
// Apply the menu selection, store it in NVS and return to the skin. Shared by
// both ways of leaving the menu: long press and inactivity timeout.
static void applyMenuSelectionAndExit(uint32_t now) {
  uint8_t selected = MenuMode::selectedIndex();
  if (selected >= SkinRegistry::COUNT) {
    // "Random": сохраняем маркер, скин выберется случайно при пробуждении
    // "Random": keep the marker, a random skin is picked on wake-up
    randomSkinMode = true;
    SettingsStore::saveSkinIndex(SkinRegistry::COUNT);
    currentSkin->onExit(); // скин покидает сцену — гасим ESP-NOW / leaving the stage: shut ESP-NOW down
    currentSkinIndex = SkinRegistry::randomSoloIndex();
    currentSkin = SkinRegistry::get(currentSkinIndex);
    currentSkin->onEnter();
  } else {
    randomSkinMode = false;
    if (selected != currentSkinIndex) {
      currentSkin->onExit();
      currentSkinIndex = selected;
      currentSkin = SkinRegistry::get(currentSkinIndex);
      currentSkin->onEnter();
      SettingsStore::saveSkinIndex(currentSkinIndex);
    }
  }
  wasIdle = false;
  sleepManager.reset(now); // после меню отсчёт простоя с нуля / idle countdown restarts after the menu
  appMode = AppMode::SKIN;
}

void setup() {
  displayInit();
  SettingsStore::begin();

  // UART — только для отладочного баннера при старте (версия прошивки).
  // UART is used only for the debug banner on boot (firmware version).
  Serial.begin(115200);
  Serial.printf("ClickerBot fw %s\n", FW_VERSION);

  clickBtn.begin();
  menuBtn.begin();

  // Замер заряда — один раз на пробуждение: каждое пробуждение из deep sleep
  // проходит через setup().
  // Charge is measured once per wake-up: every deep-sleep wake goes through setup().
  Battery::begin();
  int battPct = Battery::measurePercent();
  bool lowBatt = Battery::isLow();

  // Заставка: приветствие + батарея с процентом. При низком заряде батарея
  // перечёркнута, но кликер работает — у аккумулятора своя защита от разряда.
  // Splash: greeting plus battery with percent. At a low charge the battery is
  // crossed out, but the clicker still works — the cell protects itself.
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);
  String hello = "Hello, " + SettingsStore::loadNick() + "!";
  if (hello == "Hello, !") {
    hello = "Hello!";
  }
  while (hello.length() > 0 &&
         display.getStrWidth(hello.c_str()) > SCREEN_WIDTH) {
    hello = hello.substring(0, hello.length() - 1); // обрезаем по ширине экрана / trim to screen width
  }
  display.drawStr((SCREEN_WIDTH - display.getStrWidth(hello.c_str())) / 2, 9,
                  hello.c_str());
  Battery::draw(battPct, lowBatt);
  display.sendBuffer();
  // Держим заставку короткое время; заодно "проглатываем" пробуждающее
  // нажатие — оно не станет кликом по скину.
  // Keep the splash up briefly; this also swallows the wake-up press so it does
  // not turn into a click on the skin.
  uint32_t splashEnd = millis() + BATTERY_SPLASH_MS;
  while (millis() < splashEnd) {
    delay(10);
  }

  // Ниже критического заряда — сразу deep sleep, иначе ESP32 зацикливается
  // на brownout-перезагрузках: клик снова покажет заставку и снова уснёт.
  // Below the critical level: straight to deep sleep, otherwise the ESP32 loops
  // on brownout resets. A click shows the splash again and sleeps again.
  if (Battery::isLow()) {
    goToDeepSleep(WAKE_PIN_MASK);
  }

  appMode = AppMode::SKIN;

  currentSkinIndex = SettingsStore::loadSkinIndex();
  if (currentSkinIndex >= SkinRegistry::COUNT) {
    // Сохранён маркер "Random" — включаем режим случайного скина
    // The "Random" marker is stored — switch to random-skin mode
    randomSkinMode = true;
    currentSkinIndex = (uint8_t)random(0, SkinRegistry::COUNT);
  }
  currentSkin = SkinRegistry::get(currentSkinIndex);
  currentSkin->onEnter();

  sleepManager.reset(millis());
}

void loop() {
  const uint32_t now = millis();
  clickBtn.poll(now);
  menuBtn.poll(now);

  // Удержание дольше входа в меню — статичный экран "готов к прошивке":
  // дальше пользователь сам зажмёт BOOT и нажмёт RESET.
  // Hold longer than the menu press: show the static "ready to flash" screen;
  // from there the user holds BOOT and presses RESET.
  if (appMode != AppMode::FLASH_READY &&
      menuBtn.isPressed() && menuBtn.heldMs(now) >= FLASH_MODE_HOLD_MS) {
    appMode = AppMode::FLASH_READY;
  }
  // Отпустили кнопку, так и не нажав RESET — просто выходим, как из паузы
  // Button released without pressing RESET — just leave, as if resuming
  if (appMode == AppMode::FLASH_READY && menuBtn.justReleased()) {
    appMode = AppMode::SKIN;
    sleepManager.reset(now);
  }

  display.clearBuffer();

  if (appMode == AppMode::FLASH_READY) {
    // sleepManager.tick() здесь нет намеренно: пока висит этот экран, устройство
    // не должно засыпать и менять картинку
    // No sleepManager.tick() here on purpose: while this screen is up the device
    // must not sleep or change the picture
    FlashReadyMode::draw();
  } else if (appMode == AppMode::SKIN) {
    if (clickBtn.longPressFired()) {
      appMode = AppMode::STATS;
      sleepManager.reset(now);
      currentSkin->flushCounters(); // статистика видит свежие числа / stats see fresh numbers
      StatsMode::onEnter();
    } else if (menuBtn.longPressFired()) {
      appMode = AppMode::MENU;
      menuLastActivity = now;
      sleepManager.reset(now); // вход в меню — тоже активность / entering the menu counts as activity
      currentSkin->flushCounters(); // и меню — тоже фиксируем счётчик / and the menu flushes it too
      // В режиме Random меню открывается на пункте "Random"
      // In Random mode the menu opens on the "Random" entry
      MenuMode::onEnter(randomSkinMode ? SkinRegistry::COUNT : currentSkinIndex);
    } else {
      bool clicked = clickBtn.justPressed();
      if (clicked || menuBtn.justPressed()) {
        sleepManager.reset(now);
      }

      // Доп. вход для скинов со сложной логикой (дуэль, KOTH): см. Skin::pollInput
      // Extra input for skins with complex logic (duel, KOTH): see Skin::pollInput
      currentSkin->pollInput(now, menuBtn.justPressed(), clickBtn.justReleased(), clickBtn.heldMs(now));

      bool idleNow = sleepManager.wantsIdle(now) && currentSkin->canIdle();

      if (idleNow) {
        if (!wasIdle) {
          currentSkin->onIdleEnter();
          wasIdle = true;
        }
        currentSkin->drawIdle();
        sleepManager.onIdleShown(now);
      } else {
        if (wasIdle) {
          // Момент пробуждения: в режиме Random выбираем случайный скин
          // Wake-up moment: in Random mode pick a random skin
          if (randomSkinMode) {
            currentSkin->onExit();
            currentSkinIndex = SkinRegistry::randomSoloIndex();
            currentSkin = SkinRegistry::get(currentSkinIndex);
            currentSkin->onEnter();
          }
          currentSkin->onWake(now);
          wasIdle = false;
        }
        currentSkin->update(now, clicked);
        currentSkin->draw();
      }

      sleepManager.tick(now); // уводит в deep sleep, если пора / enters deep sleep when due
    }
  } else if (appMode == AppMode::STATS) {
    StatsMode::update(now);
    StatsMode::draw();
    // Выход по любому нажатию или по завершению прокрутки
    // Leave on any button press or when the scroll finishes
    if (clickBtn.justPressed() || menuBtn.justPressed() || StatsMode::isDone()) {
      appMode = AppMode::SKIN;
      wasIdle = false;
      sleepManager.reset(now);
    }
  } else if (appMode == AppMode::WIFI) {
    WifiMode::tick();
    WebSrv::handle();
    if (clickBtn.justPressed()) {
      WifiMode::nextMode();
      wifiLastActivity = now;
    }
    if (menuBtn.justPressed()) {
      WifiMode::stop();
      appMode = AppMode::SKIN;
      wasIdle = false;
      sleepManager.reset(now);
    } else if (now - wifiLastActivity >= WIFI_TIMEOUT_MS) {
      // Авто-выход из WiFi-меню по бездействию
      // WiFi menu auto-exit on inactivity
      WifiMode::stop();
      appMode = AppMode::SKIN;
      wasIdle = false;
      sleepManager.reset(now);
    }
    WifiMode::draw();
  } else { // AppMode::MENU
    if (menuBtn.longPressFired()) {
      applyMenuSelectionAndExit(now);
    } else {
      // Удержание клик-кнопки 3 сек → меню WiFi
      // Click button held for 3 s → WiFi menu
      if (clickBtn.isPressed() && clickBtn.heldMs(now) >= WIFI_LONG_PRESS_MS) {
        appMode = AppMode::WIFI;
        wifiLastActivity = now;
        sleepManager.reset(now);
        WifiMode::onEnter();
      } else {
        if (clickBtn.justPressed()) {
          MenuMode::next();
          menuLastActivity = now;
        }
        if (menuBtn.justPressed()) {
          menuLastActivity = now;
        }

        if (now - menuLastActivity >= MENU_TIMEOUT_MS) {
          applyMenuSelectionAndExit(now);
        } else {
          MenuMode::draw();
        }
      }
    }
  }

  display.sendBuffer();
  delay(1);
}
