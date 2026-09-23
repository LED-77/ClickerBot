#pragma once

// Экран "готов к прошивке": рисуется, пока держат кнопку меню (GPIO9 = BOOT)
// дольше FLASH_MODE_HOLD_MS, и остаётся в видеопамяти OLED после RESET.
// "Ready to flash" screen: drawn while the menu button (GPIO9 = BOOT) is held
// longer than FLASH_MODE_HOLD_MS; it stays in the OLED RAM after RESET.
namespace FlashReadyMode {
  void draw();
}
