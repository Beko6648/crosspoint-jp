# Issue #40 X3バッテリー診断

`py -3 test/run_battery_diagnostics_test.py --compiler <C++ compiler> --zig`
（Zig以外では `--zig` を省略。）本番のHalPowerManager.cppをI2C/時計/ADCスタブで実行する。
C3 release、C3 debug、S3 releaseの3構成でpoll抑制、84→7、15ポイント境界、
電流の符号、SOC送信失敗/短い応答、診断各項目の失敗、最後の正常値保持、
起動時/0%/100%超の既存表示処理、X4/S3のADC分離を確認する。

## 実機ログ

通常のdefault/gh_releaseは、直前に成功した生SOCとの差が15ポイント以上のときだけ
追加の4コマンドを読み、INFOのBAT/SOC_JUMPを出す。起動時の最初の値は急変とみなさない。
debug (`LOG_LEVEL=2`) では成功した各pollでBAT/SAMPLEも出す。
`X3_BATTERY_DIAGNOSTICS=1` を明示しても有効（INFOログも有効にする）。
pollは既存の呼び出しに従い最短1500ms間隔。独立した常時サンプリングではない。

例（架空値）:

```text
[INF] BAT: X3 SOC_JUMP poll_ms=120000 old_soc=84 soc=7 voltage_mV=3450 current_mA=-120 remaining_mAh=21 full_mAh=300 valid=0xF read_ms=2
```

`old_soc` / `soc` は補正前の16bit値。UIの100%上限処理は変更していない。
`valid` はbit0=電圧、bit1=電流、bit2=残容量、bit3=満充電容量。
失敗した値は電流=-32769、他=-1。無効値を実測値として扱わない。
初回SAMPLEのold_soc=-1は過去の成功値なし。SOC失敗はdebugでSOC_READ_FAILEDを記録し、
表示と比較元を最後の正常値に保つ。追加測定だけ失敗した場合は正常なSOCを表示する。

SOC(0x2C)、Voltage(0x08)、Current(0x0C)、RemainingCapacity(0x10)、
FullChargeCapacity(0x12)を読む。Currentはsigned mA、容量はmAh。
出典: [TI BQ27220 TRM SLUUBD4A, chapter 2](https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf)。
同じpollで順次読むため、厳密な同時測定ではない。read_msはSOC取得からの所要時間。
ゲージ更新間隔より短い瞬間的な電圧低下は捉えられない。

X3で充電/USB接続状況、使用フォント、読書/ページ更新/復帰の操作とともに
急落前後のBATログを保存する。必要ならdebugで前後のSAMPLEも記録する。
EDV到達やセル設定不一致はまだ原因候補であり、このログだけで確定しない。
ゲージ設定変更、残量の平滑化・補正は行わない。
