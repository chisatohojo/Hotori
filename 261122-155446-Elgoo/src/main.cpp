#include <Arduino.h>

#include "CatCommand.h"
#include "TouchSensor.h"
#include "AudioPlayer.h"



const unsigned long SLEEP_TIMEOUT = 10000;

unsigned long lastTouchTime = 0;
bool isSleeping = false;
bool returnToSleep = false;
unsigned long returnToSleepTime = 0;
void onTouchDetected();
TouchSensor headSensor{27, 30, false, 0, false, false, 0, 0};
TouchSensor chinSensor{14, 30, false, 0, false, false, 0, 0};
TouchSensor cheekLSensor{12, 30, false, 0, false, false, 0, 0};
TouchSensor cheekRSensor{13, 30, false, 0, false, false, 0, 0};
TouchSensor backSensor{02, 30, false, 0, false, false, 0, 0};
TouchSensor hipSensor{15, 30, false, 0, false, false, 0, 0};
void headShort() {

    // 睡眠中に触られた場合
    if (isSleeping) {
        Serial.println("【睡眠中】頭を触られた");

        // 起きて一瞬目を開ける
        sendCatCommand(
            1, 15, 0, 0, 0,
            90, 35, 255
        );

        playVoice(1);

        // 3秒後に再び眠る予約
        returnToSleep = true;
        returnToSleepTime = millis() + 3000;

        onTouchDetected();
        return;
    }

    // 通常時
    onTouchDetected();
    Serial.println("【頭】短くタッチ");

    sendCatCommand(
        1, 20, 3, 100, 2,
        90, 40, 1
    );

    playVoice(1);
}


void headDummy() {}

void headLong() {
  onTouchDetected();
  Serial.println("【頭】長押し");
  sendCatCommand(2, 15, 0, 0, 0, 0, 15, 2);
  playVoice(2);
}

void hiphit() {
  onTouchDetected();
  Serial.println("【おしり】強くたたく");
  sendCatCommand(6, 25, 1, 100, 2, 0, 0, 255);
  playVoice(6);

}

void hiptap() {
  onTouchDetected();
  Serial.println("【おしり】弱くたたく");
  sendCatCommand(3, 25, 3, 100, 2, 0, 15, 1);
  playVoice(3);
}

void backShort() {
  onTouchDetected();
  Serial.println("【背中】サッと1回");
  sendCatCommand(4, 20, 3, 50, 2, 0, 10, 1);
  playVoice(4);
}

void backCont() {
  onTouchDetected();
  Serial.println("【背中】連続撫で");
  sendCatCommand(0, 15, 3, 10, 2, 0, 0, 2);
  
}

void backLong() {
  onTouchDetected();
  Serial.println("【背中】長押し");
  sendCatCommand(5, 12, 0, 0, 0, 0, -15, 0);
  playVoice(5);
}


void cheekL() {
    onTouchDetected();
    Serial.println("【左頬】すりすり");

    sendCatCommand(
        0, 20, 1, 10, 2,
        60, 30, 1
    );
}

void cheekR() {
    onTouchDetected();
    Serial.println("【右頬】嫌がって顔を逸らす");

    sendCatCommand(
        0, 25, 2, 80, 2,
        60, 30, 255
    );
}


void chin() {
  onTouchDetected();
  Serial.println("【あご】タッチ！");
  sendCatCommand(3, 15, 0, 0, 0, 0, 20, 2);
  playVoice(3);
}


void onTouchDetected() {
    lastTouchTime = millis();

    if (isSleeping) {
        isSleeping = false;
        Serial.println("起床");
    }
}

void updateSleepState() {
    if (!isSleeping &&
        millis() - lastTouchTime >= SLEEP_TIMEOUT) {

        isSleeping = true;
        Serial.println("睡眠状態に移行");
        sendCatCommand(0, 12, 0, 0, 0, 0, -10, 0);

        // 睡眠用UART信号は後で追加
    }
}

void updateReturnToSleep() {

    if (!returnToSleep) return;

    if ((long)(millis() - returnToSleepTime) >= 0) {

        Serial.println("【睡眠】再び眠る");

        sendCatCommand( 0, 12, 0, 0, 0, 90, 20, 0
        );

        isSleeping = true;
        returnToSleep = false;
    }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 16, 17);
  delay(1000);
  setupAudio();
  delay(3000);
  playVoice(2);
  lastTouchTime = millis();
  Serial.println("猫ロボット・センサー制御システム起動");
}

void loop() {
//   if (!isAudioConnected()) {
//       delay(100);
//       return;
//   }
  serviceCatCommand(Serial1);
  updatehipSensor(hipSensor, hiphit, hiptap);
  update1StageSensor(chinSensor, chin);
  update1StageSensor(cheekLSensor, cheekL);
  update1StageSensor(cheekRSensor, cheekR);
  update3StageSensor(backSensor, backShort, backCont, backLong);
  update3StageSensor(headSensor, headShort, headDummy, headLong);
  updateSleepState();
  updateReturnToSleep();
  delay(200);
}
