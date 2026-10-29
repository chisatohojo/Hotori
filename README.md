# Hotori / 夢卵

Arduino Mega 2560、TMC2209 × 2、DFPlayer Miniを使ったデバイスです。現在はハードウェア単体確認フェーズです。

- 本番制御（ESP32 UART受信）: [`hotori_runtime`](hotori_runtime/README.md)
- デバッグ・単体試験: [`hotori_hardware_test`](hotori_hardware_test/README.md)
- 正式な配線資料: [`docs/PIN_ASSIGNMENT.md`](docs/PIN_ASSIGNMENT.md)
- ピン・動作設定: [`HotoriConfig.h`](hotori_hardware_test/HotoriConfig.h)
- Codexの作業ルール: [`AGENTS.md`](AGENTS.md)

## 開発環境

Windows / PowerShellでの開発を想定しています。2026年10月29日に確認した環境です。

| 項目 | バージョン・設定 |
|---|---|
| Arduino CLI | 1.5.1（PATHに登録済み） |
| Arduino AVR Boards | 1.8.6 |
| DFRobotDFPlayerMini | 1.0.6 |
| ボード / FQBN | Mega 2560 / `arduino:avr:mega:cpu=atmega2560` |
| USB Serial Monitor | 115200 bps、改行はNewlineまたはBoth NL & CR |

Arduino CLIを導入し、`arduino-cli version`が実行できる状態にします。別のPCで依存関係を揃える場合は、プロジェクト直下から次を実行します（インターネット接続が必要です）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup.ps1
```

セットアップはユーザー共通のArduino環境に上記バージョンを導入します。既に同じバージョンが入っている場合は、そのままビルドできます。

## ビルド

```powershell
# 主スケッチ
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1

# 本番制御（既存スクリプトはデバッグスケッチ用のため直接指定）
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 --warnings all --build-path build/hotori_runtime hotori_runtime

# DFPlayer単体確認用
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Sketch dfplayer_min_test

# 履歴用スケッチも含めた全スケッチのコンパイル確認
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Sketch all
```

出力先は`build/<スケッチ名>/`です。ビルド処理はコンパイルのみ行います。

VS Codeでこのフォルダを開くと、推奨拡張機能とタスクを利用できます。`Ctrl+Shift+B`で主スケッチをビルドし、「タスクの実行」から依存関係のセットアップ、全スケッチのビルド、ボード一覧、Serial Monitorを実行できます。C/C++の補完には、主スケッチのビルドで生成される`compile_commands.json`を使用します。

## 実機への書き込み

```powershell
arduino-cli board list
# 上で確認したMega 2560のポートをCOMxへ指定
arduino-cli upload -p COMx --fqbn arduino:avr:mega:cpu=atmega2560 --input-dir .\build\hotori_hardware_test .\hotori_hardware_test
arduino-cli monitor -p COMx -c baudrate=115200
```

書き込みは主スケッチのビルド成功後に行い、先に他のSerial Monitorを閉じてください。ボード名がUnknownの場合は、COM番号だけでMega 2560と判断せず、実機を確認してください。

`mega_step_test`は履歴用です。現在の配線とピン定義が異なるため、現行の実機には書き込まないでください。主スケッチの操作方法と実機確認状況は[詳細README](hotori_hardware_test/README.md)を参照してください。

## ソース管理

元リポジトリは[`chisatohojo/Hotori`](https://github.com/chisatohojo/Hotori)です。この作業フォルダは元の`main`の履歴を引き継ぎ、`origin/main`を追跡しています。`build/`は`.gitignore`で除外し、テキストの改行は`.gitattributes`でLFに揃えています。

## Codexでの作業

プロジェクト直下の`AGENTS.md`に、使用環境、検証コマンド、配線の前提、リミットスイッチの停止条件、実装方針をまとめています。Codexでこのフォルダを開いて利用してください。作成前から開いているセッションでは、新しいセッションで読み込みを確認できます（[OpenAI公式ドキュメント](https://learn.chatgpt.com/docs/agent-configuration/agents-md)）。
