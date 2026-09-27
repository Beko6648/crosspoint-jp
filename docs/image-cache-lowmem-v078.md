# v0.7.8 P0-1: 画像キャッシュ低メモリ土台

> 2026-09-27: この文書は試験版ごとの経過を含みます。現在のmain統合・実機受入状況と継続調査は[v0.7.8リリース監査](development/v0.7.8-release-audit-ja.md)を参照してください。

2026-09-25。実機確認待ち。起点は origin/main `121380a4`、作業ブランチは
`feat/v0.7.8-image-cache-lowmem`、作業場所は `C:\Src\crosspoint-jp-image-cache-lowmem`。
元の checkout の未コミット変更、serial.txt、ローカル main は変更していない。

## 変更

- `ImageBlock::ensurePixelCache()` を分離。既定では font cache を解放せず、画面へ書かない。
  戻り値は AlreadyValid / Generated / Failed。将来の scheduler はこの区別を使用できる。
  x/y は描画時の位置を渡す必要がある。画面外の cache-only 要求は拒否する。
- 既存 `pregeneratePixelCache()` は積極的な font cache 解放と「新規生成時だけ true」を維持。
  通常描画の cache miss 時の既存 font cache 解放も変更していない。
- JPEG/PNG とも cache-only では begin/finalize 失敗を成功として返さない。
  通常描画は cache 書き込みに失敗しても画像描画を続ける既存フォールバックを維持する。
- PixelCache は必要な最大 callback 行数とゼロ補充1行だけ確保。
  PNG は1行+補充1行、JPEG は MCU の出力行数+補充1行。
  24 KiB の上限に補充行も含め、小さい画像は高さまでに制限する。
- abort と finalize で band をすぐ解放。close の失敗も検出し、途中ファイルを削除する。
  途中書き込み後に caching=false になった場合も finalize/破棄で除去する。
  削除自体が失敗した場合はログを出す。SD取り外しや電源断時の削除成功は保証できない。
- JPEGDEC/PNG は destructor で callback のファイルを閉じないため、共通 DecoderFileScope で
  正常終了・ヘッダーエラー・早期 return の全経路を閉じる。JPEG寸法取得も対象。
  decoder 本体の所有とは分離しており、次段階の placement-new を妨げない。
- 無効 cache は既存の寸法・長さ検証で拒否する。cache-only は削除に失敗したら停止し、
  生成後の検証に失敗したファイルも削除する。PXC6の符号化・dither・配置計算は変更なし。

idle scheduler、decoder の FrameBufferLoan 化、Home縮小表紙仕様、次章先読みは未変更。

## メモリ測定と根拠

ESP32-C3用 GCC 14.2.0 で実際の依存ヘッダーをコンパイルし、sizeof配列のシンボルサイズを
nmで取得した。ホスト64bitのsizeofではない。再現手順は
`test/measure_image_decoder_sizes.py --compiler <riscv32-esp-elf-g++> --nm <riscv32-esp-elf-nm>`。
PNG_MAX_BUFFERED_PIXELS=15424、PNGdec 1.1.6、JPEGDEC 8628297。
実機の空きheap・MaxAlloc・貸出成功量は未測定。DEBUGログで別途確認する。

| 項目 | bytes | 根拠 |
|---|---:|---|
| sizeof(JPEGDEC) | 17,884 | C3 ABI測定 |
| sizeof(PNG) | 58,464 | C3 ABI測定 |
| alignof(JPEGDEC), alignof(PNG) | 4 | C3 ABI測定 |
| X4 framebuffer貸出容量 | 48,000 | SDKの800×480/8、貸出時は全量を返す実装 |
| X3 framebuffer貸出容量 | 52,272 | SDKの792×528/8、同上 |
| JPEG本体配置後のX4 / X3残量 | 30,116 / 34,388 | 上記から算出、他の利用者なしの場合 |
| PNG本体配置時のX4 / X3不足 | 10,464 / 6,192 | 本体だけでも収まらない |

PNGの42～44KBという前提は今回の設定に一致しない。元checkoutもPNGdec 1.1.6だった。
今回decoderライブラリやscanline設定は変更していない。PNGを単一framebufferへそのまま
placement-newする案は現設定では不可。内部buffer分離など別設計の検討が必要。

JPEGの追加band量は `(min(最大MCU出力行数, 出力高さ)+1) × ceil(出力幅/4)`。
1:1時の最大行数は18なので、幅480なら2,280 bytes、幅528なら2,508 bytes。
拡大率によって増え、補充行込み24,576 bytes以下に制限する。独立したgray lineはない。

PNGの追加連続領域は gray line=元画像幅 bytes、上限7,712 bytes。
bandは `2 × ceil(出力幅/4)`：縦向き全幅X4=240、X3=264 bytes
（変更前2,040 / 2,244）。横向き全幅はX4=400、X3=396 bytes。
内部zlib/scanline領域は58,464 bytesに含む。画像形式ごとの内部scanline制限も継続する。
これら以外にファイルhandle、HalFile内部、パス文字列、SD層の小さなallocationがあるため、
単純合計を「保証される最低空きheap」と扱わない。

既存の総空きheap gateはJPEG=36,864、PNG=61,440 bytesを維持。
共通gateは実際のsizeofに対するMaxAllocも確認する。
総空き量を満たしても連続領域や追加bufferを確保できなければ失敗する。
今回まだdecoder本体の連続allocation制約は解消していない。

## 診断

debug環境のIMEM開始ログ：format、draw/cache、free、maxAlloc、decoder、出力寸法、cache path、
framebuffer容量、現在claim可能なBuildScratch容量。後続ログ：元画像寸法、gray/band必要量。
通常の読書でscratch=0は正常。実際に貸出中か、他処理がclaim中かによって変わる。
ログは観測のみで、将来の貸出獲得はclaimの成否で判断する。

通常buildでは開始・scratchのDEBUGログは含まれず、失敗時だけIMEM INFOを出す。
stageは gate / decoder-allocation / open / dimensions / line-buffer / cache-begin / decode / finalize。
gate判定時もdecoderサイズを出すため、freeが十分でmaxAllocだけ不足する状況を判別できる。
危険な実機heap圧迫機能は追加していない。低MaxAllocの自然発生をログで観測する。

## 自動検証

- default / gh_release / debug の3環境をビルド。
- `test/run_image_cache_test.py`：本番PixelCache writerでPXCバイト列一致、短い画像、
  allocation失敗、open失敗、header/途中/最終行の書込み失敗、close失敗、途中破棄、再試行。
  本番validatorで寸法不一致・切り詰めcacheの拒否と削除後の再生成。
  DecoderFileScopeの早期return時のcloseも確認。
- `git diff --check`。

ホストテストはSDの実I/O、decoderの実描画、E-Ink refresh、実heap断片化の代用ではない。

変更ファイル：

- `lib/Epub/Epub/blocks/ImageBlock.cpp`, `ImageBlock.h`
- `lib/Epub/Epub/converters/JpegToFramebufferConverter.cpp`, `PngToFramebufferConverter.cpp`
- 同ディレクトリの `PixelCache.h`, `DecoderFileScope.h`, `ImageDecodeDiagnostics.h`
- `lib/Memory/BuildScratch.cpp`, `BuildScratch.h`
- `test/run_image_cache_test.py`, `test/measure_image_decoder_sizes.py`, `test/image_cache/` のテストとstub
- `docs/image-cache-lowmem-v078.md`（本書）

## X3 / X4確認手順

まず通常defaultビルドで画像とUI、次にdebugビルドで同じ条件のシリアルログを採取する。
自動書込みは行っていない。配布した各BINは今回の試験用で、v0.7.8リリースではない。

1. `lowmem-jpeg-cover.epub`、`lowmem-png-cover.epub`、`lowmem-body-mixed.epub` をSDへ置く。
   前2冊は既存fixtureにcover-image/guideを付けた表紙テスト、3冊目はJPEG/PNG混在本文。
   画像自体は既存test/epubsのもの。実際に普段読む画像入りEPUBも併用する。
2. 各書籍のメニューからその書籍のキャッシュだけ削除し、JPEG表紙・PNG表紙・本文画像の
   初回表示を確認。次ページ→戻るでcacheからの再表示も確認。欠落・黒線・位置ずれがないこと。
3. debugで初回のIMEMとCache stream/Cache writtenを保存。再表示がLoading from cacheになり、
   同画像を繰り返しdecodeしていないことを確認。free/maxAllocは開始行を記録する。
4. 縦書き/横書き×4方向orientationで2～3を繰り返す。設定変更後はキャッシュを作り直す。
   SDフォントあり/なしで実施し、画像前後の文字欠け・操作停止も確認する。
5. 画像→本文→画像と移動し、画像前後の残像、白黒/階調、refreshがv0.7.7と同等なことを確認。
   Homeへ戻り、JPEG/PNGの縮小表紙と一覧表示が従来どおりか確認する。
6. 「全書籍キャッシュ生成」を実行してから再実行。cacheあり再表示、途中中断→再実行も確認。
   既存boolean APIは生成数用の意味を維持しているため、完了表示だけでPXC全成功とは判定せず、
   ログと各画像の再表示を確認する。再実行の既存preflightは欠落/無効PXCを再探索する。
7. 長時間読書後、まだ開いていない画像ページを開く。失敗時はIMEMのstage/free/maxAllocと
   cache pathを保存。gateならdecoderサイズとmaxAllocを比較する。
   対象PXCが残っていないか確認し、再起動でheap条件を戻して同ページを開き直す。
   画像が復旧し、cacheから再表示でき、placeholderが固定されないことを確認する。
8. 無効cacheの再生成試験はテスト書籍のPXCをPCへ退避してから、そのコピーを短縮して置き戻す。
   端末で同ページを開き、無効cache削除→再生成を確認する。本番書籍のcacheでは行わない。

結果は「機種 / ビルド / 書籍・ページ / 書字方向 / orientation / SDフォント /
初回・再表示 / IMEMログ / 残像」を揃えて返す。自然発生の低MaxAlloc失敗がない場合、
「再現なし」と記録し、耐断片化が実機確認できたとは扱わない。

## 次段階

JPEGDEC FrameBufferLoanへ進むAPI・cleanup・診断の土台は準備できた。
既存loanはframebufferを上書きし、返却時に白で初期化するので、cache-onlyフラグだけで
画面状態の復元が保証されるわけではない。利用タイミング、4byte整列、排他claim、
decoder破棄→scratch返却→framebuffer復元の順序、入力/refreshをP0-2で実機検証する。

PNGは現設定の単一loan案を採用しない。実機heap値、失敗率、Home/画像/SDフォント回帰は未確認。
電源断中の原子的なcache公開やSD障害の全経路まで保証する変更ではない。
