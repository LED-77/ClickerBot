#pragma once
#include <Arduino.h>
#include <functional>

// Растущие трещины поверх произвольной фигуры. Уровень (level) непрерывный
// float 0..MAX_CRACKS и полностью обратимый: пока growing == false, трещины
// плавно "заживают". Рисуются инверсией текущего фона, поэтому видны и на
// светлых, и на тёмных участках фигуры.
// Growing cracks over an arbitrary shape. The level is a continuous float
// 0..MAX_CRACKS and fully reversible: while growing == false the cracks heal.
// Drawn by inverting the current background, so they stay visible on both light
// and dark parts of the shape.
class CrackFX {
public:
  static constexpr uint8_t MAX_CRACKS = 4;
  static constexpr uint8_t CRACK_PTS  = 4;

  // true, если пиксель принадлежит фигуре — по нему генерим и рисуем трещины
  // true if the pixel belongs to the shape — used to generate and draw the cracks
  using PixelTest = std::function<bool(int x, int y)>;

  void reset();

  // origin — вокруг чего генерим трещины (обычно центр фигуры),
  // *RatePerSec — скорость роста/заживления в трещинах в секунду
  // origin — the point the cracks are generated around (usually the centre),
  // *RatePerSec — growth/healing speed in cracks per second
  void update(uint32_t now, bool growing, float growRatePerSec, float healRatePerSec,
              int originX, int originY, int spreadX, int spreadY);

  void draw(const PixelTest& bgTest);

  bool isMaxed() const { return level_ >= (float)MAX_CRACKS; }
  float level() const { return level_; }

private:
  void generate(int originX, int originY, int spreadX, int spreadY);
  void drawPartial(int idx, float progress, const PixelTest& bgTest);
  void drawLine(int x0, int y0, int x1, int y1, const PixelTest& bgTest);
  void plot(int x, int y, const PixelTest& bgTest);

  int crackX_[MAX_CRACKS][CRACK_PTS];
  int crackY_[MAX_CRACKS][CRACK_PTS];
  float level_ = 0;
  bool generated_ = false;
  uint32_t lastTime_ = 0;
};
