#include "BatteryManager.h"
#include "../Config.h"
#include "DisplayManager.h"

namespace {
  int s_percent = 100;        // заряд, округлённый до 5% / charge rounded to 5%
  int s_rawPercent = 100;     // точный заряд (для порога) / exact charge (threshold)
  uint32_t s_millivolts = 0;  // последнее напряжение, мВ / last voltage in mV
}

void Battery::begin() {
  // Точка ADC = Vbat/2 — безопасно для входа АЦП. Ток делителя ~45 мкА,
  // в deep sleep он размыкается sleepSafe().
  // ADC node = Vbat/2, safe for the ADC input. Divider current is ~45 uA; the
  // divider is opened in deep sleep by sleepSafe().
  pinMode(PIN_BATT_CTRL, OUTPUT);
  digitalWrite(PIN_BATT_CTRL, LOW);
}

int Battery::measurePercent() {
  delay(20); // стабилизация напряжения на ёмкости делителя / let the divider settle
  uint32_t nodeMv = analogReadMilliVolts(PIN_BATT_ADC); // Vbat / BATT_DIVIDER
  s_millivolts = (uint32_t)(nodeMv * BATT_DIVIDER);
  int pct;
  if (s_millivolts >= BATT_FULL_MV) {
    pct = 100;
  } else if (s_millivolts <= BATT_EMPTY_MV) {
    pct = 0;
  } else {
    pct = (int)((s_millivolts - BATT_EMPTY_MV) * 100 / (BATT_FULL_MV - BATT_EMPTY_MV));
  }
  s_rawPercent = pct; // точный замер для порога / exact value for the threshold
  // Округление до 5% — измерение грубое, дробные проценты бессмысленны
  // Round to 5%: the reading is coarse, fractional percent is meaningless
  s_percent = ((pct + 2) / 5) * 5;
  if (s_percent > 100) s_percent = 100;
  return s_percent;
}

int Battery::percent() {
  return s_percent;
}

uint32_t Battery::millivolts() {
  return s_millivolts;
}

bool Battery::isLow() {
  // Сравниваем с точным замером, а не с округлённым до 5%
  // Compare against the exact reading, not the value rounded to 5%
  return s_rawPercent < BATT_CRITICAL_PERCENT;
}

void Battery::sleepSafe() {
  // GPIO20 в высокий импеданс: он не RTC-пин, во сне ток через делитель не течёт
  // GPIO20 to high impedance: it is not an RTC pin, so no divider current in deep sleep
  pinMode(PIN_BATT_CTRL, INPUT);
}

void Battery::draw(int percent, bool low) {
  // Корпус батареи 22x10 + контакт справа; пара "иконка + %" по центру экрана
  // 22x10 battery body plus a terminal on the right; "icon + %" pair centred
  const int bw = 22, bh = 10;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", percent);

  display.setFont(u8g2_font_5x7_tr);
  int tw = display.getStrWidth(buf);
  int bx = (SCREEN_WIDTH - (bw + 4 + tw)) / 2;
  // Чуть ниже центра: на заставке сверху приветствие — чтобы не смотрелось "налеплено"
  // Slightly below centre so it does not stick to the greeting above on the splash
  int by = (SCREEN_HEIGHT - bh) / 2 + 4;

  display.drawFrame(bx, by, bw, bh);
  display.drawBox(bx + bw, by + 2, 2, bh - 4); // контакт / terminal

  if (low) {
    // Залитая батарея с белым крестом — читается однозначно как "разряжено"
    // Filled battery with a white cross reads unambiguously as "empty"
    display.drawBox(bx, by, bw, bh);
    display.setDrawColor(0);
    display.drawLine(bx, by, bx + bw - 1, by + bh - 1);
    display.drawLine(bx + bw - 1, by, bx, by + bh - 1);
    display.drawLine(bx + bw - 1, by + 2, bx, by + bh - 3);
    display.setDrawColor(1);
  } else {
    int fill = (percent * (bw - 2)) / 100;
    if (fill > 0) display.drawBox(bx + 1, by + 1, fill, bh - 2);
  }
  display.drawStr(bx + bw + 4, by + bh - 1, buf);
}
