# X3/X4/X4Cの回帰確認

この手順はSDK/HAL・描画・先読み・ネットワーク変更時の共通入口。掲載されている項目は、自動的に実機確認済みになるものではない。
詳細な表示基準は [device-verification-ja.md](device-verification-ja.md)、X4Cは [Experimental案内](../x4-classic-experimental-ja.md) を参照。

## 自動確認

Linux/WSLでPython 3、C/C++コンパイラ（cc/c++）、初期化済みfreeink-sdk submoduleを用意する。

```sh
python3 test/run_hardware_regression.py
```

13スイートを実行し、`build/hardware-regression/`へ各ログと`summary.json`を保存する。
`--output <directory>`で出力先を変更できる。1件失敗しても残りを実行し、全体の終了コードは失敗になる。
SDK検出、X3 IMU補正設定、電源診断、X4C診断、描画待機、Web終了入力、縦書き字形、先読み入力/キャンセル/分割/終了、SDK表示ドライバーを対象にする。
各スイートはスタブや抽出関数を含む。実通信・実描画・停止電流の確認の代わりにはならない。

CIはこのhost確認と`default`/`gh_release`/`x4c`のビルドを実行する。既存Test Statusにhost結果を加える。
X4Cのビルド成功は正式対応の宣言ではない。CI変更はpush後の実行確認が別途必要。

Windowsで手元の通常版を上書きしないビルド例:

```powershell
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
$env:PLATFORMIO_BUILD_DIR='.pio/build-regression'
& 'C:\Users\pompo\.platformio\penv\Scripts\platformio.exe' run -e default -e gh_release -e x4c
```

各環境のFlash/RAMを基準版と比較し、firmware.binのSHA256を記録する。検証済みbinと新しいbinは別名/別フォルダーで保持する。

## 実機確認の共通順序

最初は普段のキャッシュを残す。必要に応じてテスト用の本1冊だけキャッシュを削除して本文/画像/章移動を再確認する。
全蔵書の削除は不要。本文AA機能は廃止済みなので、AA設定操作を求めない。

| 項目 | X3 | 通常X4 | X4C |
|---|---|---|---|
| 起動、縦横EPUB、ページ送り/戻し | 確認 | 確認 | 実機入手時 |
| JPEG/PNG、画像→本文、章移動、再開 | 確認 | 確認 | 実機入手時 |
| 画像手前/章末で待ってから入力・メニュー | 先読み中断確認 | 同左 | 実機入手時 |
| 自動ページ送りの表示・開始・停止 | 5秒ごと | 同左 | 実機入手時 |
| IMU診断・傾き送り | 向き/ON/OFF | 対象外 | 現時点で有効化しない |
| sleep/wake | RTCあり/なしを区別 | 電源OFF/起動（RTCなし） | RTC・SD再マウントは未検証 |
| 電源の電気的確認 | 消費電流は別測定 | GPIO13 latch解放は別測定 | 未検証 |
| SD更新/Recovery | 入口と実書込みを区別 | 同左 | 物理キー/書込み未検証 |
| Web接続・設定表示・本体終了 | AP/STAを区別 | 同左 | 実機入手時 |
| Web小ファイル転送・終了後の本再開 | 実施時に記録 | 同左 | 実機入手時 |
| APを3回終了した後のNoto Serif句読点 | ログ付き推奨 | 同左 | 実機入手時 |

AP連続試験では途中で本を開かず、3回終了後に同じ本・同じページを開く。USB接続とログ受信の有無を明記する。
異常時は本を開き直す前に画面とログを保存する。`loaded=1`は読み込み済みデータの再利用も含むため、低いmaxAlloc値だけで新規確保の成功と断定しない。

## 結果を残すテンプレート

変更ごとに別の記録へコピーして使用する。未実施・非該当・成功・失敗・保留を混同しない。

```text
対象コミット / dirty差分:
SDK revision:
端末 / build環境 / build ID:
bin SHA256:
比較基準 / Flash・RAM差分:
本・ページ / フォント・サイズ / 縦横:
キャッシュ状態:
USB接続 / ログ受信 / APまたはSTA / 接続端末:
実施手順:
期待結果 / 実際の結果:
画面・診断・ログの保存先:
未実施項目と理由:
既知問題 / 今回の変更との因果:
ユーザー実機確認の結果:
採用判断 / 未解決項目:
```

診断レポート、SD更新時のbin、実際に起動したbuild IDを混同しない。versionが同じでもbinが同一とは限らない。
生ログ・画面は確認用成果物に保存し、通常のソースコミットへ一括追加しない。

## v0.8.0で残る項目

- X3の最初のWebフリーズ: 再試行は成功したが原因未確定。
- X3の句読点異常: 32KB連続領域条件の改善はX3/X4で確認。最初の異常との因果は未確定。
- WiFi終了時の`netstack cb reg failed with 12308`: ログに残るがその後動作継続、原因・影響未確定。
- X3の傾き感度: v0.7.8でも反応しづらいという報告を維持。
- X4 GPIO13の電気測定、SD更新の実書込み、小ファイル転送は入口確認と分けて扱う。
- X4Cは実機未検証・Experimentalを維持。
