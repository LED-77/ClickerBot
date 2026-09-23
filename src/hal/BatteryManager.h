#pragma once
#include <Arduino.h>

// Контроль заряда аккумулятора.
// Делитель 47k/47k: +аккум ──[47k]──●──[47k]── GPIO20, ● = GPIO0 (ADC).
// При равных резисторах на ADC половина напряжения аккумулятора — так мы
// вписываемся в диапазон АЦП ESP32-C3 (~0..2.5 В). Измеряем один раз при
// старте (каждое пробуждение из deep sleep проходит через setup()); перед
// сном делитель размыкается (sleepSafe), чтобы не сажать аккумулятор.
// Battery gauge.
// 47k/47k divider: +batt ──[47k]──●──[47k]── GPIO20, ● = GPIO0 (ADC).
// With equal resistors the ADC sees half of the battery voltage, which fits the
// ESP32-C3 ADC range (~0..2.5 V). Measured once on boot (every deep-sleep wake
// goes through setup()); before sleeping the divider is opened (sleepSafe) so it
// does not drain the battery.
namespace Battery {
  void begin();            // заземлить пин делителя / ground the divider pin
  int  measurePercent();   // измерить заряд, округлить до 5% / measure, round to 5%
  int  percent();          // последний замер (0..100, кратно 5) / last reading
  uint32_t millivolts();   // последнее напряжение, мВ / last battery voltage in mV
  bool isLow();            // ниже BATT_CRITICAL_PERCENT — deep sleep / below critical level
  void sleepSafe();        // разомкнуть делитель перед сном / open the divider before sleep
  void draw(int percent, bool low); // иконка батареи + процент / battery icon + percent
}
