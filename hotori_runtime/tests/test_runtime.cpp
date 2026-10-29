#include <Arduino.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

// Compile the real orchestration as well as the real controller .cpp files.
#include "../hotori_runtime.ino"

namespace {
unsigned assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  std::ostringstream error; error << __FILE__ << ':' << __LINE__ << \
  ": " << #condition; throw std::runtime_error(error.str()); } } while (0)

class CapturingPrint : public Print {
 public:
  std::string text;
  size_t write(uint8_t value) override { text.push_back(static_cast<char>(value)); return 1; }
};

std::vector<uint8_t> frame(uint8_t flags = 7, uint8_t voice = 2,
                           uint8_t breathValue = 20, uint8_t dir = 3,
                           uint8_t speed = 50, uint8_t duration = 30) {
  std::vector<uint8_t> bytes = {0xAA, 0x55, flags, voice, breathValue, dir, speed, duration, 0};
  bytes[8] = UartFrameReceiver::crc8(bytes.data() + 2, 6);
  return bytes;
}

UartFrameReceiver::Result feed(UartFrameReceiver& parser, RuntimeCommand& command,
                                const std::vector<uint8_t>& bytes) {
  UartFrameReceiver::Result result = UartFrameReceiver::Incomplete;
  for (uint8_t byte : bytes) result = parser.feed(byte, command);
  return result;
}

void crcAndParser() {
  const uint8_t samples[5][9] = {
    {0xAA,0x55,0x07,0x02,0x14,0x03,0x32,0x1E,0x10},
    {0xAA,0x55,0x01,0x03,0x00,0x00,0x00,0x00,0x8F},
    {0xAA,0x55,0x02,0x00,0x14,0x00,0x00,0x00,0x6D},
    {0xAA,0x55,0x04,0x00,0x00,0x03,0x32,0x1E,0x90},
    {0xAA,0x55,0x07,0x00,0x00,0x00,0x00,0x00,0xDF}
  };
  const uint8_t check[] = {'1','2','3','4','5','6','7','8','9'};
  CHECK(UartFrameReceiver::crc8(check, 9) == 0xF4);
  for (const auto& sample : samples) {
    CHECK(UartFrameReceiver::crc8(sample + 2, 6) == sample[8]);
    UartFrameReceiver parser;
    RuntimeCommand command = {};
    for (unsigned i = 0; i < 9; ++i) {
      CHECK(parser.feed(sample[i], command) ==
            (i == 8 ? UartFrameReceiver::Valid : UartFrameReceiver::Incomplete));
    }
    CHECK(command.flags == sample[2]);
  }
  struct InvalidCase { std::vector<uint8_t> bytes; UartFrameReceiver::Result error; };
  auto badCrc = frame(); badCrc[8] ^= 1;
  const InvalidCase cases[] = {
    {badCrc, UartFrameReceiver::CrcMismatch},
    {frame(0), UartFrameReceiver::InvalidFlags},
    {frame(8), UartFrameReceiver::InvalidFlags},
    {frame(0xFF), UartFrameReceiver::InvalidFlags},
    {frame(7,255), UartFrameReceiver::InvalidVoice},
    {frame(7,2,61), UartFrameReceiver::InvalidBreath},
    {frame(7,2,255), UartFrameReceiver::InvalidBreath},
    {frame(7,2,20,4), UartFrameReceiver::InvalidTailDir},
    {frame(7,2,20,255), UartFrameReceiver::InvalidTailDir},
    {frame(7,2,20,3,101), UartFrameReceiver::InvalidTailSpeed},
    {frame(7,2,20,3,255), UartFrameReceiver::InvalidTailSpeed},
    {frame(4,0,0,0,255,0), UartFrameReceiver::InvalidTailSpeed}
  };
  for (const auto& test : cases) {
    UartFrameReceiver parser;
    RuntimeCommand command = {7,11,12,2,88,44};
    const RuntimeCommand original = command;
    CHECK(feed(parser, command, test.bytes) == test.error);
    CHECK(std::memcmp(&command, &original, sizeof(command)) == 0);
    CHECK(feed(parser, command, frame()) == UartFrameReceiver::Valid);
  }
  // Even out-of-range/header-looking unselected fields are ignored.
  const std::vector<uint8_t> selectedOnly[] = {
    frame(1,254,255,255,255,255), frame(2,255,60,255,255,255),
    frame(4,255,255,3,100,255), frame(1,0xAA,0x55,255,255,255),
    frame(7,254,60,3,100,255), frame(7,0,0,0,0,0)
  };
  for (const auto& bytes : selectedOnly) {
    UartFrameReceiver parser; RuntimeCommand command = {};
    CHECK(feed(parser, command, bytes) == UartFrameReceiver::Valid);
  }
  // Noise, repeated AA, broken header, truncated frame, and overlapping headers.
  const std::vector<std::vector<uint8_t>> prefixes = {
    {0,0x55,0xFF}, {0xAA}, {0xAA,0xAA}, {0xAA,0x12},
    {0xAA,0x55,7}, {0xAA,0x55,7,2,20},
    {0xAA,0x55,7,2,0xAA,0x55}, {0xAA,0x55,7,2,20,3,50,30,0xAA}
  };
  for (const auto& prefix : prefixes) {
    UartFrameReceiver parser; RuntimeCommand command = {};
    feed(parser, command, prefix);
    CHECK(feed(parser, command, frame()) == UartFrameReceiver::Valid);
    CHECK(command.voice == 2 && command.tailTime == 30);
  }
  // A lost byte at every position must recover by subsequent intact frames.
  for (size_t missing = 0; missing < 9; ++missing) {
    UartFrameReceiver parser; RuntimeCommand command = {};
    auto damaged = frame(); damaged.erase(damaged.begin() + missing);
    feed(parser, command, damaged);
    feed(parser, command, frame());
    CHECK(feed(parser, command, frame()) == UartFrameReceiver::Valid);
  }
  std::cout << "PASS CRC: 5/5 workbook samples; parser validation/resynchronization\n";
}

void motorsAndTail() {
  fake::reset(); CapturingPrint log;
  StepDirMotor motor(8,9,10); motor.begin();
  CHECK(!motor.isRunning() && fake::pins[8] == LOW && fake::pins[10] == HIGH);
  CHECK(fake::events[0].kind == 'W' && fake::events[0].pin == 10 && fake::events[0].value == HIGH);
  CHECK(fake::events[1].kind == 'M' && fake::events[1].pin == 10);
  TailController control(motor, log); control.begin();
  CHECK(fake::modes[25] == INPUT_PULLUP && fake::modes[26] == INPUT_PULLUP);
  CHECK(TailController::halfPeriodForSpeed(0) == 0);
  CHECK(TailController::halfPeriodForSpeed(1) == 20000);
  CHECK(TailController::halfPeriodForSpeed(100) == 5000);
  for (uint8_t speed = 2; speed <= 100; ++speed) {
    const uint32_t half = TailController::halfPeriodForSpeed(speed);
    CHECK(half < TailController::halfPeriodForSpeed(speed - 1));
    const double rate = 25.0 + 75.0 * (speed - 1) / 99.0;
    CHECK(std::abs(500000.0 / half - rate) < 0.021);
  }
  const uint8_t times[] = {1,10,30,255};
  for (uint8_t time : times) {
    control.apply(1,50,time);
    CHECK(control.durationMs() == static_cast<uint32_t>(time) * 100);
  }
  for (uint8_t direction = 1; direction <= 2; ++direction) {
    for (uint8_t limit = 25; limit <= 26; ++limit) {
      control.apply(direction,100,30);
      CHECK(motor.isRunning());
      CHECK(fake::pins[9] == (direction == 1 ? HIGH : LOW));
      fake::us += 5000; control.update(); CHECK(fake::pins[8] == HIGH);
      fake::pins[limit] = HIGH;
      const size_t before = fake::events.size();
      control.update();
      CHECK(fake::events[before].pin == 8 && fake::events[before].value == LOW);
      CHECK(fake::events[before + 1].pin == 10 && fake::events[before + 1].value == HIGH);
      CHECK(!motor.isRunning() && control.state() == TailController::Stopped);
      control.apply(direction,100,30); CHECK(!motor.isRunning());
      fake::pins[limit] = LOW; control.update(); CHECK(!motor.isRunning());
    }
  }
  control.apply(3,50,30); CHECK(fake::pins[9] == HIGH);
  fake::pins[25] = HIGH; control.update();
  CHECK(control.state() == TailController::WaitingForRelease && fake::pins[10] == HIGH);
  for (int i = 0; i < 10; ++i) {
    fake::advanceMs(10); control.update(); CHECK(!motor.isRunning());
  }
  fake::pins[25] = LOW; control.update();
  CHECK(motor.isRunning() && fake::pins[9] == LOW);
  fake::pins[26] = HIGH; control.update(); CHECK(!motor.isRunning());
  fake::pins[26] = LOW; control.update();
  CHECK(motor.isRunning() && fake::pins[9] == HIGH);
  fake::pins[25] = fake::pins[26] = HIGH; control.update();
  CHECK(!motor.isRunning());
  fake::pins[25] = LOW; control.update(); CHECK(!motor.isRunning());
  fake::advanceMs(3000); control.update();
  CHECK(control.state() == TailController::Stopped);
  fake::pins[26] = LOW; control.update(); CHECK(!motor.isRunning());
  // A fresh command replaces a waiting alternating command.
  fake::pins[25] = HIGH; control.apply(3,50,30);
  CHECK(control.state() == TailController::WaitingForRelease);
  control.apply(0,50,30); fake::pins[25] = LOW; control.update();
  CHECK(control.state() == TailController::Stopped && !motor.isRunning());
  for (unsigned zero = 0; zero < 3; ++zero) {
    control.apply(1,100,30);
    control.apply(zero == 0 ? 0 : 1, zero == 1 ? 0 : 100, zero == 2 ? 0 : 30);
    CHECK(!motor.isRunning() && fake::pins[10] == HIGH);
  }
  // Deadline replacement and silence; millis wrap across 0xFFFFFFFF.
  fake::ms = 100; control.apply(1,100,1);
  fake::advanceMs(99); control.update(); CHECK(motor.isRunning());
  control.apply(2,50,10);
  fake::advanceMs(999); control.update(); CHECK(motor.isRunning());
  fake::advanceMs(1); control.update(); CHECK(!motor.isRunning());
  fake::ms = std::numeric_limits<uint32_t>::max() - 49;
  control.apply(1,100,1);
  fake::advanceMs(99); control.update(); CHECK(motor.isRunning());
  fake::advanceMs(1); control.update(); CHECK(!motor.isRunning());
  // Independent micros wrap test; no compressed catch-up pulse train.
  fake::us = std::numeric_limits<uint32_t>::max() - 99;
  motor.start(true,1000); fake::us += 999; motor.update(); CHECK(fake::pins[8] == LOW);
  fake::us += 1; motor.update(); CHECK(fake::pins[8] == HIGH);
  fake::us += 100000; motor.update(); CHECK(fake::pins[8] == LOW);
  motor.update(); CHECK(fake::pins[8] == LOW);
  std::cout << "PASS Tail: all speed percentages, deadlines/wrap, limits, alternation, safe startup\n";
}

void breathCalibration() {
  fake::reset(); CapturingPrint log;
  StepDirMotor motor(22,23,24); motor.begin();
  BreathController uncalibrated(motor,log,0);
  uncalibrated.apply(20);
  CHECK(uncalibrated.breathsPerMinute() == 20 && !motor.isRunning() && fake::pins[24] == HIGH);
  uncalibrated.apply(20); uncalibrated.apply(30);
  const std::string marker = "Calibration required";
  const size_t occurrence = log.text.find(marker);
  CHECK(occurrence != std::string::npos && log.text.find(marker,occurrence + 1) == std::string::npos);
  uncalibrated.apply(0); CHECK(uncalibrated.breathsPerMinute() == 0 && fake::pins[24] == HIGH);
  // Synthetic calibration for math only; never set on the production device.
  BreathController calibrated(motor,log,200);
  calibrated.apply(20); CHECK(motor.isRunning() && motor.halfPeriodUs() == 7500);
  fake::advanceMs(60000); calibrated.update();
  CHECK(calibrated.breathsPerMinute() == 20 && motor.isRunning());
  calibrated.apply(60); CHECK(motor.halfPeriodUs() == 2500);
  calibrated.apply(0); CHECK(!motor.isRunning() && fake::pins[24] == HIGH);
  BreathController oversized(motor,log,std::numeric_limits<uint32_t>::max());
  oversized.apply(60); CHECK(!motor.isRunning());
  std::cout << "PASS Breath: calibration 0 inhibits drive; calibrated rate/hold/stop\n";
}

void checkDfPacket(const std::vector<uint8_t>& bytes, size_t offset,
                   uint8_t command, uint8_t argument) {
  CHECK(bytes.size() >= offset + 10);
  CHECK(bytes[offset] == 0x7E && bytes[offset + 1] == 0xFF && bytes[offset + 2] == 6);
  CHECK(bytes[offset + 3] == command && bytes[offset + 4] == 0);
  CHECK(bytes[offset + 5] == 0 && bytes[offset + 6] == argument && bytes[offset + 9] == 0xEF);
  uint16_t sum = 0;
  for (size_t i = 1; i < 9; ++i) {
    if (i == 7) sum += static_cast<uint16_t>(bytes[offset + i]) << 8;
    else sum += bytes[offset + i];
  }
  CHECK(sum == 0);
}

void audioAndLog() {
  fake::reset(); CapturingPrint log; DfPlayerController audio;
  audio.begin(Serial1,log);
  CHECK(fake::ms == 0 && Serial1.baud == 9600);
  checkDfPacket(Serial1.tx,0,0x16,0);
  CHECK(audio.enqueue(3) && audio.enqueue(3) && audio.enqueue(0));
  fake::advanceMs(1499); audio.update(); CHECK(Serial1.tx.size() == 10);
  fake::advanceMs(1); audio.update(); checkDfPacket(Serial1.tx,10,0x06,20);
  fake::advanceMs(10); audio.update(); checkDfPacket(Serial1.tx,20,0x16,0);
  fake::advanceMs(10); Serial1.room = 9; audio.update(); CHECK(Serial1.tx.size() == 30);
  Serial1.room = 64; audio.update(); checkDfPacket(Serial1.tx,30,0x12,3);
  audio.update(); CHECK(Serial1.tx.size() == 40);
  fake::advanceMs(10); audio.update(); checkDfPacket(Serial1.tx,40,0x12,3);
  fake::advanceMs(10); audio.update(); checkDfPacket(Serial1.tx,50,0x16,0);
  fake::advanceMs(100000); audio.update(); CHECK(Serial1.tx.size() == 60);
  for (unsigned i = 0; i < HotoriRuntimeConfig::DFPLAYER_QUEUE_CAPACITY; ++i) CHECK(audio.enqueue(1));
  CHECK(!audio.enqueue(2));
  // Valid DFPlayer error response, including its checksum.
  const uint8_t response[] = {0x7E,0xFF,0x06,0x40,0,0,6,0xFE,0xB5,0xEF};
  for (uint8_t byte : response) { Serial1.rx.push_back(byte); audio.update(); }
  CHECK(log.text.find("[DFPlayer ERROR] Code=6") != std::string::npos);

  RuntimeLog buffered;
  HardwareSerial debug;
  debug.room = 0;
  for (unsigned i = 0; i < 100; ++i) buffered.println(F("[TEST] Line"));
  const uint32_t before = fake::ms;
  buffered.update(debug); CHECK(debug.tx.empty() && fake::ms == before);
  CHECK(buffered.droppedLines() > 0);
  debug.room = 1; buffered.update(debug); CHECK(debug.tx.size() == 1);
  debug.room = 64; buffered.update(debug); CHECK(debug.tx.size() == 33);
  std::cout << "PASS audio/log: nonblocking boot/TX, volume/checksum, repeated IDs, bounded logs\n";
}

void sendToRuntime(const std::vector<uint8_t>& bytes) {
  for (uint8_t byte : bytes) Serial2.rx.push_back(byte);
  for (size_t i = 0; i < bytes.size() + 2; ++i) loop();
}

void runtimeIntegration() {
  fake::reset(); setup();
  CHECK(!tailMotor.isRunning() && !breathMotor.isRunning());
  CHECK(fake::pins[8] == LOW && fake::pins[10] == HIGH);
  CHECK(fake::pins[22] == LOW && fake::pins[24] == HIGH);
  CHECK(Serial.baud == 115200 && Serial2.baud == 115200 && Serial2.format == SERIAL_8N1);
  CHECK(Serial2.tx.empty());
  sendToRuntime(frame());
  CHECK(tailMotor.isRunning() && breath.breathsPerMinute() == 20 && !breathMotor.isRunning());
  sendToRuntime(frame(4,0,0,1,75,10));
  CHECK(breath.breathsPerMinute() == 20 && tailMotor.isRunning());
  // Unselected invalid fields are ignored by the parser AND dispatcher.
  sendToRuntime(frame(1,3,255,255,255,255));
  CHECK(breath.breathsPerMinute() == 20 && tailMotor.isRunning());
  sendToRuntime(frame(2,255,60,255,255,255));
  CHECK(breath.breathsPerMinute() == 60 && tailMotor.isRunning());
  const uint32_t tailPeriod = tailMotor.halfPeriodUs();
  // Invalid Breath must prevent otherwise valid Voice and Tail-stop changes.
  sendToRuntime(frame(7,4,61,0,0,0));
  CHECK(breath.breathsPerMinute() == 60 && tailMotor.isRunning());
  CHECK(tailMotor.halfPeriodUs() == tailPeriod);
  auto crcBroken = frame(7,0,0,0,0,0); crcBroken[8] ^= 1;
  sendToRuntime(crcBroken);
  CHECK(breath.breathsPerMinute() == 60 && tailMotor.isRunning());
  // Voice-only requests leave Tail's original deadline unchanged.
  fake::advanceMs(999); loop(); CHECK(tailMotor.isRunning());
  fake::advanceMs(1); loop(); CHECK(!tailMotor.isRunning());
  CHECK(breath.breathsPerMinute() == 60);
  sendToRuntime(frame(4,0,0,3,50,1));
  fake::pins[25] = HIGH; loop(); CHECK(!tailMotor.isRunning());
  // TX blockage and input activity do not interfere with immediate limit stop.
  Serial.room = Serial1.room = 0;
  fake::advanceMs(100); Serial.room = Serial1.room = 0; loop();
  fake::pins[25] = LOW; loop(); CHECK(!tailMotor.isRunning());
  CHECK(tail.state() == TailController::Stopped);
  sendToRuntime(frame(7,0,0,0,0,0));
  CHECK(breath.breathsPerMinute() == 0 && !breathMotor.isRunning() && !tailMotor.isRunning());
  fake::advanceMs(60000); loop();
  CHECK(Serial2.tx.empty());
  // Drain audio and assert only VALID selected Voice commands were enqueued.
  for (int i = 0; i < 20; ++i) { fake::advanceMs(10); loop(); }
  std::vector<uint8_t> voiceIds;
  for (size_t i = 0; i + 9 < Serial1.tx.size(); i += 10) {
    if (Serial1.tx[i + 3] == 0x12) voiceIds.push_back(Serial1.tx[i + 6]);
  }
  CHECK((voiceIds == std::vector<uint8_t>{2,3}));
  // Audio queue saturation holds the next voice command, without waiting in
  // loop or dropping repeated IDs. Motor/timer servicing continues meanwhile.
  Serial1.room = 0;
  for (unsigned i = 0; i < HotoriRuntimeConfig::DFPLAYER_QUEUE_CAPACITY + 1; ++i) {
    sendToRuntime(frame(1,5,0,0,0,0));
  }
  CHECK(voicePending);
  sendToRuntime(frame(4,0,0,1,100,1));
  for (unsigned i = 0; i < 100; ++i) { fake::advanceMs(10); loop(); }
  CHECK(!voicePending && Serial2.rx.empty() && !tailMotor.isRunning());
  unsigned repeated = 0;
  for (size_t i = 0; i + 9 < Serial1.tx.size(); i += 10) {
    if (Serial1.tx[i + 3] == 0x12 && Serial1.tx[i + 6] == 5) ++repeated;
  }
  CHECK(repeated == HotoriRuntimeConfig::DFPLAYER_QUEUE_CAPACITY + 1);
  CHECK(Serial2.tx.empty());
  std::cout << "PASS integration: partial updates, atomic rejection, silence, queue pressure, no ACK\n";
}
}

int main() {
  try {
    crcAndParser();
    motorsAndTail();
    breathCalibration();
    audioAndLog();
    runtimeIntegration();
    std::cout << "PASS " << assertions << " assertions (host GPIO/clock/UART mocks; no hardware used)\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
