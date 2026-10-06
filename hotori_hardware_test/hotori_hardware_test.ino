#include <Arduino.h>
#include <string.h>

#include "DfPlayerController.h"
#include "HotoriConfig.h"
#include "SerialCommandReader.h"
#include "StepDirMotor.h"

StepDirMotor motor1(HotoriConfig::MOTOR1_STEP_PIN,
                    HotoriConfig::MOTOR1_DIR_PIN,
                    HotoriConfig::MOTOR1_ENABLE_PIN,
                    HotoriConfig::MOTOR_ENABLE_LEVEL,
                    HotoriConfig::MOTOR_FORWARD_LEVEL,
                    HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);

StepDirMotor motor2(HotoriConfig::MOTOR2_STEP_PIN,
                    HotoriConfig::MOTOR2_DIR_PIN,
                    HotoriConfig::MOTOR2_ENABLE_PIN,
                    HotoriConfig::MOTOR_ENABLE_LEVEL,
                    HotoriConfig::MOTOR_FORWARD_LEVEL,
                    HotoriConfig::MOTOR_STEP_HALF_PERIOD_US);

DfPlayerController dfPlayer;
SerialCommandReader commandReader;
// Bit 0 = left active, bit 1 = right active.
uint8_t tailLimitStateReported = 0;

uint8_t readTailLimitState() {
  const bool leftActive =
      digitalRead(HotoriConfig::TAIL_LEFT_LIMIT_PIN) ==
      HotoriConfig::TAIL_LIMIT_ACTIVE_LEVEL;
  const bool rightActive =
      digitalRead(HotoriConfig::TAIL_RIGHT_LIMIT_PIN) ==
      HotoriConfig::TAIL_LIMIT_ACTIVE_LEVEL;
  return (leftActive ? 1 : 0) | (rightActive ? 2 : 0);
}

void reportTailLimitStopped(uint8_t state) {
  if (state == 3) {
    Serial.println(F("[TAIL LIMIT] Both active; Motor1 stopped and disabled."));
  } else if (state == 1) {
    Serial.println(F("[TAIL LIMIT] Left active; Motor1 stopped and disabled."));
  } else if (state == 2) {
    Serial.println(F("[TAIL LIMIT] Right active; Motor1 stopped and disabled."));
  }
}

void serviceTailLimit() {
  const uint8_t state = readTailLimitState();

  if (state != 0) {
    // Stop before logging; never wait for debounce or check direction.
    motor1.stop();
    if (state != tailLimitStateReported) {
      reportTailLimitStopped(state);
    }
  }

  tailLimitStateReported = state;
}

bool rejectMotor1StartIfTailLimitActive() {
  const uint8_t state = readTailLimitState();
  if (state == 0) {
    return false;
  }

  motor1.stop();
  if (state != tailLimitStateReported) {
    reportTailLimitStopped(state);
  }
  tailLimitStateReported = state;
  Serial.println(F("[TAIL LIMIT] Active; Motor1 start rejected."));
  return true;
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  m1f  - Motor 1 forward"));
  Serial.println(F("  m1r  - Motor 1 reverse"));
  Serial.println(F("  m1s  - Motor 1 stop and disable"));
  Serial.println(F("  m2f  - Motor 2 forward"));
  Serial.println(F("  m2r  - Motor 2 reverse"));
  Serial.println(F("  m2s  - Motor 2 stop and disable"));
  Serial.println(F("  p1   - Play /mp3/0001.mp3"));
  Serial.println(F("  p2   - Play /mp3/0002.mp3"));
  Serial.println(F("  stop - Stop DFPlayer playback"));
  Serial.println(F("  help - Show this list"));
  Serial.println(F("Set the Serial Monitor line ending to Newline or Both NL & CR."));
}

void handleCommand(const char* command) {
  if (strcmp(command, "m1f") == 0) {
    if (rejectMotor1StartIfTailLimitActive()) {
      return;
    }
    motor1.startForward();
    Serial.println(F("[Motor 1] Forward."));
  } else if (strcmp(command, "m1r") == 0) {
    if (rejectMotor1StartIfTailLimitActive()) {
      return;
    }
    motor1.startReverse();
    Serial.println(F("[Motor 1] Reverse."));
  } else if (strcmp(command, "m1s") == 0) {
    motor1.stop();
    Serial.println(F("[Motor 1] Stopped and driver disabled."));
  } else if (strcmp(command, "m2f") == 0) {
    motor2.startForward();
    Serial.println(F("[Motor 2] Forward."));
  } else if (strcmp(command, "m2r") == 0) {
    motor2.startReverse();
    Serial.println(F("[Motor 2] Reverse."));
  } else if (strcmp(command, "m2s") == 0) {
    motor2.stop();
    Serial.println(F("[Motor 2] Stopped and driver disabled."));
  } else if (strcmp(command, "p1") == 0) {
    dfPlayer.playMp3(1);
  } else if (strcmp(command, "p2") == 0) {
    dfPlayer.playMp3(2);
  } else if (strcmp(command, "stop") == 0) {
    dfPlayer.stop();
  } else if (strcmp(command, "help") == 0) {
    printHelp();
  } else {
    Serial.print(F("[ERROR] Unknown command: "));
    Serial.println(command);
    Serial.println(F("        Enter 'help' to show available commands."));
  }
}

void setup() {
  Serial.begin(HotoriConfig::DEBUG_BAUD_RATE);

  pinMode(HotoriConfig::TAIL_LEFT_LIMIT_PIN, INPUT_PULLUP);
  pinMode(HotoriConfig::TAIL_RIGHT_LIMIT_PIN, INPUT_PULLUP);
  motor1.begin();
  motor2.begin();
  Serial.println(F("\nHotori hardware test starting..."));
  Serial.println(F("[OK] Motor pins initialized; both drivers are disabled."));
  Serial.println(F("[INFO] STEP/DIR mode has no driver feedback; verify motor response physically."));

  serviceTailLimit();
  if (tailLimitStateReported == 0) {
    Serial.println(F("[OK] Tail left/right limits ready (normal LOW)."));
  }

  dfPlayer.begin(Serial1, Serial);
  printHelp();
}

void loop() {
  // Each service is short and non-blocking, so both motors, audio events, and
  // new commands continue to be processed independently.
  serviceTailLimit();
  motor1.update();
  motor2.update();
  dfPlayer.update();

  const char* command = commandReader.poll(Serial);
  if (command != nullptr) {
    handleCommand(command);
  }
}
