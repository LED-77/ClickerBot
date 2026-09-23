#pragma once
#include <Arduino.h>

// ---------------- Пины / Pins ----------------
constexpr uint8_t PIN_CLICK_BUTTON = 3; // кликательная кнопка / click button
constexpr uint8_t PIN_MENU_BUTTON  = 9; // кнопка BOOTплаты ESP32-C3 / BOOT button

// Пробуждение из deep sleep / Wake-up from deep sleep
constexpr uint64_t WAKE_PIN_MASK = (1ULL << PIN_CLICK_BUTTON);

// ---------------- Экран / Display ----------------
constexpr uint8_t SCREEN_WIDTH  = 72;
constexpr uint8_t SCREEN_HEIGHT = 40;
constexpr int LEFT_X  = 4;
constexpr int RIGHT_X = 68;

// ---------------- Кнопки / Buttons ----------------
constexpr uint32_t BTN_DEBOUNCE_MS    = 25;
// удержание кнопки меню — вход/выход / menu button hold: enter/exit
constexpr uint32_t MENU_LONG_PRESS_MS = 1000;
// дольше — экран "готов к прошивке" / longer: "ready to flash" screen
constexpr uint32_t FLASH_MODE_HOLD_MS = 5000;

// ---------------- Сон / Sleep ----------------
constexpr uint32_t TO_SLEEP_MS = 20000; // бездействие -> "засыпание" / idle -> falling asleep
constexpr uint32_t TO_DEEP_MS  = 20000; // ещё столько же -> deep sleep / same again -> deep sleep

// ---------------- Батарея / Battery ----------------
// Делитель 47k/47k: +аккум ──[47k]──●──[47k]── GPIO20, средняя точка ● = GPIO0 (ADC)
// 47k/47k divider: +batt ──[47k]──●──[47k]── GPIO20, midpoint ● = GPIO0 (ADC)
constexpr uint8_t PIN_BATT_ADC  = 0;
constexpr uint8_t PIN_BATT_CTRL = 20;
constexpr float BATT_DIVIDER = 2.0f; // на ADC половина напряжения аккума / ADC reads half of Vbat
constexpr uint32_t BATT_FULL_MV  = 4200;
constexpr uint32_t BATT_EMPTY_MV = 3300;
// Ниже этого заряда — перечёркнутая батарея на заставке и сразу deep sleep:
// на грани разряда ESP32 зацикливается на brownout-перезагрузках.
// Below this level: crossed-out battery on the splash, then straight to deep sleep —
// near empty the ESP32 gets stuck in a brownout reset loop.
constexpr int BATT_CRITICAL_PERCENT = 5;
// сколько висит заставка при старте / splash duration on boot
constexpr uint32_t BATTERY_SPLASH_MS = 1500;

// ---------------- Меню / Menu ----------------
// Без нажатий дольше этого времени меню само возвращается в скин
// No button press for this long: the menu goes back to the skin
constexpr uint32_t MENU_TIMEOUT_MS = 60000;

// ---------------- Статистика / Statistics ----------------
constexpr uint32_t STATS_LONG_PRESS_MS = 3000; // удержание клик-кнопки / click button hold

// ---------------- WiFi ----------------
constexpr uint32_t WIFI_LONG_PRESS_MS = 3000; // удержание клик-кнопки в меню / hold in menu
// 5 минут — чтобы WiFi не отключался, пока настраиваешь с телефона
// 5 minutes, so WiFi stays up while you set it up from a phone
constexpr uint32_t WIFI_TIMEOUT_MS = 300000;

// ---------------- Версия / Version ----------------
// Печатается в UART при старте и лежит меткой в .bin (FW_BIN_MARKER)
// Printed to UART on boot and embedded in the .bin as a CLICKERFW: marker
#define FW_VERSION "2.1"
