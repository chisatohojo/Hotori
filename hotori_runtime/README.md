# Hotori runtime / 本番制御

`hotori_runtime/`は、ESP32のUART指示でArduino Mega 2560の音声・呼吸・尻尾を動かす本番制御用スケッチです。`hotori_hardware_test/`はデバッグ・単体試験用として残しています。既存の3スケッチや設定ファイルは変更していません。

一次仕様はプロジェクト直下の[`nekocat_ESP32_Arduino_UART_spec_v0.3.xlsx`](../nekocat_ESP32_Arduino_UART_spec_v0.3.xlsx)です。依頼文の`(1)`付きファイル名とは異なりますが、v0.3の4シートと5サンプルを読み取り、記載内容が依頼の通信仕様と一致することを確認しました。

## 接続

| 接続先 | Mega 2560 | 設定・注意 |
|---|---|---|
| ESP32 TX | RX2 / D17 | `Serial2`、115200 bps、8N1 |
| ESP32 GND | GND | 共通GND |
| 未使用 | TX2 / D16 | ESP32側に接続不要。ACK等を送信しない |
| USB / デバッグPC | `Serial` | 115200 bps、ログ出力専用 |
| DFPlayer RX | TX1 / D18 | `Serial1`、9600 bps。既存配線どおり約1kΩの直列抵抗を介す |
| DFPlayer TX | RX1 / D19 | `Serial1`、9600 bps |
| DFPlayer VCC / GND | 5V / GND | 既存配線どおり |
| Motor1 / Tail STEP, DIR, EN | D8, D9, D10 | ENはLOWで有効、HIGHで無効 |
| Motor2 / Rib STEP, DIR, EN | D22, D23, D24 | ENはLOWで有効、HIGHで無効 |
| Tail Left NC | D25 | `INPUT_PULLUP`、COM→GND |
| Tail Right NC | D26 | `INPUT_PULLUP`、COM→GND |

左右のスイッチのCOM側GNDは共通で構いません。LOWは通常、HIGHはリミット作動または断線です。スピーカーはDFPlayerのSPK1とSPK2の間へ接続します。

既存の正式資料[`docs/PIN_ASSIGNMENT.md`](../docs/PIN_ASSIGNMENT.md)とMotor、Limit、DFPlayerのピン番号・極性が一致しています。Serial2のD17は今回のruntimeで追加する受信配線です。既存資料に記載されたデバッグスケッチの「リミット解除後も自動再始動しない」動作はそのままで、runtimeの左右交互モードのみ、以下の条件で反対向きに再開します。

## UARTフレーム

バイナリ9 byte固定です。HEX文字列や改行を送る方式ではありません。一方向通信、ACKなし、動作変更時のみ送信します。無通信は正常で、通信タイムアウトによる自動停止はありません。

| Byte | フィールド | 値 |
|---|---|---|
| 1 | HEADER1 | `AA` |
| 2 | HEADER2 | `55` |
| 3 | FLAGS | `01`～`07`。bit0=VOICE、bit1=BREATH、bit2=TAIL |
| 4 | VOICE | 0=停止、1～7=音声ID、8～255=未対応（VOICE更新時はフレーム全体を拒否） |
| 5 | BREATH | 0=停止、1～60=回/分、61以上=無効 |
| 6 | TAIL_DIR | 0=停止、1=左、2=右、3=左右交互、4以上=無効 |
| 7 | TAIL_SPEED | 0=停止、1～100=速度%、101以上=無効 |
| 8 | TAIL_TIME | 0=停止、1～255=0.1秒単位 |
| 9 | CRC8 | Byte 3～8の6 byteから計算 |

FLAGS=0、予約bit 3～7の使用、CRC不一致、更新対象グループ内の無効値は、フレーム全体を破棄します。有効性の確認が終わるまで、どのグループにも反映しません。

FLAGSで指定していないグループは検証せず、状態も変更しません。たとえばFLAGS=`04`のVOICE/BREATHが0でも音声や呼吸を停止しません。ESP32側は未指定フィールドを0で送信してください。受信途中の破損、欠落、ノイズからは次の`AA 55`を再検出して復帰します。正しいフレームのペイロード中に現れた`AA 55`で受信を途中リセットしません。

CRCはCRC-8/SMBUS相当です。

- polynomial=`07`、init=`00`、RefIn=false、RefOut=false、xorout=`00`
- FLAGS～TAIL_TIMEの計6 byteを対象とする。ヘッダとCRC自身は除外する。
- 不一致フレームは完全に破棄する。

| ケース | 仕様の9 byteサンプル | CRC確認 |
|---|---|---|
| 全項目更新 | `AA 55 07 02 14 03 32 1E 10` | `10`一致 |
| 音声のみ | `AA 55 01 03 00 00 00 00 8F` | `8F`一致 |
| 呼吸のみ | `AA 55 02 00 14 00 00 00 6D` | `6D`一致 |
| 尻尾のみ | `AA 55 04 00 00 03 32 1E 90` | `90`一致 |
| 全停止 | `AA 55 07 00 00 00 00 00 DF` | `DF`一致 |

## 起動と音声

起動時はMotor1/Motor2のENを最初にHIGHへ設定し、STEPをLOWにします。音声・呼吸・尻尾の指示状態は全停止です。ESP32の有効フレームを受信するまではモータの駆動や音声再生を開始しません。

DFPlayerには起動時に停止コマンドを送ります。既存単体試験と同じ1500 msの起動猶予を、タイマによる状態遷移として設けています。その後、現在の音量20を設定し、再度停止コマンドを送ってから最後に受信した音声指示だけを実行します。この間もUART受信、リミット、モータ、時間制限の処理は動きます。実際にモジュールが停止指示を受け付ける時期は実機確認が必要です。

使用音声は次の7ファイルです。VOICE=0は停止、1～7は対応するファイルを再生します。VOICE=8～255は未対応IDとして、VOICEがFLAGSで指定されたフレーム全体を拒否します。TAILやBREATHも更新しません。VOICEがFLAGSで指定されていなければ、VOICEフィールドの値によらず音声状態を変更しません。

```text
/mp3/0001.mp3
/mp3/0002.mp3
/mp3/0003.mp3
/mp3/0004.mp3
/mp3/0005.mp3
/mp3/0006.mp3
/mp3/0007.mp3
```

既存のDFRobotDFPlayerMini 1.0.6には、初期化・ACK待ち・ACK無効送信でも待機処理が含まれます。runtimeの`DfPlayerController`は、そのライブラリと同じ10 byteのDFPlayerコマンド形式を直接生成し、チェックサムを付け、TXバッファの空きを確認して送信します。ライブラリのACK無効経路と同じ10 msの送信間隔をタイマで確保し、`delay()`や応答待ちは使いません。メディア・再生完了・エラーの応答は別途受信してログに出します。ログの「Command output enabled」は送信準備の完了を意味し、実機応答の確認済みを意味しません。

音声は最新VOICE指示を優先します。有効なVOICEを受信するたびに以前の未送信指示を破棄し、最後の1件だけを保持します。再生中も新しいIDへ切り替え、同じIDでも再生コマンドを再送して先頭から再生し直します。ただし、送信前にさらに指示を受けた場合は、同じIDの指示も最後の1件へまとめます。VOICE=0は古い再生予定を消して停止します。起動待機中やTXバッファ不足中も最後の指示だけを残し、ESP32受信は音声待ちで止めません。

音声の長さによる待機はせず、再生時間はコードへ登録しません。再生完了・ACK応答も待ちません。物理UARTへ送信済みのコマンドは取り消せないため、最新指示は10 msの送信間隔と10 byte分のTX空きを満たす次の送信可能タイミングで反映します。新しいフレームが成立したループでは、その指示を反映してからDFPlayer送信を処理します。UART受信は引き続き1ループ1 byteのため、未受信・検証途中の指示を先読みしません。長時間の過剰送信によるRXバッファ溢れは防げないため、通常仕様の「動作変更時のみ送信」を前提とします。

## 尻尾

`HotoriRuntimeConfig.h`の`TAIL_LEFT_USES_FORWARD=true`は暫定値です。実機確認後に必要なら反転してください。この値だけで左右とDIRレベルの対応を逆転できます。

速度は次のように線形変換します。

```text
steps/s = 250 + (TAIL_SPEED - 1) × (1000 - 250) / 99
STEP半周期[µs] = 500000 / steps/s
```

TAIL_SPEED=1～100%が有効で、1%は250 steps/s、50%は約621.21 steps/s、100%は1000 steps/sです。途中の速度も整数steps/sへ丸めず、1～100%の各値を個別の半周期へ変換します。実際のSTEP出力にはループ処理と`micros()`の分解能による遅れが含まれます。遅れを取り戻すための連続パルスは出しません。

| STEP速度 | STEP半周期 |
|---|---:|
| 250 steps/s（1%） | 2000 µs |
| 500 steps/s（34%） | 1000 µs |
| 約621 steps/s（50%） | 804 µs（整数切り捨て） |
| 1000 steps/s（100%） | 500 µs |

従来の10倍速のため、加減速なしの起動・速度変更・交互反転で脱調、振動、機構への衝撃が増える可能性があります。低い速度から実負荷で確認し、ドライバ電流・電源・マイクロステップ設定・発熱、左右Limitの停止距離と脱力後の挙動を確認してください。最大速度ではUART受信・音声切替・ログ出力を同時に行い、STEP半周期とジッタ、停止応答を実測してください。`micros()`による非ブロッキングSTEP生成を維持し、今回は加減速を追加していません。Motor2 / Breathの速度・校正値は変更していません。

DIR/SPEED/TIMEは一括更新します。どれか1つでも0なら停止し、ENを無効化します。新しいTail指示は古い動作・待機状態・期限を上書きします。TIMEは`値×100 ms`で、1=0.1秒、10=1秒、30=3秒、255=25.5秒です。`millis()`の符号なし差分で期限を判定し、周回にも対応します。

- DIR=1/2: 左/右へ移動し、時間満了またはどちらかのLimitがHIGHになった時点で停止・EN無効化。LimitがLOWに戻っても、新しいTail指示がなければ再始動しません。
- DIR=3: 初動は左です。どちらかのLimitがHIGHになったら即停止・EN無効化し、両方がLOWへ戻るまで待機します。両方LOWになり、TIMEが残っていれば反対方向へ再開します。
- 指示受信時にLimitがHIGHなら駆動しません。DIR=1/2はその指示を終了し、DIR=3は解放待ちへ移ります。この場合も初動の左を反転した右が次の方向となります。
- 待機中もTIMEを消費します。脱力してもLimitが解放されない機構では、期限まで待機して終了します。
- Limitの停止は、方向・ログ出力・デバウンスより優先します。動作中のLimitを押し切る制御は行いません。交互切替用の時間定数も設けていません。

状態変化は`[TAIL LIMIT] ... active; motor disabled.`、`Waiting for both limits LOW`、`Limits released; moving in the opposite direction`、`Time expired; stopped.`などのログで確認できます。

## 呼吸と実機校正

`BREATH_STEPS_PER_CYCLE=0`を初期値としています。実機の校正値はプロジェクト内に記録されていないため、Motor2は駆動しません。BREATH=1～60の指示は保持し、初めて受信したときに次のログを1回出します。

```text
[BREATH] Calibration required; Motor2 remains disabled.
```

機構が完成したら、現在のドライバのマイクロステップ設定で、完全な1呼吸を作るSTEP数を測定し、`HotoriRuntimeConfig.h`の値を変更して再ビルドしてください。

```text
steps/s = BREATH_STEPS_PER_CYCLE × breathsPerMinute / 60
STEP半周期[µs] = 30000000 / (BREATH_STEPS_PER_CYCLE × breathsPerMinute)
```

この実装はMotor2を一方向へ連続回転させ、リンク・カムなど機構側で呼吸周期を作る方式です。回転方向は`BREATH_USES_FORWARD`で指定します。モータ自体の往復駆動が必要な機構では、原点・ストローク・反転条件が追加で必要です。未定義の値や反転パターンは実装していません。

次のBREATH更新まで設定を保持します。BREATH=0は必ずSTEP停止・EN無効化します。未校正時も停止指示を受け付けます。

## ビルドと検証

Arduino CLI 1.5.1、Arduino AVR Boards 1.8.6で確認しています。runtimeはDFRobotライブラリに依存しません。既存のデバッグスケッチのビルドスクリプトは変更していないため、プロジェクト直下から直接実行します。

```powershell
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 --warnings all --build-path build/hotori_runtime hotori_runtime
```

ホスト検証は、実際のruntime C++と`.ino`を、GPIO・時刻・UARTのテスト用実装と合わせてコンパイルして動かします。PythonとC++11対応のホスト用g++が必要です。ArduinoのAVR用コンパイラはホスト実行には使えません。

```powershell
# g++がPATHにある場合
python hotori_runtime/tests/run_tests.py

# g++の場所を明示する場合
python hotori_runtime/tests/run_tests.py --cxx 'C:/path/to/g++.exe'
```

テスト出力は`build/hotori_runtime_tests/`へ置きます。テスト用コードは`tests/`配下に隔離し、Arduinoのruntimeスケッチには組み込みません。CRCの5サンプル、不正値、再同期、未指定グループ保持、フレームの一括検証、全速度値、時間換算、時刻周回、起動時disable、リミットと交互動作、呼吸校正、音声再送、ログ・音声送信の非ブロッキング動作を検証します。

ログはUSBのTX空きを確認し、1ループ最大32 byteずつ出力します。ログが溢れた場合は行単位で破棄し、制御を止めません。`RuntimeLog::droppedLines()`で破棄数を参照できます。毎ループのログや無通信エラーは出しません。

変更ファイル一覧、検証結果と残る実機確認項目は[`VALIDATION.md`](VALIDATION.md)を参照してください。最新の変更検証と過去のruntime実装時の記録を区別して記載しています。Megaへのuploadと実機動作確認は別途実施します。
