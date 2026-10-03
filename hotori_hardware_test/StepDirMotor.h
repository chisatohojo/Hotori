#pragma once

#include <Arduino.h>

class StepDirMotor {
 public:
  StepDirMotor(uint8_t stepPin,
               uint8_t directionPin,
               uint8_t enablePin,
               uint8_t enableLevel,
               uint8_t forwardLevel,
               uint32_t stepHalfPeriodUs);

  void begin();
  void startForward();
  void startReverse();
  void stop();
  void update();
  bool isRunning() const;

 private:
  void start(uint8_t directionLevel);

  const uint8_t stepPin_;
  const uint8_t directionPin_;
  const uint8_t enablePin_;
  const uint8_t enableLevel_;
  const uint8_t disableLevel_;
  const uint8_t forwardLevel_;
  const uint32_t stepHalfPeriodUs_;

  bool running_;
  bool stepHigh_;
  uint32_t lastStepChangeUs_;
};
