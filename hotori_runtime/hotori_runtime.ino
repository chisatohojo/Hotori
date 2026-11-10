#include <Arduino.h>

#include "BreathController.h"
#include "DfPlayerController.h"
#include "HotoriRuntimeConfig.h"
#include "RuntimeLog.h"
#include "TailController.h"
#include "UartFrameReceiver.h"

RuntimeLog runtimeLog;
StepDirMotor tailMotor(HotoriRuntimeConfig::MOTOR1_STEP_PIN,
                       HotoriRuntimeConfig::MOTOR1_DIR_PIN,
                       HotoriRuntimeConfig::MOTOR1_ENABLE_PIN);
StepDirMotor breathMotor(HotoriRuntimeConfig::MOTOR2_STEP_PIN,
                         HotoriRuntimeConfig::MOTOR2_DIR_PIN,
                         HotoriRuntimeConfig::MOTOR2_ENABLE_PIN);
TailController tail(tailMotor, runtimeLog);
BreathController breath(breathMotor, runtimeLog,
                         HotoriRuntimeConfig::BREATH_STEPS_PER_CYCLE);
DfPlayerController dfPlayer;
UartFrameReceiver receiver;
RuntimeCommand receivedCommand = {};
bool voicePending = false;
uint8_t pendingVoice = 0;
uint32_t lastUartByteMs = 0;

void reportUartError(UartFrameReceiver::Result result) {
  runtimeLog.print(F("[UART ERROR] "));
  runtimeLog.print(F("RX="));
  for (uint8_t i = 0; i < receiver.errorLength(); ++i) {
    const uint8_t value = receiver.errorBytes()[i];
    if (value < 16) runtimeLog.print('0');
    runtimeLog.print(value, HEX);
    runtimeLog.print(' ');
  }
  runtimeLog.print(F("- "));
  switch (result) {
    case UartFrameReceiver::TruncatedFrame: runtimeLog.println(F("Incomplete frame (timeout)")); break;
    case UartFrameReceiver::InvalidHeader: runtimeLog.println(F("Invalid HEADER")); break;
    case UartFrameReceiver::CrcMismatch: runtimeLog.println(F("CRC mismatch")); break;
    case UartFrameReceiver::InvalidFlags: runtimeLog.println(F("Invalid FLAGS")); break;
    case UartFrameReceiver::InvalidVoice: runtimeLog.println(F("Invalid VOICE")); break;
    case UartFrameReceiver::InvalidBreath: runtimeLog.println(F("Invalid BREATH")); break;
    case UartFrameReceiver::InvalidTailDir: runtimeLog.println(F("Invalid TAIL_DIR")); break;
    case UartFrameReceiver::InvalidTailSpeed: runtimeLog.println(F("Invalid TAIL_SPEED")); break;
    default: runtimeLog.println(F("Invalid frame")); break;
  }
}

void applyCommand(const RuntimeCommand& command) {
  // Nothing reaches here until CRC AND every selected group are valid.
  // Apply motor stops/updates before diagnostic output and audio scheduling.
  if (command.flags & UartFrameReceiver::TAIL_FLAG) {
    tail.apply(command.tailDir, command.tailSpeed, command.tailTime);
  }
  if (command.flags & UartFrameReceiver::BREATH_FLAG) breath.apply(command.breath);
  if (command.flags & UartFrameReceiver::VOICE_FLAG) {
    if (!dfPlayer.enqueue(command.voice)) {
      pendingVoice = command.voice;
      voicePending = true;
      runtimeLog.println(F("[VOICE] Output queue full; deferring UART consumption."));
    }
  }
  runtimeLog.print(F("[UART] Valid frame flags=0x"));
  if (command.flags < 16) runtimeLog.print('0');
  runtimeLog.println(command.flags, HEX);
}

void setup() {
  // Safe latches and disables come before UART, diagnostics, or audio startup.
  tailMotor.begin();
  breathMotor.begin();
  tail.begin();
  Serial.begin(HotoriRuntimeConfig::DEBUG_BAUD_RATE, SERIAL_8N1);
  Serial2.begin(HotoriRuntimeConfig::ESP32_BAUD_RATE, SERIAL_8N1);
  dfPlayer.begin(Serial1, runtimeLog);
  runtimeLog.println(F("[RUNTIME] All stopped; waiting for ESP32 on Serial2 (115200 8N1)."));
}

void loop() {
  tail.update();
  breath.update();
  dfPlayer.update();

  if (voicePending) {
    if (dfPlayer.enqueue(pendingVoice)) voicePending = false;
  } else if (Serial2.available() > 0) {
    lastUartByteMs = millis();
    // One byte per pass: no input flood can monopolize STEP/limit servicing.
    const UartFrameReceiver::Result result =
        receiver.feed(static_cast<uint8_t>(Serial2.read()), receivedCommand);
    if (result == UartFrameReceiver::Valid) applyCommand(receivedCommand);
    else if (result != UartFrameReceiver::Incomplete) reportUartError(result);
  } else if (static_cast<uint32_t>(millis() - lastUartByteMs) >=
             HotoriRuntimeConfig::UART_FRAME_TIMEOUT_MS) {
    const UartFrameReceiver::Result result = receiver.expire();
    if (result != UartFrameReceiver::Incomplete) reportUartError(result);
  }

  tail.update();
  breath.update();
  runtimeLog.update(Serial);
  // No communication watchdog: silence is normal for change-only commands.
  // Serial2 is receive-only; no ACK or other data is transmitted to the ESP32.
}
