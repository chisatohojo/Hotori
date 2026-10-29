#include "DfPlayerController.h"

DfPlayerController::DfPlayerController()
    : port_(nullptr), log_(nullptr), state_(BootWait), bootMs_(0), lastSentMs_(0),
      queue_{}, head_(0), count_(0), response_{}, received_(0) {}

void DfPlayerController::begin(HardwareSerial& port, Print& log) {
  port_ = &port;
  log_ = &log;
  port_->begin(HotoriRuntimeConfig::DFPLAYER_BAUD_RATE, SERIAL_8N1);
  bootMs_ = millis();
  lastSentMs_ = bootMs_;
  state_ = BootWait;
  head_ = count_ = received_ = 0;
  // Also stop a module that stayed powered while only the Mega reset.
  send(0x16, 0);
  log_->println(F("[DFPlayer] Boot wait; initial voice state is stopped."));
}

bool DfPlayerController::enqueue(uint8_t voiceId) {
  if (count_ == sizeof(queue_)) return false;
  queue_[(head_ + count_) % sizeof(queue_)] = voiceId;
  ++count_;
  return true;
}

bool DfPlayerController::send(uint8_t command, uint16_t argument) {
  if (port_->availableForWrite() < 10) return false;
  // 7E FF 06 CMD ACK PARAM_H PARAM_L CHECK_H CHECK_L EF
  uint8_t frame[10] = {0x7E, 0xFF, 0x06, command, 0,
                      static_cast<uint8_t>(argument >> 8),
                      static_cast<uint8_t>(argument), 0, 0, 0xEF};
  uint16_t sum = 0;
  for (uint8_t i = 1; i < 7; ++i) sum += frame[i];
  const uint16_t checksum = static_cast<uint16_t>(0U - sum);
  frame[7] = checksum >> 8;
  frame[8] = checksum;
  port_->write(frame, sizeof(frame));  // Fits TX buffer; never wait for space.
  lastSentMs_ = millis();
  return true;
}

void DfPlayerController::update() {
  if (port_ == nullptr) return;
  receiveByte();  // Bounded work even if the module floods responses.
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastSentMs_) <
      HotoriRuntimeConfig::DFPLAYER_COMMAND_GAP_MS) return;
  if (state_ == BootWait) {
    if (static_cast<uint32_t>(now - bootMs_) <
        HotoriRuntimeConfig::DFPLAYER_STARTUP_MS) return;
    if (send(0x06, HotoriRuntimeConfig::DFPLAYER_VOLUME)) state_ = InitialStop;
    return;
  }
  if (state_ == InitialStop) {
    if (send(0x16, 0)) {
      state_ = Running;
      log_->print(F("[DFPlayer] Command output enabled (no ACK); volume="));
      log_->println(HotoriRuntimeConfig::DFPLAYER_VOLUME);
    }
    return;
  }
  if (count_ == 0) return;
  const uint8_t voice = queue_[head_];
  if (!send(voice == 0 ? 0x16 : 0x12, voice)) return;
  head_ = (head_ + 1) % sizeof(queue_);
  --count_;
  if (voice == 0) log_->println(F("[VOICE] Stop"));
  else {
    log_->print(F("[VOICE] Play ID="));
    log_->println(voice);
  }
}

void DfPlayerController::reportResponse() {
  const uint16_t value = static_cast<uint16_t>(response_[5]) << 8 | response_[6];
  switch (response_[3]) {
    case 0x40:
      log_->print(F("[DFPlayer ERROR] Code="));
      log_->println(value);
      break;
    case 0x3D:
      log_->print(F("[DFPlayer] Track finished: "));
      log_->println(value);
      break;
    case 0x3B:
      log_->println(F("[DFPlayer] Media removed."));
      break;
    case 0x3A:
    case 0x3F:
      log_->println(F("[DFPlayer] Media event received."));
      break;
    default:
      break;
  }
}

void DfPlayerController::receiveByte() {
  if (port_->available() == 0) return;
  const uint8_t value = static_cast<uint8_t>(port_->read());
  if (received_ == 0 && value != 0x7E) return;
  response_[received_++] = value;
  if (received_ != sizeof(response_)) return;
  uint16_t sum = 0;
  for (uint8_t i = 1; i < 7; ++i) sum += response_[i];
  const uint16_t checksum = static_cast<uint16_t>(response_[7]) << 8 | response_[8];
  if (response_[1] == 0xFF && response_[2] == 0x06 && response_[9] == 0xEF &&
      static_cast<uint16_t>(0U - sum) == checksum) {
    reportResponse();
    received_ = 0;
    return;
  }
  // Find the next potential response start without a blocking retry.
  received_ = 0;
  for (uint8_t i = 1; i < sizeof(response_); ++i) {
    if (response_[i] == 0x7E) {
      received_ = sizeof(response_) - i;
      for (uint8_t j = 0; j < received_; ++j) response_[j] = response_[i + j];
      break;
    }
  }
}
