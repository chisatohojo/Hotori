#include "StepDirMotor.h"

StepDirMotor::StepDirMotor(uint8_t stepPin,
                           uint8_t directionPin,
                           uint8_t enablePin,
                           uint8_t enableLevel,
                           uint8_t forwardLevel,
                           uint32_t stepHalfPeriodUs)
    : stepPin_(stepPin),
      directionPin_(directionPin),
      enablePin_(enablePin),
      enableLevel_(enableLevel),
      disableLevel_(enableLevel == LOW ? HIGH : LOW),
      forwardLevel_(forwardLevel),
      stepHalfPeriodUs_(stepHalfPeriodUs),
      running_(false),
      stepHigh_(false),
      lastStepChangeUs_(0) {}

void StepDirMotor::begin() {
  // Preload safe output levels before changing pin direction. This avoids a
  // brief active-LOW enable pulse while the MCU pins are being configured.
  digitalWrite(stepPin_, LOW);
  digitalWrite(directionPin_, forwardLevel_);
  digitalWrite(enablePin_, disableLevel_);

  pinMode(stepPin_, OUTPUT);
  pinMode(directionPin_, OUTPUT);
  pinMode(enablePin_, OUTPUT);
}

void StepDirMotor::startForward() {
  start(forwardLevel_);
}

void StepDirMotor::startReverse() {
  start(forwardLevel_ == HIGH ? LOW : HIGH);
}

void StepDirMotor::start(uint8_t directionLevel) {
  // Start every run from a known STEP level before enabling the driver.
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
  digitalWrite(directionPin_, directionLevel);
  digitalWrite(enablePin_, enableLevel_);
  lastStepChangeUs_ = micros();
  running_ = true;
}

void StepDirMotor::stop() {
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
  digitalWrite(enablePin_, disableLevel_);
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
