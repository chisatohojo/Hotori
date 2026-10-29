#pragma once

#include <stddef.h>
#include <stdint.h>

struct RuntimeCommand {
  uint8_t flags;
  uint8_t voice;
  uint8_t breath;
  uint8_t tailDir;
  uint8_t tailSpeed;
  uint8_t tailTime;
};

class UartFrameReceiver {
 public:
  enum Result : uint8_t {
    Incomplete,
    Valid,
    InvalidHeader,
    CrcMismatch,
    InvalidFlags,
    InvalidVoice,
    InvalidBreath,
    InvalidTailDir,
    InvalidTailSpeed
  };

  static constexpr uint8_t VOICE_FLAG = 0x01;
  static constexpr uint8_t BREATH_FLAG = 0x02;
  static constexpr uint8_t TAIL_FLAG = 0x04;

  UartFrameReceiver();
  Result feed(uint8_t value, RuntimeCommand& command);
  static uint8_t crc8(const uint8_t* data, size_t length);

 private:
  Result validate() const;
  void resynchronize();

  uint8_t buffer_[9];
  uint8_t length_;
};
