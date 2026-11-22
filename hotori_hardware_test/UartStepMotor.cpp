#include "UartStepMotor.h"

namespace {
constexpr uint8_t TMC2209_VERSION = 0x21;

uint32_t distance(int32_t a, int32_t b) {
  return static_cast<uint32_t>(a > b ? a - b : b - a);
}
}  // namespace

UartStepMotor::UartStepMotor(uint8_t stepPin,
                             uint8_t directionPin,
                             uint8_t forwardLevel,
                             TMC2209Stepper& driver,
                             uint32_t stepHalfPeriodUs)
    : stepPin_(stepPin),
      directionPin_(directionPin),
      forwardLevel_(forwardLevel),
      driver_(driver),
      stepHalfPeriodUs_(stepHalfPeriodUs),
      rampStartHalfPeriodUs_(stepHalfPeriodUs),
      rampSteps_(0),
      toffRun_(0),
      configured_(false),
      outputOn_(false),
      running_(false),
      stepHigh_(false),
      reverse_(false),
      hasTarget_(false),
      position_(0),
      target_(0),
      moveStart_(0),
      lastStepChangeUs_(0) {}

void UartStepMotor::begin() {
  digitalWrite(stepPin_, LOW);
  digitalWrite(directionPin_, forwardLevel_);
  pinMode(stepPin_, OUTPUT);
  pinMode(directionPin_, OUTPUT);
}

bool UartStepMotor::configure(uint16_t runCurrentMa, float holdRatio,
                              uint16_t microsteps, uint8_t toffRun,
                              uint8_t attempts) {
  toffRun_ = toffRun;
  configured_ = false;
  outputOn_ = false;

  for (uint8_t attempt = 0; attempt < attempts; ++attempt) {
    if (attempt != 0) {
      delay(100);  // Setup / 'tmc' only; never called from the motion loop.
    }
    if (readVersion() != TMC2209_VERSION) {
      continue;
    }
    driver_.pdn_disable(true);      // PDN_UART is the UART line.
    driver_.mstep_reg_select(true); // Microsteps from the register.
    driver_.toff(0);                // Output stage off until a start.
    driver_.I_scale_analog(false);  // Current from registers, not VREF.
    driver_.rms_current(runCurrentMa, holdRatio);
    driver_.microsteps(microsteps);
    driver_.intpol(true);           // 256-step interpolation: smoother.
    driver_.en_spreadCycle(false);  // StealthChop: quiet.
    driver_.pwm_autoscale(true);
    driver_.shaft(false);           // Direction comes from the DIR pin.
    driver_.GSTAT(0b111);           // Clear reset / drv_err / uv_cp.
    if (driver_.microsteps() == microsteps && !driver_.CRCerror) {
      configured_ = true;
      return true;
    }
  }
  return false;
}

bool UartStepMotor::startForward() {
  hasTarget_ = false;
  return start(false);
}

bool UartStepMotor::startReverse() {
  hasTarget_ = false;
  return start(true);
}

bool UartStepMotor::moveTo(int32_t target) {
  if (target == position_) {
    hold();
    return configured_;
  }
  target_ = target;
  moveStart_ = position_;
  hasTarget_ = true;
  return start(target < position_);
}

bool UartStepMotor::start(bool reverse) {
  running_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
  reverse_ = reverse;
  digitalWrite(directionPin_,
               reverse ? (forwardLevel_ == HIGH ? LOW : HIGH) : forwardLevel_);

  // Without UART the driver keeps its power-on defaults (output on, VREF
  // current, MS1/MS2 microsteps), so STEP / DIR alone still move it.
  // The UART write is skipped when the output is already on.
  if (configured_ && !outputOn_) {
    driver_.toff(toffRun_);
    outputOn_ = true;
  }
  lastStepChangeUs_ = micros();
  running_ = true;
  return configured_;
}

void UartStepMotor::stop() {
  hold();
  // Free the shaft once; avoid repeating UART writes while held stopped.
  if (outputOn_) {
    driver_.toff(0);
    outputOn_ = false;
  }
}

void UartStepMotor::hold() {
  running_ = false;
  hasTarget_ = false;
  stepHigh_ = false;
  digitalWrite(stepPin_, LOW);
}

uint32_t UartStepMotor::currentHalfPeriodUs() const {
  if (!hasTarget_ || rampSteps_ == 0 ||
      rampStartHalfPeriodUs_ <= stepHalfPeriodUs_) {
    return stepHalfPeriodUs_;
  }
  // Accelerate over the first steps, decelerate over the last ones.
  uint32_t ramp = distance(target_, moveStart_) / 2;
  if (ramp > rampSteps_) {
    ramp = rampSteps_;
  }
  const uint32_t done = distance(position_, moveStart_);
  const uint32_t remaining = distance(target_, position_);
  const uint32_t n = done < remaining ? done : remaining;
  if (ramp == 0 || n >= ramp) {
    return stepHalfPeriodUs_;
  }
  return rampStartHalfPeriodUs_ -
         (rampStartHalfPeriodUs_ - stepHalfPeriodUs_) * n / ramp;
}

void UartStepMotor::update() {
  if (!running_) {
    return;
  }

  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - lastStepChangeUs_) < currentHalfPeriodUs()) {
    return;
  }

  if (!stepHigh_) {
    // A target move ends before the next rising edge and keeps the output on
    // so the axis holds its position.
    if (hasTarget_ && position_ == target_) {
      hold();
      return;
    }
    position_ += reverse_ ? -1 : 1;
  }
  stepHigh_ = !stepHigh_;
  digitalWrite(stepPin_, stepHigh_ ? HIGH : LOW);
  lastStepChangeUs_ = now;
}

bool UartStepMotor::isRunning() const {
  return running_;
}

bool UartStepMotor::isConfigured() const {
  return configured_;
}

bool UartStepMotor::isReverse() const {
  return reverse_;
}

int32_t UartStepMotor::position() const {
  return position_;
}

void UartStepMotor::setPosition(int32_t position) {
  position_ = position;
}

void UartStepMotor::setStepHalfPeriodUs(uint32_t stepHalfPeriodUs) {
  stepHalfPeriodUs_ = stepHalfPeriodUs;
}

void UartStepMotor::setRamp(uint32_t startHalfPeriodUs, uint16_t rampSteps) {
  rampStartHalfPeriodUs_ = startHalfPeriodUs;
  rampSteps_ = rampSteps;
}

uint8_t UartStepMotor::readVersion() {
  return driver_.version();
}

void UartStepMotor::checkUart(UartCheck& result) {
  result.ioin = driver_.IOIN();
  const bool ioinValid = !driver_.CRCerror;
  result.version = ioinValid ? static_cast<uint8_t>(result.ioin >> 24) : 0;
  result.readOk = ioinValid && result.version == TMC2209_VERSION;
  result.writeTested = false;
  result.writeOk = false;
  result.ifcntBefore = 0;
  result.ifcntAfter = 0;
  result.gstat = 0;
  result.chopconf = 0;
  result.drvStatus = 0;
  if (!result.readOk) {
    return;
  }

  result.gstat = driver_.GSTAT();
  result.chopconf = driver_.CHOPCONF();
  result.drvStatus = driver_.DRV_STATUS();
  result.ifcntBefore = driver_.IFCNT();
  if (driver_.CRCerror) {
    return;
  }
  const uint32_t gconf = driver_.GCONF();
  if (driver_.CRCerror) {
    return;
  }

  // Same value written back: IFCNT counts every accepted write datagram.
  driver_.GCONF(gconf);
  result.ifcntAfter = driver_.IFCNT();
  result.writeTested = !driver_.CRCerror;
  result.writeOk = result.writeTested &&
                   static_cast<uint8_t>(result.ifcntBefore + 1) ==
                       result.ifcntAfter;
}
