#!/usr/bin/env python3
"""Generate the compact, copyright-safe font-weight verification EPUB."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_font_weight_check.epub"

FILES = {
    "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>
""",
    "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="bookid">urn:uuid:11bcf052-f68c-4744-bf60-967f445bf6a2</dc:identifier><dc:title>Yomuka フォント太さ確認</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="style" href="style.css" media-type="text/css"/><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>
""",
    "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja"><head><title>目次</title></head><body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">太字確認</a></li></ol></nav></body></html>
""",
    "OEBPS/style.css": """body { margin: 1em; line-height: 1.55; } h1 { font-size: 1.3em; } h2 { font-size: 1.1em; margin-top: 1.4em; } p { margin: 0.55em 0; } .bold700 { font-weight: 700; } .normal { font-weight: normal; } ruby rt { font-size: 0.48em; } .samples { letter-spacing: 0.03em; }""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>Yomuka フォント太さ確認</title><link rel="stylesheet" type="text/css" href="style.css"/></head><body>
<h1>Yomuka フォント太さ確認</h1>
<p>対象：Noto Serif JP。通常表示と太字の差が、日本語・英字・数字・句読点で視認できることを確認します。</p>
<h2>CSS font-weight: bold</h2>
<p class="samples normal">通常：漢字かな交じり文 ABCdef 0123456789 、。！？（）「」</p>
<p class="samples"><span style="font-weight: bold">太字：漢字かな交じり文 ABCdef 0123456789 、。！？（）「」</span></p>
<h2>CSS font-weight: 700</h2>
<p class="samples normal">通常：日本語 Japanese English 2026 年、確認用です。</p>
<p class="samples bold700">太字：日本語 Japanese English 2026 年、確認用です。</p>
<h2>HTML b / strong</h2>
<p>通常の文と、<b>太字の b：日本語 Bold ABC 123、。</b>、そして <strong>太字の strong：日本語 Strong XYZ 456！？</strong> を比較します。</p>
<h2>ルビの前後</h2>
<p>通常の<ruby>漢字<rt>かんじ</rt></ruby>の直後と、<b><ruby>太字<rt>ふとじ</rt></ruby>日本語 Bold 789、。</b>の太さ・ルビの位置を確認します。</p>
<p><ruby>確認<rt>かくにん</rt></ruby>する通常文。<span class="bold700"><ruby>確認<rt>かくにん</rt></ruby>する太字文。</span></p>
<h2>見比べ方</h2>
<p>同じ文字サイズ・行間で、通常行より太字行の線が明確に太く、ルビの有無で太字が解除されないことを確認してください。</p>
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
