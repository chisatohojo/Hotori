#include "EspFrameReceiver.h"

namespace {
// Byte offsets inside the frame.
constexpr uint8_t FLAGS = 2;
constexpr uint8_t ACTION_ID = 3;
constexpr uint8_t CAT_STATE = 4;
constexpr uint8_t VOICE_ID = 5;
constexpr uint8_t BREATH_RATE = 6;
constexpr uint8_t TAIL_DIR = 7;
constexpr uint8_t TAIL_SPEED = 8;
constexpr uint8_t TAIL_TIME = 9;
constexpr uint8_t NECK_LR = 10;
constexpr uint8_t NECK_UD = 11;
constexpr uint8_t NECK_SPEED = 12;
constexpr uint8_t EYE_STATE = 13;
constexpr uint8_t CRC = 14;
constexpr uint8_t CRC_LENGTH = CRC - FLAGS;  // FLAGS..eyeState = 12 bytes.
}  // namespace

EspFrameReceiver::EspFrameReceiver()
    : buffer_{}, length_(0), errorBytes_{}, errorLength_(0) {}

void EspFrameReceiver::captureError() {
  errorLength_ = length_;
  for (uint8_t i = 0; i < length_; ++i) {
    errorBytes_[i] = buffer_[i];
  }
}

EspFrameReceiver::Result EspFrameReceiver::expire() {
  if (length_ == 0) {
    return Incomplete;
  }
  captureError();
  length_ = 0;
  return TruncatedFrame;
}

uint8_t EspFrameReceiver::crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

EspFrameReceiver::Result EspFrameReceiver::validate() const {
  if (crc8(buffer_ + FLAGS, CRC_LENGTH) != buffer_[CRC]) {
    return CrcMismatch;
  }
  const uint8_t flags = buffer_[FLAGS];
  if (flags == 0 || (flags & RESERVED_FLAGS) != 0) {
    return InvalidFlags;
  }
  if ((flags & STATE_FLAG) &&
      (buffer_[ACTION_ID] < 1 || buffer_[ACTION_ID] > 9 ||
       buffer_[CAT_STATE] > 3)) {
    return InvalidState;
  }
  if ((flags & VOICE_FLAG) && buffer_[VOICE_ID] > 6) {
    return InvalidVoice;
  }
  if ((flags & BREATH_FLAG) && buffer_[BREATH_RATE] > 60) {
    return InvalidBreath;
  }
  if ((flags & TAIL_FLAG) &&
      (buffer_[TAIL_DIR] > 3 || buffer_[TAIL_SPEED] > 100)) {
    return InvalidTail;  // tailTime accepts the full 0-255.
  }
  if (flags & NECK_FLAG) {
    const int8_t lr = static_cast<int8_t>(buffer_[NECK_LR]);
    const int8_t ud = static_cast<int8_t>(buffer_[NECK_UD]);
    if (lr < -90 || lr > 90 || ud < -30 || ud > 30 ||
        buffer_[NECK_SPEED] > 100) {
      return InvalidNeck;
    }
  }
  if ((flags & EYE_FLAG) && buffer_[EYE_STATE] > 1) {
    return InvalidEye;
  }
  return Valid;
}

void EspFrameReceiver::resynchronize() {
  // Keep a header that appeared inside a rejected candidate. A valid frame is
  // never cut short by AA 55 in its payload: values may contain those bytes.
  for (uint8_t i = 1; i + 1 < length_; ++i) {
    if (buffer_[i] == HEADER1 && buffer_[i + 1] == HEADER2) {
      const uint8_t remaining = length_ - i;
      for (uint8_t j = 0; j < remaining; ++j) {
        buffer_[j] = buffer_[i + j];
      }
      length_ = remaining;
      return;
    }
  }
  const bool trailingHeader = buffer_[length_ - 1] == HEADER1;
  length_ = trailingHeader ? 1 : 0;
  if (trailingHeader) {
    buffer_[0] = HEADER1;
  }
}

EspFrameReceiver::Result EspFrameReceiver::feed(uint8_t value,
                                                EspCommand& command) {
  if (length_ == 0) {
    buffer_[length_++] = value;
    if (value == HEADER1) {
      return Incomplete;
    }
    captureError();
    length_ = 0;
    return InvalidHeader;
  }
  if (length_ == 1) {
    if (value == HEADER2) {
      buffer_[length_++] = value;
      return Incomplete;
    }
    buffer_[1] = value;
    length_ = 2;
    captureError();
    length_ = value == HEADER1 ? 1 : 0;
    return InvalidHeader;
  }
  buffer_[length_++] = value;
  if (length_ != FRAME_SIZE) {
    return Incomplete;
  }

  const Result result = validate();
  if (result != Valid) {
    captureError();
    resynchronize();
    return result;
  }
  // Published only after every selected group passed validation.
  command.flags = buffer_[FLAGS];
  command.actionId = buffer_[ACTION_ID];
  command.catState = buffer_[CAT_STATE];
  command.voiceId = buffer_[VOICE_ID];
  command.breathRate = buffer_[BREATH_RATE];
  command.tailDir = buffer_[TAIL_DIR];
  command.tailSpeed = buffer_[TAIL_SPEED];
  command.tailTime = buffer_[TAIL_TIME];
  command.neckLR = static_cast<int8_t>(buffer_[NECK_LR]);
  command.neckUD = static_cast<int8_t>(buffer_[NECK_UD]);
  command.neckSpeed = buffer_[NECK_SPEED];
  command.eyeState = buffer_[EYE_STATE];
  length_ = 0;
  return Valid;
}
