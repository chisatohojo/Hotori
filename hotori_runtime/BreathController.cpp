#include "BreathController.h"

#include "HotoriRuntimeConfig.h"

BreathController::BreathController(StepDirMotor& motor, Print& log,
                                   uint32_t stepsPerCycle)
    : motor_(motor), log_(log), stepsPerCycle_(stepsPerCycle),
      breathsPerMinute_(0), calibrationReported_(false) {}

void BreathController::apply(uint8_t breathsPerMinute) {
  motor_.stop();
  breathsPerMinute_ = breathsPerMinute;
  log_.print(F("[BREATH] Set "));
  log_.print(breathsPerMinute);
  log_.println(F(" bpm"));
  if (breathsPerMinute == 0) return;
  if (stepsPerCycle_ == 0) {
    if (!calibrationReported_) {
      log_.println(F("[BREATH] Calibration required; Motor2 remains disabled."));
      calibrationReported_ = true;
    }
    return;
  }
  const uint64_t stepsPerMinute = static_cast<uint64_t>(stepsPerCycle_) * breathsPerMinute;
  const uint32_t halfPeriodUs = static_cast<uint32_t>(30000000ULL / stepsPerMinute);
  if (halfPeriodUs == 0) {
    log_.println(F("[BREATH ERROR] Calibration exceeds timer resolution; motor disabled."));
    return;
  }
  // Forward rotation: the calibrated mechanical linkage defines one breath.
  motor_.start(HotoriRuntimeConfig::BREATH_USES_FORWARD, halfPeriodUs);
}

void BreathController::update() {
  motor_.update();
}
