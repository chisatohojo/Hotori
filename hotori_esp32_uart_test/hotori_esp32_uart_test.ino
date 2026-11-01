// Hotori ESP32 -> Mega 2560 simple UART tester (protocol v0.3)
// For ESP32 DevKit / ESP32-WROOM: GPIO17 (TX) -> Mega D17 (RX2).
// ESP32 GND -> Mega GND. Never connect Mega 5V TX directly to ESP32 RX.
// PC USB Serial monitor: 115200 bps, line ending = Newline.

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

constexpr int MEGA_TX_PIN = 17;  // ESP32 TX GPIO; adjust to your board.
constexpr uint32_t BAUD = 115200;

char line[48];
uint8_t lineLength = 0;

// CRC-8/SMBUS: poly=0x07, init=0, no reflection, xorout=0.
uint8_t crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                         : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

void sendFrame(uint8_t flags, uint8_t voice, uint8_t breath,
               uint8_t dir, uint8_t speed, uint8_t timeTenths) {
  uint8_t frame[9] = {0xAA, 0x55, flags, voice, breath,
                      dir, speed, timeTenths, 0};
  frame[8] = crc8(frame + 2, 6);
  Serial2.write(frame, sizeof(frame));

  Serial.print("TX: ");
  for (uint8_t value : frame) {
    if (value < 16) Serial.print('0');
    Serial.print(value, HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void showHelp() {
  Serial.println("Commands (end each with Newline):");
  Serial.println("  v 1         Play /mp3/0001.mp3 (v 0 = stop, v 1..7)");
  Serial.println("  b 20        Breathing 20 bpm (b 0 = stop)");
  Serial.println("  t 1 1 10    Tail left, 1% speed, 1.0 sec");
  Serial.println("  t 2 1 10    Tail right, 1% speed, 1.0 sec");
  Serial.println("  t 3 10 30   Tail alternating, 10% speed, 3.0 sec");
  Serial.println("  t 0 0 0     Tail stop");
  Serial.println("  stop        Stop voice, breath, and tail");
  Serial.println("  help        Show commands");
  Serial.println("Tail: direction 0..3, speed 0..100%, time 0..255 (0.1 s units)");
}

void handleLine(const char* command) {
  int a, b, c;
  char extra;
  if (strcmp(command, "help") == 0) {
    showHelp();
  } else if (strcmp(command, "stop") == 0) {
    sendFrame(0x07, 0, 0, 0, 0, 0);
  } else if (sscanf(command, "v %d %c", &a, &extra) == 1 &&
             a >= 0 && a <= 7) {
    sendFrame(0x01, (uint8_t)a, 0, 0, 0, 0);
  } else if (sscanf(command, "b %d %c", &a, &extra) == 1 &&
             a >= 0 && a <= 60) {
    sendFrame(0x02, 0, (uint8_t)a, 0, 0, 0);
  } else if (sscanf(command, "t %d %d %d %c", &a, &b, &c, &extra) == 3 &&
             a >= 0 && a <= 3 && b >= 0 && b <= 100 &&
             c >= 0 && c <= 255) {
    sendFrame(0x04, 0, 0, (uint8_t)a, (uint8_t)b, (uint8_t)c);
  } else if (command[0] != '\0') {
    Serial.println("Invalid command. Type help.");
  }
}

void setup() {
  Serial.begin(BAUD);
  Serial2.begin(BAUD, SERIAL_8N1, -1, MEGA_TX_PIN);
  Serial.println("Hotori ESP32 UART test ready.");
  showHelp();
}

void loop() {
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      line[lineLength] = '\0';
      handleLine(line);
      lineLength = 0;
    } else if (lineLength < sizeof(line) - 1) {
      line[lineLength++] = ch;
    } else {
      lineLength = 0;
      Serial.println("Command too long. Type help.");
    }
  }
}
