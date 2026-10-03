# Hotori / 夢卵

Hotori（夢卵）は、猫をモチーフにしたデバイスです。体温、呼吸、鼓動、尻尾、鳴き声などを組み合わせ、生き物らしい安心感を作ることを目指しています。

現在は**ハードウェア単体確認フェーズ**です。Arduino Mega 2560にTMC2209とステッピングモータを2組、DFPlayer Mini、スピーカー、microSDを接続し、最低限の実機動作を確認しています。呼吸、尻尾、鼓動、体温、タッチ反応などを統合した全体制御は今後実装する予定です。

## 実機動作確認時の基準コミット

- 実機確認に使用したGitコミット: `6a1bf426384656001892b058b7bc52fbb8ebdc62`
- コミットメッセージ: `Add dual motor and DFPlayer hardware test`
- 使用マイコン: Arduino Mega 2560
- 開発形式: Arduinoスケッチ
- 現在の主スケッチ: `hotori_hardware_test/hotori_hardware_test.ino`

この文書では、確認状況を次のように区別します。

- **実機確認済み**: 接続した実機で動作を確認した内容
- **現在の実装**: ソースコードから確認できる内容
- **予定**: まだ実装していない内容

## 現在接続しているハードウェア

### 実機確認済み

- Arduino Mega 2560
- TMC2209 × 2
- ステッピングモータ × 2
- DFPlayer Mini
- スピーカー
- microSDカード

### 予定

- 呼吸機構
- 尻尾機構
- LRAによる鼓動
- ヒーターとサーミスタによる体温制御
- タッチセンサ／圧力センサ

## TMC2209とステッピングモータ

### Megaとの制御配線

現在の実機と`HotoriConfig.h`で一致しているピン割り当てです。

| 対象 | TMC2209信号 | Arduino Mega 2560 |
|---|---|---:|
| Motor1 | STEP | D8 |
| Motor1 | DIR | D9 |
| Motor1 | EN | D10 |
| Motor2 | STEP | D22 |
| Motor2 | DIR | D23 |
| Motor2 | EN | D24 |

ENはLOWアクティブです。

| ENレベル | 状態 |
|---|---|
| LOW | ドライバ有効。モータを励磁する |
| HIGH | ドライバ無効。モータ軸がフリーになる |

### 使用中モジュールの物理配置

実機で使用しているTMC2209モジュールには青いヒートシンクがあり、次の向きで確認しています。

- 左側: 黄色ピンヘッダ
- 右側: 黒ピンヘッダ
- EN側が上
- DIR側が下

部品面を正面から見た配置メモ:

```text
                 上
    黄色ピン       青い        黒ピン
    ヘッダ       ヒートシンク    ヘッダ

    EN   ●       ┌─────┐       ●
         ●       │     │       ●
         ●       │     │       ●
         ●       │     │       ●
    STEP ●       └─────┘       ●   ← 下から2番目
    DIR  ●                       ●   ← 下端

                 下
```

左側制御ピンは、この向きで次の位置です。

- 上端: EN
- 下から2番目: STEP
- 下端: DIR

**注意:** 実機では、この物理ピン位置を正しく接続したことでモータが回転しました。シルク印刷やピン位置を思い込みで判断せず、使用しているモジュール本体の表示と照合してください。

### 電源、GND、モータ出力

現在の実機構成は次のとおりです。

| TMC2209側 | 接続先 |
|---|---|
| VIO | Mega 5V |
| ロジックGND | Mega GND |
| VM / VMOT | モータ用外部電源 |
| モータ電源GND | Mega GNDと共通 |
| 1A / 1B | モータの一方のコイルペア |
| 2A / 2B | モータのもう一方のコイルペア |

モータ用外部電源の仕様、TMC2209の電流設定値、モータ固有の定格は、このリポジトリのコードには記録されていません。実機の定格と設定を別途確認してください。

### 現在の制御方式

TMC2209のUART設定は使用していません。STEP、DIR、ENの3信号で制御しています。そのため、ドライバの接続状態やエラー状態をMegaから読み取ることはできません。

`StepDirMotor`クラスは`micros()`を使い、`delay()`でメインループを止めずにSTEP出力を更新します。Motor1とMotor2は別々のインスタンスであり、独立して開始、方向変更、停止できます。

現在の`HotoriConfig.h`の設定値:

```cpp
constexpr uint32_t MOTOR_STEP_HALF_PERIOD_US = 5000UL;
```

STEP出力は5,000µs以上ごとにHIGHとLOWを反転します。1パルスは約10,000µsなので、現在の診断用速度は**約100 step/s**です。ループ処理時間による小さな遅れは発生し得ます。

過去には半周期500µs、約1,000 step/sで試験しました。停止状態から加速なしで開始する速度として高い可能性を切り分けるため、現在は約100 step/sへ落としています。

## DFPlayer Mini

### Megaとの配線

Arduino Mega 2560のハードウェアシリアル`Serial1`を使用します。

| Arduino Mega 2560 | 接続 | DFPlayer Mini |
|---|---|---|
| D18 / TX1 | 1kΩ程度の直列抵抗を介して接続 | RX |
| D19 / RX1 | 直接接続 | TX |
| 5V | 電源 | VCC |
| GND | 共通GND | GND |

通信速度は9,600bpsです。

### スピーカー配線

スピーカーはDFPlayer Miniの`SPK1`と`SPK2`の間へ接続します。

```text
DFPlayer SPK1 ─── スピーカー ─── DFPlayer SPK2
```

**注意:** `SPK1`と`SPK2`はスピーカー用の差動出力です。どちらもGNDへ接続しません。

### 標準DFPlayer Miniのピン配置メモ

部品面を見てmicroSDスロットを下にした場合の配置です。現在使用する主な端子はVCC、RX、TX、GND、SPK1、SPK2です。

| 左列 | ピン | 右列 | ピン |
|---:|---|---:|---|
| 1 | VCC | 9 | IO1 |
| 2 | RX | 10 | GND |
| 3 | TX | 11 | IO2 |
| 4 | DAC_R | 12 | ADKEY1 |
| 5 | DAC_L | 13 | ADKEY2 |
| 6 | SPK2 | 14 | USB+ |
| 7 | GND | 15 | USB- |
| 8 | SPK1 | 16 | BUSY |

モジュールの製造元や互換品によって外観が異なる可能性があるため、実物のシルク印刷も確認してください。

### microSDのファイル構成

```text
mp3/
├── 0001.mp3
└── 0002.mp3
```

統合テストではDFRobotDFPlayerMiniライブラリの`playMp3Folder()`を使用します。

- `playMp3Folder(1)` → `/mp3/0001.mp3`
- `playMp3Folder(2)` → `/mp3/0002.mp3`

`hotori_hardware_test`の初期化成功時の音量は20です。DFPlayer単体確認用の`dfplayer_min_test`では音量15を設定し、起動後に`0001.mp3`を自動再生します。

## 開発環境と依存ライブラリ

2026年10月3日に現在の環境で確認したバージョンです。

| 項目 | バージョン |
|---|---:|
| Arduino CLI | 1.5.1 |
| Arduino AVR Boards | 1.8.8 |
| DFRobotDFPlayerMini | 1.0.6 |

DFRobotDFPlayerMiniが未導入の場合:

```powershell
arduino-cli lib install "DFRobotDFPlayerMini"
```

## テストプログラム

### hotori_hardware_test

現在の主テストです。Motor1、Motor2、DFPlayerを同じスケッチから確認できます。モータのSTEP更新、DFPlayerイベント処理、Serialコマンド受付を`loop()`から順番に呼び出しています。

Serial Monitorへコマンドを1行ずつ送信します。行末は`Newline`または`Both NL & CR`を使用してください。

| コマンド | 動作 |
|---|---|
| `m1f` | Motor1 Forward。ドライバを有効化して正転 |
| `m1r` | Motor1 Reverse。ドライバを有効化して逆転 |
| `m1s` | Motor1 Stop。STEPを停止してドライバを無効化 |
| `m2f` | Motor2 Forward。ドライバを有効化して正転 |
| `m2r` | Motor2 Reverse。ドライバを有効化して逆転 |
| `m2s` | Motor2 Stop。STEPを停止してドライバを無効化 |
| `p1` | `/mp3/0001.mp3`を再生 |
| `p2` | `/mp3/0002.mp3`を再生 |
| `stop` | DFPlayerの再生を停止 |
| `help` | コマンド一覧を表示 |

### dfplayer_min_test

DFPlayer Miniだけを確認する独立スケッチです。モータ処理は含みません。

1. Serialを115,200bps、Serial1を9,600bpsで開始
2. DFPlayerの起動を1,500ms待機
3. `dfPlayer.begin(Serial1)`を実行
4. 初期化成功時に音量15を設定
5. さらに500ms待機
6. `/mp3/0001.mp3`を自動再生
7. `loop()`でDFPlayerのエラーイベントを表示

### mega_step_test

開発初期の1モータ単体確認用スケッチとして残しています。このスケッチは`EN=D8`、`STEP=D9`、`DIR=D10`を定義しており、**現在の実機配線とは一致しません**。現在の構成確認には使用せず、履歴資料として扱ってください。

## Serial Monitor

`hotori_hardware_test`の`DEBUG_BAUD_RATE`は115,200bpsです。

接続ポートを確認します。

```powershell
arduino-cli board list
```

表示されたMega 2560のポートを指定してMonitorを開きます。

```powershell
arduino-cli monitor -p COMx -c baudrate=115200
```

`COMx`は例です。実際のCOM番号はPCや接続状態によって変わるため、必ず`board list`の結果を使用してください。アップロード前にはArduino IDEや別のSerial Monitorを閉じ、ポートを解放します。

## コンパイルと書き込み

Arduino Mega 2560のFQBNは`arduino:avr:mega`です。以下はリポジトリ直下で実行します。

### 統合ハードウェアテスト

```powershell
arduino-cli compile --fqbn arduino:avr:mega hotori_hardware_test
arduino-cli upload -p COMx --fqbn arduino:avr:mega hotori_hardware_test
```

### DFPlayer単体テスト

```powershell
arduino-cli compile --fqbn arduino:avr:mega dfplayer_min_test
arduino-cli upload -p COMx --fqbn arduino:avr:mega dfplayer_min_test
```

アップロードはコンパイル成功後に行います。`COMx`は`arduino-cli board list`でMega 2560として認識されたポートへ置き換えてください。

## 実機確認済み

以下はコード上の想定だけではなく、現在の実機で確認済みです。

- Motor1 Forward
- Motor1 Reverse
- Motor1 Stop
- Motor2 Forward
- Motor2 Reverse
- Motor2 Stop
- Stop時にENが無効となり、保持トルクが解除されること
- DFPlayer Miniの起動
- `/mp3/0001.mp3`の再生
- `/mp3/0002.mp3`の再生
- Mega 2560、TMC2209 × 2、ステッピングモータ × 2、DFPlayer Miniを接続した最低限の同時構成

TMC2209はUARTを使用していないため、ドライバ自身の状態確認は実際のモータ動作によって行っています。

## トラブルシュート

### モータが固定されるが回転しない

実際に発生した症状:

- `m1f`／`m2f`などのForwardコマンドで保持トルクだけが入る
- `m1s`／`m2s`などのStopコマンドで軸がフリーになる
- モータは回転しない

今回の原因は、TMC2209モジュール側の物理ピン位置の認識違いでした。上記のモジュール配置に従ってEN、STEP、DIRの実際の位置を確認し、対応するMegaピンへ正しく接続したことで回転しました。

同じ症状が出た場合は、まずMega側のピン番号だけでなく、TMC2209側で配線した物理端子が本当にEN、STEP、DIRか確認してください。

### モータが熱くなる

ステッピングモータはドライバが有効で励磁されている間、停止中でも発熱します。実機でもForward中に保持トルクが入り、発熱することを確認しています。

`m1s`または`m2s`を送ると、対応するENを無効化して軸をフリーにします。異常に熱くなる場合は次を確認してください。

- TMC2209の電流設定
- 長時間の連続励磁
- 1A/1B、2A/2Bのコイルペア
- モータと電源の定格
- 冷却状態

### DFPlayerがnot readyになる

実機ではDFPlayerの電源配線を修正した後に動作しました。初期化に失敗する場合は次を確認してください。

- VCCが正しく5Vへ接続されているか
- MegaとDFPlayerのGNDが共通か
- Mega TX1からDFPlayer RX、DFPlayer TXからMega RX1へクロス接続されているか
- Mega TX1とDFPlayer RXの間に1kΩ程度の直列抵抗があるか
- microSDが認識され、`mp3/0001.mp3`と`mp3/0002.mp3`が存在するか
- `Serial1`を9,600bpsで使用しているか
- DFPlayerの電源投入後、初期化まで十分な待ち時間があるか

DFPlayerだけを切り分ける場合は`dfplayer_min_test`を使用します。このスケッチには1,500msの起動待ちがあります。

### アップロード時にCOMポートを開けない

Arduino IDEや`arduino-cli monitor`がCOMポートを使用していると、アクセス拒否でアップロードできません。Serial Monitorを終了し、再度`arduino-cli board list`でポートを確認してからアップロードします。

## リポジトリ構成

```text
tail/
├── .gitignore
├── dfplayer_min_test/
│   └── dfplayer_min_test.ino
├── hotori_hardware_test/
│   ├── hotori_hardware_test.ino
│   ├── HotoriConfig.h
│   ├── StepDirMotor.h
│   ├── StepDirMotor.cpp
│   ├── DfPlayerController.h
│   ├── DfPlayerController.cpp
│   ├── SerialCommandReader.h
│   ├── SerialCommandReader.cpp
│   └── README.md
└── mega_step_test/
    └── mega_step_test.ino
```

| ファイル | 役割 |
|---|---|
| `hotori_hardware_test/hotori_hardware_test.ino` | 各コンポーネントを生成し、初期化、Serialコマンド振り分け、毎ループの更新を行う |
| `hotori_hardware_test/HotoriConfig.h` | ピン番号、STEP周期、Serial速度、DFPlayer音量をまとめる |
| `hotori_hardware_test/StepDirMotor.h/.cpp` | TMC2209をSTEP、DIR、ENで非ブロッキング制御する |
| `hotori_hardware_test/DfPlayerController.h/.cpp` | Serial1経由でDFPlayerを初期化し、再生、停止、イベント表示を行う |
| `hotori_hardware_test/SerialCommandReader.h/.cpp` | Serial Monitorから改行区切りのコマンドを受信する |
| `dfplayer_min_test/dfplayer_min_test.ino` | DFPlayer Miniの起動、初期化、音声再生を単体確認する |
| `mega_step_test/mega_step_test.ino` | 初期の1モータ試験。現在の配線とはピン定義が異なるため履歴用 |

## 今後の予定

以下は未実装です。

1. 呼吸モータの往復制御
2. 尻尾モータの動作パターン
3. 呼吸動作中でも尻尾とDFPlayerを並行動作できる統合制御
4. タッチセンサ／圧力センサ入力との連携
5. 猫の鳴き声パターン追加
6. LRAによる鼓動
7. ヒーターとサーミスタによる体温制御
8. 展示用の状態管理
9. 異常時の安全停止

機能を追加する際も、モータ、音声、センサ処理が互いを長時間停止させない構成を維持します。
