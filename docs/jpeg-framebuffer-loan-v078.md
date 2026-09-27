# v0.7.8 P0-2: JPEGDEC FrameBufferLoan

> 2026-09-27: この文書は試験版ごとの経過を含みます。現在のmain統合・実機受入状況と継続調査は[v0.7.8リリース監査](development/v0.7.8-release-audit-ja.md)を参照してください。

P0-1の `18d8806f` を起点とする `feat/v0.7.8-jpeg-framebuffer-loan`。
X3/X4の基本動作・中断再開・通常heap経路は確認済み。実際の低MaxAlloc時の自動切替と成功率は未検証。

## 動作

- 通常は既存のheap経路。総空き36,864 bytes以上、MaxAllocがsizeof(JPEGDEC)以上の場合に確保する。
- heap条件を満たさない場合、またはnew(nothrow)が失敗した場合、明示的に許可されたcache-only呼出しで
  FrameBufferLoan/BuildScratchを使用する。貸出経路でもfile/band用に16 KiBの総空きを要求する。
  bandは別途mallocの成否を確認するので、この値は任意の画像での成功保証ではない。
- 通常描画、JPEG寸法取得、PNG、Home縮小表紙は今回の貸出切替の対象外。
- `RenderConfig::framebufferInvalidated` が非nullの呼出しだけが貸出を許可する。
  貸出を取得するとtrueになり、失敗した場合も呼出側が全画面を描き直す。
  `ImageBlock::ensurePixelCache()` の既定値はnullのままで、暗黙に読書中の画面を破壊しない。
- 既に貸出されたBuildScratchがある場合はそのclaimだけを試し、他のclaimを奪わない。
  外部所有のloanを勝手に返却しない。配置はalignof(JPEGDEC)に合わせる。
- 解放順はファイルclose → PixelCache cleanup → JPEGDEC destructor → scratch release → loan返却。
  placement-newされたオブジェクトをdeleteしない。
- 全書籍生成・書籍を開く際の一括生成では貸出を許可。画像処理から戻った後、貸出があれば
  背景、タイトル、中断案内、進捗popupを再構築する。キャッシュ済みなら貸出も再描画も行わない。
  screenshot/中断の入力確認は貸出返却・進捗再描画後の既存チェックポイントで行う。

## メモリ

JPEGDECは17,884 bytes。SDKの貸出全量はX4=48,000、X3=52,272 bytes。
整列済みの場合の残りはX4=30,116、X3=34,388 bytes。ただしBuildScratchは単一claimであり、
今回この余りをband等へ再分配しない。bandとファイル管理はheapのまま。
PNG=58,464 bytesはどちらにも収まらないため、その配置方法は変更しない。

## 診断ビルド

`jpeg_loan_test` はdebugを継承し、`JPEG_FRAMEBUFFER_LOAN_TEST=1` を付ける。
heapを消費するfault injectionではなく、許可済みJPEG cache-only呼出しの配置先だけを強制する。
通常描画のJPEG、PNGは強制対象外。default/gh_release/debugに強制フラグはない。

正常な貸出のログ例（値は実機で確認する）：

```text
[IMEM] JPG start mode=cache ... decoder=17884 ...
[IMEM] JPG storage=framebuffer decoder=17884 scratch=48000 ... forced=1
[IMG] Cache written: ...pxc6 ...
[IMEM] Restored progress UI after JPEG framebuffer loan
```

X3のscratchは52,272 bytesが想定。通常版でheapに余裕がある場合はstorage=heap、forced=0。
貸出を強制して成功しても、実際の低heap/断片化状態で成功することを証明するものではない。

## 自動検証

- `test/run_decoder_loan_test.py` は本番LoanedDecoderとBuildScratchを使い、heap失敗からの切替、
  整列、他者claimの拒否、外部loanの保持、容量不足、close/destructor/release/restore順序を検証。
  rendererはhost stubであり、実際のSDK/E-Ink復元は実機で確認する。
- 実JPEG 8画像 × 4縮小率 × X3/X4容量の比較。56組で復号成功・画素一致・領域外書込みなし。
  wide_scaling_test.jpgの8組はPC上の既存heap経路でもerror=2となり、貸出経路も同じ結果。
  この横長画像はX3/X4でキャッシュ生成成功を確認。ホストJPEGDEC自身の未定義動作検出は無効化し、所有権テスト側には有効。
- P0-1のPixelCache allocation・書込み・close失敗、無効cache再生成テストを継続。
- default、gh_release、debug、jpeg_loan_testのビルド。

## X3 / X4での確認

最初は前回の3冊だけでよい。重い書籍での長時間試験は基本動作確認後に行う。

1. **jpeg_loan_test版**へ更新し、シリアルログを115200bpsで採取する。
2. 3冊のキャッシュを削除してから、書籍を開かず「全書籍キャッシュ生成」を実行。
   JPEGでstorage=framebuffer / forced=1、PXC生成成功、進捗UI復元が記録されること。
   進捗画面のタイトル・中断案内が欠けず、白画面や操作停止にならないこと。
3. 設定を変えずキャッシュを削除せず、もう一度全書籍生成。
   既存22画像が有効で、JPEGの再decode・貸出が発生しないこと。
4. JPEG・PNGの表紙/本文画像を開き、生成済みcacheから表示でき、画像前後の残像が従来どおりなこと。
   JPEG画像を含む書籍の初回一括生成（読書開始時の生成案内）も、別途その書籍のcacheを削除して確認。
5. 貸出を含む一括生成を中断してから再実行し、進捗画面・通常読書に戻れること。
   縦/横、4方向orientationでも進捗復元と画像再表示を確認する。SDフォントあり/なしも確認する。
6. **通常debug版**でもcacheを削除して生成し、余裕がある状態ではstorage=heap / forced=0になることを確認。
   重い書籍や長時間読書後に本当にMaxAllocが17,884 bytesを下回った場合は、その前後をログに残す。

初回から2回目までを `x3-jpeg-loan.txt` / `x4-jpeg-loan.txt` として保存。
強制版は貸出経路で全画面再描画するため、通常版より生成が遅くなる場合がある。
P0-1で見つかったCSS解析不足と長時間生成後の自動スリープは、今回変更していない。

## 変更範囲

`LoanedDecoder.h`、JPEG converter、RenderConfig、ImageBlock、BuildScratch、
EpubReaderActivity/GenerateAllCacheActivityの一括生成側、専用テストとPlatformIO診断環境。
PXC6形式・dither・画像配置計算・idle schedulerは変更しない。

## 2026-09-25 実機検証結果

検証したファームウェアのbuild IDは `18d8806f-dirty-3e1978ea`。コミット前の同一ソースで、default / gh_release / debug / jpeg_loan_testの4環境をビルド済み。

- X3/X4とも専用診断版でJPEG貸出・PNG生成・進捗画面復元を確認。初回3冊22画像、2回目22画像有効・再decodeなし。ユーザーから表示上の問題なしとの報告。
- 追加の全書籍中断・再実行試験では、両機とも最終的に5冊64画像すべて有効、再生成なし。
- 単体生成の中断・再開後、JPEG貸出・PNG生成に成功。JPEG/PNGのPXC表示完了ログを確認。
- 通常debug版では両機ともJPEG `storage=heap / forced=0` で成功し、2回目64画像を再利用。
- 短い本で中断の反応が分かりにくいとの申告があったが、大きい本では正常に反応したとの追試報告。原因は未確定。入力処理は変更していない。
- CSS解析時の低heapと、キャッシュ削除後のHome縮小表紙生成エラーは別件として未解決。
- ログに一部行欠落あり。貸出のstorage行数だけで失敗件数を推定しない。

残る検証は実際の低MaxAlloc状態での自動切替、4方向すべて・SDフォントなしなど未報告の組合せ。
小規模書籍（10章以下かつ256 KiB以下）は読書開始時の確認画面を省略する。今回の単体生成は読書メニュー経由で確認しており、初回確認画面からの起動は未検証。
