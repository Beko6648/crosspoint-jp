# EPUBページ内の単独所有への移行

## 実装
PageがPageElementを、PageLine/PageImage/PageTableRowがそれぞれのblockをunique_ptrで所有する。ParsedTextの完成行/列はcallbackへmoveする。行の受け入れを示すboolは維持し、失敗した入力の語・ルビ・傍点・インライン画像を消費しない。

Epub本体と表のTableColumnLayoutはshared_ptrを維持する。表の列幅情報は複数行から実際に共有される。

PageLine本体を脚注や語数の更新より先に確保し、失敗時は章生成を中断する。今回触れるPage/画像/表行の本体確保はnothrowとし、失敗をlowMemoryAbortRequestedへ伝える。Pageの読み込みはnullのblock/elementと不足したヘッダーを拒否する。

PXC6、章キャッシュの書き込み形式、配置・dither・refresh設定は変更しない。通常版のidle画像先読みも既存設定を維持。

## ホスト検証
- layout_memory: 本番ParsedTextの縦/横、全layout admission箇所での失敗、callback拒否、TextBlock本体確保失敗、残入力のメタデータ、出力所有者の寿命。
- page_ownership: 本番PageとTableRowBlock。null画像/本文payloadの拒否、Page/要素本体の確保失敗、不足ヘッダー、ページ形式の往復、先行ページ破棄後の表行と共有列幅の寿命。
- 画像/本文payload codecはpage_ownershipではstub。実画面やparserの表改ページそのものは実機検証が必要。
- 既存layoutテストのrenderer stubに現在の計測APIを補完。失敗位置のカウントは入力構築とlayoutで分離するよう修正。

## 実機比較
同一診断機構v5を使い、同じEPUB・フォント・ページ・キャッシュ状態・待機時間で比較する。今回のhost成功だけでは実機のメモリ削減量、速度、描画一致を確定しない。

## 限界
文字列/vectorの内部確保や共有の表列幅管理領域は残る。nothrowによる検出はオブジェクト本体が中心。既存のheap admissionも、他処理からの同時確保まで保証する仕組みではない。

## 実機確認状況（2026-09-26）
X3通常読書の目視確認に加え、CSSストリーミングを含む最終版でX3/X4とも全書籍生成、本文・ルビ・太字・挿絵、再実行時の再利用をユーザー確認済み。低メモリ長文ストレス章の安全なメモリエラーは通常章の合格と区別する。所有権変更単独の同条件メモリ比較は未完了で、削減量はまだ断定しない。
