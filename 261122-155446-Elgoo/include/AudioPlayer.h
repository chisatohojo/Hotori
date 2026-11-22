
#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>

// Bluetooth音声再生の初期化
void setupAudio();

// 指定番号の音声を再生
// 1〜5：対応するMP3
// 6：6a.mp3 / 6b.mp3 をランダム再生
void playVoice(uint8_t voiceId);
bool isAudioConnected();
#endif
