
#ifndef CAT_COMMAND_H
#define CAT_COMMAND_H

#include <Arduino.h>

constexpr uint8_t CAT_VOICE_FLAG = 0x01;
constexpr uint8_t CAT_BREATH_FLAG = 0x02;
constexpr uint8_t CAT_TAIL_FLAG = 0x04;

void sendCatCommand(
    uint8_t flags,
    uint8_t voice = 0,
    uint8_t breath = 0,
    uint8_t tailDir = 0,
    uint8_t tailSpeed = 0,
    uint8_t tailTime = 0
);

void serviceCatCommand(HardwareSerial& uart);

#endif
