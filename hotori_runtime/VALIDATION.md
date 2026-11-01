# 実装・検証記録

## 2026年11月1日: 音声7種類・最新指示優先・Tail 10倍速

今回もMegaへのupload、Serial Monitor接続、実機コマンド送信、commit、pushは行っていません。

### 開始状態と既存変更の保全

- 作業対象は `Hotori-main/hotori_runtime/`。作業フォルダと親に `.git` がなく、`git status --short` はexit 128。ステージ状態・未コミット変更・ローカルHEADは判定できません。
- GitHub接続で最新mainを確認: [`81fdc82692b339374c1f47397c3d23f866b382e4`](https://github.com/chisatohojo/Hotori/commit/81fdc82692b339374c1f47397c3d23f866b382e4)（`Create readme.md`）。取得したツリーと編集前の45ファイルをGit blob SHAで比較し、全件一致（CRLF/LF差のみ許容）。main側の `hardware/cad/README.md` とCADファイルはローカルにありません。同期・上書きはしていません。
- 編集前の全ファイルを `C:/Users/megum/AppData/Local/Temp/hotori-before-20261101-115247/project/` へ退避。編集後はSHA-256で照合し、既存ファイルの削除なし、runtime外の既存ファイルの変更なしを確認しました。3つのデバッグ用スケッチ、ピン資料、通信仕様書XLSXはそのままです。
- 元フォルダでの `git diff --check` は `.git` 不在で利用できません。退避先の `diff-review/` に一時Gitインデックスを作り、編集前を登録した後、編集後との `git diff --check` を実行して成功。比較用コピーのみLFへ正規化し、作業フォルダのGit初期化やcommitはしていません。

### 変更ファイル

すべて `hotori_runtime/` 内です。

| ファイル | 変更 |
|---|---|
| `HotoriRuntimeConfig.h` | 音声最大ID=7、Tail=250～1000 steps/s、旧キュー容量を削除 |
| `UartFrameReceiver.cpp` | 選択されたVOICE=8～255を従来の一括検証で拒否 |
| `DfPlayerController.h` / `.cpp` | 16件FIFOを最新1件の保留へ変更。送信成功時だけ保留解除 |
| `hotori_runtime.ino` | キュー満杯時のUART保留を削除。フレーム反映後にDFPlayerを更新 |
| `TailController.cpp` | 速度補間コメントのみ更新。補間式・リミット処理は維持 |
| `tests/test_runtime.cpp` | 音声範囲・最新優先・高速Tail・統合安全停止の試験を拡張 |
| `tests/fakes/Arduino.h` | UART読取直前に安全停止を検証するテスト用フック |
| `README.md` / `VALIDATION.md` | 動作仕様、リスク、今回の結果を記録 |

生成物はMega用が `%TEMP%/hotori-runtime-build/`、ホスト用が既存の無視対象 `build/hotori_runtime_tests/`。ソース変更は上記10ファイルです。

### コンパイルとホスト試験

```powershell
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 --warnings all --build-path "$env:TEMP/hotori-runtime-build" hotori_runtime
python hotori_runtime/tests/run_tests.py --cxx "$env:TEMP/hotori-test-toolchain/w64devkit/bin/g++.exe"
```

- Mega 2560 compile成功。実環境はArduino CLI **1.4.1**、Arduino AVR Boards **1.8.6**。プログラム **8856 / 253952 bytes**、SRAM **1103 / 8192 bytes**。runtime警告なし。Arduinoコア `new.cpp` の未使用引数警告4件のみです。
- 退避した変更前の既存runtime試験は **488 assertions成功**。変更後の拡張試験は **2372 assertions成功**。ホストコンパイラは一時フォルダへ展開した [w64devkit 2.11.0](https://github.com/skeeto/w64devkit/releases/tag/v2.11.0) のg++ **16.2.0**。システムPATHやArduinoコアを更新していません。
- 実際のruntime `.ino` と `.cpp` を `-std=c++11 -Wall -Wextra -Werror -pedantic` で検証。GPIO・時刻・UARTはモックで、音声が実際に鳴ることや実機の応答時間を確認した結果ではありません。

| 検証項目 | 結果 |
|---|---|
| VOICE全256値 | 0～7受理、8～255拒否、未選択なら全値を無視、後続フレームへ復帰 |
| 不正VOICEの全体拒否 | 有効なTail/Breath停止値を含めても一切反映せず、以前の音声保留も保持 |
| 全対応IDの送信と同一ID再受信 | 0は停止コマンド、1～7は指定IDの再生コマンド。同じIDも送信後の再受信で再送 |
| 再生中の別ID | 再生終了通知なしでも10 ms後に切替コマンドを送信 |
| 起動待機中の上書き | 最後のIDだけ再生。40件以上の指示後に0なら停止のみ |
| TX不足中の上書き・停止 | 未送信の古い指示を破棄、UART消費を継続、空き9 byteでは送信せず10 byteで送信 |
| 最新フレーム成立と送信期限の一致 | 同ループで新VOICEを反映してから送信、旧保留は送信しない |
| 古い指示・遅延完了通知 | 時間経過や旧曲の再生完了通知で、破棄した指示を再生しない |
| Tail 1～100% | 線形補間と半周期の整数切り捨てを全値で確認。1%=250、50%≈621、100%=1000 steps/s |
| STEPエッジ | 250/500/1000 steps/sの半周期2000/1000/500 µs、期限未満で変化なし |
| 左右Limit | D25/D26、INPUT_PULLUP、HIGH即停止。方向1/2/3・速度1/50/100%でUART読取より先にSTEP=LOW、EN=HIGH |
| 従来Tail停止条件 | 単方向の非自動再始動、交互動作の両LOW待ち・反転、待機中の期限満了、停止値、時刻周回を維持 |
| Motor2 | 既存試験成功。未校正抑止、校正時の速度計算、停止・指示保持を維持。ソース変更なし |
| UART・CRC・FLAGS | 9 byte固定、CRC-8/SMBUSの既存5サンプル、破損・欠落からの再同期、一括拒否、未指定保持を維持 |
| 非ブロッキング・通信 | 送信間隔10 ms、TX空き確認、ログ上限を維持。delay/応答待ち追加なし、Serial2送信なし、無通信停止なし |

### 今回残る実機試験

1. `/mp3/0001.mp3`～`0007.mp3` の実再生、再生中の別ID切替、同一IDの先頭再生、0停止、起動直後の最新指示反映。DFPlayerは送信済みコマンドを取り消せないため、実際の音の切替遅延も確認すること。
2. 250～1000 steps/sで低い速度から負荷試験。加減速なしの起動・速度変更・交互反転による脱調、振動、機構への衝撃、電源・ドライバ電流・マイクロステップ設定・発熱を確認すること。
3. 最大速度で左右Limit作動・断線時のSTEP停止とEN無効化、停止距離、脱力・Limit解放後の交互再開と時間満了を確認すること。
4. UART受信・音声切替・USBログ同時動作でSTEP周期とジッタ、実速度、リミット停止応答をロジックアナライザ等で実測すること。ループ遅延時は追い付きパルスを出さず、指示より遅くなる可能性があること。
5. Motor2は既存の `BREATH_STEPS_PER_CYCLE=0` を維持。実機校正・駆動確認は今回実施していません。

---

## 過去の記録（2026年10月29日、変更前仕様）

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
