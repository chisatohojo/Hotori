#include "HeadAxis.h"

#include "HotoriConfig.h"

namespace {
int32_t distance(int32_t a, int32_t b) {
  return a > b ? a - b : b - a;
}

// Speed used after homing when no angle command is pending.
constexpr uint8_t RETURN_SPEED_PERCENT = 50;
}  // namespace

HeadAxis::HeadAxis(UartStepMotor& motor, const Config& config)
    : motor_(motor),
      config_(config),
      state_(Unhomed),
      probing_(false),
      blocked_(false),
      blockedPositive_(false),
      escapeStart_(0),
      releaseCount_(0),
      homingStart_(0),
      hasPending_(false),
      pendingAngle_(0),
      pendingSpeed_(0) {}

void HeadAxis::begin() {
#if ENABLE_LIMIT_SWITCHES
  pinMode(config_.limitPin, INPUT_PULLUP);
#endif
  motor_.setRamp(halfPeriodForSteps(HotoriConfig::HEAD_START_STEPS_PER_SECOND),
                 HotoriConfig::HEAD_RAMP_STEPS);
}

bool HeadAxis::limitPressed() const {
#if ENABLE_LIMIT_SWITCHES
  return digitalRead(config_.limitPin) == HotoriConfig::LIMIT_ACTIVE_LEVEL;
#else
  return false;
#endif
}

int32_t HeadAxis::angleToSteps(float angle) const {
  const float steps = angle * config_.stepsPerDegree;
  return static_cast<int32_t>(steps >= 0 ? steps + 0.5f : steps - 0.5f);
}

int32_t HeadAxis::degreesToSteps(float degrees) const {
  return angleToSteps(degrees);
}

uint32_t HeadAxis::halfPeriodForSteps(uint32_t stepsPerSecond) {
  return 500000UL / stepsPerSecond;
}

uint32_t HeadAxis::halfPeriodForSpeed(uint8_t speedPercent) const {
  // Linear 1-100 % -> HEAD_MIN..HEAD_MAX steps/s, kept as steps/s * 99 so
  // every percent gets its own period.
  const uint32_t rateTimes99 =
      HotoriConfig::HEAD_MIN_STEPS_PER_SECOND * 99UL +
      (HotoriConfig::HEAD_MAX_STEPS_PER_SECOND -
       HotoriConfig::HEAD_MIN_STEPS_PER_SECOND) *
          (speedPercent - 1UL);
  return 500000UL * 99UL / rateTimes99;
}

void HeadAxis::logPrefix() {
  Serial.print('[');
  Serial.print(config_.name);
  Serial.print(F("] "));
}

void HeadAxis::fail(const __FlashStringHelper* reason) {
  motor_.hold();
  state_ = Fault;
  probing_ = false;
  hasPending_ = false;
  logPrefix();
  Serial.print(F("Homing failed: "));
  Serial.print(reason);
  Serial.println(F(". Send 'home' to retry."));
}

// A move may start unless it heads into a pressed switch. Escaping from
// the switch that stopped the axis is allowed.
bool HeadAxis::canStart(bool positive) {
  if (!limitPressed()) {
    return true;
  }
  if (blocked_ && positive != blockedPositive_) {
    return true;
  }
  logPrefix();
  Serial.println(F("Limit pressed; move toward it rejected."));
  return false;
}

void HeadAxis::startHoming() {
  motor_.hold();
  hasPending_ = hasPending_ && state_ != Fault;
  bool noSwitch = false;
#if !ENABLE_LIMIT_SWITCHES
  noSwitch = true;
#endif
  if (noSwitch) {
    motor_.setPosition(0);
    state_ = Ready;
    logPrefix();
    Serial.println(F("No limit switches; current position is 0 deg."));
    goTo(hasPending_ ? pendingAngle_ : 0,
         hasPending_ ? pendingSpeed_ : RETURN_SPEED_PERCENT);
    hasPending_ = false;
    return;
  }

  motor_.setStepHalfPeriodUs(HotoriConfig::HOMING_STEP_HALF_PERIOD_US);
  logPrefix();
  if (limitPressed()) {
    // Which end is pressed is unknown. Treat it as the min end and move
    // toward max; it must release within LIMIT_ESCAPE_MAX_DEGREES.
    probing_ = true;
    blocked_ = true;
    blockedPositive_ = false;
    escapeStart_ = motor_.position();
    releaseCount_ = 0;
    Serial.println(F("Homing: on a switch; moving toward max to release it."));
  } else {
    probing_ = false;
    homingStart_ = motor_.position();
    Serial.println(F("Homing: moving toward the max switch."));
  }
  state_ = Homing;
  motor_.startForward();
}

void HeadAxis::hitSwitch(bool positive) {
  motor_.hold();  // Stop first and keep holding.
  const float switchAngle =
      positive ? config_.maxSwitchAngle : config_.minSwitchAngle;
  motor_.setPosition(angleToSteps(switchAngle));
  blocked_ = true;
  blockedPositive_ = positive;
  escapeStart_ = motor_.position();
  releaseCount_ = 0;

  logPrefix();
  if (state_ == Homing) {
    state_ = Ready;
    Serial.print(F("Homed at the max switch ("));
    Serial.print(switchAngle, 1);
    Serial.println(F(" deg)."));
    goTo(hasPending_ ? pendingAngle_ : 0,
         hasPending_ ? pendingSpeed_ : RETURN_SPEED_PERCENT);
    hasPending_ = false;
    return;
  }
  state_ = Ready;  // The end is known, so the angle is valid again.
  Serial.print(positive ? F("Max") : F("Min"));
  Serial.print(F(" switch hit; stopped. Angle set to "));
  Serial.print(switchAngle, 1);
  Serial.println(F(" deg."));
}

void HeadAxis::goTo(float angle, uint8_t speedPercent) {
  const float low = max(static_cast<float>(config_.minCommandAngle),
                        config_.minSwitchAngle +
                            HotoriConfig::HEAD_SWITCH_MARGIN_DEGREES);
  const float high = min(static_cast<float>(config_.maxCommandAngle),
                         config_.maxSwitchAngle -
                             HotoriConfig::HEAD_SWITCH_MARGIN_DEGREES);
  angle = constrain(angle, low, high);
  const int32_t target = angleToSteps(angle);
  if (target != motor_.position() && !canStart(target > motor_.position())) {
    return;
  }
  motor_.setStepHalfPeriodUs(halfPeriodForSpeed(speedPercent));
  motor_.moveTo(target);
  logPrefix();
  Serial.print(F("Move to "));
  Serial.print(angle, 1);
  Serial.print(F(" deg, speed "));
  Serial.print(speedPercent);
  Serial.println('%');
}

void HeadAxis::moveToAngle(int8_t angle, uint8_t speedPercent) {
  if (speedPercent == 0) {
    hasPending_ = false;
    if (state_ == Homing) {
      motor_.hold();
      probing_ = false;
      state_ = Unhomed;
    } else if (motor_.isRunning()) {
      motor_.hold();
    }
    logPrefix();
    Serial.println(F("Speed 0; stopped."));
    return;
  }

  switch (state_) {
    case Ready:
      goTo(angle, speedPercent);
      break;
    case Homing:
      pendingAngle_ = angle;
      pendingSpeed_ = speedPercent;
      hasPending_ = true;
      break;
    case Unhomed:
      pendingAngle_ = angle;
      pendingSpeed_ = speedPercent;
      hasPending_ = true;
      startHoming();
      break;
    case Fault:
      logPrefix();
      Serial.println(F("Homing fault; command ignored. Send 'home' to retry."));
      break;
  }
}

bool HeadAxis::manualStart(bool reverse) {
  if (!canStart(!reverse)) {
    motor_.hold();
    return false;
  }
  hasPending_ = false;
  if (state_ == Homing) {
    probing_ = false;
    state_ = Unhomed;
  }
  motor_.setStepHalfPeriodUs(HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
  if (reverse) {
    motor_.startReverse();
  } else {
    motor_.startForward();
  }
  return true;
}

void HeadAxis::stop() {
  motor_.hold();
  hasPending_ = false;
  if (state_ == Homing) {
    probing_ = false;
    state_ = Unhomed;
  }
}

void HeadAxis::update() {
  const bool pressed = limitPressed();
  if (pressed) {
    releaseCount_ = 0;
  } else if (blocked_ && ++releaseCount_ >= HotoriConfig::LIMIT_RELEASE_CONFIRM) {
    blocked_ = false;  // Released for enough reads in a row.
  }

  if (!motor_.isRunning()) {
    return;
  }
  const bool positive = !motor_.isReverse();
  const int32_t escapeMax = degreesToSteps(HotoriConfig::LIMIT_ESCAPE_MAX_DEGREES);

  if (state_ == Homing && probing_) {
    if (!blocked_) {
      probing_ = false;
      homingStart_ = motor_.position();
      logPrefix();
      Serial.println(F("Switch released; moving toward the max switch."));
    } else if (distance(motor_.position(), escapeStart_) > escapeMax) {
      fail(F("the switch pressed at start did not release"));
    }
    return;
  }

  if (pressed) {
    if (blocked_ && positive != blockedPositive_) {
      if (distance(motor_.position(), escapeStart_) <= escapeMax) {
        return;  // Escaping from the switch that stopped the axis.
      }
      logPrefix();
      Serial.println(F("Still pressed after escaping; taken as the opposite switch."));
    }
    hitSwitch(positive);
    return;
  }

  if (state_ == Homing &&
      distance(motor_.position(), homingStart_) >
          degreesToSteps(HotoriConfig::HOMING_MAX_DEGREES)) {
    fail(F("max switch not found"));
  }
}

void HeadAxis::printStatus() {
  logPrefix();
  switch (state_) {
    case Unhomed: Serial.print(F("Unhomed")); break;
    case Homing: Serial.print(F("Homing")); break;
    case Ready: Serial.print(F("Ready")); break;
    case Fault: Serial.print(F("Fault")); break;
  }
  Serial.print(F(" pos="));
  Serial.print(motor_.position());
  if (state_ == Ready) {
    Serial.print(F(" angle="));
    Serial.print(motor_.position() / config_.stepsPerDegree, 1);
  }
  Serial.print(F(" switch="));
  Serial.print(limitPressed() ? F("pressed") : F("released"));
  if (blocked_) {
    Serial.print(blockedPositive_ ? F(" (stopped at max)") : F(" (stopped at min)"));
  }
  Serial.println();
}
