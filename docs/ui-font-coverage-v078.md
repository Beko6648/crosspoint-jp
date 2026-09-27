# UIフォント拡張 v2（JIS第2水準・記号）

基点e9a5989a。内蔵21px UIフォントを3898→7190文字へ拡張。既存3898文字の幅とbitmapは完全保持し、3292文字のみ追加した。データ増分は3292×(63byte bitmap+2byte codepoint+1byte width)=217272byte。静的Flashのデータであり、同量のheapを常駐確保しない。

JIS X 0208第2水準3390文字中243文字が既存、3147文字を追加。EUC-JPの48〜84区で照合した。非漢字1〜15区の不足105文字、全角ASCII互換文字の不足10文字、丸数字21〜50の30文字、特殊括弧8文字も対象（重複あり）。追加3292文字のうち3291文字はNoto Sans JP Semibold、≒（U+2252）だけはSource Han Sans SC Mediumのcmapから補完する。ローマ数字50/100/500/1000と小文字版の8文字は元フォントにないため対象外。

目次のEpubReaderChapterSelectionActivityはUI_10_FONT_IDで幅計算・省略・描画する。同じ内蔵UIフォントを使う一覧・Home等にも反映。本文のSDフォントは変更しない。reader-modは選択SDフォントをUIにfallbackする設計で、固定の文字数とは比較できない。

## 再生成・検査

基点e9a5989aのヘッダーを入力として、≒を先に補完し、残りをSemiboldで追加する。生成元フォントは外部入力（リポジトリに含めない）。

```powershell
'2252' | Set-Content -Encoding ascii ui-fallback-codepoints.txt
py -3 scripts/generate_cjk_ui_font.py --size 21 --font 'SourceHanSansSC-Medium.otf' --codepoints-file ui-fallback-codepoints.txt --extend-existing-header
py -3 scripts/generate_cjk_ui_font.py --size 21 --font 'NotoSansJP-SemiBold.ttf' --codepoints-file scripts/codepoints/ui_jis2_symbols.txt --extend-existing-header
py -3 scripts/check_cjk_ui_font.py
py -3 test/check_ui_font_coverage.py
```

全角＾と｀は共通baselineでは枠外になりbitmapが空白だったため、新規字形だけ、描画結果が空白なら文字bboxが枠内に入るbaselineで再描画する。既存字形は再描画しない。全追加文字の非空bitmap、幅、全JIS第2水準、旧字形保持を検査した。ビルド前チェックに追加リストを含め、コメント中の符号位置を収録済みと誤認しないよう実codepoint配列のみを解析する。

## 実機確認（まずX3）

1. yomuka-ui-font-coverage-v2.binを通常のSD更新で適用する。ボタン確保削減の試験版とは別ファームで、その変更は含まない。
2. 羞贅卍＊餡_UI文字確認v1.epubをSDへコピー。一覧のファイル名、開いた後にHomeの書名で5文字が表示されることを確認。
3. 本を開き、読書中メニューから目次を表示。7章の名前で漢字、全角記号、丸数字、特殊括弧、矢印等を確認する。特に＾と｀が空白にならないこと。本文の字形はSDフォント依存なのでUIの合否と分ける。
4. 横向きのUIでも目次・一覧を確認し、欠け・行間の乱れ・省略表示の異常がないこと。長い章名が従来どおり省略されること自体は正常。
5. 既存の設定メニュー、日本語の書名・目次、通常読書とHomeへの往復が以前どおりであることを確認。

キャッシュ全消去は不要。追加フォントの表示はファーム適用で変わる。シリアルログを記録し、目視結果とともに提示する。X3の結果を確認してからX4へ進む。X3・X4ともv2の太さ・表示に問題なしとのユーザー確認を取得。

## v1ビルド結果（表示は不合格）

最終default / gh_releaseは成功（66.979秒／22.049秒）。Flash 4483567 / 6815744 byte（65.8%）、静的RAM 49036 byte。両ELFに修正後の全7190字のbitmap列が一致して含まれることを確認。v1は追加字形の太さで不合格となり、v2で修正した。

## v2: 追加文字の太さ修正

v1は追加字形をBoldで生成したため、実機で追加文字だけ太いとの指摘があった。v2では追加3291文字をNoto Sans JPのweight=600（Semibold）で再生成。≒だけは同フォントにないためSource Han Sans SC Mediumで補完（1文字の明示的な例外）。既存3898文字は基点e9a5989aの幅・bitmapを完全保持。既存の「日本語文字図書館設定明朝漢作品目次上下左右」21文字と、同じ条件で生成したSemiboldのbitmapが全て完全一致することを確認した。

生成元はGoogle Fonts公式リポジトリのofl/notosansjp/NotoSansJP[wght].ttf。fontTools.varLib.instancer.instantiateVariableFontでwght=600に固定したTTFを使用する。再生成時は拡張済みヘッダーを入力にすると追加字形も保持されるため、基点のヘッダーを入力として使用すること。

v1のEPUBをそのまま使用でき、キャッシュ削除は不要。X3で既存文字と追加文字の太さ・欠けを再確認し、その後X4を確認する。v1の表示は不合格、v2はX3・X4ともユーザーが太さ・表示に問題なしと確認済み。

生成元可変TTF SHA256: `c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f`

補完元: https://raw.githubusercontent.com/adobe-fonts/source-han-sans/release/OTF/SimplifiedChinese/SourceHanSansSC-Medium.otf

## v2ビルド結果

default / gh_release成功。両ELFに全7190字のbitmap列が一致して含まれることを確認。既存3898字の保持、追加3292字の非空、追加字の生成元cmap（≒のみ明示的補完）を確認。X3・X4の実機表示確認は完了。

## 実機受入結果

ユーザーからv2について「太さも表示も問題ありません」、続いて「X4OKです」との報告を受け、X3・X4の表示確認を完了した。今回の判定は画面の目視によるもの。v2のシリアルログは不要と案内しており、ログによる表示の合否判定は行っていない。
