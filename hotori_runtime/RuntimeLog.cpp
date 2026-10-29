#include "RuntimeLog.h"

RuntimeLog::RuntimeLog()
    : queue_{}, line_{}, head_(0), queued_(0), lineLength_(0),
      lineOverflow_(false), droppedLines_(0) {}

size_t RuntimeLog::write(uint8_t value) {
  if (value == '\r') return 1;
  if (value != '\n') {
    if (lineLength_ < sizeof(line_)) line_[lineLength_++] = value;
    else lineOverflow_ = true;
    return 1;
  }
  const uint16_t required = lineLength_ + 1;
  if (lineOverflow_ || required > sizeof(queue_) - queued_) {
    ++droppedLines_;
  } else {
    for (uint8_t i = 0; i < lineLength_; ++i) {
      queue_[(head_ + queued_++) % sizeof(queue_)] = line_[i];
    }
    queue_[(head_ + queued_++) % sizeof(queue_)] = '\n';
  }
  lineLength_ = 0;
  lineOverflow_ = false;
  return 1;
}

void RuntimeLog::update(HardwareSerial& port) {
  int room = port.availableForWrite();
  uint8_t budget = 32;
  while (queued_ != 0 && room > 0 && budget > 0) {
    port.write(static_cast<uint8_t>(queue_[head_]));
    head_ = (head_ + 1) % sizeof(queue_);
    --queued_;
    --room;
    --budget;
  }
}
