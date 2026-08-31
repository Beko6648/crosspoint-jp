## CrossPoint Yomuka v0.6.5.2

> CrossPoint Yomuka は CrossPoint Reader / CrossPoint JP を基にした非公式コミュニティフォークです。

### 修正内容

- EPUBの読書キャッシュ生成中、空のHTML要素の境界を処理した際にメモリが破損し、端末が再起動することがある問題を修正しました。
- 縦書きEPUBを含む2冊でキャッシュ生成が完了することを実機で確認しています。

### 更新時の注意

キャッシュ形式を更新しています。v0.6.5.2へ更新後、対象のEPUBを初めて開くと読書キャッシュを自動で再生成します。再生成中は最初の表示に時間がかかることがありますが、読書位置と設定は保持されます。

### 更新方法

1. [v0.6.5.2 リリース](https://github.com/ponto1216-ai/crosspoint-jp/releases/tag/yomuka-v0.6.5.2) から `firmware.bin` をダウンロードします。
2. 更新前に書籍、設定、SDフォントをバックアップします。
3. `firmware.bin` を名前を変えずにSDカードへ置き、端末で **設定 → 本体 → SDカードファームウェア更新** を実行します。
4. 更新中は電源を切ったり、SDカードを抜いたりしません。

`bootloader.bin` と `partitions.bin` は初回書き込み・復旧用、`SHA256SUMS.txt` はダウンロードした配布物の検証用です。通常のSDカード更新では `firmware.bin` を使用します。

詳しい操作は [基本操作・設定・不具合の確認](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/basic-operations-ja.md)、フォント導入は [日本語フォントの導入](https://github.com/ponto1216-ai/crosspoint-jp/blob/main/docs/cjk-fonts.md) を参照してください。
