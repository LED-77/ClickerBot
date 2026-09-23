#pragma once
#include <Arduino.h>

// Случайное целое в диапазоне [minV, maxV)
// Random integer in [minV, maxV)
int getRnd(int minV, int maxV);

// Аналог Arduino map(), но для float
// Arduino map() equivalent, but for floats
float mapf(float x, float inMin, float inMax, float outMin, float outMax);
