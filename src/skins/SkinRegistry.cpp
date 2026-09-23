#include "SkinRegistry.h"
#include "EyeSkin.h"
#include "BubbleWrapSkin.h"
#include "OdometerSkin.h"
#include "CardioSkin.h"
#include "DuelSkin.h"
#include "KothSkin.h"
#include "BanditSkin.h"

namespace {
  EyeSkin eyeSkin;
  BubbleWrapSkin bubbleWrapSkin;
  OdometerSkin odometerSkin;
  CardioSkin cardioSkin;
  DuelSkin duelSkin;
  KothSkin kothSkin;
  BanditSkin banditSkin;
  Skin* skins[SkinRegistry::COUNT] = { &eyeSkin, &bubbleWrapSkin, &odometerSkin, &cardioSkin, &duelSkin, &kothSkin, &banditSkin };
}

Skin* SkinRegistry::get(uint8_t index) {
  if (index >= COUNT) index = 0;
  return skins[index];
}

const char* SkinRegistry::nameOf(uint8_t index) {
  return get(index)->name();
}

uint8_t SkinRegistry::randomSoloIndex() {
  // Режим Random: командные скины (Duel, KOTH) не выбираем
  // Random mode: skip multiplayer skins (Duel, KOTH)
  uint8_t solo[COUNT];
  uint8_t n = 0;
  for (uint8_t i = 0; i < COUNT; i++) {
    if (!skins[i]->isMultiplayer()) solo[n++] = i;
  }
  if (n == 0) return 0; // не должно случиться — одиночные скины есть всегда / should never happen
  return solo[(uint8_t)random(0, n)];
}
