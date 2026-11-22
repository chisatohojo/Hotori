#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <Arduino.h>

struct TouchSensor {
  uint8_t pin;
  uint16_t threshold;
  bool isTouching;
  unsigned long startTime;
  bool continuousHandled;
  bool longPressHandled;
  uint8_t tapCount;
  unsigned long releaseTime;
};

void update3StageSensor(TouchSensor &sensor, void (*onShortPress)(),
                       void (*onContinuousPress)(), void (*onLongPress)());
void update1StageSensor(TouchSensor &sensor, void (*onPress)());
void updatehipSensor(TouchSensor &sensor, void (*onHit)(), void (*onTap)());

#endif
