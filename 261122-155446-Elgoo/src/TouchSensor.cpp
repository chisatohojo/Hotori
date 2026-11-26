#include <Arduino.h>

#include "CatCommand.h"
#include "TouchSensor.h"

extern void onTouchDetected();

void update3StageSensor(TouchSensor &sensor, void (*onShortPress)(),
                        void (*onContinuousPress)(), void (*onLongPress)()) {
  const unsigned long currentMillis = millis();
  const int touchValue = touchRead(sensor.pin);

  if (touchValue < sensor.threshold) {
    onTouchDetected();
    if (!sensor.isTouching) {
      sensor.isTouching = true;
      sensor.startTime = currentMillis;
      sensor.continuousHandled = false;
      sensor.longPressHandled = false;

      if (onShortPress != nullptr) {
        onShortPress();
      }
    } else {
      const unsigned long duration = currentMillis - sensor.startTime;

      if (!sensor.longPressHandled && duration > 10000UL) {
        sensor.longPressHandled = true;
        if (onLongPress != nullptr) {
          onLongPress();
        }
      } else if (!sensor.continuousHandled && duration > 2500UL &&
                 duration <= 10000UL) {
        sensor.continuousHandled = true;
        if (onContinuousPress != nullptr) {
          onContinuousPress();
        }
      }
    }
  } else if (sensor.isTouching) {
    sensor.isTouching = false;
    sendCatCommand(CAT_BREATH_FLAG | CAT_TAIL_FLAG, 0, 20, 0, 0, 0);
  }
}

void update1StageSensor(TouchSensor &sensor, void (*onPress)()) {
  const int touchValue = touchRead(sensor.pin);

  if (touchValue < sensor.threshold) {
    onTouchDetected();
    if (!sensor.isTouching) {
      sensor.isTouching = true;
      if (onPress != nullptr) {
        onPress();
      }
    }
  } else if (sensor.isTouching) {
    sensor.isTouching = false;
    sendCatCommand(CAT_BREATH_FLAG | CAT_TAIL_FLAG, 0, 20, 0, 0, 0);
  }
}

void updatehipSensor(TouchSensor &sensor, void (*onHit)(), void (*onTap)()) {
  const int touchValue = analogRead(sensor.pin);
  const unsigned long currentMillis = millis();
  static unsigned long lastTapMillis = 0;

  if (touchValue > 1850) {
    onTouchDetected();
    if (!sensor.isTouching) {
      sensor.isTouching = true;
      Serial.print("【おしり】アナログ値: ");
      Serial.println(touchValue);
      if (onHit != nullptr) {
        onHit();
      }
    }
  } else if (touchValue >= 800 && touchValue <= 1850) {
    onTouchDetected();
    if (!sensor.isTouching) {
      sensor.isTouching = true;
      Serial.print("【おしり】アナログ値: ");
      Serial.println(touchValue);

      if (currentMillis - lastTapMillis > 500UL) {
        lastTapMillis = currentMillis;
        sensor.tapCount++;
        if (sensor.tapCount > 5) {
          sensor.tapCount = 0;
          if (onTap != nullptr) {
          onTap();
          }
        }
      }
    }
  } else if (sensor.isTouching) {
    sensor.isTouching = false;
    sendCatCommand(CAT_BREATH_FLAG | CAT_TAIL_FLAG, 0, 20, 0, 0, 0);
  }
}
