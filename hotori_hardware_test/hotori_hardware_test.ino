#include <Arduino.h>
#include <string.h>

#include "EspFrameReceiver.h"
#include "HeadAxis.h"
#include "HotoriConfig.h"
#include "SerialCommandReader.h"
#include "ServoController.h"
#include "StepDirMotor.h"
#include "TailController.h"
#include "UartStepMotor.h"

// ----- TMC2209 UART drivers (Motor1-3) -----
// All drivers share one wire on TMC_UART_PIN. Passing the same pin as RX and
// TX makes TMCStepper switch it between sending and receiving. Address 3 is
// unused and only probed by the 'uart' scan.
constexpr uint8_t TMC_ADDRESS_COUNT = 4;
TMC2209Stepper tmcDriver0(HotoriConfig::TMC_UART_PIN, HotoriConfig::TMC_UART_PIN,
                          HotoriConfig::TMC_R_SENSE_OHMS, 0);
TMC2209Stepper tmcDriver1(HotoriConfig::TMC_UART_PIN, HotoriConfig::TMC_UART_PIN,
                          HotoriConfig::TMC_R_SENSE_OHMS, 1);
TMC2209Stepper tmcDriver2(HotoriConfig::TMC_UART_PIN, HotoriConfig::TMC_UART_PIN,
                          HotoriConfig::TMC_R_SENSE_OHMS, 2);
TMC2209Stepper tmcDriver3(HotoriConfig::TMC_UART_PIN, HotoriConfig::TMC_UART_PIN,
                          HotoriConfig::TMC_R_SENSE_OHMS, 3);
TMC2209Stepper* const tmcDrivers[TMC_ADDRESS_COUNT] = {
    &tmcDriver0, &tmcDriver1, &tmcDriver2, &tmcDriver3};

// ----- Motors -----
UartStepMotor motor1(HotoriConfig::MOTOR1_STEP_PIN, HotoriConfig::MOTOR1_DIR_PIN,
                     HotoriConfig::HEAD_LR_FORWARD_LEVEL,
                     *tmcDrivers[HotoriConfig::HEAD_LR_TMC_ADDRESS],
                     HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
UartStepMotor motor2(HotoriConfig::MOTOR2_STEP_PIN, HotoriConfig::MOTOR2_DIR_PIN,
                     HotoriConfig::HEAD_UD_FORWARD_LEVEL,
                     *tmcDrivers[HotoriConfig::HEAD_UD_TMC_ADDRESS],
                     HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
UartStepMotor motor3(HotoriConfig::MOTOR3_STEP_PIN, HotoriConfig::MOTOR3_DIR_PIN,
                     HotoriConfig::MOTOR_FORWARD_LEVEL,
                     *tmcDrivers[HotoriConfig::TAIL_TMC_ADDRESS],
                     HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
StepDirMotor motor4(HotoriConfig::MOTOR4_STEP_PIN, HotoriConfig::MOTOR4_DIR_PIN,
                    HotoriConfig::MOTOR_FORWARD_LEVEL,
                    HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
StepDirMotor motor5(HotoriConfig::MOTOR5_STEP_PIN, HotoriConfig::MOTOR5_DIR_PIN,
                    HotoriConfig::MOTOR_FORWARD_LEVEL,
                    HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);

// neckLR -> Motor1, neckUD -> Motor2. Log names live in flash because F()
// cannot be used at global scope.
const char HEAD_LR_NAME[] PROGMEM = "HEAD LR";
const char HEAD_UD_NAME[] PROGMEM = "HEAD UD";
HeadAxis headLR(motor1,
                {reinterpret_cast<const __FlashStringHelper*>(HEAD_LR_NAME),
                 HotoriConfig::HEAD_LR_LIMIT_PIN,
                 HotoriConfig::HEAD_LR_STEPS_PER_DEGREE,
                 HotoriConfig::HEAD_LR_MIN_SWITCH_ANGLE,
                 HotoriConfig::HEAD_LR_MAX_SWITCH_ANGLE, -90, 90});
HeadAxis headUD(motor2,
                {reinterpret_cast<const __FlashStringHelper*>(HEAD_UD_NAME),
                 HotoriConfig::HEAD_UD_LIMIT_PIN,
                 HotoriConfig::HEAD_UD_STEPS_PER_DEGREE,
                 HotoriConfig::HEAD_UD_MIN_SWITCH_ANGLE,
                 HotoriConfig::HEAD_UD_MAX_SWITCH_ANGLE, -30, 30});

TailController tail(motor3, HotoriConfig::TAIL_LEFT_LIMIT_PIN,
                    HotoriConfig::TAIL_RIGHT_LIMIT_PIN);

ServoController servo(HotoriConfig::SERVO_SIGNAL_PIN,
                      HotoriConfig::SERVO_POWER_PIN,
                      HotoriConfig::SERVO_POWER_ON_LEVEL,
                      HotoriConfig::SERVO_MIN_PULSE_US,
                      HotoriConfig::SERVO_MAX_PULSE_US,
                      HotoriConfig::SERVO_INITIAL_ANGLE);

// Text commands and ESP32 frames share Serial (D0): a line starting with AA
// is a frame, everything else is a text command.
SerialCommandReader commandReader;
EspFrameReceiver espReceiver;
EspCommand espCommand = {};
uint32_t lastEspByteMs = 0;

// ----- Shared EN -----
// One EN pin serves all five drivers, so it stays on while any motor runs or
// a homed head holds its angle. A stopped Motor4/5 then stays energized;
// Motor1-3 can also switch their own output off over UART (TOFF).
bool motorEnableNeeded() {
  return motor1.isRunning() || motor2.isRunning() || motor3.isRunning() ||
         motor4.isRunning() || motor5.isRunning() || headLR.needsHold() ||
         headUD.needsHold();
}

void updateMotorEnable() {
  digitalWrite(HotoriConfig::MOTOR_ENABLE_PIN,
               motorEnableNeeded() ? HotoriConfig::MOTOR_ENABLE_LEVEL
                                   : HotoriConfig::MOTOR_DISABLE_LEVEL);
}

void printSharedEnableState() {
  Serial.println(motorEnableNeeded()
                     ? F("        Shared EN stays enabled; another motor runs or the head holds.")
                     : F("        All motors stopped; shared EN disabled."));
}

// ----- TMC2209 configuration -----
void configureUartMotor(UartStepMotor& motor, const __FlashStringHelper* name,
                        uint16_t currentMa) {
  const bool ok = motor.configure(currentMa, HotoriConfig::TMC_HOLD_CURRENT_RATIO,
                                  HotoriConfig::UART_MOTOR_MICROSTEPS,
                                  HotoriConfig::TMC_TOFF_RUN,
                                  HotoriConfig::TMC_CONFIG_ATTEMPTS);
  Serial.print(ok ? F("[OK] ") : F("[ERROR] "));
  Serial.print(name);
  Serial.println(ok ? F(" TMC2209 UART ready (StealthChop).")
                    : F(" TMC2209 did not answer over UART."));
}

void configureUartMotors() {
  configureUartMotor(motor1, F("Motor1 / Head LR"),
                     HotoriConfig::HEAD_LR_CURRENT_MA);
  configureUartMotor(motor2, F("Motor2 / Head UD"),
                     HotoriConfig::HEAD_UD_CURRENT_MA);
  configureUartMotor(motor3, F("Motor3 / Tail"),
                     HotoriConfig::TAIL_CURRENT_MA);
  if (!motor1.isConfigured() || !motor2.isConfigured() ||
      !motor3.isConfigured()) {
    Serial.println(F("[ERROR] Check VM power, the PDN_UART wire on A0 and MS1/MS2, then send 'tmc'."));
    Serial.println(F("        Motors without UART still run by STEP/DIR on driver defaults."));
  }
}

// ----- Manual commands -----
// m1 / m2: the head keeps counting steps, so its angle stays valid.
void startHeadManual(HeadAxis& axis, UartStepMotor& motor,
                     const __FlashStringHelper* label, bool reverse) {
  if (!axis.manualStart(reverse)) {
    return;
  }
  updateMotorEnable();
  Serial.print(label);
  Serial.println(reverse ? F("Reverse.") : F("Forward."));
  if (!motor.isConfigured()) {
    Serial.println(F("        UART not ready; running on driver defaults (VREF current, MS1/MS2 microsteps)."));
  }
}

// m4 / m5 at the default test speed.
void startRibManual(StepDirMotor& motor, const __FlashStringHelper* label,
                    bool reverse) {
  motor.setStepHalfPeriodUs(HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);
  if (reverse) {
    motor.startReverse();
  } else {
    motor.startForward();
  }
  Serial.print(label);
  Serial.println(reverse ? F("Reverse.") : F("Forward."));
}

// ----- UART diagnostics -----
bool uartWatchOn = false;
uint8_t uartWatchNextAddress = 0;
uint32_t uartWatchLastMs = 0;
bool uartWatchLastOk[TMC_ADDRESS_COUNT] = {};
uint16_t uartWatchOkCount[TMC_ADDRESS_COUNT] = {};
uint16_t uartWatchNgCount[TMC_ADDRESS_COUNT] = {};

void printHex2(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void printBit(const __FlashStringHelper* label, uint32_t value, uint8_t bit) {
  Serial.print(label);
  Serial.print((value >> bit) & 1UL ? '1' : '0');
}

bool printUartCheck(UartStepMotor& motor, const __FlashStringHelper* name,
                    uint8_t address) {
  UartStepMotor::UartCheck result;
  motor.checkUart(result);

  Serial.print(F("[UART] "));
  Serial.print(name);
  Serial.print(F(" (addr "));
  Serial.print(address);
  Serial.println(')');

  Serial.print(F("  Read : "));
  if (!result.readOk) {
    if (result.version == 0) {
      Serial.println(F("NG - no valid reply"));
    } else {
      Serial.print(F("NG - unexpected version 0x"));
      printHex2(result.version);
      Serial.println(F(" (expected 0x21)"));
    }
    return false;
  }
  Serial.println(F("OK  version=0x21"));

  Serial.print(F("  Write: "));
  if (!result.writeTested) {
    Serial.println(F("NG - reply lost during the write test"));
  } else {
    Serial.print(result.writeOk ? F("OK  IFCNT ") : F("NG  IFCNT "));
    Serial.print(result.ifcntBefore);
    Serial.print(F(" -> "));
    Serial.println(result.ifcntAfter);
  }

  // IOIN: bit0 ENN, bit2 MS1, bit3 MS2, bit9 DIR. ENN=1 means EN is HIGH.
  printBit(F("  Pins : ENN="), result.ioin, 0);
  printBit(F(" MS1="), result.ioin, 2);
  printBit(F(" MS2="), result.ioin, 3);
  printBit(F(" DIR="), result.ioin, 9);
  Serial.println();
  printBit(F("  GSTAT: reset="), result.gstat, 0);
  printBit(F(" drv_err="), result.gstat, 1);
  printBit(F(" uv_cp="), result.gstat, 2);
  Serial.println();

  // CHOPCONF: toff=0 means the output stage is off (motor free).
  Serial.print(F("  Chop : toff="));
  Serial.print(static_cast<uint8_t>(result.chopconf & 0x0F));
  Serial.print(F(" mres="));
  Serial.print(static_cast<uint8_t>((result.chopconf >> 24) & 0x0F));
  Serial.println(F(" (toff 0 = output off, mres 5 = 1/8 step)"));

  // DRV_STATUS: bit0 otpw, bit1 ot, bit2 s2ga, bit3 s2gb, bit6 ola,
  // bit7 olb, bits20:16 cs_actual, bit30 stealth, bit31 stst.
  const uint32_t drv = result.drvStatus;
  Serial.print(F("  Drv  : cs_actual="));
  Serial.print(static_cast<uint8_t>((drv >> 16) & 0x1F));
  printBit(F(" stealth="), drv, 30);
  printBit(F(" stst="), drv, 31);
  printBit(F(" ola="), drv, 6);
  printBit(F(" olb="), drv, 7);
  printBit(F(" s2ga="), drv, 2);
  printBit(F(" s2gb="), drv, 3);
  printBit(F(" ot="), drv, 1);
  printBit(F(" otpw="), drv, 0);
  Serial.println();
  return result.writeOk;
}

// Reads IOIN from one address (no register writes). Returns the version
// byte, or 0 when there was no valid reply.
uint8_t probeTmcAddress(uint8_t address) {
  TMC2209Stepper& driver = *tmcDrivers[address];
  const uint32_t ioin = driver.IOIN();
  return driver.CRCerror ? 0 : static_cast<uint8_t>(ioin >> 24);
}

// Prints the address as MS2 MS1 bits, e.g. 1 -> "01".
void printAddressBits(uint8_t address) {
  Serial.print((address >> 1) & 1);
  Serial.print(address & 1);
}

const __FlashStringHelper* tmcAddressOwner(uint8_t address) {
  if (address == HotoriConfig::HEAD_LR_TMC_ADDRESS) {
    return F("M1 Head LR");
  }
  if (address == HotoriConfig::HEAD_UD_TMC_ADDRESS) {
    return F("M2 Head UD");
  }
  if (address == HotoriConfig::TAIL_TMC_ADDRESS) {
    return F("M3 Tail   ");
  }
  return F("----------");
}

bool isUsedTmcAddress(uint8_t address) {
  return address == HotoriConfig::HEAD_LR_TMC_ADDRESS ||
         address == HotoriConfig::HEAD_UD_TMC_ADDRESS ||
         address == HotoriConfig::TAIL_TMC_ADDRESS;
}

void runTmcAddressScan() {
  Serial.println(F("[UART] Address scan (MS2 MS1)"));
  for (uint8_t address = 0; address < TMC_ADDRESS_COUNT; ++address) {
    const uint8_t version = probeTmcAddress(address);
    Serial.print(F("  "));
    printAddressBits(address);
    Serial.print(F(" addr "));
    Serial.print(address);
    Serial.print(' ');
    Serial.print(tmcAddressOwner(address));
    Serial.print(F(" : "));
    if (version == 0) {
      Serial.println(isUsedTmcAddress(address)
                         ? F("no reply  <- driver expected here")
                         : F("no reply"));
      continue;
    }
    Serial.print(F("reply version=0x"));
    printHex2(version);
    if (!isUsedTmcAddress(address)) {
      Serial.println(F("  <- unexpected; check MS1/MS2"));
    } else {
      Serial.println();
    }
  }
}

void runUartCheck() {
  runTmcAddressScan();
  const bool ok1 = printUartCheck(motor1, F("Motor1 / Head LR"),
                                  HotoriConfig::HEAD_LR_TMC_ADDRESS);
  const bool ok2 = printUartCheck(motor2, F("Motor2 / Head UD"),
                                  HotoriConfig::HEAD_UD_TMC_ADDRESS);
  const bool ok3 = printUartCheck(motor3, F("Motor3 / Tail"),
                                  HotoriConfig::TAIL_TMC_ADDRESS);
  Serial.print(F("[UART] Result: M1 "));
  Serial.print(ok1 ? F("OK") : F("NG"));
  Serial.print(F(", M2 "));
  Serial.print(ok2 ? F("OK") : F("NG"));
  Serial.print(F(", M3 "));
  Serial.println(ok3 ? F("OK") : F("NG"));
  if (!ok1 || !ok2 || !ok3) {
    Serial.println(F("        Check VM power, the PDN_UART wire on A0, GND and MS1/MS2."));
  }
}

void printWatchEntry(uint8_t address) {
  printAddressBits(address);
  Serial.print(' ');
  Serial.print(uartWatchLastOk[address] ? F("OK ") : F("-- "));
  Serial.print(uartWatchOkCount[address]);
  Serial.print('/');
  Serial.print(uartWatchNgCount[address]);
}

// Probes one address per interval so a missing reply blocks loop() only once.
void serviceUartWatch() {
  if (!uartWatchOn) {
    return;
  }
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - uartWatchLastMs) <
      HotoriConfig::UART_WATCH_INTERVAL_MS) {
    return;
  }
  uartWatchLastMs = now;

  const uint8_t address = uartWatchNextAddress;
  const bool ok = probeTmcAddress(address) != 0;
  uartWatchLastOk[address] = ok;
  if (ok) {
    ++uartWatchOkCount[address];
  } else {
    ++uartWatchNgCount[address];
  }

  uartWatchNextAddress = (address + 1) % TMC_ADDRESS_COUNT;
  if (uartWatchNextAddress != 0) {
    return;
  }
  // Print once all four addresses have been probed in this round.
  Serial.print(F("[UART] "));
  for (uint8_t i = 0; i < TMC_ADDRESS_COUNT; ++i) {
    if (i != 0) {
      Serial.print(F(" | "));
    }
    printWatchEntry(i);
  }
  Serial.println();
}

void toggleUartWatch() {
  uartWatchOn = !uartWatchOn;
  if (uartWatchOn) {
    uartWatchNextAddress = 0;
    uartWatchLastMs = millis() - HotoriConfig::UART_WATCH_INTERVAL_MS;
    for (uint8_t i = 0; i < TMC_ADDRESS_COUNT; ++i) {
      uartWatchLastOk[i] = false;
      uartWatchOkCount[i] = 0;
      uartWatchNgCount[i] = 0;
    }
    Serial.println(F("[UART] Watch on: MS2MS1 OK/-- reply/no-reply counts (00=M1 01=M2 10=M3)."));
    Serial.println(F("       Send 'uartw' again to stop."));
  } else {
    Serial.println(F("[UART] Watch off."));
  }
}

// ----- SG90 servo -----
// Parses "sv<angle>" (for example sv0, sv90, sv180). Returns false when the
// text after "sv" is empty, not a number, or above 180.
bool parseServoAngle(const char* text, uint8_t& angle) {
  if (*text == '\0') {
    return false;
  }
  uint16_t value = 0;
  for (; *text != '\0'; ++text) {
    if (*text < '0' || *text > '9') {
      return false;
    }
    value = value * 10 + static_cast<uint16_t>(*text - '0');
    if (value > 180) {
      return false;
    }
  }
  angle = static_cast<uint8_t>(value);
  return true;
}

void printServoAngle() {
  Serial.print(F("[Servo] Angle "));
  Serial.print(servo.angle());
  Serial.println(F(" deg, power on."));
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  m1f/m1r/m1s - Motor 1 / Head LR forward/reverse/stop"));
  Serial.println(F("  m2f/m2r/m2s - Motor 2 / Head UD forward/reverse/stop"));
  Serial.println(F("  m3f/m3r/m3s - Motor 3 / Tail forward/reverse/stop"));
  Serial.println(F("  m4f/m4r/m4s - Motor 4 / Rib1 forward/reverse/stop"));
  Serial.println(F("  m5f/m5r/m5s - Motor 5 / Rib2 forward/reverse/stop"));
  Serial.println(F("  home - Home Motor 1/2 on their max switches"));
  Serial.println(F("  pos  - Show Motor 1/2 homing state, angle and switch"));
  Serial.println(F("  svon / svoff / sv<0-180> - Servo power on / off / angle"));
  Serial.println(F("  tmc  - Reconfigure Motor 1-3 TMC2209 over UART"));
  Serial.println(F("  uart - Scan addresses 00-11 and check Motor 1-3 UART once"));
  Serial.println(F("  uartw- Toggle UART link monitor"));
  Serial.println(F("  help - Show this list"));
  Serial.println(F("Line ending: Newline or Both NL & CR. ESP32 frames (AA 55 ...) use the same RX (D0)."));
}

void handleCommand(const char* command) {
  const bool reverse = command[2] == 'r';
  if (strcmp(command, "m1f") == 0 || strcmp(command, "m1r") == 0) {
    startHeadManual(headLR, motor1, F("[Motor 1] "), reverse);
  } else if (strcmp(command, "m1s") == 0) {
    headLR.stop();
    updateMotorEnable();
    Serial.println(F("[Motor 1] Stopped (holding)."));
  } else if (strcmp(command, "m2f") == 0 || strcmp(command, "m2r") == 0) {
    startHeadManual(headUD, motor2, F("[Motor 2] "), reverse);
  } else if (strcmp(command, "m2s") == 0) {
    headUD.stop();
    updateMotorEnable();
    Serial.println(F("[Motor 2] Stopped (holding)."));
  } else if (strcmp(command, "m3f") == 0 || strcmp(command, "m3r") == 0) {
    if (tail.manualStart(reverse)) {
      Serial.println(reverse ? F("[Motor 3] Reverse.") : F("[Motor 3] Forward."));
    }
  } else if (strcmp(command, "m3s") == 0) {
    tail.stop();
    updateMotorEnable();
    Serial.println(F("[Motor 3] Stopped and driver output off."));
    printSharedEnableState();
  } else if (strcmp(command, "m4f") == 0 || strcmp(command, "m4r") == 0) {
    startRibManual(motor4, F("[Motor 4] "), reverse);
  } else if (strcmp(command, "m4s") == 0) {
    motor4.stop();
    updateMotorEnable();
    Serial.println(F("[Motor 4] Stopped."));
    printSharedEnableState();
  } else if (strcmp(command, "m5f") == 0 || strcmp(command, "m5r") == 0) {
    startRibManual(motor5, F("[Motor 5] "), reverse);
  } else if (strcmp(command, "m5s") == 0) {
    motor5.stop();
    updateMotorEnable();
    Serial.println(F("[Motor 5] Stopped."));
    printSharedEnableState();
  } else if (strcmp(command, "home") == 0) {
    headLR.startHoming();
    headUD.startHoming();
    updateMotorEnable();
  } else if (strcmp(command, "pos") == 0) {
    headLR.printStatus();
    headUD.printStatus();
  } else if (strcmp(command, "tmc") == 0) {
    headLR.stop();
    headUD.stop();
    tail.stop();
    updateMotorEnable();
    configureUartMotors();
  } else if (strcmp(command, "svon") == 0) {
    servo.powerOn();
    printServoAngle();
  } else if (strcmp(command, "svoff") == 0) {
    servo.powerOff();
    Serial.println(F("[Servo] Power off."));
  } else if (strncmp(command, "sv", 2) == 0) {
    uint8_t angle = 0;
    if (!parseServoAngle(command + 2, angle)) {
      Serial.println(F("[ERROR] Servo angle must be 0-180, e.g. sv90."));
      return;
    }
    servo.moveTo(angle);
    printServoAngle();
  } else if (strcmp(command, "uart") == 0) {
    runUartCheck();
  } else if (strcmp(command, "uartw") == 0) {
    toggleUartWatch();
  } else if (strcmp(command, "help") == 0) {
    printHelp();
  } else {
    Serial.print(F("[ERROR] Unknown command: "));
    Serial.println(command);
    Serial.println(F("        Enter 'help' to show available commands."));
  }
}

// ----- ESP32 frames (docs/ESP32_UART_PROTOCOL.md) -----
void reportEspError(EspFrameReceiver::Result result) {
  Serial.print(F("[ESP ERROR] RX="));
  for (uint8_t i = 0; i < espReceiver.errorLength(); ++i) {
    printHex2(espReceiver.errorBytes()[i]);
    Serial.print(' ');
  }
  Serial.print(F("- "));
  switch (result) {
    case EspFrameReceiver::TruncatedFrame: Serial.println(F("Incomplete frame (timeout)")); break;
    case EspFrameReceiver::InvalidHeader: Serial.println(F("Invalid HEADER")); break;
    case EspFrameReceiver::CrcMismatch: Serial.println(F("CRC mismatch")); break;
    case EspFrameReceiver::InvalidFlags: Serial.println(F("Invalid FLAGS")); break;
    case EspFrameReceiver::InvalidState: Serial.println(F("Invalid actionId/catState")); break;
    case EspFrameReceiver::InvalidVoice: Serial.println(F("Invalid voiceId")); break;
    case EspFrameReceiver::InvalidBreath: Serial.println(F("Invalid breathRate")); break;
    case EspFrameReceiver::InvalidTail: Serial.println(F("Invalid tailDir/tailSpeed")); break;
    case EspFrameReceiver::InvalidNeck: Serial.println(F("Invalid neckLR/neckUD/neckSpeed")); break;
    case EspFrameReceiver::InvalidEye: Serial.println(F("Invalid eyeState")); break;
    default: Serial.println(F("Invalid frame")); break;
  }
}

// breathRate 0 stops both ribs; 1-60 breaths/min rotate them continuously.
void applyBreath(uint8_t breathRate) {
  if (breathRate == 0) {
    motor4.stop();
    motor5.stop();
    Serial.println(F("[BREATH] Stop."));
    return;
  }
  const uint32_t halfPeriodUs =
      30000000UL / (HotoriConfig::BREATH_STEPS_PER_CYCLE * breathRate);
  motor4.setStepHalfPeriodUs(halfPeriodUs);
  motor5.setStepHalfPeriodUs(halfPeriodUs);
  if (!motor4.isRunning()) {
    if (HotoriConfig::BREATH_RIB1_FORWARD) {
      motor4.startForward();
    } else {
      motor4.startReverse();
    }
  }
  if (!motor5.isRunning()) {
    if (HotoriConfig::BREATH_RIB2_FORWARD) {
      motor5.startForward();
    } else {
      motor5.startReverse();
    }
  }
  Serial.print(F("[BREATH] "));
  Serial.print(breathRate);
  Serial.println(F("/min"));
}

void applyEspCommand(const EspCommand& command) {
  // Every selected group was validated before this point. Motion first,
  // logs afterwards.
  if (command.flags & EspFrameReceiver::TAIL_FLAG) {
    tail.apply(command.tailDir, command.tailSpeed, command.tailTime);
  }
  if (command.flags & EspFrameReceiver::NECK_FLAG) {
    headLR.moveToAngle(command.neckLR, command.neckSpeed);
    headUD.moveToAngle(command.neckUD, command.neckSpeed);
  }
  if (command.flags & EspFrameReceiver::BREATH_FLAG) {
    applyBreath(command.breathRate);
  }
  updateMotorEnable();
  if (command.flags & EspFrameReceiver::EYE_FLAG) {
    servo.moveTo(command.eyeState ? HotoriConfig::EYE_OPEN_ANGLE
                                  : HotoriConfig::EYE_CLOSED_ANGLE);
    Serial.println(command.eyeState ? F("[EYE] Open.") : F("[EYE] Closed."));
  }
  if (command.flags & EspFrameReceiver::VOICE_FLAG) {
    // No audio module on this board yet.
    Serial.print(F("[VOICE] id="));
    Serial.print(command.voiceId);
    Serial.println(F(" (no audio output)"));
  }
  if (command.flags & EspFrameReceiver::STATE_FLAG) {
    Serial.print(F("[STATE] actionId="));
    Serial.print(command.actionId);
    Serial.print(F(" catState="));
    Serial.println(command.catState);
  }
  Serial.print(F("[ESP] Frame flags=0x"));
  printHex2(command.flags);
  Serial.println();
}

void handleEspByte(uint8_t value) {
  lastEspByteMs = millis();
  const EspFrameReceiver::Result result = espReceiver.feed(value, espCommand);
  if (result == EspFrameReceiver::Valid) {
    applyEspCommand(espCommand);
  } else if (result != EspFrameReceiver::Incomplete) {
    reportEspError(result);
  }
}

// Reads at most one byte per pass so input never starves STEP output.
void serviceSerialInput() {
  if (Serial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Serial.read());
    // AA at the start of a line begins a frame; text commands are ASCII.
    if (espReceiver.inProgress() ||
        (value == EspFrameReceiver::HEADER1 && commandReader.isIdle())) {
      handleEspByte(value);
    } else {
      const char* command = commandReader.feed(static_cast<char>(value), Serial);
      if (command != nullptr) {
        handleCommand(command);
      }
    }
  }
  if (espReceiver.inProgress() &&
      static_cast<uint32_t>(millis() - lastEspByteMs) >=
          HotoriConfig::ESP_FRAME_TIMEOUT_MS) {
    const EspFrameReceiver::Result result = espReceiver.expire();
    if (result != EspFrameReceiver::Incomplete) {
      reportEspError(result);
    }
  }
}

void setup() {
  Serial.begin(HotoriConfig::SERIAL_BAUD_RATE);

  // The EN pin shared by all five drivers is disabled before anything else.
  digitalWrite(HotoriConfig::MOTOR_ENABLE_PIN,
               HotoriConfig::MOTOR_DISABLE_LEVEL);
  pinMode(HotoriConfig::MOTOR_ENABLE_PIN, OUTPUT);

  motor1.begin();
  motor2.begin();
  motor3.begin();
  motor4.begin();
  motor5.begin();
  headLR.begin();
  headUD.begin();
  tail.begin();
  servo.begin();

  Serial.println(F("\nHotori hardware test starting..."));
  Serial.println(F("[OK] Motor pins initialized; all drivers are disabled."));
  Serial.println(F("[OK] Servo initialized; power off."));
#if !ENABLE_LIMIT_SWITCHES
  Serial.println(F("[WARN] Limit switches disabled (ENABLE_LIMIT_SWITCHES=0)."));
  Serial.println(F("       Nothing stops at a limit; the head uses its power-up position as 0 deg."));
#else
  if (tail.readLimits() != 0) {
    Serial.println(F("[WARN] A tail limit is pressed."));
  }
#endif

  for (uint8_t i = 0; i < TMC_ADDRESS_COUNT; ++i) {
    tmcDrivers[i]->beginSerial(HotoriConfig::TMC_UART_BAUD_RATE);
  }
  configureUartMotors();
  printHelp();

  // Find the head reference first; ESP32 angle commands wait for it.
  headLR.startHoming();
  headUD.startHoming();

#if AUTO_RUN
  Serial.println(F("[AUTO RUN] Starting Motor3, Motor4 and Motor5 forward."));
  tail.manualStart(false);
  motor4.startForward();
  motor5.startForward();
#endif
}

void loop() {
  // Each service is short and non-blocking. Limit checks run before any STEP
  // edge in the same pass.
  headLR.update();
  headUD.update();
  tail.update();
  updateMotorEnable();
  motor1.update();
  motor2.update();
  motor3.update();
  motor4.update();
  motor5.update();
  serviceUartWatch();
  serviceSerialInput();
}
