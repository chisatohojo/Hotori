#include "UartFrameReceiver.h"

UartFrameReceiver::UartFrameReceiver() : buffer_{}, length_(0) {}

uint8_t UartFrameReceiver::crc8(const uint8_t* data, size_t length) {
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

UartFrameReceiver::Result UartFrameReceiver::validate() const {
  if (crc8(buffer_ + 2, 6) != buffer_[8]) return CrcMismatch;
  const uint8_t flags = buffer_[2];
  if (flags == 0 || (flags & 0xF8) != 0) return InvalidFlags;
  if ((flags & VOICE_FLAG) && buffer_[3] == 255) return InvalidVoice;
  if ((flags & BREATH_FLAG) && buffer_[4] > 60) return InvalidBreath;
  if (flags & TAIL_FLAG) {
    if (buffer_[5] > 3) return InvalidTailDir;
    if (buffer_[6] > 100) return InvalidTailSpeed;
  }
  return Valid;
}

void UartFrameReceiver::resynchronize() {
  // Retain an overlapping header, including one inside a corrupt candidate.
  // Do not restart on AA 55 inside a frame until that frame fails validation:
  // unselected fields and VOICE/TIME may legitimately contain header bytes.
  for (uint8_t i = 1; i + 1 < length_; ++i) {
    if (buffer_[i] == 0xAA && buffer_[i + 1] == 0x55) {
      const uint8_t remaining = length_ - i;
      for (uint8_t j = 0; j < remaining; ++j) buffer_[j] = buffer_[i + j];
      length_ = remaining;
      return;
    }
  }
  const bool trailingHeader = buffer_[length_ - 1] == 0xAA;
  length_ = trailingHeader ? 1 : 0;
  if (trailingHeader) buffer_[0] = 0xAA;
}

UartFrameReceiver::Result UartFrameReceiver::feed(uint8_t value,
                                                RuntimeCommand& command) {
  if (length_ == 0) {
    if (value == 0xAA) buffer_[length_++] = value;
    return Incomplete;
  }
  if (length_ == 1) {
    if (value == 0x55) {
      buffer_[length_++] = value;
      return Incomplete;
    }
    length_ = value == 0xAA ? 1 : 0;
    return InvalidHeader;
  }
  buffer_[length_++] = value;
  if (length_ != sizeof(buffer_)) return Incomplete;

  const Result result = validate();
  if (result != Valid) {
    resynchronize();
    return result;
  }
  // Publish only after every selected group passes validation (atomic frame).
  command = {buffer_[2], buffer_[3], buffer_[4],
             buffer_[5], buffer_[6], buffer_[7]};
  length_ = 0;
  return Valid;
}
