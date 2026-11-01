# ESP32 UART検査ツール

PCから入力した文字コマンドを、Megaの本番制御 [`hotori_runtime`](../hotori_runtime/README.md) が受け付けるUARTフレームへ変換する検査用スケッチです。実機試験で使用した `.ino` を変更せず登録しています。

| 機器 | 役割 |
|---|---|
| PC | Arduino IDEのシリアルモニタから `v 1` などの文字コマンドを送る |
| ESP32 | 文字コマンドを9バイトのバイナリフレームへ変換し、CRCを付けて送る |
| Arduino Mega 2560 | フレームを検証し、DFPlayer・Tail・Breathへ指示を反映する |

PC → ESP32はUSB Serial、ESP32 → MegaはGPIO17からのUARTです。どちらも115200 bpsを使用します。通信・音声の実機試験結果とモータの未解決事項は [VALIDATION.md](VALIDATION.md) を参照してください。

## 必要なハードウェア

- ESP32-WROOM-32D搭載の開発ボード（CP210x USBシリアル変換器付き）
- Arduino Mega 2560（対応する `hotori_runtime` を書き込み済み）
- PC、データ通信対応USBケーブル、ジャンパ線
- 音声確認にはDFPlayer Mini・スピーカー・音声SDカード。`/mp3/0001.mp3`～`0007.mp3` を用意
- モータ確認には既存のTMC2209、モータ、左右NCリミットなど。配線は [正式資料](../docs/PIN_ASSIGNMENT.md) と [runtimeの説明](../hotori_runtime/README.md) を参照

モータ用電源をPC USBの5Vとした実機試験では、Tailの一部条件で振動・脱調が発生しました。原因は未確定です。通信試験のPASSはモータ性能の確認完了を意味しません。

## Arduino IDE 2.3.6での準備

ユーザーによる書き込み成功時の環境は **Arduino IDE 2.3.6**、**esp32 by Espressif Systems 3.3.12** です。

1. Arduino IDEの「ファイル → 基本設定（Preferences）」を開き、「追加のボードマネージャのURL」へ次の安定版URLを追加します。既存URLは残してください。

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. 左側のボードマネージャで `esp32` を検索し、**esp32 by Espressif Systems** の **3.3.12** をインストールします。導入方法は [Espressif公式手順](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) にも記載されています。
3. ESP32をUSB接続します。COMポートが出ない場合は、デバイスマネージャでCP210xの認識状態を確認し、必要に応じて [Silicon Labs公式のCP210x VCPドライバ](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers) を導入して再接続します。別のUSB変換チップを使うボードでは、そのチップ用ドライバが必要です。
4. 「ツール → ボード」から **esp32 → ESP32 Dev Module** を選択し、「ツール → ポート」でESP32のCOMポートを選びます。Megaのポートと取り違えないよう、USBの抜き差しで増減を確認します。

| 設定 | 内容 |
|---|---|
| モジュール | ESP32-WROOM-32D（従来のESP32。S3/C3などとは区別） |
| IDEのボード名 | ESP32 Dev Module |
| CLIのFQBN | `esp32:esp32:esp32` |
| ボードパッケージ | esp32 by Espressif Systems 3.3.12 |
| PSRAM | Disabled（WROOM-32D） |
| その他 | まず選択ボードの既定値を使用。Flash容量などは実ボードの仕様に合わせる |

ESP32-WROVERなどPSRAM用にGPIO17を使用する構成には、この配線をそのまま流用しないでください。[EspressifのGPIO説明](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html)も参照してください。

## 配線と電圧

配線の変更は電源を切ってから行います。

| 接続元 | 接続先 | 用途 |
|---|---|---|
| PC USB | ESP32 USB | 書き込み・文字コマンド・ESP32ログ |
| ESP32 GPIO17（TX2 / IO17） | Mega D17（RX2） | Megaへの送信、115200 bps / 8N1 |
| ESP32 GND | Mega GND | 信号基準を共通にする |
| Mega D16（TX2） | 接続しない | 本ツールではMegaからの返信を使用しない |

GPIO17はESP32の基板表示で確認してください。「端子列の17番目」を指す番号ではありません。MegaのD17はMega上のRX2です。ESP32側 `Serial2` のTXと、Mega側 `Serial2` のRXを結びます。

ESP32のGPIO信号は3.3V、Megaは5V系です。この検査では **ESP32 TX → Mega RXの一方向** で受信成功が報告されています。逆向きの **Megaの5V TXをESP32のRXへ直結しないでください**。逆方向の通信を追加する場合は、適切なレベル変換が必要です。ESP32のGPIOは5V耐性ではありません。[Espressif公式の電圧許容値](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html#what-is-the-voltage-tolerance-of-gpios-of-esp-chips)を参照してください。

開発ボードのUSB給電用5V端子と、GPIOの許容電圧は別です。GPIOや3V3端子へ5Vを入れないでください。UART確認用の追加配線はTXとGNDで、両ボードの5V端子同士を結ぶ必要はありません。

## スケッチを開いて使う

1. `hotori_esp32_uart_test/hotori_esp32_uart_test.ino` を開きます。フォルダ名と `.ino` の名前は同じです。
2. ボードが **ESP32 Dev Module**、ポートがESP32であることを確認し、「検証」でコンパイルします。
3. 他のシリアルモニタを閉じ、「書き込み」でESP32へ書き込みます。接続待ちのまま進まないボードでは、書き込み中の接続開始時にBOOTボタンを押し、接続後に離す方法を確認してください。この手順はESP32用で、Megaへこのスケッチを書き込みません。
4. シリアルモニタを開き、**115200 baud、Newline（新しい行）** を選びます。CRを無視するため **Both NL & CR** でも使用できます。改行なしではコマンドが確定しません。
5. `help` を入力して送信し、ヘルプが表示されることを確認します。起動表示は `Hotori ESP32 UART test ready.` です。

## コマンド

半角・小文字で1行ずつ入力します。数値は10進数です。`help` はローカル表示だけで、Megaへの送信は行いません。

| 入力 | 動作 | 例 |
|---|---|---|
| `v ID` | 0=音声停止、1～7=対応するMP3再生 | `v 1`、`v 7`、`v 0` |
| `b BPM` | 0=呼吸停止、1～60=毎分の呼吸指令 | `b 20`、`b 0` |
| `t DIR SPEED TIME` | Tail方向・速度・時間を一括指定 | `t 1 1 10` |
| `stop` | VOICE・BREATH・TAILを全停止 | `stop` |
| `help` | コマンド一覧を表示 | `help` |

TailのDIRは0=停止、1=左、2=右、3=左右交互。SPEEDは0=停止、1～100%。TIMEは0=停止、1～255で **0.1秒単位** です。3項目のいずれかが0ならruntimeはTailを停止します。

```text
help
v 1
v 0
b 20
b 0
t 1 1 10
t 2 1 10
t 3 10 30
t 0 0 0
stop
```

`t 1 1 10` は左へ1%で1秒、`t 2 1 10` は右へ1%で1秒、`t 3 10 30` は左右交互に10%で3秒の指令です。現在のruntimeでは1%でも **250 steps/s**、50%は約621、100%は1000 steps/sです。実機では低速・逆回転時に問題が報告されているため、これらは入力形式の例であり、安定回転を保証する条件ではありません。

範囲外の値や不正な入力には `Invalid command. Type help.` が表示されます。`v 8` はこのツール自身が拒否するため、Megaの範囲外ID拒否試験にはなりません。

## Mega本番runtimeとの対応

ESP32は `AA 55 FLAGS VOICE BREATH TAIL_DIR TAIL_SPEED TAIL_TIME CRC8` の **9バイト固定** フレームを送ります。CRC-8/SMBUSはFLAGS～TAIL_TIMEの6バイトが対象で、poly=`07`、init=`00`、非反転、xorout=`00` です。[通信仕様書v0.3](../nekocat_ESP32_Arduino_UART_spec_v0.3.xlsx) と [runtimeのフレーム説明](../hotori_runtime/README.md#uartフレーム) を参照してください。

| コマンド | FLAGS | 更新対象 |
|---|---|---|
| `v` | `01` | VOICEのみ |
| `b` | `02` | BREATHのみ |
| `t` | `04` | TAILのみ |
| `stop` | `07` | すべてを0へ更新 |

未指定グループは状態を保持します。例えば `b 0` は呼吸だけを止め、音声とTailは変更しません。runtimeはCRCと選択された全グループを検証してから反映します。

VOICEは最新指示優先で、再生中の別IDへ切り替えます。同じIDも再生し直し、0は停止します。送信前に別指示が来れば古い未送信指示は破棄されます。音声の長さによる待機はありません。

Tailは設定時間で停止します。実装上はD25/D26のどちらかがHIGHなら即停止・EN無効化し、交互モードは両方LOWへ戻り、時間が残っていれば逆方向へ再開します。ただし、左右リミット停止の完全な実機試験は未完了です。

Motor2はruntimeの `BREATH_STEPS_PER_CYCLE=0` のため未校正で駆動しません。`b 20` を受理しても、実際の呼吸動作の確認にはなりません。無通信タイムアウトによる自動停止はなく、必要なときは `stop` を送ります。

## 通信異常時の確認

| 症状 | 確認すること |
|---|---|
| ESP32のCOMが見つからない | データ対応USBケーブル、USBポート、CP210xドライバ、デバイスマネージャ |
| 書き込みできない | ESP32のボード・ポート選択、他のモニタによるポート占有、BOOTボタン |
| 表示が文字化けする | モニタが115200 baudか |
| 入力しても反応しない | Newline設定、半角小文字、末尾の改行。まず `help` でPC → ESP32だけを確認 |
| `TX:` は出るがMegaが反応しない | GPIO17 → D17/RX2、共通GND、Mega側runtime、両側115200/8N1 |
| MegaがCRC不一致を表示する | 配線接触、共通GND、ノイズ・電源変動、余分な送信機の接続を確認。9バイトをHEX文字列として直接送らない |
| UART受理ログはあるが音が出ない | DFPlayerの給電・配線、SDカードの7ファイル、スピーカー、エラーログ |
| Tailが動かない／振動する | DIR/SPEED/TIME、左右LimitのHIGH、電源・配線・ドライバ設定を切り分ける。UART受信成功だけでモータ不良の原因は確定しない |

ESP32の `TX:` はローカルの送信表示で、Megaの受信成功を保証するACKではありません。例えば `v 3` は `AA 55 01 03 00 00 00 00 8F`、`stop` は `AA 55 07 00 00 00 00 00 DF` を表示します。MegaのUSBログを見る場合は別のモニタでMegaのポートを115200 bpsで開き、`[UART] Valid frame` や `[UART ERROR]` を確認します。USBモニタの接続でボードがリセットされる場合があるため、起動完了後に指示を送り直してください。

## Arduino CLIでのコンパイル

パッケージ3.3.12を導入済みなら、リポジトリ直下から次を実行します。追加のライブラリは不要です。

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --build-path build/hotori_esp32_uart_test hotori_esp32_uart_test
```

未導入の別環境では、上記IDE手順または次のコマンドで対象パッケージだけを導入できます。既存環境の無関係なコアやライブラリは更新しません。

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

出力先 `build/` はGitの無視対象です。`scripts/build.ps1 -Sketch all` は従来のMegaデバッグスケッチ用なので、ESP32は上記コマンドで直接指定します。今回の登録作業で実機へのuploadは行いません。
