#pragma once
#include <Arduino.h>
#include <functional>

// Разбивает фигуру на NUM_FRAGMENTS кусков вокруг центра и разлетает их как
// твёрдые тела (позиция + вращение), а не отдельными пикселями.
// Splits a shape into NUM_FRAGMENTS chunks around its centre and flies them apart
// as rigid bodies (position + rotation) rather than loose pixels.
class ShatterFX {
public:
  static constexpr uint8_t NUM_FRAGMENTS = 6;
  static constexpr uint8_t FRAG_MAX_PTS  = 28;

  using PixelTest = std::function<bool(int x, int y)>;

  // scan* — прямоугольник поиска пикселей фигуры; origin — центр разлёта
  // scan* — the rectangle searched for shape pixels; origin — the blast centre
  void spawn(int scanX0, int scanY0, int scanX1, int scanY1,
             int originX, int originY, const PixelTest& test);

  void update();
  void draw();
  bool isDone() const;

private:
  struct Fragment {
    float x, y;       // мировые координаты центроида осколка
    float vx, vy;
    float rot;         // текущий угол поворота (радианы)
    float angVel;
    int8_t offX[FRAG_MAX_PTS]; // форма осколка: смещения от центроида
    int8_t offY[FRAG_MAX_PTS];
    uint8_t count;
    bool alive;
  };

  Fragment fragments_[NUM_FRAGMENTS];
};
