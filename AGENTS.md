# Hotori 作業ガイド

## プロジェクト

- ユーザーへの説明とドキュメントは日本語を基本とする。コードの識別子と既存のSerialログは英語を維持する。
- Hotori（夢卵）はArduino Nano（ATmega328P）向けのデバイス。ESP32からのUARTフレームで頭・しっぽ・呼吸・まぶたを動かす。現在はハードウェア確認フェーズ。
- 主スケッチは`hotori_hardware_test/hotori_hardware_test.ino`。
- 開発手順は`README.md`、操作方法は`hotori_hardware_test/README.md`、正式な配線は`docs/PIN_ASSIGNMENT.md`、ESP32との通信仕様は`docs/ESP32_UART_PROTOCOL.md`を参照する。
- 使用しないボードの情報はドキュメントに書かない。

## 環境と検証

- Windows / PowerShell、PlatformIO（`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`）、TMCStepper 0.7.3、Servo 1.2.2を使用する。
- 主スケッチの変更後は、プロジェクト直下で次を実行する。

  ```powershell
  & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nano_new
  ```

- 依存ライブラリは`platformio.ini`の`lib_deps`で管理する。無関係なライブラリを更新しない。
- コンパイル成功、コード上の確認、今回の実機確認、過去の実機確認を区別して報告する。コンパイルだけで実機動作を確認済みとしない。
- この作業フォルダに`.git`がない場合、Git履歴やコミット状態を推測しない。

## ハードウェアの前提

- ピン番号、STEP周期、通信速度、角度、電流などの設定は`hotori_hardware_test/HotoriConfig.h`に集約する。
- Motor1 / 頭の左右: STEP=D3、DIR=D4。Motor2 / 頭の上下: STEP=D5、DIR=D6。Motor3 / しっぽ: STEP=D7、DIR=D8。いずれもTMC2209のUARTでStealthChop（静音）、電流、1/8マイクロステップを設定する。回転方向はDIRピン。
- Motor4 / 肋骨1: STEP=D9、DIR=D10。Motor5 / 肋骨2: STEP=D11、DIR=D12。UARTは使わない。
- ENピンは使わない。5台ともTMC2209のENをGNDへ接続する。Motor1〜3の出力ON/OFFはUART（TOFF）で切り替える。Motor4/5は常に通電する。
- TMC2209のUARTはSoftwareSerial（RX=A0直結、TX=A1から1kΩ経由、57600 bps）で3台のPDN_UARTを共有する。アドレスはMotor1=0（MS1 GND / MS2 GND）、Motor2=1（VIO / GND）、Motor3=2（GND / VIO）。
- リミットスイッチは抵抗なしでGNDへ接続し、`INPUT_PULLUP`で読む。頭は押すとつながる（NO）2個の並列で押下=LOW。しっぽは押すと切れる（NC）で押下=HIGH（断線もHIGHで停止側）。
- 頭の左右（A2）と上下（A3）は、それぞれ両端の2個を並列にした1入力。押された端は移動方向で判断し、直ちに停止して角度をその端に補正する。押されたスイッチから離れる向きの移動（最大20°）だけは許可し、押されたスイッチへ向かう開始は拒否する。起動時にmax側のスイッチで原点合わせをする。
- しっぽのリミットは左（A4）と右（A5）の別々の入力。どちらかが押されたら方向によらず直ちに停止する。デバウンス待ちを入れず、押されている間は両方向の開始を拒否し、離れても自動再始動しない。
- SG90サーボは信号D2、電源スイッチD13（HIGH=ON）。
- ESP32のTXはD0（USBシリアルと共用）。115200 bps。行頭の`AA`をフレーム、それ以外をテキストコマンドとして扱う。
- 配線やピン割り当てを変更する依頼では、設定コードと配線資料、関連READMEを揃える。
- `hotori_runtime`、`dfplayer_min_test`、`mega_step_test`は旧構成のスケッチ。現在の構成へ流用しない。
- 実機への書き込み、Serial Monitor接続、モータの操作コマンド送信は、その操作がユーザーの依頼に含まれる場合にのみ行う。COMポートは固定せず、ボードを確認して指定する。

## 実装方針

- 既存の小さなクラス構成を維持する。UART制御のMotor1〜3は`UartStepMotor`、Motor4/5は`StepDirMotor`、頭の角度制御は`HeadAxis`、しっぽは`TailController`、サーボは`ServoController`、ESP32フレームは`EspFrameReceiver`、テキストコマンドは`SerialCommandReader`が担当する。
- `loop()`の処理は短く保ち、`micros()`等による非ブロッキング処理を使う。モータ、入力の更新を長い`delay()`や待機ループで止めない。
- AVRのメモリ制約を考慮し、既存の固定長バッファと`F()`によるログ文字列の格納方針に合わせる。
- UTF-8、スペース2文字を基本とし、PowerShellは4文字。詳細は`.editorconfig`に従う。
- 機能やSerialコマンドを変えたら、対応するREADMEも更新する。
