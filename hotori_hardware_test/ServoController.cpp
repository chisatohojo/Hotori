#include "ServoController.h"

namespace {
constexpr uint8_t SERVO_MAX_ANGLE = 180;
}

ServoController::ServoController(uint8_t signalPin,
                                 uint8_t powerPin,
                                 uint8_t powerOnLevel,
                                 uint16_t minPulseUs,
                                 uint16_t maxPulseUs,
                                 uint8_t initialAngle)
    : signalPin_(signalPin),
      powerPin_(powerPin),
      powerOnLevel_(powerOnLevel),
      powerOffLevel_(powerOnLevel == HIGH ? LOW : HIGH),
      minPulseUs_(minPulseUs),
      maxPulseUs_(maxPulseUs),
      angle_(initialAngle > SERVO_MAX_ANGLE ? SERVO_MAX_ANGLE : initialAngle),
      powered_(false) {}

void ServoController::begin() {
  // Preload the off level before switching the pin to output.
  digitalWrite(powerPin_, powerOffLevel_);
  pinMode(powerPin_, OUTPUT);
  digitalWrite(signalPin_, LOW);
  pinMode(signalPin_, OUTPUT);
}

void ServoController::powerOn() {
  if (powered_) {
    return;
  }
  // A valid pulse is present before power arrives, so the servo does not
  // twitch to a random position.
  servo_.attach(signalPin_, minPulseUs_, maxPulseUs_);
  servo_.write(angle_);
  digitalWrite(powerPin_, powerOnLevel_);
  powered_ = true;
}

void ServoController::powerOff() {
  digitalWrite(powerPin_, powerOffLevel_);
  if (servo_.attached()) {
    servo_.detach();
  }
  digitalWrite(signalPin_, LOW);
  powered_ = false;
}

void ServoController::moveTo(uint8_t angle) {
  angle_ = angle > SERVO_MAX_ANGLE ? SERVO_MAX_ANGLE : angle;
  if (!powered_) {
    powerOn();  // Writes angle_ before switching the power on.
    return;
  }
  servo_.write(angle_);
}

bool ServoController::isPowered() const {
  return powered_;
}

uint8_t ServoController::angle() const {
  return angle_;
}
