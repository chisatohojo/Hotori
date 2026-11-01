# ESP32 UART検査・実機試験記録

対象日: **2026年11月1日**。本記録では、ユーザーが実施した実機試験と、Git登録時にCodexが実行したソフトウェア検証を分けて記載します。

## ユーザー報告による実機結果

使用環境はArduino IDE **2.3.6**、**esp32 by Espressif Systems 3.3.12**、ESP32-WROOM-32D / **ESP32 Dev Module**。PC → ESP32はUSB Serial 115200 bps、ESP32 GPIO17 → Mega RX2/D17は115200 bps / 8N1、GND共通です。

| 項目 | 結果・範囲 |
|---|---|
| ESP32へのプログラム書き込み | PASS（ユーザーによるArduino IDEでの書き込み） |
| PC → ESP32のシリアルコマンド | PASS |
| ESP32 → MegaのUART受信とCRC検証 | PASS |
| Mega → DFPlayerの音声再生指令 | PASS |
| DFPlayer音声の実再生 | PASS。全7ファイル個別の試験結果までは本報告に含まれない |
| Tail指示の受信と時間停止 | PASS |
| Tailモータの回転 | 一部条件でPASS。全速度・両方向での安定回転確認ではない |

以上から、試験した条件での通信経路・CRC検証・音声再生は合格と扱います。モータの性能や安全停止の完全な実機検証が済んだとは扱いません。個々の入力値、波形、測定値などは提供されていないため、推測して補完していません。

## 未完了・要改善

| 項目 | 状況 |
|---|---|
| Tailの低速時・逆回転時 | 振動・脱調が発生。改善と再試験が必要 |
| モータ用電源 | 試験時はPC USBの5V。電源能力不足などを疑っているが、原因は未確定 |
| 左右Tail Limit | 停止の完全な実機試験は未完了 |
| 呼吸モータ | 実動作は未検証。runtimeの未校正設定 `BREATH_STEPS_PER_CYCLE=0` はそのまま |

今後は電源・負荷・配線・ドライバ設定を切り分け、電源能力不足を原因と断定する前に測定してください。250～1000 steps/sでの起動・逆転・速度変更、両リミットの作動／断線時の停止・EN無効化、交互モードの再開と時間停止を実機確認する必要があります。呼吸は実機校正と駆動試験が別途必要です。今回の登録作業では制御コードを変更せず、加減速や電源対策も追加していません。

## Git登録時のソフトウェア検証

開始ブランチは `feature/runtime-voice7-tail-10x`、開始コミットは `55cc1c92f62348b5ccf2afb21bdf6e66abc22938`。作業開始時は未コミット変更なし、fetch後のリモートとの差分はahead/behind **0/0**でした。

### ソースの一致と変更範囲

- 追加元: `C:/Users/megum/Documents/__hardDev/hotori/hotori_esp32_uart_test/hotori_esp32_uart_test.ino`
- 追加先: 正式リポジトリ内 `hotori_esp32_uart_test/hotori_esp32_uart_test.ino`
- `.ino` の名前とスケッチフォルダ名は一致。スケッチの内容は変更せずコピー。
- コピー前後のSHA-256はともに次の値です。

  ```text
  9451815ce2ca6f6577e48132d0814d6114bb5f4b24b4fe37db3a3961c50dae7f
  ```

- 変更対象はルート `README.md` と、新規のESP32用 `.ino`・`README.md`・本記録の計4ファイルだけです。
- 元フォルダ、旧 `Hotori-main`、既存バックアップは読み取りのみ。既存runtime、デバッグスケッチ、CAD、音声資料、UART仕様書を上書きしていません。
- Windowsの `HardWare` のskip-worktree設定は変更していません。生成物は既存の無視対象 `build/` に出力し、コミット対象から除外します。

### ESP32 Dev Module向けコンパイル

正式リポジトリ直下で次を実行し、**成功**しました。uploadは実行していません。

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --build-path build/hotori_esp32_uart_test hotori_esp32_uart_test
```

| 項目 | 結果 |
|---|---|
| Arduino CLI | 1.4.1 |
| インストール済みコア | esp32:esp32 3.3.12（既存環境を使用、更新なし） |
| FQBN | `esp32:esp32:esp32`（ESP32 Dev Module、既定設定） |
| プログラム領域 | 288424 / 1310720 bytes（22%） |
| グローバル変数 | 22164 / 327680 bytes（6%） |
| コンパイル出力の警告 | なし |

この結果はコンパイルの成功です。今回Codexが新たに実機の通信・音声・モータ動作を確認した結果ではありません。

### 既存runtimeテスト

```powershell
python hotori_runtime/tests/run_tests.py --cxx "$env:TEMP/hotori-test-toolchain/w64devkit/bin/g++.exe"
```

既存ホストテストは **2372 assertions成功**（g++ 16.2.0）。CRC・受信、FLAGS個別更新、VOICE全256値の検証と最新指示優先、Tail速度換算・期限・リミット、呼吸校正・停止、非ブロッキング送信を確認しています。GPIO・UART・時刻はモックであり、ESP32上での実行試験や、未完了の実機リミット／モータ性能試験の代わりにはなりません。

READMEのローカルリンク・アンカー15件、入力例のCRC（`8F` / `DF`）、スケッチとの記載整合性を確認しました。`git diff --check` とステージ済み差分の `git diff --cached --check` は成功し、コピー元とステージ済み `.ino` のSHA-256も一致しました。Mega本番runtimeの変更・書き込み、ESP32への新規upload、mainへのmergeは行っていません。
