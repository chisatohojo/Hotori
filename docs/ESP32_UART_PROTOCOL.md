# ESP32 → Arduino UART通信仕様 v0.4（案）

ESP32からArduino Nano（`hotori_hardware_test`）へ動作指示を送るための仕様です。v0.3（9 byte固定、音声・呼吸・尻尾のみ）を、統一した変数名（actionId〜eyeState）を送れるように拡張しました。v0.3のフレームとは長さが違うため、互換性はありません。

## 通信の基本

| 項目 | 値 |
|---|---|
| 方向 | ESP32 → Arduinoの一方向。ACKなし |
| 速度 | 115200 bps、8N1 |
| 形式 | バイナリ15 byte固定（HEX文字列や改行ではない） |
| 送信タイミング | 動作を変えるときだけ送る。無通信は正常 |

### 接続

| ESP32 | Arduino Nano | 注意 |
|---|---|---|
| TX | D0（RX） | USBと共用。Nanoへの書き込み中はESP32のTX線を外す |
| GND | GND | 共通にする |

ArduinoからESP32へは何も送りません（NanoのTXはESP32へ接続しない）。ESP32のTXは3.3V、Nanoは5Vロジックです。3.3VのHIGHはATmegaの入力しきい値（約3.0V）をわずかに上回る程度なので、受信が不安定な場合はレベル変換を入れてください。

## フレーム

| Byte | 名前 | 型 | 範囲 | 内容 |
|---:|---|---|---|---|
| 1 | HEADER1 | uint8 | `AA` | 固定 |
| 2 | HEADER2 | uint8 | `55` | 固定 |
| 3 | FLAGS | uint8 | 下表 | 反映するグループ |
| 4 | actionId | uint8 | 1〜9 | タッチ判定の種類 |
| 5 | catState | uint8 | 0〜3 | 猫の現在の状態 |
| 6 | voiceId | uint8 | 0〜6 | 再生する音声番号。0 = 停止 |
| 7 | breathRate | uint8 | 0〜60 | 呼吸速度（回/分）。0 = 停止 |
| 8 | tailDir | uint8 | 0〜3 | しっぽの方向。0 = 停止、1 = 左、2 = 右、3 = 左右交互 |
| 9 | tailSpeed | uint8 | 0〜100 | しっぽの速度（%）。0 = 停止 |
| 10 | tailTime | uint8 | 0〜255 | しっぽの動作時間（0.1秒単位）。0 = 停止 |
| 11 | neckLR | **int8** | -90〜+90 | 首の左右角度（°）。負 = 左 |
| 12 | neckUD | **int8** | -30〜+30 | 首の上下角度（°）。負 = 下 |
| 13 | neckSpeed | uint8 | 0〜100 | 首の動作速度（%）。0 = 停止 |
| 14 | eyeState | uint8 | 0〜1 | 両目。0 = 閉、1 = 開 |
| 15 | CRC8 | uint8 | | Byte 3〜14（12 byte）から計算 |

neckLR / neckUDは2の補数の符号付き1 byteです（例: -30 = `E2`、-15 = `F1`、+45 = `2D`）。

### FLAGS

| bit | 値 | グループ | 対象の変数 |
|---:|---|---|---|
| 0 | `01` | VOICE | voiceId |
| 1 | `02` | BREATH | breathRate |
| 2 | `04` | TAIL | tailDir, tailSpeed, tailTime |
| 3 | `08` | NECK | neckLR, neckUD, neckSpeed |
| 4 | `10` | EYE | eyeState |
| 5 | `20` | STATE | actionId, catState |
| 6〜7 | | 予約 | 必ず0 |

- FLAGSで選んだグループだけを反映します。選んでいないグループの値は検査せず、Arduino側の状態も変えません。
- 選んでいないグループの値は0で送ってください。
- 同じグループの変数は一括で更新します。たとえば首だけ速度を変えたいときも、neckLR / neckUD / neckSpeedの3つをすべて送ります。

### CRC8

CRC-8/SMBUS相当です（v0.3と同じ計算方法）。

- polynomial = `07`、init = `00`、RefIn = false、RefOut = false、xorout = `00`
- 対象はFLAGS〜eyeStateの12 byte。ヘッダとCRC自身は含めない。

```c
uint8_t crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}
```

## 不正なフレーム

次の場合、フレーム全体を破棄し、どのグループにも反映しません。

- CRC不一致
- FLAGS = 0、または予約bit（6〜7）が1
- 選んだグループに範囲外の値がある（例: STATEでactionId = 0、NECKでneckUD = 40）
- フレームの途中で受信が100 ms途切れた

破棄したフレームは、ArduinoのUSBログに`[ESP ERROR] RX=... - 理由`として表示します。破損やノイズのあとは、次の`AA 55`から受信を再開します。

## Arduino側の動作

| グループ | 動作 |
|---|---|
| VOICE | ログに表示するだけ（音声出力はまだない） |
| BREATH | 肋骨2台（Motor4 / Motor5）を連続回転。速度は1呼吸あたりのSTEP数（`BREATH_STEPS_PER_CYCLE`）× 回/分 から計算。0で停止 |
| TAIL | しっぽ（Motor3）を左／右／左右交互に、tailTime × 0.1秒動かす。交互は`TAIL_SWING_HALF_PERIOD_MS`（500 ms）ごとに反転。どれか1つでも0なら停止 |
| NECK | 頭の左右（Motor1、neckLR）と上下（Motor2、neckUD）を指定角度へ動かす。neckSpeedは両方に共通。0でその場に停止 |
| EYE | SG90サーボをまぶたの閉／開の角度へ動かす（電源OFFなら先にON） |
| STATE | ログに表示するだけ（今は動作に使わない） |

首の角度は、起動時のリミットスイッチでの原点合わせ（ホーミング）を基準にします。原点合わせが終わる前に受け取った角度は、終わってから実行します。

指示できる範囲より機構の可動範囲が狭い場合は、スイッチの手前（2°）までに制限して動かします。現在の設定では、neckLRは実質±60.8°までです（スイッチ間の実測125.6°）。

## フレームの例

| 内容 | 15 byte |
|---|---|
| 全グループ（actionId 3、catState 1、voiceId 2、呼吸20回/分、しっぽ交互・50%・3.0秒、首 左右-30°・上下+10°・50%、目 開） | `AA 55 3F 03 01 02 14 03 32 1E E2 0A 32 01 02` |
| STATEのみ（actionId 1、catState 0） | `AA 55 20 01 00 00 00 00 00 00 00 00 00 00 8E` |
| 音声のみ（voiceId 3） | `AA 55 01 00 00 03 00 00 00 00 00 00 00 00 D6` |
| 呼吸のみ（20回/分） | `AA 55 02 00 00 00 14 00 00 00 00 00 00 00 C1` |
| しっぽのみ（交互、50%、3.0秒） | `AA 55 04 00 00 00 00 03 32 1E 00 00 00 00 82` |
| 首のみ（左右+45°、上下-15°、60%） | `AA 55 08 00 00 00 00 00 00 00 2D F1 3C 00 D4` |
| 目を開く | `AA 55 10 00 00 00 00 00 00 00 00 00 00 01 CC` |
| 目を閉じる | `AA 55 10 00 00 00 00 00 00 00 00 00 00 00 CB` |
| 全停止（音声・呼吸・しっぽ・首） | `AA 55 0F 00 00 00 00 00 00 00 00 00 00 00 72` |

PCからテスト送信するには`scripts/send_esp_frame.ps1`を使います（USB経由でそのままNanoに届きます）。

```powershell
.\scripts\send_esp_frame.ps1 -Port COM5 -Neck -NeckLR 45 -NeckUD -15 -NeckSpeed 60
```

## 未確定事項

- actionId / catStateの値の意味と、それに応じた動作
- voiceIdで鳴らす音声の出力方法（Nanoには音声モジュール用のピンが残っていない）
- 首の角度とモータのSTEP数の換算値、目の閉／開の角度、呼吸1回あたりのSTEP数（Arduino側の`HotoriConfig.h`で調整）
