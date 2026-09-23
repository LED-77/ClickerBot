#pragma once
#include "Skin.h"
#include "../Config.h"
#include "../fx/ClickIntensity.h"
#include "../fx/PopCounter.h"

// Кардиомонитор: ЭКГ-линия + пульсирующее сердце в углу.
// Чем чаще кликаешь — тем выше амплитуда зубцов ЭКГ и тем быстрее/сильнее
// пульс сердца. Перестал кликать — всё успокаивается.
// Heart monitor: an ECG trace plus a pulsing heart in the corner. The faster you
// click, the taller the ECG spikes and the faster/stronger the pulse. Stop
// clicking and everything calms down.
class CardioSkin : public Skin {
public:
  void onEnter() override;
  void onExit() override;
  void flushCounters() override;
  void update(uint32_t now, bool clickPressed) override;
  void draw() override;

  void onIdleEnter() override;
  void drawIdle() override;
  void onWake(uint32_t now) override;
  bool canIdle() const override;

  const char* name() const override { return "Cardio"; }
  const char* counterKey() const override { return "cntCardio"; }
  const char* wireKey() const override { return "cardio"; }

private:
  static constexpr uint8_t ECG_LEN = SCREEN_WIDTH; // сэмпл на каждый пиксель ширины
  static constexpr int BASE_Y = 28;                // базовая линия ЭКГ
  static constexpr uint32_t SAMPLE_MS = 25;        // период обновления сэмпла

  int8_t ecgBuf_[ECG_LEN];
  uint32_t lastSampleMs_ = 0;

  ClickIntensity intensity_;
  PopCounter counter_;
  uint32_t now_ = 0;

  int8_t ecgValue(float phase, float amp) const;
  void addSample(uint32_t now);
  void drawHeart(int cx, int cy, float scale);
};
