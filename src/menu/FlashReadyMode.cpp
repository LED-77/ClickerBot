#include "FlashReadyMode.h"
#include "../Config.h"
#include "../hal/DisplayManager.h"
#include <math.h>

namespace {
  // Шестерёнка / gear
  void drawGear(int cx, int cy, int outerR, int innerR, uint8_t numTeeth, int toothSize) {
    display.setDrawColor(1);
    display.drawDisc(cx, cy, outerR);
    for (uint8_t i = 0; i < numTeeth; i++) {
      float angle = (2.0f * PI * i) / numTeeth;
      int tx = cx + (int)(cosf(angle) * outerR);
      int ty = cy + (int)(sinf(angle) * outerR);
      display.drawBox(tx - toothSize / 2, ty - toothSize / 2, toothSize, toothSize);
    }
    display.setDrawColor(0);
    display.drawDisc(cx, cy, innerR);
    display.setDrawColor(1);
  }
}

void FlashReadyMode::draw() {
  drawGear(SCREEN_WIDTH / 2, 13, 9, 3, 8, 3);

  display.setFont(u8g2_font_5x7_tf);

  const char* line1 = "FLASH MODE";
  int w1 = display.getStrWidth(line1);
  display.drawStr((SCREEN_WIDTH - w1) / 2, 30, line1);

  const char* line2 = "press RESET";
  int w2 = display.getStrWidth(line2);
  display.drawStr((SCREEN_WIDTH - w2) / 2, 39, line2);
}
