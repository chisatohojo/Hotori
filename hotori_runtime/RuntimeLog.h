#pragma once

#include <Arduino.h>

// Buffer complete log lines so a slow/disconnected USB monitor cannot hold up
// limit checks or STEP generation. Drop whole lines if diagnostics are flooded.
class RuntimeLog : public Print {
 public:
  RuntimeLog();
  using Print::write;
  size_t write(uint8_t value) override;
  void update(HardwareSerial& port);
  uint32_t droppedLines() const { return droppedLines_; }

 private:
  char queue_[384];
  char line_[96];
  uint16_t head_;
  uint16_t queued_;
  uint8_t lineLength_;
  bool lineOverflow_;
  uint32_t droppedLines_;
};
