#pragma once
#include "Skin.h"

// Единая точка регистрации скинов. Чтобы добавить новый скин: написать
// класс, реализующий Skin, и добавить одну строку в SkinRegistry.cpp.
// Single place where skins are registered. To add one: implement the Skin
// interface and add a single line in SkinRegistry.cpp.
namespace SkinRegistry {
  constexpr uint8_t COUNT = 7;

  Skin* get(uint8_t index);
  const char* nameOf(uint8_t index);
  // Случайный индекс среди одиночных (не командных) скинов — для режима Random
  // Random index among solo (non-multiplayer) skins, for Random mode
  uint8_t randomSoloIndex();
}
