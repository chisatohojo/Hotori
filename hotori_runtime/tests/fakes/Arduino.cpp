#include "Arduino.h"

#include <algorithm>

HardwareSerial Serial;
HardwareSerial Serial1;
HardwareSerial Serial2;

namespace fake {
uint8_t pins[70] = {};
uint8_t modes[70] = {};
uint32_t ms = 0;
uint32_t us = 0;
std::vector<PinEvent> events;

void reset() {
  std::fill(pins, pins + 70, LOW);  // Connected NC contacts pull inputs LOW.
  std::fill(modes, modes + 70, 0);
  events.clear();
  ms = us = 0;
  Serial = HardwareSerial();
  Serial1 = HardwareSerial();
  Serial2 = HardwareSerial();
}

void advanceMs(uint32_t amount) {
  ms += amount;
  us += amount * 1000UL;
  Serial.room = Serial1.room = Serial2.room = 64;
}
}

uint32_t micros() { return fake::us; }
uint32_t millis() { return fake::ms; }
void digitalWrite(uint8_t pin, uint8_t value) {
  fake::pins[pin] = value;
  fake::events.push_back({'W', pin, value});
}
int digitalRead(uint8_t pin) { return fake::pins[pin]; }
void pinMode(uint8_t pin, uint8_t mode) {
  fake::modes[pin] = mode;
  fake::events.push_back({'M', pin, mode});
}
