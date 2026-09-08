#!/usr/bin/env python3
"""Generate a copyright-safe EPUB for checking SD-font symbol coverage."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_sd_font_symbols_check.epub"

FILES = {
    "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>
""",
    "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="bookid">urn:uuid:1f3f6d9b-9e5f-4e4a-9ec5-2a2b55e1c1d7</dc:identifier><dc:title>Yomuka SDフォント記号確認</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="style" href="style.css" media-type="text/css"/><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>
""",
    "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja"><head><title>目次</title></head><body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">記号表示確認</a></li></ol></nav></body></html>
""",
    "OEBPS/style.css": """body { margin: 1em; line-height: 1.55; } h1 { font-size: 1.3em; } h2 { font-size: 1.1em; margin-top: 1.2em; } p { margin: 0.55em 0; } .bold { font-weight: 700; } .vertical { writing-mode: vertical-rl; height: 22em; } .symbols { letter-spacing: 0.08em; } ruby rt { font-size: 0.48em; }""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>Yomuka SDフォント記号確認</title><link rel="stylesheet" type="text/css" href="style.css"/></head><body>
<h1>Yomuka SDフォント記号確認</h1>
<p>このEPUBは、SDカードフォントの追加記号を通常表示と太字で確認するための合成テストです。</p>
<h2>6文字まとめ</h2>
<p class="symbols">通常：✓ ✿ ✽ ❖ № ℡</p>
<p class="symbols bold">太字：✓ ✿ ✽ ❖ № ℡</p>
<h2>文字名とコードポイント</h2>
<p>チェック：✓（U+2713）　花：✿（U+273F）　花形：✽（U+273D）</p>
<p>菱形：❖（U+2756）　番号：№（U+2116）　電話：℡（U+2121）</p>
<p class="bold">太字：チェック✓　花✿　花形✽　菱形❖　番号№　電話℡</p>
<h2>本文・ルビ・句読点との組み合わせ</h2>
<p>通常の<ruby>記号<rt>きごう</rt></ruby>確認です。✓と✿の後に、句読点（、。）や括弧「」『』が続きます。</p>
<p class="bold">太字の<ruby>記号<rt>きごう</rt></ruby>確認です。✽❖№℡の後に、英字ABCと数字123、長音ーを置きます。</p>
<h2>縦書き確認</h2>
<p class="vertical symbols">縦書き：✓ ✿ ✽ ❖ № ℡。<br/>ルビ<ruby>確認<rt>かくにん</rt></ruby>と括弧「」も一緒に確認します。</p>
<h2>判定</h2>
<p>選択したフォントに字形がある記号は、通常・太字とも欠落や豆腐にならず表示されることを確認してください。フォント固有の未収録文字は、今回の仕様では無理に代替表示しません。</p>
</body></html>
""",
}


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, content in FILES.items():
            archive.writestr(name, content.encode("utf-8"), compress_type=ZIP_DEFLATED)
    print(OUTPUT)


if __name__ == "__main__":
    main()
