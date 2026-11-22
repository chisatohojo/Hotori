#include "SerialCommandReader.h"

SerialCommandReader::SerialCommandReader() : length_(0), overflowed_(false) {
  buffer_[0] = '\0';
}

const char* SerialCommandReader::poll(Stream& serial) {
  while (serial.available() > 0) {
    const char* command = feed(static_cast<char>(serial.read()), serial);
    if (command != nullptr) {
      return command;
    }
  }
  return nullptr;
}

const char* SerialCommandReader::feed(char input, Print& log) {
  if (input == '\r') {
    return nullptr;
  }

  if (input == '\n') {
    if (overflowed_) {
      log.println(F("[ERROR] Command is too long."));
      length_ = 0;
      overflowed_ = false;
      return nullptr;
    }

    if (length_ == 0) {
      return nullptr;
    }

    buffer_[length_] = '\0';
    length_ = 0;
    return buffer_;
  }

  if (length_ < BUFFER_SIZE - 1) {
    buffer_[length_++] = input;
  } else {
    overflowed_ = true;
  }
  return nullptr;
}
