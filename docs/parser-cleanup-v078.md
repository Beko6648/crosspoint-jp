# 章解析の開始前中断時の後始末

基点 main 7aaabbef。device-monitor-260926-235308.logのELF SHA256先頭64c33e42dが保存済みmain回帰確認用ELFと一致。スタック内アドレスはHalFile::close、ChapterHtmlSlimParser::stepParseAndBuildPages（2036/2040行）、EpubReaderActivity::pregenerateCacheの中断判定に対応する。スタック走査は正式なフレーム復元ではないが、assert内容とコード経路が整合する。

stepParseAndBuildPagesはファイルを開く前にcancelFnを評価し、trueならfail→closeIncrementalParserを呼ぶ。従来は未初期化HalFileにもcloseを呼んでおり、impl != nullptrのassertに到達する。XML確保失敗、解析開始前の破棄でも同じ後始末を通る。

修正は開いているincrementalFileだけ閉じる。HAL全体のassertは変更しない。既に終了したパーサーのデストラクターでも再度closeを呼ばない。

実際のcleanup関数本体を抜き出したホストテストで、未開始・open失敗・open済み・後始末の繰り返しを検査。ファイルとXMLは厳格なテスト用実装なので、実SD/FreeRTOS上の動作は下記の実機確認で評価した。

## 実機確認（X3・X4完了）

1. 確認用EPUBだけキャッシュを削除し、本ごとの生成開始直後に戻る操作で中断。数回繰り返し、再起動やフリーズがないこと。
2. 同じ本を中断せず生成し、完了後の再実行が再生成せず終了すること。
3. 本文・画像を開き、Homeへ戻れること。

X3: device-monitor-260927-101800.log。ユーザーから1〜3 OKの報告。中断に整合するsection 3失敗後に同章を再生成し、残りの章も生成。クラッシュ・再起動なし。

X4: device-monitor-260927-102425.log。ユーザーから1〜3 OKの報告。中断に整合する生成失敗後の再生成を確認。クラッシュ・再起動なし。ただしタイミングによってキャンセルが利かないとの報告があり、短押しを現在のポーリング方式で取りこぼす可能性は別件として残す。INFOログでは開始前中断の厳密なタイミングや各失敗原因を確定できない。

今回の実機試験対象は本単体の生成。一括生成の中断・再開を今回の修正の受入済み項目には含めない。

## ビルド・検査

default / gh_release成功（67.178秒 / 65.712秒）。run_parser_cleanup_test.py成功。実機確認したparser-cleanup-v1から本体コードの追加変更なし。

## 別件のCSSメモリ不足

以前の一括生成ログの約63KBの空きはEpub::parseCssFilesの64KiB事前条件を下回る。これは今回のassertとは別の経路。GenerateAllCacheActivityはload前にfont cacheとkern/ligatureを既に解放しているため、単純な解放忘れとは断定できない。大量書籍の保持データなどを別途調べる必要がある。閾値は変更していない。過去のX4 section 6生成失敗の原因も、この修正で解決したとは扱わない。
