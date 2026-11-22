# Hotori / 夢卵 hardware test

Hotori（夢卵）は、猫をモチーフにしたデバイスです。体温、呼吸、鼓動、尻尾、鳴き声などを組み合わせ、生き物らしい安心感を作ることを目指しています。

このスケッチ（`hotori_hardware_test`）は、Arduino Nanoで5台のステッピングモータとサーボを動かし、ESP32からの指示フレームを受けて動作するハードウェア確認用プログラムです。

- 使用マイコン: Arduino Nano（ATmega328P）
- ビルド: PlatformIO（`platformio.ini`、環境`nano_new`）
- 正式なピンアサイン: [`docs/PIN_ASSIGNMENT.md`](../docs/PIN_ASSIGNMENT.md)
- ESP32との通信仕様: [`docs/ESP32_UART_PROTOCOL.md`](../docs/ESP32_UART_PROTOCOL.md)

**この構成（Nano、5モータ、並列リミット、1線UART）での実機確認はまだです。** ビルドの成功だけを確認しています。

## 構成

| Motor | 部位 | 制御 | リミットスイッチ |
|---|---|---|---|
| Motor1 | 頭の左右（neckLR） | STEP / DIR + UART（StealthChop、静音） | 左右2個を並列で1入力 |
| Motor2 | 頭の上下（neckUD） | STEP / DIR + UART（StealthChop、静音） | 上下2個を並列で1入力 |
| Motor3 | しっぽ | STEP / DIR + UART（StealthChop、静音） | 左右を別々の入力 |
| Motor4 | 肋骨1（呼吸） | STEP / DIR | なし |
| Motor5 | 肋骨2（呼吸） | STEP / DIR | なし |

ほかにSG90サーボ（まぶた）を1台、電源ON/OFFピン付きで使います。ピン番号は[`docs/PIN_ASSIGNMENT.md`](../docs/PIN_ASSIGNMENT.md)を参照してください。

頭とうなずきの制御（ギア比、スイッチ間の角度、原点合わせ、並列スイッチの扱い、ドライバ設定）は、ESP32-S3で同じ機構を動かした参考プログラムに合わせています。

## Serialコマンド

Serial Monitor（115,200 bps）へ1行ずつ送ります。行末は`Newline`または`Both NL & CR`にします。

| コマンド | 動作 |
|---|---|
| `m1f` / `m1r` / `m1s` | Motor1 / 頭の左右 正転／逆転／停止。押されているスイッチへ向かう開始は拒否。停止は角度を保持 |
| `m2f` / `m2r` / `m2s` | Motor2 / 頭の上下 正転／逆転／停止。同上 |
| `m3f` / `m3r` / `m3s` | Motor3 / しっぽ 正転／逆転／停止。左右どちらかのスイッチが押されていれば開始を拒否。停止でドライバ出力をOFF |
| `m4f` / `m4r` / `m4s` | Motor4 / 肋骨1 正転／逆転／停止 |
| `m5f` / `m5r` / `m5s` | Motor5 / 肋骨2 正転／逆転／停止 |
| `home` | 頭の2軸の原点合わせを実行（失敗後の再実行にも使う） |
| `pos` | 頭の2軸の状態、STEP位置、角度、スイッチの状態を表示 |
| `svon` / `svoff` | サーボの電源ON／OFF |
| `sv<角度>` | サーボを0〜180°へ動かす（例: `sv90`）。電源OFFなら先にON |
| `tmc` | Motor1〜3を止め、TMC2209をUARTで設定し直す |
| `uart` | アドレス00〜11を走査し、Motor1〜3のUARTを詳しく診断 |
| `uartw` | UART通信モニタの開始／停止 |
| `help` | コマンド一覧 |

手動運転（`m1f`など）の速度は`MOTOR_STEP_HALF_PERIOD_US`（5,000 µs、約100 steps/s）です。

## ESP32からの指示

ESP32のTXをNanoのD0へつなぎます。D0はUSBと共用なので、Serial Monitorのテキストコマンドとフレームを同じ受信線で受けます。行の先頭に`AA`が来たらフレーム、それ以外はテキストコマンドとして扱います。**書き込みのときはESP32のTX線を外してください。**

| 変数 | 動かすもの |
|---|---|
| neckLR / neckUD / neckSpeed | Motor1 / Motor2（角度。速度は両方共通） |
| tailDir / tailSpeed / tailTime | Motor3 |
| breathRate | Motor4とMotor5（連続回転） |
| eyeState | SG90サーボ（まぶた） |
| voiceId | ログ表示のみ（音声出力なし） |
| actionId / catState | ログ表示のみ |

受信ログの例:

```text
[HEAD LR] Move to 45.0 deg, speed 60%
[HEAD UD] Move to -15.0 deg, speed 60%
[ESP] Frame flags=0x08
[ESP ERROR] RX=AA 55 08 00 ... - CRC mismatch
```

PCからのテスト送信（USB経由でそのまま届く。ESP32のTX線は外しておく）:

```powershell
.\scripts\send_esp_frame.ps1 -Port COM5 -Neck -NeckLR 45 -NeckUD -15 -NeckSpeed 60
.\scripts\send_esp_frame.ps1 -Port COM5 -Tail -TailDir 3 -TailSpeed 50 -TailTime 30
.\scripts\send_esp_frame.ps1 -Port COM5 -Eye -EyeState 1
```

ポートを開くとボードがリセットされることがあるため、スクリプトは送信前に2.5秒待ちます（`-WaitMs`で変更）。COM番号は接続ごとに確認してください。

## 頭（Motor1 / Motor2）

### 並列スイッチと原点合わせ

各軸の両端のスイッチは並列で1本の入力につながっているため、どちらの端が押されたかは移動方向で判断します。

1. 起動時（または`home`）に、max側へゆっくり動く。
2. スイッチが押されたら止まり、その位置をmax側のスイッチ角度とする（原点合わせ完了）。
3. 0°（またはESP32から受けた角度）へ移動する。

- 移動中にスイッチが押されたら直ちに止まり、角度をその端の値に補正します。
- 押されたスイッチから離れる向きには動けます（退避）。`LIMIT_ESCAPE_MAX_DEGREES`（20°）動いてもまだ押されていれば、反対側のスイッチとみなして止まります。
- 押されたスイッチへ向かう開始は拒否します。
- 起動時にスイッチが押されていた場合は、どちらの端か分からないため、max側へ動いてスイッチが離れるか確かめます。20°以内に離れなければ失敗として止まります。
- max側のスイッチが`HOMING_MAX_DEGREES`（270°）以内に見つからない場合も失敗です。原因を直して`home`を送ります。
- 原点合わせのあとは、角度を保つため共用ENを有効のままにします（`HEAD_HOLD_POSITION`）。この間は5台すべてのドライバに通電します。

### 設定（`HotoriConfig.h`）

| 設定 | 値 | 内容 |
|---|---:|---|
| `HEAD_LR_STEPS_PER_DEGREE` | 11.26 | 200 × 1/8 ÷ 360 × ギア比38/15 |
| `HEAD_UD_STEPS_PER_DEGREE` | 19.26 | 200 × 1/8 ÷ 360 × ギア比65/15 |
| `HEAD_LR_MIN/MAX_SWITCH_ANGLE` | -62.8 / 62.8 | スイッチ間の実測125.6° |
| `HEAD_UD_MIN/MAX_SWITCH_ANGLE` | -47.3 / 47.3 | スイッチ間の実測94.6°。**中央（0°）の位置は仮** |
| `HEAD_SWITCH_MARGIN_DEGREES` | 2 | 指示角度をスイッチの手前で止める余裕。左右は実質±60.8°まで |
| `HEAD_MIN/MAX_STEPS_PER_SECOND` | 100 / 667 | neckSpeed 1〜100%の巡航速度 |
| `HEAD_START_STEPS_PER_SECOND` / `HEAD_RAMP_STEPS` | 400 / 800 | 加速・減速（台形） |
| `HEAD_LR/UD_FORWARD_LEVEL` | HIGH | 角度が増える向きのDIRレベル。向きが逆なら反転する |

1°あたりのSTEP数は、UARTで1/8マイクロステップに設定できたときの値です。UARTの設定に失敗すると、ドライバはMS1/MS2のピンで決まる分割で動くため、角度がずれます。

## TMC2209（Motor1〜3）

### UART

3台のPDN_UARTを1本にまとめ、Nanoの**A0へ直接**つなぎます（抵抗は不要）。A0は送信と受信を切り替えて使います（TMCStepperの1線SoftwareSerial、57,600 bps）。

| Motor | MS1 | MS2 | アドレス |
|---|---|---|---:|
| Motor1 / 頭の左右 | GND | GND | 0 |
| Motor2 / 頭の上下 | VIO | GND | 1 |
| Motor3 / しっぽ | GND | VIO | 2 |

起動時に、次の設定を書き込んで読み返します（最大5回）。

- StealthChop（静音）、256分割補間、PWM自動調整
- 電流: 頭の左右2,000 mA（0.11 Ωでは上限約1,770 mAに丸められる）、頭の上下1,000 mA、しっぽ400 mA。停止中は半分
- 1/8マイクロステップ
- 回転方向はDIRピン（UARTの方向設定は使わない）

応答がなければ`[ERROR] ... did not answer over UART.`を表示します。その場合もSTEP / DIRで動きますが、電流はVREF、マイクロステップはMS1/MS2で決まります。モータ電源（VM）を後から入れた場合は`tmc`を送ります。

### UART通信の確認（`uart` / `uartw`）

`uart`は4つのアドレス（MS2 MS1 = `00`、`01`、`10`、`11`）すべてに問い合わせ、そのあとMotor1〜3を詳しく診断します。

```text
[UART] Address scan (MS2 MS1)
  00 addr 0 M1 Head LR : reply version=0x21
  01 addr 1 M2 Head UD : reply version=0x21
  10 addr 2 M3 Tail    : no reply  <- driver expected here
  11 addr 3 ---------- : reply version=0x21  <- unexpected; check MS1/MS2
[UART] Motor1 / Head LR (addr 0)
  Read : OK  version=0x21
  Write: OK  IFCNT 5 -> 6
  Pins : ENN=1 MS1=0 MS2=0 DIR=0
  GSTAT: reset=0 drv_err=0 uv_cp=0
  Chop : toff=0 mres=5 (toff 0 = output off, mres 5 = 1/8 step)
  Drv  : cs_actual=0 stealth=1 stst=1 ola=0 olb=0 s2ga=0 s2gb=0 ot=0 otpw=0
...
[UART] Result: M1 OK, M2 OK, M3 NG
```

この例では、しっぽのドライバがアドレス`11`で応答しています。MS1もVIOにつながっている可能性があります。

| 行 | 意味 |
|---|---|
| `Read` | IOINレジスタを読み、CRCが正しくversionが`0x21`ならOK |
| `Write` | GCONFを現在値のまま書き戻し、書き込み受付カウンタIFCNTが1増えればOK。設定は変わらない |
| `Pins` | ドライバが見ているピンのレベル。`ENN=1`は共用ENがHIGH（無効） |
| `GSTAT` | `reset=1`は電源投入後またはリセット後、`drv_err=1`は過熱・短絡、`uv_cp=1`はチャージポンプの電圧不足 |
| `Chop` | `toff=0`は出力段オフ（停止中は正常）。動作中は`toff=5`。`mres=5`は1/8ステップ |
| `Drv` | `cs_actual`は実際の電流スケール（0〜31）。`stealth=1`は静音モードで動作中。`stst=1`はSTEPが来ていない。`ola`/`olb`はコイル断線、`s2ga`/`s2gb`はGNDショート、`ot`/`otpw`は過熱 |

`uartw`は4アドレスに0.25秒ごとに1つずつ問い合わせ、約1秒ごとに1行表示します。正常なら`00`〜`10`が`OK`、`11`が`--`です。

```text
[UART] 00 OK 12/0 | 01 OK 12/0 | 10 OK 12/0 | 11 -- 0/12
```

応答がない問い合わせでは`loop()`が約20 ms止まり、その間はSTEPの生成とリミットの確認も遅れます。モニタは診断時だけ使ってください。

## しっぽ・呼吸・目

| 設定（`HotoriConfig.h`） | 値 | 内容 |
|---|---:|---|
| `TAIL_LEFT_USES_FORWARD` | true | 「左」を正転にする。**仮の値** |
| `TAIL_MIN/MAX_STEPS_PER_SECOND` | 25 / 100 | tailSpeed 1〜100%の速度範囲 |
| `TAIL_SWING_HALF_PERIOD_MS` | 500 | tailDir = 3（左右交互）の反転間隔 |
| `BREATH_STEPS_PER_CYCLE` | 1600 | 1呼吸あたりのSTEP数。**仮の値**（1/8ステップで1回転） |
| `BREATH_RIB1/RIB2_FORWARD` | true | 肋骨の回転方向 |
| `EYE_CLOSED_ANGLE` / `EYE_OPEN_ANGLE` | 0 / 90 | まぶたの閉／開のサーボ角度。**仮の値** |

しっぽのスイッチは左右別々の入力です。どちらかが押されたら、方向に関係なくしっぽを直ちに止め、押されている間は開始を拒否し、離れても自動では再始動しません。

サーボはSG90の一般的なパルス幅（0°で500 µs、180°で2,400 µs）で動かします。電源ピンは外部5Vとサーボの間のスイッチ（MOSFETなど）を操作します。UARTの通信中（数ms）は、サーボがわずかに震えることがあります。

## TMC2209モジュールの物理配置

青いヒートシンクのモジュールを、EN側を上、DIR側を下にして見たときの左側（黄色ピンヘッダ）の配置です。

```text
    EN   ●   ← 上端
         ●
         ●
         ●
    STEP ●   ← 下から2番目
    DIR  ●   ← 下端
```

過去に、この物理ピン位置の認識違いで「保持トルクは入るが回転しない」問題が起きました。シルク印刷や思い込みで判断せず、モジュール本体と照合してください。

| TMC2209側 | 接続先 |
|---|---|
| VIO | Nano 5V |
| ロジックGND | Nano GND |
| VM / VMOT | モータ用外部電源 |
| モータ電源GND | Nano GNDと共通 |
| 1A / 1B | モータの一方のコイルペア |
| 2A / 2B | モータのもう一方のコイルペア |

## ビルドと書き込み

```powershell
# ビルド
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nano_new
# 書き込み（ESP32のTX線を外してから。COM番号は接続ごとに確認）
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nano_new -t upload --upload-port COM5
# Serial Monitor
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -e nano_new --port COM5
```

書き込みに失敗する場合は、旧ブートローダ用の環境`nano`を試してください。

## トラブルシュート

### モータが固定されるが回転しない

- TMC2209側のSTEP / DIRの物理端子を確認する（上の配置図）。
- 共用ENは、どれか1台が動いている間（または頭が角度を保持している間）は5台すべてで有効です。トルクがかかっていても、そのモータにSTEPが届いているとは限りません。
- Motor1〜3は`uart`の`stst`で確認できます。動作中に`stst=1`ならSTEPが届いていません。

### 小刻みに震えて回らない

- モータの4本の線のうち、どの2本が同じコイルかをテスターで確認し、1A/1Bと2A/2Bに正しくまとめる。
- DIRの線が浮いていないか確認する。

### COMポートを開けない

Serial Monitorなど、ほかのプログラムがCOMポートを使っていないか確認します。ESP32のTX線がD0につながったままでも書き込みに失敗します。

## ファイル構成

| ファイル | 役割 |
|---|---|
| `hotori_hardware_test.ino` | 各部品の生成、初期化、Serialコマンドとフレームの振り分け、毎ループの更新 |
| `HotoriConfig.h` | ピン番号、速度、角度、電流などの設定 |
| `UartStepMotor.h/.cpp` | Motor1〜3。STEP / DIR出力、位置の記録、台形加減速、UARTでのドライバ設定と診断 |
| `HeadAxis.h/.cpp` | 頭の1軸。並列スイッチでの原点合わせ、角度指示、退避 |
| `TailController.h/.cpp` | しっぽ。ESP32の指示、手動運転、左右リミットでの停止 |
| `StepDirMotor.h/.cpp` | Motor4 / Motor5。STEP / DIRのみの出力 |
| `ServoController.h/.cpp` | SG90と電源ピン |
| `EspFrameReceiver.h/.cpp` | ESP32の15 byteフレームの受信と検証 |
| `SerialCommandReader.h/.cpp` | 改行区切りのテキストコマンドの受信 |
| `DfPlayerController.h/.cpp` | 未使用（このボードには音声モジュール用のピンがない） |
