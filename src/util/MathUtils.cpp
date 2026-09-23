#include "MathUtils.h"
#include <esp_random.h>

int getRnd(int minV, int maxV) {
  return minV + (esp_random() % (maxV - minV));
}

float mapf(float x, float inMin, float inMax, float outMin, float outMax) {
  return outMin + (x - inMin) * (outMax - outMin) / (inMax - inMin);
}
