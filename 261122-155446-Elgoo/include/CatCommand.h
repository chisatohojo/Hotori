
#ifndef CAT_COMMAND_H
#define CAT_COMMAND_H

#include <Arduino.h>

void sendCatCommand(
    uint8_t voice,
    uint8_t breath,
    uint8_t tailDir,
    uint8_t tailSpeed,
    uint8_t tailTime,
    int8_t neckLR,
    int8_t neckUD,
    uint8_t eyes
);

void serviceCatCommand(HardwareSerial& uart);

#endif
