#pragma once

#include <Arduino.h>

#include "UartStepMotor.h"

// Angle control of one head axis (Motor1 / left-right or Motor2 / up-down).
//
// The two end switches of an axis are wired in parallel on one input, so the
// pressed end is taken from the direction of travel. Angles grow with
// forward steps.
//
// Homing runs toward the max switch and sets the angle to maxSwitchAngle.
// Limit rule: a pressed switch stops the axis at once and corrects the angle
// to that end. Moving away from it (escaping) is allowed; if the switch is
// still pressed after LIMIT_ESCAPE_MAX_DEGREES, it is taken as the opposite
// switch and the axis stops again.
class HeadAxis {
 public:
  struct Config {
    const __FlashStringHelper* name;  // Log prefix, e.g. "HEAD LR".
    uint8_t limitPin;
    float stepsPerDegree;
    float minSwitchAngle;
    float maxSwitchAngle;
    int8_t minCommandAngle;  // Range accepted from the ESP32.
    int8_t maxCommandAngle;
  };

  enum State : uint8_t { Unhomed, Homing, Ready, Fault };

  HeadAxis(UartStepMotor& motor, const Config& config);

  void begin();
  // Finds the max switch, then moves to the pending angle (0 if none).
  // Without limit switches the current position becomes 0 degrees.
  void startHoming();
  // speedPercent 0 stops the axis. Homes first when not homed yet. Angles
  // are clamped to the command range and kept clear of the switches.
  void moveToAngle(int8_t angle, uint8_t speedPercent);
  // Manual run (m1f / m2f ...) at the test speed. Rejected toward a pressed
  // switch.
  bool manualStart(bool reverse);
  // Stops and keeps holding the angle (the head would sag if freed).
  void stop();
  // Call every loop() before motor.update().
  void update();

  State state() const { return state_; }
  // True when the axis should keep the shared EN on to hold its angle.
  bool needsHold() const;
  void printStatus();

 private:
  bool limitPressed() const;
  int32_t angleToSteps(float angle) const;
  int32_t degreesToSteps(float degrees) const;
  static uint32_t halfPeriodForSteps(uint32_t stepsPerSecond);
  uint32_t halfPeriodForSpeed(uint8_t speedPercent) const;
  bool canStart(bool positive);
  void goTo(float angle, uint8_t speedPercent);
  void hitSwitch(bool positive);
  void fail(const __FlashStringHelper* reason);
  void logPrefix();

  UartStepMotor& motor_;
  const Config config_;
  State state_;
  bool probing_;          // Homing started on a pressed switch.
  bool blocked_;          // Stopped on a switch, not yet released.
  bool blockedPositive_;  // Direction that hit the switch.
  int32_t escapeStart_;   // Position where the switch was hit.
  uint8_t releaseCount_;
  int32_t homingStart_;
  bool hasPending_;
  int8_t pendingAngle_;
  uint8_t pendingSpeed_;
};
