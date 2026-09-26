# Yomuka heap tracing: baseline

## 目的と範囲

上流 #3702 (`dc3963db`) の記録・解析基盤を移植する。画像キャッシュ、先読み、フォント設定は通常版を継承する。`shared_ptr` の変更や新しい回復処理は今回行わない。

`heaptrace` 専用に ESP-IDF の heap hooks と RISC-V frame pointers を有効化する。通常の `default` / `gh_release` では記録バッファ、タスク、フック、new の置換はコンパイルされない。

計測そのものがメモリと処理時間を消費する。診断版の free heap / MaxAlloc を通常版の値と直接引いて改善量にしない。今後の変更前後は同じ診断環境・同じ本・フォント・ページ・キャッシュ状態で比較し、最後に通常版で確認する。

## 診断v5の実機確認（X3のみ）

初回ログではUSB出力の途中欠落が発生した。通常版の1 ms送信待ちを診断版だけ50 msへ変更し、1行が入る空きを待ってから送信する。実機で欠落が解消するかは未確認。入力不要で10秒ごとにもスナップショットを取得する。

1. `yomuka-heaptrace-v5.bin` をX3へ書き込む。対応ELFは `yomuka-heaptrace-v5.elf`。
2. PowerShellで次のコマンドを実行する。

```powershell
cd C:\Src\crosspoint-jp
& 'C:\Users\pompo\.platformio\penv\Scripts\platformio.exe' device monitor --port COM4 --baud 115200 --filter log2file
```

3. Homeで15秒待つ。
4. 日本語テストEPUBを開き、2〜3ページ進め、15秒待つ。
5. Homeへ戻って15秒待ち、PowerShellでCtrl+Cを押して終了する。
6. `C:\Src\crosspoint-jp\logs` 内で一番新しい `device-monitor-*.log` を渡す。`serial.txt` はこのコマンドでは更新されない。

端末への文字入力、キャッシュ削除は不要。書籍、SDフォント、向きは前回と同じにする。X4での繰り返しはまだ不要。途中文字・画像欠け、操作停止、再起動があれば記録する。更新中のログは不要。

正常な記録であることを確認した後に、長めの読書・再オープンでbaselineを取得する。今回の短い確認だけでリークや改善量は結論しない。

## 解析

リポジトリ直下で実行する。ログと ELF のパスは実際のファイルに置き換える。

```powershell
py -3 scripts/heap_trace.py capture.txt summary --elf firmware.elf
py -3 scripts/heap_trace.py capture.txt snapshots --elf firmware.elf
py -3 scripts/heap_trace.py capture.txt pins --elf firmware.elf
py -3 scripts/heap_trace.py capture.txt map --snap last --out heap-map.svg --elf firmware.elf
```

UTF-8 と BOM 付き UTF-16 のログに対応。最初に summary で欠落・破損と snapshots の完了を確認する。snapshot は生存割り当てを再照合するが、欠落した過去の割り当て元や寿命は復元できない。unknown はリークの証明ではない。Home復帰後の生存量、MaxAllocを分断する割り当て、同じ操作を繰り返した際の増加を調べる。

## ビルド

```powershell
& C:/Users/pompo/.platformio/penv/Scripts/platformio.exe run -e heaptrace
py -3 test/test_heap_trace.py
```

初回はカスタムSDKの取得・ビルドが必要。上流と同じSDKビルド対策として未使用のRainMaker/Insights等を診断環境だけ除外する（証明書埋め込みの生成エラーを回避）。通常版のSDKは変更しない。対応する firmware.bin と firmware.elf を同時に保存し、SHA-256 とソース状態を記録する。通常版へ戻す際は default / gh_release を再ビルドし、SDKのhooks無効とトレースシンボル不在を確認する。

## 初回ビルド確認（2026-09-25）

上流 `dc3963db` をベースにした診断版と同じソースの default の ELF を比較した静的サイズ。実機の free heap の実測値ではない。

| 項目 | default | heaptrace | 増加 |
|---|---:|---:|---:|
| IRAM text | 67,124 B | 76,820 B | 9,696 B |
| DRAM data | 17,281 B | 17,281 B | 0 B |
| DRAM bss | 31,752 B | 41,944 B | 10,192 B |

これに記録タスクの3,072 Bスタックとタスク管理領域などが加わる。実際の free heap / MaxAlloc、ログ欠落率、読書中の動作は実機確認待ち。

解析テスト11件（UTF-16、Windows addr2line探索を含む）と clang-format 21.1.8 による確認を実施。生成ELFから関数名とソース行を引けることも確認した。

ビルド設定を途中で変更した場合、生成済みの `sdkconfig.heaptrace` に以前の設定が残ることがある。新しい custom_sdkconfig が反映されない場合は、生成ファイルを退避してから再ビルドする。これはユーザーのフォント・ログ等を削除する操作ではない。

## v2の解析器修正

チェックサム不一致・パケット欠落・形式不正が発生した途中スナップショットは破棄する。passのレコード数、ID、範囲、全heap範囲の走査完了も確認し、欠けたブロックで生存割り当てを照合しない。summaryの rejected_snapshots に除外数を表示する。旧解析器の「完了件数」は健全性を保証しなかったため、その値をbaselineには使わない。

`device-monitor-260925-230140.log` は正常パケット3,685、リング取りこぼし累計519、修正後に受理できたスナップショット1件、除外25件。通常の `serial.txt` はこの直接保存ログとは別の旧ファイルだった。受信破損は239件（従来未計数の形式不正48件を含む）。破損の全原因を確定したわけではなく、診断v2の実機再確認が必要。

## v3: 一時的な送信不可で行を捨てない

v2のUSB接続判定がfalseになると待機ループを即終了し、未送信の行も破棄していた。v3は接続状態と1行分の空きが揃うまで最大250 ms待つ。待つのは診断の送信タスクだけで、allocator hookに待機や動的メモリ確保を追加しない。期限を超えた場合は欠落を隠さず再同期する。書き込みが一部だけ成功した場合は行を終端し、二重送信になる可能性があるため同じ内容を再送しない。

`[HTRACE] v3 transport ready_timeouts=... failed_lines=... ring_drops=...` を10秒ごとに自動表示する。入力コマンドは不要。ready_timeoutsは待機期限超過、failed_linesは送信できなかった行（待機期限超過も含む）、ring_dropsは観測済みの本体リング取りこぼし累計。

C++ホストテストで、即時送信可、一時的な送信不可からの回復、恒久的な不可の期限終了、millisの桁あふれを確認する。

```powershell
py -3 test/run_heap_trace_output_test.py --compiler PATH_TO_ZIG
py -3 test/test_heap_trace.py
```

v2ログの73箇所の欠落について前後の時刻差の中央値は16 msだった。即時破棄経路と整合するが、すべての欠落の原因が同じと確定したわけではない。v3実機ログでbad、seq_gaps、ready_timeouts、failed_linesとスナップショット健全性を再確認する。

## v4: 診断中のCPU低速化を切り分ける

Yomukaは無操作3秒後にCPUを10 MHzへ落とす。v3ログはHome待機中に失敗が増え、読書操作中の約45〜65秒では失敗カウンターが止まっていた。CPU/APBクロック切り替えと継続USB出力の関係を調べるため、`CROSSPOINT_HEAP_TRACE` のビルドだけ無操作時の低速化を抑止する。通常版の省電力、診断版のループ末尾50 ms待機、送信待機250 ms・SDK書き込み50 msは維持する。

仮説の比較用であり、v3の全欠落の原因を確定したとは扱わない。診断版の時間・消費電力は通常版と直接比較しない。低速化の抑止に既存の排他的HalPowerManager::Lockを長時間保持する方法は使わず、他処理のロックと競合させない。

10秒ごとの `[HTRACE] v4 transport` で現在の `cpu_mhz`、待機期限超過・行失敗・リング取りこぼしに加え、最後の期限超過時のCPU速度、USB接続判定、空き容量、要求行サイズを出す。期限超過前のusb/spaceは-1。

最新ログの破損、途中の連番欠落、受理・除外snapshot数、接続後のカウンター増加を確認する。接続前や起動中のring_drops累計だけで不合格とはしない。前回と同じX3手順でよく、手入力とキャッシュ削除は不要。

## v5: 全行分の空き容量を事前条件にしない

v4はCPU 160 MHzのまま、USB接続判定1・空き62/63 B・要求194/193 Bで期限超過した。24行欠落とfailed_linesの増加24が一致し、10件のsnapshotは受理できた。本体リング取りこぼしは接続後526のまま増えなかった。

Arduino 3.3.7の `availableForWrite()` はTXリングの空きを照会するだけで送信割り込みを起こさない。`write()` は一部をenqueueしてTXを起動し、残りを送る。v5はUSB接続だけを待ち、全行分の空き容量の事前待機を外して、1回のwriteに送信を任せる。SDK内部のwriter mutexと50 msの進捗待ちは維持する。短い書き込み結果を検出した場合は再同期し、無条件再送はしない。CPU通常速度・接続待機250 ms・自動snapshot10秒はv4のまま。

C++ホストテストでは「空き62 Bから194 Bを送る際、書き込み呼び出しで初めてキューが動く」モデルについて旧事前条件は期限切れ、新経路はwriteを呼ぶことを確認する。満杯、部分書き込みを再送しない、接続断、接続回復も確認。これは実USBの動作を検証する実機テストの代わりではない。

診断行は `[HTRACE] v5 transport`。ready_timeoutsは接続待機期限超過のみとなり、空き容量不足をこの値には含めない。failed_linesとの差がSDKの短い／失敗した書き込みを示す。次の実機ログで途中連番欠落・破損・接続後のring_drops増加・完全snapshotを確認する。

## v5 X3確認結果（2026-09-26）
device-monitor-260926-093831.logでは4333有効レコード、破損0、途中連番欠落0、9件の完全snapshotを確認。ready_timeouts/failed_linesは0、ring_dropsは526で接続後増加なし。起動時の取りこぼしがないという意味ではない。通常default/gh_releaseは診断hookを含めない。所有権変更後のログも正常に解析できたが、操作経路が異なるため削減量の比較には使わない。
