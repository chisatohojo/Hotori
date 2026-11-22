#include "StepDirMotor.h"

StepDirMotor::StepDirMotor(uint8_t stepPin,
                           uint8_t directionPin,
                           uint8_t forwardLevel,
                           uint32_t stepHalfPeriodUs)
    : stepPin_(stepPin),
      directionPin_(directionPin),
      forwardLevel_(forwardLevel),
      stepHalfPeriodUs_(stepHalfPeriodUs),
      running_(false),
      stepHigh_(false),
      lastStepChangeUs_(0) {}

void StepDirMotor::begin() {
  // Preload output levels before changing pin direction.
  digitalWrite(stepPin_, LOW);
  digitalWrite(directionPin_, forwardLevel_);

  pinMode(stepPin_, OUTPUT);
  pinMode(directionPin_, OUTPUT);
}

void StepDirMotor::startForward() {
  start(forwardLevel_);
}

void StepDirMotor::startReverse() {
  start(forwardLevel_ == HIGH ? LOW : HIGH);
}

void StepDirMotor::start(uint8_t directionLevel) {
  // Start every run from a known STEP level.
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
  digitalWrite(directionPin_, directionLevel);
  lastStepChangeUs_ = micros();
  running_ = true;
}

void StepDirMotor::stop() {
  // Only STEP stops; the driver stays energized (EN tied to GND).
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
}

void StepDirMotor::update() {
  if (!running_) {
    return;
  }

  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - lastStepChangeUs_) < stepHalfPeriodUs_) {
    return;
  }

  stepHigh_ = !stepHigh_;
  digitalWrite(stepPin_, stepHigh_ ? HIGH : LOW);
  lastStepChangeUs_ = now;
}

bool StepDirMotor::isRunning() const {
  return running_;
}

void StepDirMotor::setStepHalfPeriodUs(uint32_t stepHalfPeriodUs) {
  stepHalfPeriodUs_ = stepHalfPeriodUs;
}
