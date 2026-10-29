# 実装・検証記録

検証日: 2026年10月29日。以下はGit履歴を接続してコミットする前のruntime実装時の記録です。この検証では、実機への書き込みやSerial Monitorへの接続、commit、pushは行っていません。

## 新規ファイル一覧

ソース・ドキュメント・テストの追加先は`hotori_runtime/`のみです。生成物は既存の`.gitignore`対象である`build/hotori_runtime/`と`build/hotori_runtime_tests/`へ出力しました。

```text
hotori_runtime/
├── hotori_runtime.ino
├── HotoriRuntimeConfig.h
├── UartFrameReceiver.h
├── UartFrameReceiver.cpp
├── StepDirMotor.h
├── StepDirMotor.cpp
├── TailController.h
├── TailController.cpp
├── BreathController.h
├── BreathController.cpp
├── DfPlayerController.h
├── DfPlayerController.cpp
├── RuntimeLog.h
├── RuntimeLog.cpp
├── README.md
├── VALIDATION.md
└── tests/
    ├── run_tests.py
    ├── test_runtime.cpp
    └── fakes/
        ├── Arduino.h
        └── Arduino.cpp
```

## 設計上の判断

- ESP32はSerial2のRX2/D17で受信し、Serial1のDFPlayerと分離。Serial2への送信は行わない。
- CRCとFLAGSで選択された全フィールドを確認してから指示を公開。不正フレームの一部分だけを反映しない。未選択フィールドは検証・反映しない。
- フレーム候補の失敗時は、その内部や末尾に残る次のヘッダ候補を保存して再同期する。正しいペイロード中の`AA 55`は許容する。
- UARTは1ループ1 byte、DFPlayer応答も1 byteに制限。ログ出力も空き確認付きの固定予算で処理し、入力やログの集中でリミット監視を占有しない。
- STEPは`micros()`、Tail期限は`millis()`で生成・判定する。いずれも符号なしの時刻差分で周回を扱う。
- 交互動作は左から開始し、Limitで停止・EN無効化、両Limit解放を待って反転する。解除待ちの間もTIMEを消費する。固定時間による方向切替は実装しない。
- DFPlayerは既存ライブラリのブロッキング処理を避けるため、互換コマンドを直接組み立てる。音量20、非ブロッキング起動猶予1500 ms、送信間隔10 ms。繰り返しのVOICE IDもキューに追加する。
- 呼吸は校正値0で駆動を抑止し、指示値は保持。校正後のMotor2は機構側で呼吸周期を作る連続回転を前提とする。未定義のストロークや反転周期は作らない。

## 検証結果

### Mega 2560コンパイル

```powershell
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 --warnings all --build-path build/hotori_runtime hotori_runtime
```

Arduino CLI 1.5.1、Arduino AVR Boards 1.8.6で成功しました。

| 項目 | 結果 |
|---|---:|
| プログラム領域 | 9042 / 253952 bytes |
| グローバル変数 / SRAM | 1121 / 8192 bytes |
| 警告 | Arduinoコア`new.cpp`の未使用引数4件 |
| runtimeソースの警告 | なし |

### CRCと受信・制御のホスト検証

実際のC++ソースと`.ino`をホスト用g++ 5.3.0で、`-std=c++11 -Wall -Wextra -Werror -pedantic`を指定してコンパイル・実行しました。GPIO・時刻・UARTのみテスト用実装へ置き換えています。結果は**488 assertions成功**です。実機の電気的・機械的動作を示す結果ではありません。

| 検証 | 結果 |
|---|---|
| XLSXと依頼文のCRCサンプル5件 | `10 / 8F / 6D / 90 / DF`、全件一致 |
| CRC-8/SMBUS標準確認値`123456789` | `F4`一致 |
| 正常9 byte、途中受信 | 9 byte揃うまで未反映、成立後に公開 |
| CRC不正、FLAGS=0、予約bit | 拒否、前の指示に変更なし |
| VOICE=255、BREATH≥61、TAIL_DIR≥4、TAIL_SPEED≥101 | 選択されたグループではフレーム全体を拒否 |
| 停止値を含むTailグループ内の別フィールド不正 | 停止として受理せず、フレームを拒否 |
| 未指定グループの0、範囲外値、ヘッダ状バイト | 検証せず、従来状態を保持 |
| ノイズ、ヘッダ不正、AAの連続、途中欠落、重複ヘッダ候補 | 後続の正常フレームへ復帰 |
| Tail速度1～100% | 全値の半周期が単調変化、1%=25、100%=100 steps/s |
| Tail TIME 1/10/30/255 | 100/1000/3000/25500 msへ変換 |
| Tail期限の上書き、無通信中の期限、millis周回 | 期限で停止・EN無効化 |
| STEPのmicros周回と遅延後の更新 | 周回後も更新、パルスの連続追い付きなし |
| 起動時のMotor1/Motor2 | STEP=LOW、EN=HIGH。EN出力設定前にHIGHをラッチ |
| 左右Limitと両回転方向 | STEP停止→EN無効化、単方向指示の自動再開なし |
| 左右交互、片方のみの解除、待機中の時間満了 | 両LOWでのみ反転再開、期限後は再開なし |
| 校正0のBREATH、繰り返し指示、停止 | 値保持、駆動なし、校正ログ1回、停止時disable |
| テスト用の仮校正値で呼吸速度計算 | 計算・継続・停止を確認。実設定は0のまま |
| DFPlayerの音量、停止・再生フレーム、チェックサム | 指定値と形式に一致 |
| 同じVOICE IDの連続受信、キュー満杯 | 毎回別指示として送信、満杯時は保留して制御ループを継続 |
| DFPlayer/ログTXに空きがない場合 | 待機せず復帰、ログ過多は行単位で破棄 |
| 無通信中のBREATH保持、Serial2のACK | 無通信停止なし、Serial2 TXデータなし |

### 既存変更の保全と差分確認

- `git status --short`はexit 128、`git diff --check`はexit 129。このフォルダに`.git`がないため、Gitの未commit変更や履歴に基づく比較はできません。
- 作業開始時に、既存22ファイルのSHA-256を記録して照合しました。内容変更・欠落は0件、runtimeとビルド生成物以外の新規ファイルは0件です。
- `hotori_hardware_test/`、`dfplayer_min_test/`、`mega_step_test/`のすべての既存ファイルが開始時と同じハッシュです。ローカル変更を破棄・上書きしていません。
- 代替として、新規20ファイルを空ファイルとの差分として`git -c core.autocrlf=false diff --no-index --check`で検査し、成功しました。`-c`はこの実行だけの設定で、既存のGit設定は変更していません。末尾空白、競合マーカー、最終改行、runtime内の`delay()`/`delayMicroseconds()`呼び出しとSerial2送信の検査も実施しました。

## 残る実機確認

1. ESP32 TX→Mega RX2/D17と共通GNDで、115200 8N1のバイナリフレームを正しく受信すること。
2. `TAIL_LEFT_USES_FORWARD`と実際の左右の対応。必要なら設定値を反転すること。
3. 25～100 steps/sでの脱調・振動、実際の移動範囲、左右Limitの停止・脱力・断線時の挙動。
4. 交互動作で脱力後にLimitが両LOWへ戻れるか。戻らない場合は待機し、期限で終了すること。
5. タイマ周回以外の、実負荷下でのSTEP周期・受信遅延・停止応答時間。
6. 呼吸機構が連続回転方式かを確認し、1周期のSTEP数を実測。実測前は`BREATH_STEPS_PER_CYCLE=0`を維持すること。
7. DFPlayerの起動停止、音量20、各`/mp3/XXXX.mp3`、同一ID再生、停止、メディア・エラーログを確認すること。
8. 起動直後の実ピンレベルとドライバ無効状態、無通信で状態を維持する動作、Tail期限による停止を確認すること。
