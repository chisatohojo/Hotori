#pragma once

#include <Arduino.h>

namespace HotoriConfig {

// TMC2209 drivers (STEP / DIR / EN standalone control)
constexpr uint8_t MOTOR1_STEP_PIN = 8;
constexpr uint8_t MOTOR1_DIR_PIN = 9;
constexpr uint8_t MOTOR1_ENABLE_PIN = 10;

constexpr uint8_t MOTOR2_STEP_PIN = 22;
constexpr uint8_t MOTOR2_DIR_PIN = 23;
constexpr uint8_t MOTOR2_ENABLE_PIN = 24;

// Left/right tail limit switches (normally closed contacts to GND).
constexpr uint8_t TAIL_LEFT_LIMIT_PIN = 25;
constexpr uint8_t TAIL_RIGHT_LIMIT_PIN = 26;
constexpr uint8_t TAIL_LIMIT_ACTIVE_LEVEL = HIGH;

// Most TMC2209 carrier boards use LOW to enable the output stage.
constexpr uint8_t MOTOR_ENABLE_LEVEL = LOW;
constexpr uint8_t MOTOR_DISABLE_LEVEL = HIGH;
constexpr uint8_t MOTOR_FORWARD_LEVEL = HIGH;

// One step is generated every 2 * MOTOR_STEP_HALF_PERIOD_US.
// 500 us produces 1000 steps/second.
constexpr uint32_t MOTOR_STEP_HALF_PERIOD_US = 5000UL;

// USB Serial Monitor and DFPlayer Mini serial settings.
constexpr uint32_t DEBUG_BAUD_RATE = 115200UL;
constexpr uint32_t DFPLAYER_BAUD_RATE = 9600UL;
constexpr uint8_t DFPLAYER_VOLUME = 20;  // Valid range: 0 to 30.

}  // namespace HotoriConfig
