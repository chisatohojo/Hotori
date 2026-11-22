
#include <Arduino.h>
#include "CatCommand.h"

namespace {

struct CatCommand {
    uint8_t voice;
    uint8_t breath;
    uint8_t tailDir;
    uint8_t tailSpeed;
    uint8_t tailTime;
    int8_t neckLR;
    int8_t neckUD;
    uint8_t eyes;
};

CatCommand pendingCommand = {};
CatCommand lastSentCommand = {};

bool hasPendingCommand = false;
bool hasSentCommand = false;

unsigned long lastSendMillis = 0;

// 前回の指示と同じか比較
bool sameCommand(const CatCommand& a,
                 const CatCommand& b) {
    return a.voice == b.voice &&
           a.breath == b.breath &&
           a.tailDir == b.tailDir &&
           a.tailSpeed == b.tailSpeed &&
           a.tailTime == b.tailTime &&
           a.neckLR == b.neckLR &&
           a.neckUD == b.neckUD &&
           a.eyes == b.eyes;
}

// CRC-8/SMBus
uint8_t crc8Smbus(const uint8_t* data,
                  size_t length) {
    uint8_t crc = 0;

    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80)
                ? static_cast<uint8_t>(
                    (crc << 1) ^ 0x07)
                : static_cast<uint8_t>(crc << 1);
        }
    }

    return crc;
}

} // namespace

// 送信指示を登録
void sendCatCommand(
    uint8_t voice,
    uint8_t breath,
    uint8_t tailDir,
    uint8_t tailSpeed,
    uint8_t tailTime,
    int8_t neckLR,
    int8_t neckUD,
    uint8_t eyes
) {
    CatCommand next = {
        voice,
        breath,
        tailDir,
        tailSpeed,
        tailTime,
        neckLR,
        neckUD,
        eyes
    };

    if (hasSentCommand &&
        sameCommand(next, lastSentCommand)) {
        hasPendingCommand = false;
        return;
    }

    pendingCommand = next;
    hasPendingCommand = true;
}

// loop()から定期的に呼ぶ
void serviceCatCommand(HardwareSerial& uart) {
    if (!hasPendingCommand) return;

    unsigned long now = millis();

    // 前回送信から2秒間隔を空ける
    if (hasSentCommand &&
        now - lastSendMillis < 2000UL) {
        return;
    }

    uint8_t frame[12] = {
        0xAA,
        0x55,
        0x1F,
        pendingCommand.voice,
        pendingCommand.breath,
        pendingCommand.tailDir,
        pendingCommand.tailSpeed,
        pendingCommand.tailTime,
        static_cast<uint8_t>(pendingCommand.neckLR),
        static_cast<uint8_t>(pendingCommand.neckUD),
        pendingCommand.eyes,
        0x00
    };

    // Byte 3〜11の9バイトをCRC対象にする
    frame[11] = crc8Smbus(&frame[2], 9);

    uart.write(frame, sizeof(frame));

    Serial.print("送信: ");

    for (uint8_t i = 0; i < sizeof(frame); ++i) {
        Serial.printf("%02X ", frame[i]);
    }

    Serial.println();

    lastSentCommand = pendingCommand;
    hasSentCommand = true;
    hasPendingCommand = false;
    lastSendMillis = now;
}
