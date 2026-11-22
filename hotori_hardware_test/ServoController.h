#pragma once

#include <Arduino.h>
#include <Servo.h>

// Hobby servo (SG90) with a separate power switch pin. The servo is powered
// only while on; the signal is detached and held LOW while off.
class ServoController {
 public:
  ServoController(uint8_t signalPin,
                  uint8_t powerPin,
                  uint8_t powerOnLevel,
                  uint16_t minPulseUs,
                  uint16_t maxPulseUs,
                  uint8_t initialAngle);

  // Leaves the servo powered off.
  void begin();
  // Starts the pulse at the last angle, then switches the power on.
  void powerOn();
  // Switches the power off, then stops the pulse.
  void powerOff();
  // Powers on if needed and moves to angle (clamped to 0-180).
  void moveTo(uint8_t angle);
  bool isPowered() const;
  uint8_t angle() const;

 private:
  Servo servo_;
  const uint8_t signalPin_;
  const uint8_t powerPin_;
  const uint8_t powerOnLevel_;
  const uint8_t powerOffLevel_;
  const uint16_t minPulseUs_;
  const uint16_t maxPulseUs_;
  uint8_t angle_;
  bool powered_;
};
