#pragma once

#include <Arduino.h>

// Hotori runs on an Arduino Nano (ATmega328P) only.
#if !defined(__AVR_ATmega328P__)
#error "hotori_hardware_test supports the Arduino Nano (ATmega328P) only."
#endif

namespace HotoriConfig {

// ----- Pins -----
// Motors follow their number: STEP = odd pin, DIR = the next even pin.
// One EN pin is shared by all five TMC2209 drivers.
constexpr uint8_t MOTOR_ENABLE_PIN = 2;

constexpr uint8_t MOTOR1_STEP_PIN = 3;   // Motor1 / Head left-right (UART)
constexpr uint8_t MOTOR1_DIR_PIN = 4;
constexpr uint8_t MOTOR2_STEP_PIN = 5;   // Motor2 / Head up-down (UART)
constexpr uint8_t MOTOR2_DIR_PIN = 6;
constexpr uint8_t MOTOR3_STEP_PIN = 7;   // Motor3 / Tail (UART)
constexpr uint8_t MOTOR3_DIR_PIN = 8;
constexpr uint8_t MOTOR4_STEP_PIN = 9;   // Motor4 / Rib1 (STEP/DIR only)
constexpr uint8_t MOTOR4_DIR_PIN = 10;
constexpr uint8_t MOTOR5_STEP_PIN = 11;  // Motor5 / Rib2 (STEP/DIR only)
constexpr uint8_t MOTOR5_DIR_PIN = 12;

// SG90 servo. D13 also drives the on-board LED, so the servo power blinks
// briefly while the bootloader runs.
constexpr uint8_t SERVO_POWER_PIN = 13;
constexpr uint8_t SERVO_SIGNAL_PIN = A1;

// TMC2209 UART for Motor1-3: one wire, wired straight to every PDN_UART.
// The pin switches between sending and receiving (SoftwareSerial).
constexpr uint8_t TMC_UART_PIN = A0;

// Limit switches: plain switches to GND, read with INPUT_PULLUP.
// Each head axis has two switches wired in parallel on one input, so the
// pressed end is found from the direction of travel.
constexpr uint8_t HEAD_LR_LIMIT_PIN = A2;   // Head left + right in parallel.
constexpr uint8_t HEAD_UD_LIMIT_PIN = A3;   // Head up + down in parallel.
constexpr uint8_t TAIL_LEFT_LIMIT_PIN = A4;
constexpr uint8_t TAIL_RIGHT_LIMIT_PIN = A5;

// The ESP32 TX goes to D0 (hardware Serial RX, shared with USB). Disconnect
// it while uploading.

// ----- Limit switches -----
// Set to 0 to ignore every limit switch (the head then treats its power-up
// position as 0 deg).
#ifndef ENABLE_LIMIT_SWITCHES
#define ENABLE_LIMIT_SWITCHES 1
#endif
// Parallel switches only work as normally-open contacts: pressed = LOW.
// A broken wire reads as "not pressed".
constexpr uint8_t LIMIT_ACTIVE_LEVEL = LOW;
constexpr uint8_t TAIL_LIMIT_ACTIVE_LEVEL = LOW;

// ----- TMC2209 UART (Motor1-3) -----
// Address = MS1 + 2 x MS2.
constexpr uint8_t HEAD_LR_TMC_ADDRESS = 0;  // MS1 GND, MS2 GND
constexpr uint8_t HEAD_UD_TMC_ADDRESS = 1;  // MS1 VIO, MS2 GND
constexpr uint8_t TAIL_TMC_ADDRESS = 2;     // MS1 GND, MS2 VIO
constexpr uint32_t TMC_UART_BAUD_RATE = 57600UL;
constexpr float TMC_R_SENSE_OHMS = 0.11f;
// RMS run current. With 0.11 ohm the TMC2209 tops out near 1770 mA.
constexpr uint16_t HEAD_LR_CURRENT_MA = 2000;
constexpr uint16_t HEAD_UD_CURRENT_MA = 1000;
constexpr uint16_t TAIL_CURRENT_MA = 400;
constexpr float TMC_HOLD_CURRENT_RATIO = 0.5f;
constexpr uint16_t UART_MOTOR_MICROSTEPS = 8;
// Chopper off time while running. Stop writes 0 to free the motor shaft.
constexpr uint8_t TMC_TOFF_RUN = 5;
// Configuration is written and read back up to this many times.
constexpr uint8_t TMC_CONFIG_ATTEMPTS = 5;
// 'uartw' monitor: one TMC2209 address (0-3) is probed per interval, so all
// four are checked every 4 intervals (1 s). A probe that gets no reply
// blocks loop() for about 20 ms (TMCStepper retries and timeouts).
constexpr uint32_t UART_WATCH_INTERVAL_MS = 250UL;

// ----- Motor common -----
// Most TMC2209 carrier boards use LOW to enable the output stage.
// The shared EN is on while any motor runs or a homed head holds its angle.
constexpr uint8_t MOTOR_ENABLE_LEVEL = LOW;
constexpr uint8_t MOTOR_DISABLE_LEVEL = HIGH;
constexpr uint8_t MOTOR_FORWARD_LEVEL = HIGH;
// Manual test commands (m1f etc.) step every 2 x this period.
constexpr uint32_t MOTOR_STEP_HALF_PERIOD_US = 5000UL;

// ----- Head angles (neckLR = Motor1, neckUD = Motor2) -----
// Forward steps increase the angle. If an axis moves the wrong way, swap
// its FORWARD_LEVEL.
constexpr uint8_t HEAD_LR_FORWARD_LEVEL = MOTOR_FORWARD_LEVEL;
constexpr uint8_t HEAD_UD_FORWARD_LEVEL = MOTOR_FORWARD_LEVEL;
// 200 full steps x 8 microsteps / 360 deg x gear ratio (output / motor).
// Only valid while UART sets 1/8 microsteps.
constexpr float MOTOR_STEPS_PER_DEGREE = 200.0f * 8.0f / 360.0f;
constexpr float HEAD_LR_STEPS_PER_DEGREE = MOTOR_STEPS_PER_DEGREE * 38.0f / 15.0f;
constexpr float HEAD_UD_STEPS_PER_DEGREE = MOTOR_STEPS_PER_DEGREE * 65.0f / 15.0f;
// Angle at each limit switch. Measured switch-to-switch spans: left-right
// 125.6 deg, up-down 94.6 deg. The up-down centre is provisional.
constexpr float HEAD_LR_MIN_SWITCH_ANGLE = -62.8f;
constexpr float HEAD_LR_MAX_SWITCH_ANGLE = 62.8f;
constexpr float HEAD_UD_MIN_SWITCH_ANGLE = -47.3f;
constexpr float HEAD_UD_MAX_SWITCH_ANGLE = 47.3f;
// Commands stop this far before a switch so normal moves do not hit it.
constexpr float HEAD_SWITCH_MARGIN_DEGREES = 2.0f;
// Homing runs toward the max switch at this speed.
constexpr uint32_t HOMING_STEP_HALF_PERIOD_US = 1250UL;  // 400 steps/s.
// Homing gives up after this many output degrees without the switch.
constexpr float HOMING_MAX_DEGREES = 270.0f;
// Moving away from a pressed switch is allowed for this far. Still pressed
// after that means it is the opposite switch, so the axis stops.
constexpr float LIMIT_ESCAPE_MAX_DEGREES = 20.0f;
// Reads in a row that must show "released" before a switch counts as left.
constexpr uint8_t LIMIT_RELEASE_CONFIRM = 10;
// neckSpeed 1-100 % maps linearly to this cruise speed range.
constexpr uint16_t HEAD_MIN_STEPS_PER_SECOND = 100;
constexpr uint16_t HEAD_MAX_STEPS_PER_SECOND = 667;  // 1500 us per step.
// Trapezoidal ramp for angle moves: start speed and ramp length.
constexpr uint16_t HEAD_START_STEPS_PER_SECOND = 400;  // 2500 us per step.
constexpr uint16_t HEAD_RAMP_STEPS = 800;
// Keep the shared EN on while a homed head holds its angle.
constexpr bool HEAD_HOLD_POSITION = true;

// ----- Tail (tailDir / tailSpeed / tailTime) -----
// Provisional: change after checking which DIR level moves the tail left.
constexpr bool TAIL_LEFT_USES_FORWARD = true;
// tailSpeed 1-100 % maps linearly to this range.
constexpr uint16_t TAIL_MIN_STEPS_PER_SECOND = 25;
constexpr uint16_t TAIL_MAX_STEPS_PER_SECOND = 100;
// tailDir=3 (left-right swing) reverses every interval, starting left.
constexpr uint32_t TAIL_SWING_HALF_PERIOD_MS = 500UL;

// ----- Breathing (breathRate) -> Motor4 / Rib1 and Motor5 / Rib2 -----
// Provisional: STEP pulses for one full breath (one turn at 1/8 step).
constexpr uint32_t BREATH_STEPS_PER_CYCLE = 1600UL;
constexpr bool BREATH_RIB1_FORWARD = true;
constexpr bool BREATH_RIB2_FORWARD = true;

// ----- SG90 servo / eyes (eyeState) -----
// The power pin drives a switch (MOSFET / transistor / relay module)
// between the external 5 V supply and the servo. HIGH = power on.
constexpr uint8_t SERVO_POWER_ON_LEVEL = HIGH;
constexpr uint16_t SERVO_MIN_PULSE_US = 500;   // SG90 0 degrees.
constexpr uint16_t SERVO_MAX_PULSE_US = 2400;  // SG90 180 degrees.
constexpr uint8_t SERVO_INITIAL_ANGLE = 90;
// Provisional eyelid angles.
constexpr uint8_t EYE_CLOSED_ANGLE = 0;
constexpr uint8_t EYE_OPEN_ANGLE = 90;

// ----- Serial -----
// USB Serial Monitor and the ESP32 link share the hardware Serial.
constexpr uint32_t SERIAL_BAUD_RATE = 115200UL;
// A frame whose bytes stop arriving for this long is dropped.
constexpr uint32_t ESP_FRAME_TIMEOUT_MS = 100UL;

// Only for DfPlayerController.cpp, which this sketch does not use (the
// board has no free pins for an audio module).
constexpr uint8_t DFPLAYER_VOLUME = 20;

// AUTO_RUN=1 starts the tail and both ribs forward at power-up.
#ifndef AUTO_RUN
#define AUTO_RUN 0
#endif

}  // namespace HotoriConfig
