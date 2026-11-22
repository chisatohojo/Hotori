#pragma once

#include <Arduino.h>
#include <TMCStepper.h>

// TMC2209 driven by STEP / DIR pins. Current, microsteps, StealthChop
// (quiet mode) and per-driver output enable (TOFF) are set over UART. The
// hardware EN pin is shared and handled by the sketch.
class UartStepMotor {
 public:
  // Result of one UART diagnostic (read, write and status registers).
  struct UartCheck {
    bool readOk;          // IOIN reply had a valid CRC and version 0x21.
    uint8_t version;      // IOIN[31:24]; 0 when there was no valid reply.
    uint32_t ioin;        // Raw IOIN (pin levels seen by the driver).
    bool writeTested;     // false when the read failed and write was skipped.
    bool writeOk;         // IFCNT advanced by one after a write.
    uint8_t ifcntBefore;
    uint8_t ifcntAfter;
    uint8_t gstat;        // bit0 reset, bit1 drv_err, bit2 uv_cp.
    uint32_t chopconf;    // toff = [3:0], mres = [27:24].
    uint32_t drvStatus;   // Load / short / temperature flags, cs_actual.
  };

  UartStepMotor(uint8_t stepPin,
                uint8_t directionPin,
                uint8_t forwardLevel,
                TMC2209Stepper& driver,
                uint32_t stepHalfPeriodUs);

  void begin();
  // Writes the driver registers over UART (StealthChop, interpolation,
  // current, microsteps) and reads them back, up to `attempts` times.
  // Returns true when the driver answered and kept the settings.
  bool configure(uint16_t runCurrentMa, float holdRatio, uint16_t microsteps,
                 uint8_t toffRun, uint8_t attempts);
  // Always starts STEP / DIR. Returns false when UART is not confirmed: the
  // motor then runs on driver defaults and stop() cannot free the shaft.
  bool startForward();
  bool startReverse();
  // Runs until position() reaches target, then stops by itself and keeps
  // holding. Forward steps increase position(). With a ramp set, the speed
  // rises from the ramp start speed to the cruise speed and back.
  bool moveTo(int32_t target);
  // Stops STEP and frees the shaft (TOFF=0).
  void stop();
  // Stops STEP but keeps the output on so the axis holds its position.
  void hold();
  void update();
  bool isRunning() const;
  bool isConfigured() const;
  // True while moving (or last moved) toward lower positions.
  bool isReverse() const;
  // Position in STEP pulses counted from the last setPosition().
  int32_t position() const;
  void setPosition(int32_t position);
  // Cruise speed. Takes effect from the next STEP edge, also while running.
  void setStepHalfPeriodUs(uint32_t stepHalfPeriodUs);
  // Trapezoidal ramp for moveTo(). rampSteps 0 disables it.
  void setRamp(uint32_t startHalfPeriodUs, uint16_t rampSteps);
  // Returns the raw TMC2209 version (0x21) or 0 / 0xFF on no reply.
  uint8_t readVersion();
  // Reads IOIN/GSTAT and verifies a write by rewriting GCONF with its current
  // value and checking IFCNT. Does not change the driver configuration.
  void checkUart(UartCheck& result);

 private:
  bool start(bool reverse);
  uint32_t currentHalfPeriodUs() const;

  const uint8_t stepPin_;
  const uint8_t directionPin_;
  const uint8_t forwardLevel_;
  TMC2209Stepper& driver_;
  uint32_t stepHalfPeriodUs_;
  uint32_t rampStartHalfPeriodUs_;
  uint16_t rampSteps_;
  uint8_t toffRun_;

  bool configured_;
  bool outputOn_;
  bool running_;
  bool stepHigh_;
  bool reverse_;
  bool hasTarget_;
  int32_t position_;
  int32_t target_;
  int32_t moveStart_;
  uint32_t lastStepChangeUs_;
};
