#pragma once

#include <Arduino.h>

// STEP/DIR stepper output. The EN pin is shared by all drivers, so it is
// switched by the sketch from isRunning() of every motor.
class StepDirMotor {
 public:
  StepDirMotor(uint8_t stepPin,
               uint8_t directionPin,
               uint8_t forwardLevel,
               uint32_t stepHalfPeriodUs);

  void begin();
  void startForward();
  void startReverse();
  void stop();
  void update();
  bool isRunning() const;
  // Takes effect from the next STEP edge, also while running.
  void setStepHalfPeriodUs(uint32_t stepHalfPeriodUs);

 private:
  void start(uint8_t directionLevel);

  const uint8_t stepPin_;
  const uint8_t directionPin_;
  const uint8_t forwardLevel_;
  uint32_t stepHalfPeriodUs_;

  bool running_;
  bool stepHigh_;
  uint32_t lastStepChangeUs_;
};
