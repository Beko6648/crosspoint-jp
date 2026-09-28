# X4 Classic（X4 V2）Experimental確認手順

X4 Classic（X4 V2）は、ESP32-S3、800×480パネル、PSRAM、SDMMCを備えたX4世代の端末です。Yomukaでは`x4c`ビルドをExperimentalとして用意しています。この段階では正式対応ではありません。

## 現在の対象範囲

`x4c`は次の共通基盤を使います。

- ESP32-S3、16 MBフラッシュ、8 MB PSRAM
- SDKのX4 Classicボード定義
- 1-bit SDMMC
- BM8563互換RTC
- CW2017バッテリーゲージ
- 物理ボタン入力
- SSD1677、UC8179、UC8279の表示コントローラ検出

スリープ前にはSDMMCを停止します。これにより、SDカードのバス信号がスリープ中にカード電源へ回り込むことを防ぎます。

この停止・信号ピン解放はコード上の対策です。Yomukaでの実際の停止電流や再マウントは未測定です。

## v0.8.0の診断基盤

FreeInk SDKはStage 1と同じ `18e73e16a1965fb19c62fa3a717cbc3a5b43add3` に固定しています。
factory NVSの `hw_calib/screenType` があればそれを優先し、ない場合にVER probeを行います。
このrevisionはVER `0x01`をUC8179、`0x02/0x68/0x69`をUC8279として認識し、未知の値もUC8279へfallbackします。
未知の値で表示できたことを、そのパネルの正式対応とは扱いません。

診断レポートに `freeink_revision`, `build_id`, `display_controller_source`（NVSまたはVER）、
`display_controller_variant`を保存します。VER経路では `display_probe_ver`,
`display_probe_recognized`, `display_probe_busy_timeout` も保存します。
保存時に再probeは行わないため、表示バスには触れません。

`battery_backend=cw2017`, `battery_i2c_address=0x63`, `rtc_i2c_address=0x51`、
センサーI2CのSDA/SCL、SDMMCのCLK/CMD/D0も保存します。
これらは選択されたprofileの構成値であり、各部品の実検出に成功した証明ではありません。
RTC検出は既存の `rtc_available`、SD動作は `sd_ready` と実際のファイル操作で確認します。

デバッグ表示を有効にした際の `/.crosspoint/power_log.txt` は、X4CではCW2017から電圧を取得します。
通常X4用のGPIO0 ADC測定は行いません（X4CのGPIO0はボタン）。機種名は `device=X4 Classic` です。
未取得の電圧は `voltage_mV=-1`、ADC値と電圧換算残量は `raw_adc=-1 voltage_pct=-1`、
未対応の電流は `current_mA=-32769` とします。
USB検出ピンのない現profileでは `usb=-1`、診断レポートは `usb_detection_supported=false` となります。
これは「USB未接続」ではなく「接続状態が不明」です。充電STATの極性は未検証のため、新たな充電判定は追加しません。

X4CのIMUはSDK profileにありますが、Yomukaの傾き診断・ページ送りは引き続きX3のみです。
この段階でX4Cの傾き対応を有効にはしません。

## 未確認項目

実機がないため、次は未確認です。

- パネルの向きと表示領域
- USB接続の検出とUSB転送
- 充電状態の表示
- スリープ／復帰時の消費電力
- RTC保持、SDMMC再マウント
- コントローラ別のリフレッシュ品質

これらが確認されるまで、配布版・正式対応・既存X4の代替として扱いません。

## ビルドとSimulatorの扱い

`pio run -e x4c`でESP32-S3向けのコンパイルは確認しています。これは、X4 Classicのボード定義、PSRAM、SDMMC、RTC、バッテリー、ボタン入力に必要なコードが同じバイナリで解決できることを示す確認です。

現時点のYomukaリポジトリと同梱FreeInk SDKには、X4 Classicを起動して画面やボタンを操作できるSimulator環境はありません。そのため、Simulatorでの画面・入力確認は実施済みとして扱わず、下記の実機確認を最初の動作証跡とします。

## 初回確認の順序

1. 起動し、ホーム画面と診断画面が表示されることを確認する。
2. 診断レポートを保存し、以下を確認する。

   ```text
   device=X4 Classic
   sd_transport=sdmmc
   psram_available=true
   rtc_available=true
   input_style=digital_buttons
   freeink_revision=18e73e16
   battery_backend=cw2017
   usb_detection_supported=false
   ```

3. SDカードからEPUBを開き、ページ送り、戻し、決定、戻る、電源ボタンを確認する。
4. 縦向き、横向き右上、横向き左上、上下反転で、読書中と読書メニュー中のボタンを確認する。
5. スリープ後に復帰し、SDカードとEPUBを再び利用できることを確認する。
6. RTC、バッテリー、画面リフレッシュを確認する。USB接続・取り外しの実動作は観察できるが、現profileの自動検出は未対応として扱う。

異常がある場合は、診断レポートと再現手順を添えて報告してください。

## 将来のX4 Proへの再利用

X4 ProもESP32-S3、800×480、SDMMC、RTC、CW2017を共有します。X4 Classicで確認するSDMMC、RTC、スリープ、表示コントローラの基盤はX4 Proでも再利用します。X4 Pro固有のタッチ、Homeキー、フロントライト、USB転送は別途確認が必要です。
