#include "StepDirMotor.h"

#include "HotoriRuntimeConfig.h"

StepDirMotor::StepDirMotor(uint8_t stepPin, uint8_t directionPin, uint8_t enablePin)
    : stepPin_(stepPin), directionPin_(directionPin), enablePin_(enablePin),
      running_(false), stepHigh_(false), halfPeriodUs_(0), lastEdgeUs_(0) {}

void StepDirMotor::begin() {
  // Disable first and preload latches BEFORE switching pins to outputs.
  const uint8_t disabled = HotoriRuntimeConfig::MOTOR_ENABLE_LEVEL == LOW ? HIGH : LOW;
  digitalWrite(enablePin_, disabled);
  pinMode(enablePin_, OUTPUT);
  digitalWrite(stepPin_, LOW);
  pinMode(stepPin_, OUTPUT);
  digitalWrite(directionPin_, HotoriRuntimeConfig::MOTOR_FORWARD_LEVEL);
  pinMode(directionPin_, OUTPUT);
  running_ = false;
  stepHigh_ = false;
}

void StepDirMotor::start(bool forward, uint32_t halfPeriodUs) {
  stop();
  if (halfPeriodUs == 0) return;
  const uint8_t forwardLevel = HotoriRuntimeConfig::MOTOR_FORWARD_LEVEL;
  digitalWrite(directionPin_, forward ? forwardLevel : (forwardLevel == HIGH ? LOW : HIGH));
  halfPeriodUs_ = halfPeriodUs;
  lastEdgeUs_ = micros();
  digitalWrite(enablePin_, HotoriRuntimeConfig::MOTOR_ENABLE_LEVEL);
  running_ = true;
}

void StepDirMotor::stop() {
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
  digitalWrite(enablePin_, HotoriRuntimeConfig::MOTOR_ENABLE_LEVEL == LOW ? HIGH : LOW);
}

void StepDirMotor::update() {
  if (!running_) return;
  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - lastEdgeUs_) < halfPeriodUs_) return;
  stepHigh_ = !stepHigh_;
  digitalWrite(stepPin_, stepHigh_ ? HIGH : LOW);
  // No burst of catch-up pulses after slow work; speed can only decrease.
  lastEdgeUs_ = now;
}
