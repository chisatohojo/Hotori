# Hotori Pin Assignment

この文書をHotori（夢卵）プロジェクトの正式なピンアサイン資料とします。

## Arduino Mega 2560

### Motor1 / Tail

| Signal | Mega Pin | Device |
|---|---|---|
| STEP | D8 | TMC2209 #1 STEP |
| DIR | D9 | TMC2209 #1 DIR |
| EN | D10 | TMC2209 #1 EN |

Motor1はしっぽ用です。

### Motor2 / Rib

| Signal | Mega Pin | Device |
|---|---|---|
| STEP | D22 | TMC2209 #2 STEP |
| DIR | D23 | TMC2209 #2 DIR |
| EN | D24 | TMC2209 #2 EN |

Motor2は肋骨／呼吸機構用です。

### Tail Limit Switch

| Function | Mega Pin | Connection |
|---|---:|---|
| Tail Left Limit | D25 | NC switch -> GND |
| Tail Right Limit | D26 | NC switch -> GND |

配線:

```text
Mega D25 -> Left NC -> Left COM -> GND
Mega D26 -> Right NC -> Right COM -> GND
```

Mega D25とD26は両方`INPUT_PULLUP`として使用します。左右スイッチのCOM側GNDは共通で構いません。NC（Normally Closed）接点を使用するため、各入力の状態は次のとおりです。

- LOW = Normal（通常）
- HIGH = Limit active / wire disconnected（リミットスイッチ押下／配線断線、安全側）

左右どちらか一方でもHIGHを検出したら、Motor1の移動方向（Forward / Reverse）に依存せず、デバウンス待ちなしで`motor1.stop()`を呼び出します。STEPを停止し、TMC2209 #1のENを無効化して保持トルクを解除します。両方がLOWへ戻っても自動再始動せず、次の明示的なSerialコマンドまで停止を維持します。左右どちらかがHIGHの間は`m1f` / `m1r`の両方を拒否します。ログではLeft / Right / Bothを区別します。

### DFPlayer Mini

| Mega | DFPlayer |
|---|---|
| D18 / TX1 | RX |
| D19 / RX1 | TX |
| 5V | VCC |
| GND | GND |

Mega TX1からDFPlayer RXへの配線には、1kΩ程度の直列抵抗を使用します。

スピーカーはDFPlayerのSPK1とSPK2の間へ接続し、どちらもGNDへ接続しません。

### TMC2209物理配置メモ

現在使用中のモジュール:

- 青いヒートシンク
- 左側が黄色ピンヘッダ
- 右側が黒ピンヘッダ
- EN側が上
- DIR側が下

この向きで黄色側:

- 上端 = EN
- 下から2番目 = STEP
- 下端 = DIR

過去にこの物理ピン位置の認識違いで、保持トルクは入るがモータが回転しない問題が発生しました。配線時は必ず実物の向きを確認してください。

電源:

- VIO -> Mega 5V
- Logic GND -> Mega GND
- VM / VMOT -> Motor external power
- Motor power GND -> Mega GNDと共通
