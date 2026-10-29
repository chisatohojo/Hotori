#include "TailController.h"

#include "HotoriRuntimeConfig.h"

TailController::TailController(StepDirMotor& motor, Print& log)
    : motor_(motor), log_(log), state_(Stopped), direction_(0),
      speedPercent_(0), lastLimits_(0), movingLeft_(true),
      startedMs_(0), durationMs_(0) {}

void TailController::begin() {
  pinMode(HotoriRuntimeConfig::TAIL_LEFT_LIMIT_PIN, INPUT_PULLUP);
  pinMode(HotoriRuntimeConfig::TAIL_RIGHT_LIMIT_PIN, INPUT_PULLUP);
  motor_.stop();
  state_ = Stopped;
  update();
}

uint32_t TailController::halfPeriodForSpeed(uint8_t speedPercent) {
  if (speedPercent == 0 || speedPercent > 100) return 0;
  // Preserve fractional steps/s instead of rounding to just 76 speed levels.
  const uint32_t rateTimes99 = HotoriRuntimeConfig::TAIL_MIN_STEPS_PER_SECOND * 99UL +
      (HotoriRuntimeConfig::TAIL_MAX_STEPS_PER_SECOND -
       HotoriRuntimeConfig::TAIL_MIN_STEPS_PER_SECOND) * (speedPercent - 1UL);
  return 500000UL * 99UL / rateTimes99;
}

uint8_t TailController::readLimits() const {
  const bool left = digitalRead(HotoriRuntimeConfig::TAIL_LEFT_LIMIT_PIN) ==
      HotoriRuntimeConfig::TAIL_LIMIT_ACTIVE_LEVEL;
  const bool right = digitalRead(HotoriRuntimeConfig::TAIL_RIGHT_LIMIT_PIN) ==
      HotoriRuntimeConfig::TAIL_LIMIT_ACTIVE_LEVEL;
  return (left ? 1 : 0) | (right ? 2 : 0);
}

void TailController::startMotion() {
  const bool forward = movingLeft_ ? HotoriRuntimeConfig::TAIL_LEFT_USES_FORWARD
                                  : !HotoriRuntimeConfig::TAIL_LEFT_USES_FORWARD;
  motor_.start(forward, halfPeriodForSpeed(speedPercent_));
  state_ = Moving;
}

void TailController::apply(uint8_t direction, uint8_t speedPercent,
                           uint8_t timeTenths) {
  // A new complete Tail group replaces its previous motion AND deadline.
  motor_.stop();
  state_ = Stopped;
  direction_ = direction;
  speedPercent_ = speedPercent;
  durationMs_ = static_cast<uint32_t>(timeTenths) * 100UL;
  startedMs_ = millis();
  if (direction == 0 || speedPercent == 0 || timeTenths == 0) {
    log_.println(F("[TAIL] Stop command; motor disabled."));
    return;
  }
  movingLeft_ = direction != 2;  // Alternating starts left.
  log_.print(F("[TAIL] "));
  if (direction == 3) log_.print(F("Alternate"));
  else log_.print(movingLeft_ ? F("Left") : F("Right"));
  log_.print(F(" speed="));
  log_.print(speedPercent);
  log_.print(F("% time="));
  log_.print(timeTenths / 10);
  log_.print('.');
  log_.print(timeTenths % 10);
  log_.println(F("s"));

  // Check BEFORE enabling, including commands received during a limit fault.
  state_ = Moving;
  if (readLimits() == 0) startMotion();
  update();
}

void TailController::reportLimits(uint8_t limits) {
  if (limits == lastLimits_) return;
  lastLimits_ = limits;
  if (limits == 0) {
    log_.println(F("[TAIL LIMIT] Both released."));
    return;
  }
  log_.print(F("[TAIL LIMIT] "));
  if (limits == 3) log_.print(F("Both"));
  else log_.print(limits == 1 ? F("Left") : F("Right"));
  log_.println(F(" active; motor disabled."));
}

void TailController::update() {
  const uint8_t limits = readLimits();
  // Stop before any logging/timer/state work. No debounce and no direction test.
  if (limits != 0) motor_.stop();

  if (limits != 0 && state_ == Moving) {
    if (direction_ == 3) {
      state_ = WaitingForRelease;
      movingLeft_ = !movingLeft_;  // Reverse exactly once per limit encounter.
      log_.println(F("[TAIL] Waiting for both limits LOW; motor disabled."));
    } else {
      state_ = Stopped;  // Single-direction commands never resume themselves.
      log_.println(F("[TAIL] Limit ended motion; a new command is required."));
    }
  }
  reportLimits(limits);

  if (state_ == Stopped) return;
  if (static_cast<uint32_t>(millis() - startedMs_) >= durationMs_) {
    motor_.stop();
    state_ = Stopped;
    log_.println(F("[TAIL] Time expired; stopped."));
    return;
  }
  if (limits != 0) return;
  if (state_ == WaitingForRelease) {
    startMotion();
    log_.println(F("[TAIL] Limits released; moving in the opposite direction."));
  }
  motor_.update();
}
