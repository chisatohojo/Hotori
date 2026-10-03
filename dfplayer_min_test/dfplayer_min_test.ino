#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

constexpr uint32_t DEBUG_BAUD_RATE = 115200UL;
constexpr uint32_t DFPLAYER_BAUD_RATE = 9600UL;
constexpr uint8_t DFPLAYER_VOLUME = 15;
constexpr uint16_t DFPLAYER_STARTUP_WAIT_MS = 1500;
constexpr uint16_t PLAYBACK_WAIT_MS = 500;

DFRobotDFPlayerMini dfPlayer;

void printDfPlayerError(int value) {
  Serial.print(F("[DFPlayer ERROR] "));

  switch (value) {
    case Busy:
      Serial.println(F("Module is busy (card not found or initializing)."));
      break;
    case Sleeping:
      Serial.println(F("Module is sleeping."));
      break;
    case SerialWrongStack:
      Serial.println(F("Serial packet format error."));
      break;
    case CheckSumNotMatch:
      Serial.println(F("Serial packet checksum mismatch."));
      break;
    case FileIndexOut:
      Serial.println(F("File index is out of range."));
      break;
    case FileMismatch:
      Serial.println(F("Requested file was not found."));
      break;
    case Advertise:
      Serial.println(F("Advertise command is not available in the current state."));
      break;
    default:
      Serial.print(F("Unknown error code: "));
      Serial.println(value);
      break;
  }
}

void reportDfPlayerEvents() {
  if (!dfPlayer.available()) {
    return;
  }

  const uint8_t eventType = dfPlayer.readType();
  const int eventValue = dfPlayer.read();

  switch (eventType) {
    case TimeOut:
      Serial.println(F("[DFPlayer ERROR] Serial communication timeout."));
      break;
    case WrongStack:
      Serial.println(F("[DFPlayer ERROR] Invalid response packet."));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("[DFPlayer ERROR] microSD card removed."));
      break;
    case DFPlayerError:
      printDfPlayerError(eventValue);
      break;
    default:
      // This test reports error events only.
      break;
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD_RATE);
  Serial1.begin(DFPLAYER_BAUD_RATE);

  Serial.println(F("\nDFPlayer Mini standalone test starting..."));
  Serial.println(F("Waiting 1500 ms for DFPlayer startup..."));
  delay(DFPLAYER_STARTUP_WAIT_MS);

  Serial.println(F("Initializing DFPlayer on Serial1..."));
  if (!dfPlayer.begin(Serial1)) {
    Serial.println(F("[ERROR] dfPlayer.begin(Serial1) failed."));
    Serial.println(F("Check power, common GND, TX/RX wiring, and microSD card."));
    return;
  }

  Serial.println(F("[OK] dfPlayer.begin(Serial1) succeeded."));
  dfPlayer.volume(DFPLAYER_VOLUME);
  Serial.println(F("Volume set to 15."));

  delay(PLAYBACK_WAIT_MS);
  dfPlayer.playMp3Folder(1);
  Serial.println(F("Playing /mp3/0001.mp3"));
}

void loop() {
  reportDfPlayerEvents();
}
