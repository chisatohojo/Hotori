#pragma once

#include <Arduino.h>

#include "StepDirMotor.h"

class BreathController {
 public:
  BreathController(StepDirMotor& motor, Print& log, uint32_t stepsPerCycle);
  void apply(uint8_t breathsPerMinute);
  void update();
  uint8_t breathsPerMinute() const { return breathsPerMinute_; }

 private:
  StepDirMotor& motor_;
  Print& log_;
  const uint32_t stepsPerCycle_;
  uint8_t breathsPerMinute_;
  bool calibrationReported_;
};
