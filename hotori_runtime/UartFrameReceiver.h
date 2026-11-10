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
    TruncatedFrame,
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
  Result expire();
  const uint8_t* errorBytes() const { return errorBytes_; }
  uint8_t errorLength() const { return errorLength_; }
  static uint8_t crc8(const uint8_t* data, size_t length);

 private:
  void captureError();
  Result validate() const;
  void resynchronize();

  uint8_t buffer_[9];
  uint8_t length_;
  uint8_t errorBytes_[9];
  uint8_t errorLength_;
};
