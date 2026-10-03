# Hotori hardware test

Arduino Mega 2560向けの、TMC2209 2台とDFPlayer Miniの単体確認用スケッチです。
既存の `mega_step_test` とは独立しています。

## 必要なライブラリ

- DFRobotDFPlayerMini

Arduino IDEのライブラリマネージャ、またはArduino CLIでインストールします。

```text
arduino-cli lib install "DFRobotDFPlayerMini"
```

## Serial Monitor

- ボーレート: 115200
- 行末: Newline または Both NL & CR

使用可能なコマンドは起動時に表示されます。`help` でも再表示できます。

## 注意

- TMC2209のENはLOWアクティブとして設定しています。
- `m1s` / `m2s` はSTEPを停止し、対応するドライバを無効化します。
- 回転速度は `HotoriConfig.h` の `MOTOR_STEP_HALF_PERIOD_US` で調整できます。
- DFPlayerのファイルは `/mp3/0001.mp3` と `/mp3/0002.mp3` に配置してください。
