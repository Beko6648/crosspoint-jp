## CrossPoint Yomuka v0.7.3

> CrossPoint Yomuka は CrossPoint Reader / CrossPoint JP を基にした非公式コミュニティフォークです。

### 主な変更

- 商業EPUBで使われる画像指定への対応を広げました。
  - CSSの`max-width`と`max-height`を反映します。
  - SVGで包まれた`image`要素と、SVG形式の表紙画像を表示できます。
  - 縦書きの本文中画像は、前後の本文と自然につながり、画像だけのページでは中央に配置されます。
- EPUBの文字装飾に対応しました。
  - 圏点、下線、取り消し線を横書き・縦書きで表示します。
  - 長い半角英数字列が改行される位置でも装飾を維持します。
  - 圏点の字形を持たないSDカードフォントを選んでも、代替表示で読書を続けられます。
- PNGとJPEGの本文画像を共通のPXCキャッシュへ保存します。画像ページを再び開く際の処理を安定させ、キャッシュ生成時のメモリ使用量を抑えます。
- 外部CSSを使用するEPUBと、画像キャッシュ生成中のメモリ使用量を見直しました。
- 多数の本・フォルダを含む場所を開いた後にフォルダを移動しても、一覧キャッシュが積み重なってクラッシュしにくくしました。大きな一覧から戻る際は再読込する場合があります。

### SDカードフォント

別配布の [SD Card Fonts](https://github.com/ponto1216-ai/crosspoint-jp/releases/tag/sd-fonts) も更新します。Noto Sans JP、Noto Serif JP、BIZ UD Gothic、BIZ UD Minchoに、圏点・ゴマルビに必要な字形を収録しています。

### 更新時の注意

EPUBのページ配置、画像の予約領域、文字装飾の保存形式を更新しています。v0.7.3へ更新後、EPUBを初めて開く際は読書キャッシュを自動で再生成します。再生成中は最初の表示に時間がかかることがありますが、読書位置、しおり、本ごとの設定、読書履歴は保持されます。

SDカードフォントを更新する場合は、端末でのダウンロード後または配布ZIPを展開後に端末を再起動してください。

### 更新方法

1. [v0.7.3 リリース](https://github.com/ponto1216-ai/crosspoint-jp/releases/tag/yomuka-v0.7.3) から `firmware.bin` をダウンロードします。
2. 更新前に書籍、設定、SDフォントをバックアップします。
3. `firmware.bin` を名前を変えずにSDカードへ置き、端末で **設定 → 本体 → SDカードファームウェア更新** を実行します。
4. 更新中は電源を切ったり、SDカードを抜いたりしません。

`bootloader.bin` と `partitions.bin` は初回書き込み・復旧用、`SHA256SUMS.txt` はダウンロードした配布物の検証用です。通常のSDカード更新では `firmware.bin` を使用します。

詳しい操作は [基本操作・設定・不具合の確認](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/basic-operations-ja.md)、フォント導入は [日本語フォントの導入](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/cjk-fonts.md) を参照してください。
