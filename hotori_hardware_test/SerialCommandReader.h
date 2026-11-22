#pragma once

#include <Arduino.h>

class SerialCommandReader {
 public:
  SerialCommandReader();

  // Returns a null-terminated command only when a complete line is available.
  // The returned pointer remains valid until the next call to poll().
  const char* poll(Stream& serial);
  // Same as poll() for one byte that the caller has already read. Errors are
  // printed to log.
  const char* feed(char input, Print& log);
  // True when no partial line is buffered.
  bool isIdle() const { return length_ == 0 && !overflowed_; }

 private:
  static constexpr uint8_t BUFFER_SIZE = 24;
  char buffer_[BUFFER_SIZE];
  uint8_t length_;
  bool overflowed_;
};
