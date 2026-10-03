#include "DfPlayerController.h"

#include "HotoriConfig.h"

DfPlayerController::DfPlayerController() : log_(nullptr), ready_(false) {}

bool DfPlayerController::begin(HardwareSerial& serialPort, Stream& log) {
  log_ = &log;
  serialPort.begin(HotoriConfig::DFPLAYER_BAUD_RATE);

  // ACK is enabled so communication failures can be reported. The module is
  // reset during initialization, as recommended by the library.
  ready_ = player_.begin(serialPort, true, true);
  if (!ready_) {
    log_->println(F("[ERROR] DFPlayer initialization failed."));
    log_->println(F("        Check 5V/GND, TX/RX wiring, and the microSD card."));
    log_->println(F("        Motor and Serial commands remain available."));
    return false;
  }

  player_.volume(HotoriConfig::DFPLAYER_VOLUME);
  log_->print(F("[OK] DFPlayer ready. Volume = "));
  log_->println(HotoriConfig::DFPLAYER_VOLUME);
  return true;
}

void DfPlayerController::playMp3(uint16_t trackNumber) {
  if (!ready_) {
    if (log_ != nullptr) {
      log_->println(F("[ERROR] DFPlayer is not ready; cannot play a track."));
    }
    return;
  }

  // playMp3Folder(1) selects /mp3/0001.mp3.
  player_.playMp3Folder(trackNumber);
  if (log_ != nullptr) {
    log_->print(F("[DFPlayer] Playing /mp3/"));
    if (trackNumber < 10) log_->print(F("000"));
    else if (trackNumber < 100) log_->print(F("00"));
    else if (trackNumber < 1000) log_->print(F("0"));
    log_->print(trackNumber);
    log_->println(F(".mp3"));
  }
}

void DfPlayerController::stop() {
  if (!ready_) {
    if (log_ != nullptr) {
      log_->println(F("[ERROR] DFPlayer is not ready; cannot stop playback."));
    }
    return;
  }

  player_.stop();
  if (log_ != nullptr) {
    log_->println(F("[DFPlayer] Stopped."));
  }
}

void DfPlayerController::update() {
  if (!ready_ || !player_.available()) {
    return;
  }

  printEvent(player_.readType(), player_.read());
}

bool DfPlayerController::isReady() const {
  return ready_;
}

void DfPlayerController::printEvent(uint8_t type, int value) {
  if (log_ == nullptr) {
    return;
  }

  switch (type) {
    case TimeOut:
      log_->println(F("[DFPlayer ERROR] Serial timeout."));
      break;
    case WrongStack:
      log_->println(F("[DFPlayer ERROR] Invalid response packet."));
      break;
    case DFPlayerCardInserted:
      log_->println(F("[DFPlayer] microSD card inserted."));
      break;
    case DFPlayerCardRemoved:
      log_->println(F("[DFPlayer ERROR] microSD card removed."));
      break;
    case DFPlayerCardOnline:
      log_->println(F("[DFPlayer] microSD card online."));
      break;
    case DFPlayerPlayFinished:
      log_->print(F("[DFPlayer] Track finished: "));
      log_->println(value);
      break;
    case DFPlayerError:
      log_->print(F("[DFPlayer ERROR] Code: "));
      log_->println(value);
      break;
    default:
      log_->print(F("[DFPlayer] Event type/value: "));
      log_->print(type);
      log_->print('/');
      log_->println(value);
      break;
  }
}
