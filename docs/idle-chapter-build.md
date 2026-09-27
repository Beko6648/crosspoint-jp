# 次章先読みの分割実行（v9実機回帰確認済み）

> 2026-09-27: この文書は試験版ごとの経過を含みます。現在のmain統合・実機受入状況と継続調査は[v0.7.8リリース監査](development/v0.7.8-release-audit-ja.md)を参照してください。

## 現在の状態（2026-09-26、v9）

- X3/X4: 通常速度で先読み完了、章cache再利用、短押し、Home復帰、画像・表・長文、画像後の句読点・括弧・ルビを確認。
- X3: PNG初回gate失敗を再現し、v9で縦書き字形の条件付き解放が1回実行。max57332→106484B、free118788→131900B。初回PNG表示正常。X4は追加回収が不要な状態で表示正常。
- X3/X4: 本ごとの生成・全書籍生成で中断、再開、完了、完了後再実行で再生成なしをユーザー確認。ログでは中断章のみ再生成し先行章を再利用。完了画面と再実行結果はINFOログだけで断定せずユーザー確認と組み合わせた。
- v10通常版有効化はX3/X4実機確認後にコミット済み。v11 default/gh_releaseビルド成功。一括生成後の一覧更新もX3/X4で確認済み。
- 実装・診断を5つのコミットに分離。未push。以下のv1〜v9節は開発履歴で、各節の「実機待ち」「未コミット」はその時点の状態。本節を現在の判定とする。

### 残る制約

画像混在章の最初のstepはX3約1秒、X4約0.65秒。画像展開は出力境界で中断可能になったが、ZIP stateを保持してmain loopへ戻る分割再開は未実装。実際に画像出力途中で入力された場合の遅延・cleanupは実機で未確認（ホスト試験は成功）。10秒診断待機中の中断試験や一般的な一括生成中断と混同しない。
HTML展開前のfont回収順は未変更。HTML展開中にboot-wide最小heapが下がることはv8/v9で確認。閾値を下げず今後の改善対象とする。通常版への次章idle有効化は別変更として判断する。

### 実機ログの対応

- X3 PNG回復: device-monitor-260926-183923.log
- X4 読書回帰: device-monitor-260926-190748.log
- X3 単体/一括: device-monitor-260926-191154.log / device-monitor-260926-193313.log
- X4 単体/一括: device-monitor-260926-193740.log / device-monitor-260926-195247.log

原ログと判定JSONは会話workspaceのoutputsに保存。シリアルログ・bin/elf・EPUBはソースコミットに含めない。

ブランチ: feat/v0.7.8-idle-chapter-build。開始点 d0574ed9。
リポジトリ確認用スキルに沿って既存の独立checkoutを使用。元のC:/Src/crosspoint-jpの変更・ログは保持。

## 実装
- 専用idle_chapter環境だけでIDLE_CHAPTER_BUILDを有効化。default/gh_releaseの先読み開始方式は従来のまま。
- 描画完了と最後の入力から1.5秒待ち、章末の1ページ手前で次章が未生成なら準備する。
- ChapterHtmlSlimParserはExpat・入力ファイル・本文/ルビ/アンカー状態を保持し、従来と同じ1024バイトの入力を1回ずつ解析する。同期呼び出しは同じstepを完了まで繰り返す。
- Sectionはparserとページ位置一覧を保持。全ページを生成し既存validationを通ったときだけ最終ファイルへrenameする。中断時にparser、CSS、一時HTMLと一時章ファイルを解放する。
- 入力、描画、読書終了で生成途中の状態を破棄する。入力を検出したGPIOイベントは既存のidle入力引き渡し機構で通常処理へ渡す。
- 各sliceはRenderLockとMeasureOnlyScope内で実行する。生成中は画像idle prefetchを同時に走らせない。自動ページ送り、ルビ調整、傾き操作中は開始しない。
- speculative buildでSD-font cacheを積極的に解放しない。メモリ不足なら見送り、通常の章移動時に既存の生成経路へ戻る。
- 同じ読書セッションでは試した次章を再試行しない。入力中断も含む。再入場で新しい試行が可能。
- PXC6、章キャッシュ形式、CSSキャッシュ版に変更なし。

## 限界・実測対象
これは厳密な時間制限付き・1ページ単位のコルーチンではない。1024バイトごとに返す構成で、段落末のlayoutは一度に複数ページを作る場合がある。入力キャンセルは行受け入れ時にも確認する。
準備（ZIP展開、CSS使用セレクター走査/読み込み）と最終validation/renameは現在同期。ZIP出力チャンク間で入力を確認するが、準備全体を複数loopに分割したわけではない。長い準備・段落が操作を阻害するなら、その箇所を追加分割してから通常版へ採用する。
診断ログNCHはprepared、25ms超のstep、完了/失敗、入力中断、cache hit/heap skipを記録。準備の失敗・cancelも記録する。実機で最大処理時間・キーイベント保持・表示・再利用を確認する。

## ホスト検証
productionのstep/lifecycleメソッドを取り出して実Expatと組み合わせ、日本語/ルビ/タグイベントが同期呼び出しと分割呼び出しで一致することを検証。キャンセル、低メモリ、open/read失敗、壊れたXML、末尾余剰データ、終了後二重呼び出しも確認。
レイアウトとストレージはstubであり、このテスト単独ではページ配置・Section公開・UI並行処理の正しさを証明しない。既存layout_memory/page_ownership/batch_cache/css_streaming/text_emphasisも成功。

## 実機検証
outputs/idle-chapter-v1-guide.md参照。まずX3、結果確認後X4。未コミット、未push。

## v2: Home移動時のデッドロック修正
X3のv1でHome移動時に停止。ActivityManager::exitActivityはRenderLock取得済みでonExitを呼ぶが、v1でonExit内に追加したRenderLockが非再帰mutexを再取得して待ち続ける。先読みが見送られてidleChapterが空でも発生する。
v2はonExitの再取得を削除し、呼出元のロック下で一時状態を破棄する。productionのonExit先読みcleanup部分を使うホスト回帰テストで、先読みあり/なし双方の破棄を検証。旧二重取得を入れるとテストが失敗することも確認。
ログdevice-monitor-260926-155632.logでは別途、次章CSS準備はfree79728 B < 必要98304 Bで安全に見送られた（reason=5、cancelled=0）。この見送りをフリーズ原因と混同しない。v2でメモリ閾値を変更しない。まず同じ操作でHomeに戻れることを実機で再確認する。

## v3: 閾値を維持した条件付きfont作業キャッシュ解放
v2のX3ではHome移動のフリーズ解消をユーザー確認。最新ログdevice-monitor-260926-161733.logは3回のCSS準備見送り（free79524/88192/79196 B、need98304 B）、通常章生成2回（4345/4332ms）、最後にcache hit。incrementalのprepared/stepはなく、成功や中断を検証済みとはしない。
96KiBは大きい章用64KiB＋CSS保持時の余裕32KiB。v3でこの判定を下げない。CSS選別データはload後すぐscopeを抜けて解放してから最終admissionを行う。
idleのZIP展開後・CSSロード前に、既存reserve＋4KiB（選別と小さいCSS用の目安）または既存16KiB連続領域に足りないときだけFontCacheManager::clearCacheを1回呼ぶ。字形bitmap、miniData、advance tablesを解放するがfont本体、coverage、kern/ligature、縦書き置換データは保持する。無条件の全SD-font解放や反復retryは行わない。CSSロード後の厳格判定とlayout admissionは従来どおり。
NCH font headroomでrelease有無、free/maxの前後、reserveを記録する。回復が足りるか、後続描画の再読込による引っかかりがないかは実機待ち。v1の「積極解放しない」方針への追加は、この不足時1回の限定回復だけ。
BudgetTestは十分な空きでは解放しない、不足/断片化では1回だけ、fontなしは何もしないことを検証。実際の回復量・次ページ表示はホストテストでは証明しない。

## 入力中断の確認用window
X3 v3完了試験は51ページの分割生成が成功（準備280ms、完了まで約7.1s、step96〜218ms）。作業キャッシュ解放でfree89152→116444 B（27292 B回復）。描画乱れなしとのユーザー報告。中断試験1回目はNCH記録がなく、通常生成4930msのみで中断は未確認。3秒の目安は描画更新時間を含めると開始前になるため不確実だった。
専用env idle_chapter_cancelにのみIDLE_CHAPTER_CANCEL_WINDOW_MS=10000を定義。parserのstepがPendingでpageCount>0になった後、1回だけ10秒待機する。sleepせず通常loopへ戻るため、RenderLockやPowerLockを保持したまま待機しない。cancel input/描画/onExitの既存cleanupを通す。時間経過後は自動的に続行し、再び待機しない。
通常のidle_chapter/default/gh_releaseには待機処理を含めない。これは途中状態の破棄と入力の引き渡しの試験で、layout実行中の最悪入力遅延を証明するものではない。
PauseTestはページ出力前に待たない、10秒境界、1回限り、リセット、millis桁あふれを確認。専用の新しいEPUBで既存cacheと区別し、手動全消去を不要にする。

## input-v5: 入力エッジで描画ロックを待たない
ユーザーが章先頭の短押しで次章へ移ると報告。device-monitor-260926-170908.logはSection0→1→2のforeground生成、NCH先読み記録なし。入力押下時間・ページ数のログがなく、ログだけでは原因確定不可。
コードではidle追加のloop先頭で全press/release時にRenderLockを待っていた。描画中のpressでmain loopが止まると、GPIO releaseの次回検出まで押下時間が延び、700ms超の章skipになる経路がある。cleanupをゼロ待ちのtry-lockへ変更し、取れない場合はrender/onExit側の既存cleanupに任せる。既存の長押し閾値・ボタン割当は変更しない。
production RenderLock try constructor/destructorのホストテストで、busy時は待機0で他者のロックを解放しない、取得時だけ解放することを確認。実機ではまず短押しの1ページ移動・通常長押し・Homeを確認する。NCH turn held/skip、view spine/page/countを専用診断環境で記録し、先読み待機版は引き続き10秒windowを持つ。

## v6: 通常速度での確認へ
X3 input-v5でユーザーが短押しの改善を確認。device-monitor-260926-172252.logは1ページ生成後のcancel input、その後の通常生成51ページと章移動を記録。短押しはskip=0。中断時はHome/再入場に相当し、戻る1ページ操作そのものをその時点で証明したとはしない。冒頭のZIP write errorはcancelled=1の準備中断に伴う。
v6は同一ソースをidle_chapter環境でビルドし、10秒の診断待機を含めない。default/gh_releaseはv5の成功済みビルドとソース一致を確認し、再ビルドしない。新しい識別子・ファイル名の通常速度v6 EPUBを用意し、初回の先読み完了、通常短押し、Home、キャッシュ再利用をX3→X4で確認する。通常速度v6の実機結果は待ち。通常版への有効化・コミットはまだ行わない。

## v7: mixed fixtureの初回step診断
通常速度v6はX3/X4で表示・操作の異常なし。mixed fixtureはX3 34ページ/4945ms（準備込み）、X4 37ページ/3744ms。最初のstepは1029/639ms、X4のboot-wide Min Freeは生成後15180 B。これだけでは各allocationのピークや入力遅延を断定できない。
第2章先頭1024bytesにJPEG/PNG画像・表が入る。parser callback内で画像ZIP展開（8192B転送buffer指定）と寸法取得、表配置が同期実行される。PXC生成は後の表示時であり、この段階の遅さをPNG全画像decodeと混同しない。
idle_chapter_probe環境だけでIDLE_CHAPTER_STAGE_DIAGNOSTICSを定義。NCPはparser-step/image-extract/image-dimensions/table/text-layoutの時間とfree/max/bootMinの境界値を記録。ネストした時間は重複し、ログ時間も外側計測に入る。free/max境界値は途中ピークを保証せず、bootMinは起動全体の値。診断は同期foreground parserでも出るためNCHと時刻で対応する。処理やreserveの変更は行わない。
新fixtureはOPFの書名・IDとファイル名のみ変更し、XHTML/CSS/画像はmixed-v6とbyte一致。先頭chunk条件とcold cacheを両立。まずX3の実測待ち。

## v8: 画像展開のキャンセルと準備診断
CancellableImageOutputがcancelFnの検出を保持し、ZIP出力chunk境界で書込を停止。ファイルclose後に中断判定し、部分画像の削除を試み、parserの既存abortフラグで終了する。placeholder経路へ入らずSectionの一時章を破棄する。ストレージ削除自体の失敗は物理媒体依存であり、成功を保証しない。次回画像抽出は既存の書込経路で再実行される。通常のI/O失敗と入力キャンセルは区別する。
新しいホスト試験は実adapterとproduction callbackのcleanup部分を使い、出力途中キャンセルの保持、後続書込なし、short writeとの区別、末尾chunk後のキャンセル、部分画像削除とplaceholder回避を検証。実SD/ZIP/入力タイミングの結合確認を代替しない。既存incremental parser試験も実行。
診断helperをChapterStageProbe.hへ分離し、専用環境でHTML展開、CSS、incremental parser確保の前後を計測。時間・heap境界値・boot-wide最小値の意味はv7と同じ。閾値・font回収順・ZIP辞書寿命は変更しない。実機中断遅延は未測定。まず新しいv8 fixtureで通常完了と準備メモリを測定する。

## v9: PNG drawの連続heap不足時だけ縦書き字形を回収
v8 X3はPNG初回欠落。IMEM gateでdecoder58464Bに対しmax57332B、free119180B。章先読みは正常に34ページ完了、画像抽出キャンセルなし。Home再入場後max65524Bで表示復旧。コード変更がheap配置に及ぼした厳密な因果は未確定。
ImageBlockの通常cache missは既にclearCache/freeKernLigatureDataを実行しており、さらに無条件に繰返さない。PNG converterで通常描画かつfree60KiBまたはsizeof(PNG)の連続条件に足りない場合だけFontCacheManager::releaseSdFontVerticalGlyphsを1回実行して既存gateへ進む。cache-only/idle prefetchの字体保持とbudget判定は変更しない。効果が不足すれば既存gateで安全に失敗する。
GfxRenderer::drawの縦書き経路はloadVertDataを描画冒頭で実行するので、後続本文は再ロードできる。converter実行中に文字描画は行わない。実機では句読点・括弧・ルビ・画像前後の回帰確認が必要。PngDrawRecoveryTestで十分なheap/境界/断片化/総量不足/fontなし/cache-onlyを確認。v9実機結果は待ち。準備HTML展開のbootMin54656→31684をv8診断で確認できたが、今回は回収順やZIPメモリ構造を変更しない。

## v10: 通常構成への有効化（X3/X4確認済み）
defaultとgh_releaseでIDLE_CHAPTER_BUILDを定義。idle_chapter環境はIDLE_CHAPTER_DIAGNOSTICSを追加する診断環境として残す。turn/viewとstepの詳細は診断環境のみ、通常版は先読み終了時のresult/pagesと既存のprepare/回復等を記録する。NCP計測と10秒待機は通常版に含めない。解析/メモリ回復/キャンセルのアルゴリズム変更なし。実機確認後に有効化を別コミットにする。

## v10実機結果とv11一覧状態修正（X3/X4確認済み）
v10通常版X3/X4で1〜4正常をユーザー確認。X3は34ページ4935ms、PNG縦書き字形回復57332→106484B。X4は37ページ3664ms、後続cache hit確認。通常版への有効化は実機確認済みだが、追加報告の一覧状態問題を確認してからコミットする。
全書籍生成は完了markerを更新する一方、FileBrowserはbook-list-status.binの既知状態を優先し再取得しない。GenerateAllCacheActivityに索引無効化がなく、古いResumableが残る経路を確認。v11は生成開始前に派生索引ファイルのみ無効化し、次回一覧の可視範囲で実ファイルから再取得する。書籍cacheやprogress、完了markerは保持。ホスト試験で対象範囲・ファイルなし・削除失敗・走査前呼出を検証。実機表示修正は待ち。

## 最終受入結果
v10通常構成有効化は0d24577cで保存。v11はX3のdevice-monitor-260926-205320.logにエラーなし、一覧の完了表示と再入場後保持をユーザー確認。v10書籍の未生成第3章だけ新規生成されており、すべての旧「途中」表示が索引不整合だったとは断定しない。X4はユーザーが同じ1〜4の正常を確認（この確認時点で追加ログの提示はなし）。一覧更新修正も受入済み。未push。画像出力途中の最悪応答時間の未測定という制約は継続。
