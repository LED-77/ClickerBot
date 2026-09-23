#pragma once
#include <U8g2lib.h>

// U8g2 не имеет конструктора под этот SSD1306 72×40 — отсюда свой класс.
// U8g2 ships no constructor for this particular SSD1306 72x40, hence the class.
class U8G2_SSD1306_72X40_NONAME_F_HW_I2C : public U8G2 {
  public:
    U8G2_SSD1306_72X40_NONAME_F_HW_I2C(
        const u8g2_cb_t* rot,
        uint8_t rst = U8X8_PIN_NONE,
        uint8_t clk = U8X8_PIN_NONE,
        uint8_t dat = U8X8_PIN_NONE) : U8G2() {
      u8g2_Setup_ssd1306_i2c_72x40_er_f(
        &u8g2, rot, u8x8_byte_arduino_hw_i2c,
        u8x8_gpio_and_delay_arduino
      );
      u8x8_SetPin_HW_I2C(getU8x8(), rst, clk, dat);
    }
};

// Единственный экземпляр дисплея, доступный всему проекту
// The single display instance shared by the whole project
extern U8G2_SSD1306_72X40_NONAME_F_HW_I2C display;

void displayInit();

// Гасит экран и уводит контроллер в deep sleep; просыпается по LOW на любом
// пине из wakePinMask (см. WAKE_PIN_MASK в Config.h)
// Blanks the screen and puts the controller into deep sleep; wakes on LOW on any
// pin from wakePinMask (see WAKE_PIN_MASK in Config.h)
void goToDeepSleep(uint64_t wakePinMask);
