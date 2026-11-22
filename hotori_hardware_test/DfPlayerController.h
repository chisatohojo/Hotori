#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

class DfPlayerController {
 public:
  DfPlayerController();

  // serialPort must already be started at DFPLAYER_BAUD_RATE.
  bool begin(Stream& serialPort, Stream& log);
  void playMp3(uint16_t trackNumber);
  void stop();
  void update();
  bool isReady() const;

 private:
  void printEvent(uint8_t type, int value);

  DFRobotDFPlayerMini player_;
  Stream* log_;
  bool ready_;
};
