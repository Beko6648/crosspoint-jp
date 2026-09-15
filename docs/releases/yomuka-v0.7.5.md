## CrossPoint Yomuka v0.7.5

> CrossPoint Yomuka は CrossPoint Reader / CrossPoint JP を基にした非公式コミュニティフォークです。

### 上流・派生プロジェクトからの取り込み

- 日本語組版の改善は、[zrn-ns/crosspoint-jp PR #144](https://github.com/zrn-ns/crosspoint-jp/pull/144)を項目ごとに比較し、Yomukaの既存の縦書き・横書き禁則、ルビ、soft hyphen、画像メタデータの構造を維持する形で選択的に取り込みました。
- EPUBレイアウトのメモリ不足対策は、[crosspoint-reader-mod の fa4a55b](https://github.com/osakanataro/crosspoint-reader-mod/commit/fa4a55b2514ceaf47187546a993dc771c186e832)を参考にしています。Yomukaのキャッシュ生成と並列メタデータに合わせて実装しています。

### 主な変更

- EPUBのレイアウトとキャッシュ生成でメモリ不足を検出した場合、再起動や停止を避けて、その章の生成を安全に中断します。
  - 中断したキャッシュを完成扱いにせず、メモリが回復した後に再生成できます。
  - 長段落、巨大なルビ、長い英単語を含む書籍で、X3/X4の安全停止と再生成を確認しました。
- 起動時にSDカードのファームウェア更新へ直接入るリカバリーモードを追加しました。
  - X3は電源＋左側面キー、X4は電源＋いずれかの側面キーで起動します。
  - 設定、読書履歴、APP_STATEを読む前に入るため、状態ファイルが壊れた場合にも更新経路を確保します。
- 日本語EPUBの組版を改善しました。
  - 禁則処理、原文中の空白と整形改行、番号付きリストの番号・入れ子、上付き／下付き、整形済みテキスト、見出し間隔を改善しました。
  - 外部CSSを持たないEPUBでも、インライン指定の圏点を反映します。
  - 組版キャッシュを自動で作り直します。初回に本を開くときは、対象章の表示に少し時間がかかる場合があります。
- X3の読書メーターで日時が未設定の場合、理由と「同期」を表示します。Wi-Fi接続で時刻を同期すると、週間グラフを表示できます。 [Issue #38](https://github.com/ponto1216-ai/crosspoint-jp/issues/38)

### 更新時の注意

- 通常の更新では、書籍・設定・SDカードフォントを削除する必要はありません。
- v0.7.5ではEPUB組版キャッシュの形式を更新しています。初回に本を開くと、必要な章のキャッシュを自動的に再生成します。
- 低メモリ時には、問題の章を安全に中断するメッセージが出ることがあります。端末を再起動して通常の本を開くか、対象書籍のキャッシュを再生成してください。

### 更新方法

1. [v0.7.5 リリース](https://github.com/ponto1216-ai/crosspoint-jp/releases/tag/yomuka-v0.7.5) から `firmware.bin` をダウンロードします。
2. 更新前に書籍、設定、SDカードフォントをバックアップします。
3. `firmware.bin` を名前を変えずにSDカードへ置き、端末で **設定 → 本体 → SDカードファームウェア更新** を実行します。
4. 通常起動できない場合は、起動時に電源と側面キーを同時に押してリカバリーモードを開き、SDカード上の `firmware.bin` を選択します。
5. 更新中は電源を切ったり、SDカードを抜いたりしません。

`bootloader.bin` と `partitions.bin` は初回書き込み・復旧用、`SHA256SUMS.txt` はダウンロードした配布物の検証用です。通常のSDカード更新では `firmware.bin` を使用します。

詳しい操作は [基本操作・設定・不具合の確認](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/basic-operations-ja.md)、フォント導入は [日本語フォントの導入](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/cjk-fonts.md) を参照してください。
