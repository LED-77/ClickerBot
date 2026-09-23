#include "DisplayManager.h"
#include "BatteryManager.h"
#include <esp_sleep.h>

U8G2_SSD1306_72X40_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 6, 5);

void displayInit() {
  display.begin();
  display.setBusClock(400000);
  display.setPowerSave(0);
}

void goToDeepSleep(uint64_t wakePinMask) {
  // Размыкаем делитель напряжения — во сне он не должен сажать аккумулятор
  // Open the voltage divider so it does not drain the battery in deep sleep
  Battery::sleepSafe();
  display.setPowerSave(1);
  delay(100);
  esp_deep_sleep_enable_gpio_wakeup(wakePinMask, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}
