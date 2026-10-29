#pragma once

#include <Arduino.h>

#include "HotoriRuntimeConfig.h"

// Implements the Mini's serial commands directly: the installed DFRobot
// library waits even with ACK disabled, which is unsuitable for runtime STEP.
class DfPlayerController {
 public:
  DfPlayerController();
  void begin(HardwareSerial& port, Print& log);
  bool enqueue(uint8_t voiceId);
  void update();

 private:
  enum State : uint8_t { BootWait, InitialStop, Running };
  bool send(uint8_t command, uint16_t argument);
  void receiveByte();
  void reportResponse();

  HardwareSerial* port_;
  Print* log_;
  State state_;
  uint32_t bootMs_;
  uint32_t lastSentMs_;
  uint8_t queue_[HotoriRuntimeConfig::DFPLAYER_QUEUE_CAPACITY];
  uint8_t head_;
  uint8_t count_;
  uint8_t response_[10];
  uint8_t received_;
};
