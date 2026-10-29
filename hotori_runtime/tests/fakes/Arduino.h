#pragma once

#include <stdint.h>
#include <stddef.h>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define F(text) text
constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t OUTPUT = 1;
constexpr uint8_t INPUT_PULLUP = 2;
constexpr uint8_t SERIAL_8N1 = 6;
constexpr uint8_t HEX = 16;

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t value) = 0;
  size_t write(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) write(data[i]);
    return length;
  }
  size_t print(const char* text) {
    size_t count = 0;
    while (*text) { write(static_cast<uint8_t>(*text++)); ++count; }
    return count;
  }
  size_t print(char value) { return write(static_cast<uint8_t>(value)); }
  template <typename T> size_t print(T value, int base = 10) {
    std::ostringstream out;
    if (base == HEX) out << std::hex << std::uppercase;
    out << static_cast<unsigned long long>(value);
    return print(out.str().c_str());
  }
  size_t println() { return print("\r\n"); }
  template <typename T> size_t println(T value, int base = 10) {
    return print(value, base) + println();
  }
  size_t println(const char* value) { return print(value) + println(); }
};

class HardwareSerial : public Print {
 public:
  using Print::write;
  std::deque<uint8_t> rx;
  std::vector<uint8_t> tx;
  int room = 64;
  uint32_t baud = 0;
  uint8_t format = 0;
  void begin(uint32_t value, uint8_t config = SERIAL_8N1) { baud = value; format = config; }
  int available() const { return static_cast<int>(rx.size()); }
  int availableForWrite() const { return room; }
  int read() {
    if (rx.empty()) return -1;
    const uint8_t value = rx.front(); rx.pop_front(); return value;
  }
  size_t write(uint8_t value) override {
    if (room == 0) throw std::runtime_error("Blocking serial write attempted");
    --room;
    tx.push_back(value);
    return 1;
  }
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
uint32_t micros();
uint32_t millis();
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
void pinMode(uint8_t pin, uint8_t mode);

namespace fake {
struct PinEvent { char kind; uint8_t pin; uint8_t value; };
extern uint8_t pins[70];
extern uint8_t modes[70];
extern uint32_t ms;
extern uint32_t us;
extern std::vector<PinEvent> events;
void reset();
void advanceMs(uint32_t amount);
}
