#pragma once

#include <Arduino.h>

#include "StepDirMotor.h"

class TailController {
 public:
  enum State : uint8_t { Stopped, Moving, WaitingForRelease };
  TailController(StepDirMotor& motor, Print& log);
  void begin();
  void apply(uint8_t direction, uint8_t speedPercent, uint8_t timeTenths);
  void update();
  State state() const { return state_; }
  uint32_t durationMs() const { return durationMs_; }
  static uint32_t halfPeriodForSpeed(uint8_t speedPercent);

 private:
  uint8_t readLimits() const;
  void startMotion();
  void reportLimits(uint8_t limits);

  StepDirMotor& motor_;
  Print& log_;
  State state_;
  uint8_t direction_;
  uint8_t speedPercent_;
  uint8_t lastLimits_;
  bool movingLeft_;
  uint32_t startedMs_;
  uint32_t durationMs_;
};
