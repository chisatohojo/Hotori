#pragma once

#include <Arduino.h>

namespace HotoriRuntimeConfig {

constexpr uint8_t MOTOR1_STEP_PIN = 8;
constexpr uint8_t MOTOR1_DIR_PIN = 9;
constexpr uint8_t MOTOR1_ENABLE_PIN = 10;
constexpr uint8_t MOTOR2_STEP_PIN = 22;
constexpr uint8_t MOTOR2_DIR_PIN = 23;
constexpr uint8_t MOTOR2_ENABLE_PIN = 24;
constexpr uint8_t TAIL_LEFT_LIMIT_PIN = 25;
constexpr uint8_t TAIL_RIGHT_LIMIT_PIN = 26;
constexpr uint8_t MOTOR_ENABLE_LEVEL = LOW;
constexpr uint8_t MOTOR_FORWARD_LEVEL = HIGH;
constexpr uint8_t TAIL_LIMIT_ACTIVE_LEVEL = HIGH;

// Confirm left/right on the mechanism; change only this to reverse Tail.
constexpr bool TAIL_LEFT_USES_FORWARD = true;
constexpr uint32_t TAIL_MIN_STEPS_PER_SECOND = 25;
constexpr uint32_t TAIL_MAX_STEPS_PER_SECOND = 100;

// Measure complete mechanical breaths before enabling Motor2. No guessed value.
constexpr uint32_t BREATH_STEPS_PER_CYCLE = 0;
constexpr bool BREATH_USES_FORWARD = true;

constexpr uint32_t DEBUG_BAUD_RATE = 115200UL;
constexpr uint32_t ESP32_BAUD_RATE = 115200UL;
constexpr uint32_t DFPLAYER_BAUD_RATE = 9600UL;
constexpr uint8_t DFPLAYER_VOLUME = 20;
// Same boot allowance as dfplayer_min_test; implemented without waiting.
constexpr uint32_t DFPLAYER_STARTUP_MS = 1500;
// DFRobotDFPlayerMini 1.0.6 uses a 10 ms interval in its no-ACK path.
constexpr uint32_t DFPLAYER_COMMAND_GAP_MS = 10;
constexpr uint8_t DFPLAYER_QUEUE_CAPACITY = 16;

}  // namespace HotoriRuntimeConfig
