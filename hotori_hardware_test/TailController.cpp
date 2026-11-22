#include "TailController.h"

#include "HotoriConfig.h"

namespace {
bool readLimit(uint8_t pin) {
#if ENABLE_LIMIT_SWITCHES
  return digitalRead(pin) == HotoriConfig::TAIL_LIMIT_ACTIVE_LEVEL;
#else
  (void)pin;
  return false;
#endif
}
}  // namespace

TailController::TailController(UartStepMotor& motor, uint8_t leftLimitPin,
                               uint8_t rightLimitPin)
    : motor_(motor),
      leftLimitPin_(leftLimitPin),
      rightLimitPin_(rightLimitPin),
      state_(Stopped),
      dir_(0),
      movingLeft_(true),
      reportedLimits_(0),
      startedMs_(0),
      durationMs_(0),
      lastSwingMs_(0) {}

void TailController::begin() {
#if ENABLE_LIMIT_SWITCHES
  pinMode(leftLimitPin_, INPUT_PULLUP);
  pinMode(rightLimitPin_, INPUT_PULLUP);
#endif
}

uint8_t TailController::readLimits() const {
  return (readLimit(leftLimitPin_) ? 1 : 0) |
         (readLimit(rightLimitPin_) ? 2 : 0);
}

uint32_t TailController::halfPeriodForSpeed(uint8_t speedPercent) {
  // Linear 1-100 % -> TAIL_MIN..TAIL_MAX steps/s, kept as steps/s * 99.
  const uint32_t rateTimes99 =
      HotoriConfig::TAIL_MIN_STEPS_PER_SECOND * 99UL +
      (HotoriConfig::TAIL_MAX_STEPS_PER_SECOND -
       HotoriConfig::TAIL_MIN_STEPS_PER_SECOND) *
          (speedPercent - 1UL);
  return 500000UL * 99UL / rateTimes99;
}

void TailController::startMotion() {
  const bool forward = movingLeft_ ? HotoriConfig::TAIL_LEFT_USES_FORWARD
                                   : !HotoriConfig::TAIL_LEFT_USES_FORWARD;
  if (forward) {
    motor_.startForward();
  } else {
    motor_.startReverse();
  }
}

void TailController::reportLimits(uint8_t limits) {
  if (limits == reportedLimits_) {
    return;
  }
  reportedLimits_ = limits;
  if (limits == 0) {
    return;
  }
  Serial.print(F("[TAIL LIMIT] "));
  if (limits == 3) {
    Serial.print(F("Both"));
  } else {
    Serial.print(limits == 1 ? F("Left") : F("Right"));
  }
  Serial.println(F(" active; tail stopped."));
}

void TailController::apply(uint8_t dir, uint8_t speedPercent,
                           uint8_t timeTenths) {
  // A new tail command replaces the previous motion and deadline.
  motor_.stop();
  state_ = Stopped;
  if (dir == 0 || speedPercent == 0 || timeTenths == 0) {
    Serial.println(F("[TAIL] Stop."));
    return;
  }
  if (readLimits() != 0) {
    reportLimits(readLimits());
    Serial.println(F("[TAIL LIMIT] Active; tail command rejected."));
    return;
  }

  dir_ = dir;
  movingLeft_ = dir != 2;  // The swing starts to the left.
  durationMs_ = static_cast<uint32_t>(timeTenths) * 100UL;
  startedMs_ = millis();
  lastSwingMs_ = startedMs_;
  motor_.setStepHalfPeriodUs(halfPeriodForSpeed(speedPercent));
  startMotion();
  state_ = Commanded;

  Serial.print(F("[TAIL] "));
  if (dir == 3) {
    Serial.print(F("Swing"));
  } else {
    Serial.print(movingLeft_ ? F("Left") : F("Right"));
  }
  Serial.print(F(" speed="));
  Serial.print(speedPercent);
  Serial.print(F("% time="));
  Serial.print(timeTenths / 10);
  Serial.print('.');
  Serial.print(timeTenths % 10);
  Serial.println('s');
}

bool TailController::manualStart(bool reverse) {
  const uint8_t limits = readLimits();
  if (limits != 0) {
    motor_.stop();
    reportLimits(limits);
    Serial.println(F("[TAIL LIMIT] Active; tail start rejected."));
    return false;
  }
  motor_.setStepHalfPeriodUs(HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
  if (reverse) {
    motor_.startReverse();
  } else {
    motor_.startForward();
  }
  state_ = Manual;
  return true;
}

void TailController::stop() {
  motor_.stop();
  state_ = Stopped;
}

void TailController::update() {
  const uint8_t limits = readLimits();
  if (limits != 0) {
    // Stop before logging; no debounce and no direction check.
    motor_.stop();
    state_ = Stopped;
  }
  reportLimits(limits);

  if (state_ != Commanded) {
    return;
  }
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - startedMs_) >= durationMs_) {
    motor_.stop();
    state_ = Stopped;
    Serial.println(F("[TAIL] Time expired; stopped."));
    return;
  }
  if (dir_ == 3 && static_cast<uint32_t>(now - lastSwingMs_) >=
                       HotoriConfig::TAIL_SWING_HALF_PERIOD_MS) {
    lastSwingMs_ = now;
    movingLeft_ = !movingLeft_;
    startMotion();
  }
}
