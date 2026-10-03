#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

class DfPlayerController {
 public:
  DfPlayerController();

  bool begin(HardwareSerial& serialPort, Stream& log);
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
