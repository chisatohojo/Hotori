# Hotori / 夢卵

Arduino Nano、TMC2209 × 5、SG90サーボを使った猫型デバイスです。ESP32から送られる指示フレームで、頭（左右・上下）、しっぽ、呼吸（肋骨2台）、まぶたを動かします。現在はハードウェア確認フェーズです。

- 主スケッチ: [`hotori_hardware_test`](hotori_hardware_test/README.md)
- 正式な配線資料: [`docs/PIN_ASSIGNMENT.md`](docs/PIN_ASSIGNMENT.md)
- ESP32との通信仕様: [`docs/ESP32_UART_PROTOCOL.md`](docs/ESP32_UART_PROTOCOL.md)
- ピン・動作設定: [`HotoriConfig.h`](hotori_hardware_test/HotoriConfig.h)
- 作業ルール: [`AGENTS.md`](AGENTS.md)

## 開発環境

Windows / PowerShellでの開発を想定しています。

| 項目 | バージョン・設定 |
|---|---|
| PlatformIO | `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` |
| 環境 | `nano_new`（新ブートローダ）、`nano`（旧ブートローダ） |
| TMCStepper | 0.7.3（Motor1〜3のTMC2209 UART制御） |
| Servo | 1.2.2（SG90） |
| USB Serial Monitor | 115200 bps、改行はNewlineまたはBoth NL & CR |

依存ライブラリは`platformio.ini`の`lib_deps`で自動的に取得されます。

## ビルドと書き込み

プロジェクト直下で実行します。

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -e nano_new                                  # ビルド
& $pio run -e nano_new -t upload --upload-port COM5     # 書き込み（失敗する場合は -e nano）
& $pio device monitor -e nano_new --port COM5           # Serial Monitor
```

- COM番号は接続ごとに確認してください（例のCOM5は固定ではありません）。
- 書き込みの前に、Serial Monitorを閉じ、ESP32のTX線（NanoのD0）を外してください。
- 出力先は`.pio/build/<環境名>/`です。

Arduino CLIを使う場合は`scripts/build.ps1`でもビルドできます（`hotori_hardware_test`はNano向けにビルドします）。

## その他のスケッチ

`hotori_runtime`、`dfplayer_min_test`、`mega_step_test`は旧構成で作ったスケッチです。現在の配線とピン定義が異なるため、現行の実機には書き込まないでください。

## ソース管理

元リポジトリは[`chisatohojo/Hotori`](https://github.com/chisatohojo/Hotori)です。この作業フォルダは元の`main`の履歴を引き継ぎ、`origin/main`を追跡しています。`build/`は`.gitignore`で除外し、テキストの改行は`.gitattributes`でLFに揃えています。
