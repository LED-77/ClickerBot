#include "CardioSkin.h"
#include "../hal/DisplayManager.h"
#include "../util/MathUtils.h"
#include <math.h>

namespace {
  // Сердце 7x6 пикселей: координаты (x, y) от левого верхнего угла
  // 7x6 pixel heart: (x, y) coordinates from the top-left corner
  const int8_t HEART_PX[][2] = {
    {1,0},{2,0},{4,0},{5,0},
    {0,1},{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},
    {0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},
    {1,3},{2,3},{3,3},{4,3},{5,3},
    {2,4},{3,4},{4,4},
    {3,5},
  };

  // Фаза сердечного цикла 0..1. Считаем через ЦЕЛОЧИСЛЕННЫЙ остаток от
  // millis(), а не fmodf((float)millis()): float точно держит только целые до
  // 2^24 (~4.6 ч аптайма), дальше фаза скачет и сердце/ЭКГ мерцают.
  // Heart-cycle phase 0..1. Computed from an INTEGER modulo of millis(), not
  // fmodf((float)millis()): a float holds integers exactly only up to 2^24
  // (~4.6 h of uptime), after which the phase jumps and the heart/ECG flicker.
  float cyclePhase(uint32_t now, float cycleMs) {
    uint32_t cm = (uint32_t)(cycleMs + 0.5f);
    if (cm == 0) cm = 1;
    return (float)(now % cm) / (float)cm;
  }
}

void CardioSkin::onEnter() {
  for (uint8_t i = 0; i < ECG_LEN; i++) ecgBuf_[i] = 0; // ровная линия
  lastSampleMs_ = millis();
  intensity_.reset();
  counter_.begin(counterKey());
}

// ЭКГ-волна: фаза 0..1 (один сердечный цикл), amp — амплитуда зубцов
// ECG waveform: phase 0..1 (one heart cycle), amp = spike amplitude
int8_t CardioSkin::ecgValue(float phase, float amp) const {
  // P-зубец (небольшой)
  // P wave (small)
  if (phase < 0.12f) return (int8_t)(sinf((phase / 0.12f) * PI) * amp * 0.25f);
  // Комплекс QRS: резкий провал-пик
  // QRS complex: a sharp dip followed by a spike
  if (phase < 0.18f) return (int8_t)(-amp * 0.4f * ((phase - 0.12f) / 0.06f));
  if (phase < 0.22f) return (int8_t)(-amp * 0.4f + amp * 1.6f * ((phase - 0.18f) / 0.04f));
  if (phase < 0.28f) return (int8_t)(amp * 1.2f * (1.0f - (phase - 0.22f) / 0.06f));
  // T-зубец (средний)
  // T wave (medium)
  if (phase < 0.40f) return (int8_t)(sinf(((phase - 0.28f) / 0.12f) * PI) * amp * 0.45f);
  return 0;
}

void CardioSkin::addSample(uint32_t now) {
  if (now - lastSampleMs_ < SAMPLE_MS) return;
  lastSampleMs_ = now;

  // Амплитуда зубцов зависит от интенсивности кликов
  // Spike amplitude follows the click intensity
  float amp = 2.5f + intensity_.value() / 100.0f * 11.0f;

  // Частота сердечных сокращений: 60..160 ударов/мин
  // Heart rate: 60..160 bpm
  float pulse = intensity_.value() / 100.0f;
  float bpm = 60.0f + pulse * 100.0f;
  float cycleMs = 60000.0f / bpm;
  float phase = cyclePhase(now, cycleMs);

  int8_t v = ecgValue(phase, amp);
  if (v > 16) v = 16;
  if (v < -16) v = -16;

  // Сдвигаем буфер влево, новый сэмпл справа
  // Shift the buffer left, the new sample goes on the right
  for (uint8_t i = 0; i < ECG_LEN - 1; i++) ecgBuf_[i] = ecgBuf_[i + 1];
  ecgBuf_[ECG_LEN - 1] = v;
}

// Сердце с плавным дробным масштабированием — без мерцания,
// размер растёт/сжимается непрерывно (доли пикселя)
// Heart drawn with smooth fractional scaling — no flicker, the size grows and
// shrinks continuously (sub-pixel steps)
void CardioSkin::drawHeart(int cx, int cy, float scale) {
  if (scale < 1.0f) scale = 1.0f;
  display.setDrawColor(1);
  for (size_t i = 0; i < sizeof(HEART_PX) / sizeof(HEART_PX[0]); i++) {
    float x0 = HEART_PX[i][0] * scale;
    float x1 = x0 + scale;
    float y0 = HEART_PX[i][1] * scale;
    float y1 = y0 + scale;
    int ix0 = (int)floorf(x0), ix1 = (int)ceilf(x1);
    int iy0 = (int)floorf(y0), iy1 = (int)ceilf(y1);
    for (int dy = iy0; dy < iy1; dy++) {
      for (int dx = ix0; dx < ix1; dx++) {
        display.drawPixel(cx + dx, cy + dy);
      }
    }
  }
}

void CardioSkin::update(uint32_t now, bool clickPressed) {
  now_ = now;
  if (clickPressed) {
    intensity_.onClick(now);
    counter_.pop(now, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 5);
  }
  intensity_.tick(now); // пассивное затухание, если кликов нет
  addSample(now);
}

void CardioSkin::draw() {
  display.setDrawColor(1);

  // --- ЭКГ-линия ---
  // --- ECG trace ---
  int prevY = BASE_Y - ecgBuf_[0];
  for (uint8_t x = 1; x < ECG_LEN; x++) {
    int y = BASE_Y - ecgBuf_[x];
    display.drawLine(x - 1, prevY, x, y);
    prevY = y;
  }

  // --- Пульсирующее сердце в левом верхнем углу (поверх линии) ---
  // Та же ЧСС и фаза, что и у ЭКГ-линии — сердце бьётся в такт с зубцами
  // --- Pulsing heart in the top-left corner (over the trace) ---
  // Same rate and phase as the ECG line, so the heart beats in time with it
  float pulse = intensity_.value() / 100.0f;
  float bpm = 60.0f + pulse * 100.0f;
  float cycleMs = 60000.0f / bpm;
  float phase = cyclePhase(millis(), cycleMs);
  // Огибающая сердечного толчка: быстрый подъём, плавный спад
  // Pulse envelope: a quick rise, a smooth fall
  float env = (phase < 0.15f) ? (phase / 0.15f) : (1.0f - (phase - 0.15f) / 0.85f);
  float scaleF = 1.0f + env * pulse * 0.6f; // 1.0..1.6, непрерывно
  drawHeart(2, 2, scaleF);

  counter_.draw(now_);
}

void CardioSkin::onExit() {
  counter_.flush();
}

void CardioSkin::flushCounters() {
  counter_.flush();
}

void CardioSkin::onIdleEnter() {
  counter_.flush();
}

void CardioSkin::drawIdle() {
  // Успокоенный монитор: почти ровная линия + маленькое тихое сердце
  // Calmed-down monitor: an almost flat trace plus a small quiet heart
  display.setDrawColor(1);

  // Едва заметное биение спокойного сердца
  // A barely visible beat of the calm heart
  float phase = cyclePhase(millis(), 2000.0f);
  float env = (phase < 0.15f) ? (phase / 0.15f) : (1.0f - (phase - 0.15f) / 0.85f);
  drawHeart(2, 2, 1.0f + env * 0.2f);

  // Едва заметное дыхание линии
  // Barely visible breathing of the trace
  float breathPhase = cyclePhase(millis(), 4000.0f);
  int off = (int)(sinf(breathPhase * 2.0f * PI) * 0.6f);
  int prevY = BASE_Y + off;
  for (uint8_t x = 1; x < ECG_LEN; x++) {
    int y = BASE_Y + off;
    display.drawLine(x - 1, prevY, x, y);
    prevY = y;
  }
}

void CardioSkin::onWake(uint32_t now) {
  lastSampleMs_ = now;
}

bool CardioSkin::canIdle() const {
  return true; // монитор всегда может "заснуть" в спокойный режим
}
