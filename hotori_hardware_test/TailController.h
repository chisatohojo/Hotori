#pragma once

#include <Arduino.h>

#include "UartStepMotor.h"

// Motor3 / Tail (TMC2209 UART, quiet): ESP32 tail commands (tailDir /
// tailSpeed / tailTime), manual m3f / m3r runs and the tail limit stop.
//
// Limit rule: either switch active stops the tail at once in any direction,
// rejects every start, and nothing restarts by itself.
class TailController {
 public:
  TailController(UartStepMotor& motor, uint8_t leftLimitPin,
                 uint8_t rightLimitPin);

  void begin();
  // dir 0 = stop, 1 = left, 2 = right, 3 = left-right swing.
  // speed 1-100 %, timeTenths 1-255 (x 0.1 s). Any 0 stops the tail.
  void apply(uint8_t dir, uint8_t speedPercent, uint8_t timeTenths);
  bool manualStart(bool reverse);
  void stop();
  // Call every loop() before motor.update().
  void update();
  // Bit 0 = left active, bit 1 = right active.
  uint8_t readLimits() const;

 private:
  enum State : uint8_t { Stopped, Commanded, Manual };

  static uint32_t halfPeriodForSpeed(uint8_t speedPercent);
  void startMotion();
  void reportLimits(uint8_t limits);

  UartStepMotor& motor_;
  const uint8_t leftLimitPin_;
  const uint8_t rightLimitPin_;
  State state_;
  uint8_t dir_;
  bool movingLeft_;
  uint8_t reportedLimits_;
  uint32_t startedMs_;
  uint32_t durationMs_;
  uint32_t lastSwingMs_;
};
