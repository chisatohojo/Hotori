#pragma once

#include <Arduino.h>

class StepDirMotor {
 public:
  StepDirMotor(uint8_t stepPin, uint8_t directionPin, uint8_t enablePin);
  void begin();
  void start(bool forward, uint32_t halfPeriodUs);
  void stop();
  void update();
  bool isRunning() const { return running_; }
  uint32_t halfPeriodUs() const { return halfPeriodUs_; }

 private:
  const uint8_t stepPin_;
  const uint8_t directionPin_;
  const uint8_t enablePin_;
  bool running_;
  bool stepHigh_;
  uint32_t halfPeriodUs_;
  uint32_t lastEdgeUs_;
};
