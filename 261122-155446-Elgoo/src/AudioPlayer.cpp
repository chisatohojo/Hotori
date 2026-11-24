
#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

#include "AudioPlayer.h"

static DFRobotDFPlayerMini player;
static HardwareSerial dfPlayerSerial(2);

static bool playerAvailable = false;

void setupAudio() {
  dfPlayerSerial.begin(9600, SERIAL_8N1, 19, 18);

  Serial.println("Starting DFPlayer...");
  delay(100);

  if (!player.begin(dfPlayerSerial)) {
    Serial.println("DFPlayer connection failed");
    playerAvailable = false;
    return;
  }

  playerAvailable = true;
  player.volume(20);

  Serial.println("DFPlayer ready");
}

void playVoice(uint8_t voiceId) {
  if (!playerAvailable) return;
  if (voiceId < 1 || voiceId > 6) return;

  Serial.print("Play DFPlayer: ");
  Serial.println(voiceId);

  player.play(voiceId);
}

bool isAudioConnected() {
  return playerAvailable;
}
// #include <Arduino.h>
// #include <LittleFS.h>
// #include <BluetoothA2DPSource.h>
// #include "MP3DecoderHelix.h"
// #include "AudioPlayer.h"

// using namespace libhelix;

// static BluetoothA2DPSource a2dp;
// static MP3DecoderHelix mp3;

// static const size_t BUFFER_FRAMES = 4096;
// static Frame *pcmData = nullptr;

// static size_t writePos = 0;
// static size_t readPos = 0;

// static portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED;

// static volatile bool playRequest = false;
// static volatile bool playing = false;
// static volatile uint8_t requestedVoiceId = 0;

// // MP3をPCMへデコード
// static void dataCallback(

//     MP3FrameInfo &info,
//     int16_t *pcm,
//     size_t len,
//     void *ref
// ) {
//     static bool printed = false;

//     if (!printed) {
//         Serial.printf(
//             "MP3 FORMAT: %d Hz, %d channels, %d bits\n",
//             info.samprate,
//             info.nChans,
//             info.bitsPerSample
//         );
//         printed = true;
//     }
//     if (info.samprate != 44100 ||
//         info.nChans != 2 ||
//         info.bitsPerSample != 16) {
//         return;
//     }

//     size_t count = len / 2;

//     for (size_t i = 0; i < count; i++) {
//         while (true) {
//             bool hasSpace;

//             portENTER_CRITICAL(&audioMux);
//             hasSpace = (writePos - readPos < BUFFER_FRAMES);
//             portEXIT_CRITICAL(&audioMux);

//             if (hasSpace) break;
//             vTaskDelay(1);
//         }

//         size_t index = writePos % BUFFER_FRAMES;

//         // 音量を小さくする
//         pcmData[index].channel1 = pcm[i * 2] / 5;
//         pcmData[index].channel2 = pcm[i * 2 + 1] / 5;

//         portENTER_CRITICAL(&audioMux);
//         writePos++;
//         portEXIT_CRITICAL(&audioMux);
//     }
// }

// // BluetoothへPCMを送信
// static int32_t getAudioFrames(Frame *frames, int32_t count) {
//     portENTER_CRITICAL(&audioMux);

//     for (int32_t i = 0; i < count; i++) {
//         frames[i].channel1 = 0;
//         frames[i].channel2 = 0;

//         if (readPos < writePos) {
//             frames[i] = pcmData[readPos % BUFFER_FRAMES];
//             readPos++;
//         }
//     }

//     portEXIT_CRITICAL(&audioMux);

//     return count;
// }

// // MP3デコード専用タスク
// static void decodeTask(void *parameter) {
//     uint8_t buffer[1024];

//     while (true) {
//         if (playRequest) {
//             uint8_t voiceId = requestedVoiceId;

//             playRequest = false;
//             playing = true;

//             String filename;

//             if (voiceId >= 1 && voiceId <= 5) {
//                 filename = "/" + String(voiceId) + ".mp3";
//             } else if (voiceId == 6) {
//                 filename = random(0, 2) == 0
//                     ? "/6a.mp3"
//                     : "/6b.mp3";
//             } else {
//                 playing = false;
//                 continue;
//             }

//             portENTER_CRITICAL(&audioMux);
//             writePos = 0;
//             readPos = 0;
//             portEXIT_CRITICAL(&audioMux);

//             File file = LittleFS.open(filename, "r");

//             if (file && !file.isDirectory()) {
//                 Serial.print("Playing: ");
//                 Serial.println(filename);
//                 Serial.print("File size: ");
//                 Serial.println(file.size());

//                 mp3.begin();

//                 Serial.println("Decoder started");

//                 while (file.available()) {
//                     size_t n = file.read(
//                         buffer, sizeof(buffer)
//                     );

//                     if (n > 0) {
//                         mp3.write(buffer, n);
//                     }

//                     vTaskDelay(1);
//                 }

//                 file.close();
//                 Serial.println("Decode finished");
//             } else {
//                 Serial.print("File not found: ");
//                 Serial.println(filename);
//             }

//             // 残ったPCMの送信完了を待つ
//             while (true) {
//                 bool empty;

//                 portENTER_CRITICAL(&audioMux);
//                 empty = (readPos >= writePos);
//                 portEXIT_CRITICAL(&audioMux);

//                 if (empty) break;

//                 vTaskDelay(pdMS_TO_TICKS(10));
//             }

//             playing = false;
//             Serial.println("Playback finished");
//         }

//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

// // 初期化
// void setupAudio() {
//     if (!LittleFS.begin(false)) {
//         Serial.println("LittleFS ERROR");
//         return;
//     }

//     pcmData = (Frame *)malloc(
//         BUFFER_FRAMES * sizeof(Frame)
//     );

//     if (!pcmData) {
//         Serial.println("Memory allocation failed");
//         return;
//     }

//     randomSeed(esp_random());

//     mp3.setDataCallback(dataCallback);

//     a2dp.set_data_callback_in_frames(getAudioFrames);
//     a2dp.set_volume(5);

//     Serial.println("Connecting to SoundCore 2...");
//     a2dp.start("SoundCore 2");

//     xTaskCreatePinnedToCore(
//         decodeTask,
//         "MP3Decode",
//         8192,
//         NULL,
//         1,
//         NULL,
//         1
//     );

//     Serial.println("Audio system READY");
// }

// // 再生指示
// void playVoice(uint8_t voiceId) {
//     if (voiceId < 1 || voiceId > 6) return;

//     if (!playing && !playRequest) {
//         requestedVoiceId = voiceId;
//         playRequest = true;
//     }
// }

// bool isAudioConnected() {
//     return a2dp.is_connected();
// }