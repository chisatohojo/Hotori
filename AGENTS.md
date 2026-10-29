# Hotori 作業ガイド

## プロジェクト

- ユーザーへの説明とドキュメントは日本語を基本とする。コードの識別子と既存のSerialログは英語を維持する。
- Hotori（夢卵）はArduino Mega 2560向けのデバイス。現在の対象はハードウェア単体確認で、統合した呼吸・尻尾・鼓動・体温・タッチ制御は今後の実装。
- 主スケッチは`hotori_hardware_test/hotori_hardware_test.ino`。
- 開発手順は`README.md`、操作方法と実機確認履歴は`hotori_hardware_test/README.md`、正式な配線は`docs/PIN_ASSIGNMENT.md`を参照する。

## 環境と検証

- Windows / PowerShell、Arduino CLI 1.5.1、Arduino AVR Boards 1.8.6、DFRobotDFPlayerMini 1.0.6を使用する。
- FQBNは`arduino:avr:mega:cpu=atmega2560`。megaAVRコアはMega 2560用ではない。
- 主スケッチの変更後は、プロジェクト直下で次を実行する。

  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
  ```

- 他のスケッチや共通ビルド設定も変更した場合は、同コマンドに`-Sketch all`を付けて検証する。出力は`build/<スケッチ名>/`。
- 依存関係が不足する場合は`scripts/setup.ps1`を使う。ユーザー共通のArduino環境を使用するため、無関係なコアやライブラリを更新しない。
- コンパイル成功、コード上の確認、今回の実機確認、過去の実機確認を区別して報告する。コンパイルだけで実機動作を確認済みとしない。
- この作業フォルダに`.git`がない場合、Git履歴やコミット状態を推測しない。

## ハードウェアの前提

- ピン番号、STEP周期、通信速度、音量などの設定は`hotori_hardware_test/HotoriConfig.h`に集約する。
- Motor1 / Tail: STEP=D8、DIR=D9、EN=D10。
- Motor2 / Rib: STEP=D22、DIR=D23、EN=D24。
- ENはLOWで有効、HIGHで無効。停止時はSTEPを停止し、ENを無効化する。
- Tail LimitはLeft=D25、Right=D26。NC接点と`INPUT_PULLUP`を使い、HIGHを押下／断線として扱う。
- 左右どちらかがHIGHなら方向によらずMotor1を直ちに停止する。デバウンス待ちを入れず、HIGH中は両方向の開始を拒否し、LOWに戻っても自動再始動しない。
- DFPlayerは`Serial1`、9600 bps。USBのSerial Monitorは115200 bps、改行区切り。TMC2209のUART制御は現在使用していない。
- 配線やピン割り当てを変更する依頼では、設定コードと配線資料、関連READMEを揃える。
- `mega_step_test`は旧配線の履歴用。現在の構成へ流用しない。
- 実機への書き込み、Serial Monitor接続、モータ・音声の操作コマンド送信は、その操作がユーザーの依頼に含まれる場合にのみ行う。COMポートは固定せず、ボードを確認して指定する。

## 実装方針

- 既存の小さなクラス構成を維持する。モータは`StepDirMotor`、音声は`DfPlayerController`、受信は`SerialCommandReader`が担当する。
- `loop()`の処理は短く保ち、`micros()`等による非ブロッキング処理を使う。モータ、音声、入力の更新を長い`delay()`や待機ループで止めない。
- AVRのメモリ制約を考慮し、既存の固定長バッファと`F()`によるログ文字列の格納方針に合わせる。
- UTF-8、スペース2文字を基本とし、PowerShellは4文字。詳細は`.editorconfig`に従う。
- 機能やSerialコマンドを変えたら、対応するREADMEも更新する。
