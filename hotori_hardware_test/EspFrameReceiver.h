#pragma once

#include <stddef.h>
#include <stdint.h>

// One validated ESP32 frame (docs/ESP32_UART_PROTOCOL.md). Only the groups
// selected by flags are meaningful.
struct EspCommand {
  uint8_t flags;
  uint8_t actionId;
  uint8_t catState;
  uint8_t voiceId;
  uint8_t breathRate;
  uint8_t tailDir;
  uint8_t tailSpeed;
  uint8_t tailTime;
  int8_t neckLR;
  int8_t neckUD;
  uint8_t neckSpeed;
  uint8_t eyeState;
};

// Assembles the 15-byte binary frame byte by byte:
// AA 55 FLAGS actionId catState voiceId breathRate tailDir tailSpeed
// tailTime neckLR neckUD neckSpeed eyeState CRC8
class EspFrameReceiver {
 public:
  enum Result : uint8_t {
    Incomplete,
    Valid,
    InvalidHeader,
    TruncatedFrame,
    CrcMismatch,
    InvalidFlags,
    InvalidState,
    InvalidVoice,
    InvalidBreath,
    InvalidTail,
    InvalidNeck,
    InvalidEye
  };

  static constexpr uint8_t FRAME_SIZE = 15;
  static constexpr uint8_t HEADER1 = 0xAA;
  static constexpr uint8_t HEADER2 = 0x55;

  static constexpr uint8_t VOICE_FLAG = 0x01;
  static constexpr uint8_t BREATH_FLAG = 0x02;
  static constexpr uint8_t TAIL_FLAG = 0x04;
  static constexpr uint8_t NECK_FLAG = 0x08;
  static constexpr uint8_t EYE_FLAG = 0x10;
  static constexpr uint8_t STATE_FLAG = 0x20;
  static constexpr uint8_t RESERVED_FLAGS = 0xC0;

  EspFrameReceiver();
  Result feed(uint8_t value, EspCommand& command);
  // Drops a partly received frame (call after ESP_FRAME_TIMEOUT_MS of silence).
  Result expire();
  // True after AA has been received, until the frame ends or fails.
  bool inProgress() const { return length_ != 0; }
  const uint8_t* errorBytes() const { return errorBytes_; }
  uint8_t errorLength() const { return errorLength_; }
  // CRC-8/SMBUS: poly 0x07, init 0x00, no reflection, xorout 0x00.
  static uint8_t crc8(const uint8_t* data, size_t length);

 private:
  void captureError();
  Result validate() const;
  void resynchronize();

  uint8_t buffer_[FRAME_SIZE];
  uint8_t length_;
  uint8_t errorBytes_[FRAME_SIZE];
  uint8_t errorLength_;
};
