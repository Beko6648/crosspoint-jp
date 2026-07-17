"""Create a compact EPUB that distinguishes all three CrossPoint book styles."""

from argparse import ArgumentParser
from pathlib import Path
import struct
import zlib
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parents[1]


def png(width: int, height: int) -> bytes:
    """Return a small black-and-white checkerboard PNG without dependencies."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            white = ((x // 8) + (y // 8)) % 2 == 0
            rows.extend((255, 255, 255, 255) if white else (0, 0, 0, 255))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(
        b"IDAT", zlib.compress(bytes(rows), 9)
    ) + chunk(b"IEND", b"")


CONTAINER = """<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""

OPF = """<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="book-id" xml:lang="ja">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="book-id">crosspoint-book-style-modes-check</dc:identifier>
    <dc:title>CrossPoint Book Style Modes Check</dc:title>
    <dc:language>ja</dc:language>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
    <item id="css" href="styles.css" media-type="text/css"/>
    <item id="visible-image" href="visible.png" media-type="image/png"/>
    <item id="css-image" href="css-image.png" media-type="image/png"/>
  </manifest>
  <spine page-progression-direction="rtl"><itemref idref="chapter"/></spine>
</package>
"""

NAV = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
  <head><title>Contents</title></head>
  <body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">Style checks</a></li></ol></nav></body>
</html>
"""

CSS = """/* Expected: ignored by CrossPoint Priority, geometry only in Balanced, fully applied by Book Priority. */
.body-probe {
  font-size: 220%; line-height: 2.0; font-weight: bold;
  margin: 2em 1em; padding: 0.5em; text-indent: 2em; text-align: right;
}
/* Expected: applied in Balanced and Book Priority. */
h1 { text-align: center; margin-top: 2em; margin-bottom: 1em; font-weight: bold; text-decoration: underline; }
/* Expected: visible and fit to viewport in CrossPoint Priority/Balanced; hidden in Book Priority. */
.css-hidden-image { width: 25%; height: 25%; display: none; }
/* Expected: visible in CrossPoint Priority/Balanced; hidden in Book Priority. */
.css-hidden-text { display: none; }
"""

CHAPTER = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Style checks</title><link rel="stylesheet" type="text/css" href="styles.css"/></head>
<body>
  <h1>Book Style Mode Check</h1>
  <p>Use the same book after selecting each mode in Settings &gt; Reader &gt; Book Style.</p>
  <p class="body-probe">BODY PROBE: this paragraph requests 220% type, double line height, bold text, a 2em indent, 2em vertical margin, padding, and right alignment.</p>
  <p class="css-hidden-text">CSS HIDDEN TEXT: visible only in CrossPoint Priority and Balanced.</p>
  <p>Ruby structure: <ruby>漢<rt>かん</rt></ruby><ruby>字<rt>じ</rt></ruby>. This must remain readable in every mode.</p>
  <p>Normal image (visible in every mode):</p>
  <p><img src="visible.png" alt="visible checkerboard"/></p>
  <p>CSS-hidden image:</p>
  <p><img class="css-hidden-image" src="css-image.png" alt="CSS hidden checkerboard"/></p>
  <span epub:type="pagebreak" role="doc-pagebreak" title="Mode-check break"/>
  <p>After the page break: confirm this paragraph starts on a new page when the reader exposes EPUB page breaks.</p>
  <p>Repeat the checks in vertical writing. Ruby, images, and the page break must remain structurally correct.</p>
</body>
</html>
"""

OPF_JA = OPF.replace("CrossPoint Book Style Modes Check", "CrossPoint 書籍スタイル3モード確認")

NAV_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
  <head><title>目次</title></head>
  <body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">表示確認</a></li></ol></nav></body>
</html>
"""

CHAPTER_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>表示確認</title><link rel="stylesheet" type="text/css" href="styles.css"/></head>
<body>
  <h1>書籍スタイル3モード確認</h1>
  <p>設定 &gt; Reader &gt; 書籍スタイルで各モードを選び、同じ書籍を開き直して比較してください。</p>
  <p class="body-probe">本文プローブ: この段落には、文字サイズ220%、行間2倍、太字、2em字下げ、上下2em余白、パディング、右揃えを指定しています。</p>
  <p class="css-hidden-text">CSS非表示テキスト: CrossPoint優先とバランスでは表示され、書籍優先では非表示になります。</p>
  <p>ルビ構造: <ruby>漢<rt>かん</rt></ruby><ruby>字<rt>じ</rt></ruby>。すべてのモードで読みやすく表示されることを確認してください。</p>
  <p>通常画像（すべてのモードで表示）:</p>
  <p><img src="visible.png" alt="通常のチェッカー画像"/></p>
  <p>CSS非表示画像:</p>
  <p><img class="css-hidden-image" src="css-image.png" alt="CSS非表示のチェッカー画像"/></p>
  <span epub:type="pagebreak" role="doc-pagebreak" title="モード確認の改ページ"/>
  <p>改ページの後: EPUB改ページが有効な場合、この段落が新しいページから始まることを確認してください。</p>
  <p>縦書きでも同じ確認を行ってください。ルビ、画像、改ページの構造が崩れないことを確認します。</p>
</body>
</html>
"""


OPF_JA = OPF.replace("CrossPoint Book Style Modes Check", "CrossPoint 書籍スタイル3モード確認")

NAV_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
  <head><title>目次</title></head>
  <body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">表示確認</a></li></ol></nav></body>
</html>
"""

CHAPTER_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>表示確認</title><link rel="stylesheet" type="text/css" href="styles.css"/></head>
<body>
  <h1>書籍スタイル3モード確認</h1>
  <p>設定の「書籍スタイル」で各モードを選び、同じ書籍を開き直して比較してください。</p>
  <p class="body-probe">本文CSS確認: この段落は文字サイズ220%、行間2倍、太字、2em字下げ、上下余白、内側余白、右揃えを指定しています。書籍優先では文字が現在の設定に近い安全な段階へ拡大し、行間も広がります。</p>
  <p class="css-hidden-text">CSS非表示テキスト: CrossPoint優先とバランスでは表示され、書籍優先では表示されません。</p>
  <p>ルビ構造: <ruby>猫<rt>ねこ</rt></ruby><ruby>好き<rt>ずき</rt></ruby>。すべてのモードで読みやすく表示されることを確認してください。</p>
  <p>通常画像（すべてのモードで表示）:</p>
  <p><img src="visible.png" alt="通常のチェッカー画像"/></p>
  <p>CSS非表示画像:</p>
  <p><img class="css-hidden-image" src="css-image.png" alt="CSS非表示のチェッカー画像"/></p>
  <span epub:type="pagebreak" role="doc-pagebreak" title="モード確認の改ページ"/>
  <p>改ページの後: EPUB改ページが有効な場合、この段落が新しいページから始まることを確認してください。</p>
  <p>縦書きでも同じ確認を行ってください。ルビ、画像、改ページの構造は崩れないことを確認します。</p>
</body>
</html>
"""


OPF_JA = OPF.replace("CrossPoint Book Style Modes Check", "CrossPoint \u66f8\u7c4d\u30b9\u30bf\u30a4\u30eb3\u30e2\u30fc\u30c9\u78ba\u8a8d")
NAV_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>\u76ee\u6b21</title></head><body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">\u8868\u793a\u78ba\u8a8d</a></li></ol></nav></body></html>
"""
CHAPTER_JA = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>\u8868\u793a\u78ba\u8a8d</title><link rel="stylesheet" type="text/css" href="styles.css"/></head><body>
<h1>\u66f8\u7c4d\u30b9\u30bf\u30a4\u30eb3\u30e2\u30fc\u30c9\u78ba\u8a8d</h1>
<p>\u8a2d\u5b9a\u306e\u300c\u66f8\u7c4d\u30b9\u30bf\u30a4\u30eb\u300d\u3067\u5404\u30e2\u30fc\u30c9\u3092\u9078\u3073\u3001\u540c\u3058\u66f8\u7c4d\u3092\u958b\u304d\u76f4\u3057\u3066\u6bd4\u8f03\u3057\u3066\u304f\u3060\u3055\u3044\u3002</p>
<p class="body-probe">\u672c\u6587CSS\u78ba\u8a8d: \u3053\u306e\u6bb5\u843d\u306f\u6587\u5b57\u30b5\u30a4\u30ba220%\u3001\u884c\u95932\u500d\u3001\u592a\u5b57\u30012em\u5b57\u4e0b\u3052\u3001\u4e0a\u4e0b\u4f59\u767d\u3001\u5185\u5074\u4f59\u767d\u3001\u53f3\u63c3\u3048\u3092\u6307\u5b9a\u3057\u3066\u3044\u307e\u3059\u3002\u66f8\u7c4d\u512a\u5148\u3067\u306f\u6587\u5b57\u304c\u73fe\u5728\u306e\u8a2d\u5b9a\u306b\u8fd1\u3044\u5b89\u5168\u306a\u6bb5\u968e\u3078\u62e1\u5927\u3057\u3001\u884c\u9593\u3082\u5e83\u304c\u308a\u307e\u3059\u3002</p>
<p class="css-hidden-text">CSS\u975e\u8868\u793a\u30c6\u30ad\u30b9\u30c8: CrossPoint\u512a\u5148\u3068\u30d0\u30e9\u30f3\u30b9\u3067\u306f\u8868\u793a\u3055\u308c\u3001\u66f8\u7c4d\u512a\u5148\u3067\u306f\u8868\u793a\u3055\u308c\u307e\u305b\u3093\u3002</p>
<p>\u30eb\u30d3\u69cb\u9020: <ruby>\u732b<rt>\u306d\u3053</rt></ruby><ruby>\u597d\u304d<rt>\u305a\u304d</rt></ruby>\u3002\u3059\u3079\u3066\u306e\u30e2\u30fc\u30c9\u3067\u8aad\u307f\u3084\u3059\u304f\u8868\u793a\u3055\u308c\u308b\u3053\u3068\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002</p>
<p>\u901a\u5e38\u753b\u50cf\uff08\u3059\u3079\u3066\u306e\u30e2\u30fc\u30c9\u3067\u8868\u793a\uff09:</p><p><img src="visible.png" alt="\u901a\u5e38\u306e\u30c1\u30a7\u30c3\u30ab\u30fc\u753b\u50cf"/></p>
<p>CSS\u975e\u8868\u793a\u753b\u50cf:</p><p><img class="css-hidden-image" src="css-image.png" alt="CSS\u975e\u8868\u793a\u306e\u30c1\u30a7\u30c3\u30ab\u30fc\u753b\u50cf"/></p>
<span epub:type="pagebreak" role="doc-pagebreak" title="\u30e2\u30fc\u30c9\u78ba\u8a8d\u306e\u6539\u30da\u30fc\u30b8"/><p>\u6539\u30da\u30fc\u30b8\u306e\u5f8c: EPUB\u6539\u30da\u30fc\u30b8\u304c\u6709\u52b9\u306a\u5834\u5408\u3001\u3053\u306e\u6bb5\u843d\u304c\u65b0\u3057\u3044\u30da\u30fc\u30b8\u304b\u3089\u59cb\u307e\u308b\u3053\u3068\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002</p>
<p>\u7e26\u66f8\u304d\u3067\u3082\u540c\u3058\u78ba\u8a8d\u3092\u884c\u3063\u3066\u304f\u3060\u3055\u3044\u3002\u30eb\u30d3\u3001\u753b\u50cf\u3001\u6539\u30da\u30fc\u30b8\u306e\u69cb\u9020\u306f\u5d29\u308c\u306a\u3044\u3053\u3068\u3092\u78ba\u8a8d\u3057\u307e\u3059\u3002</p></body></html>
"""

def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "book-style-modes-check.epub")
    parser.add_argument("--language", choices=("en", "ja"), default="en")
    args = parser.parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    opf = OPF_JA if args.language == "ja" else OPF
    nav = NAV_JA if args.language == "ja" else NAV
    chapter = CHAPTER_JA if args.language == "ja" else CHAPTER

    with ZipFile(output, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, content in {
            "META-INF/container.xml": CONTAINER,
            "OEBPS/content.opf": opf,
            "OEBPS/nav.xhtml": nav,
            "OEBPS/styles.css": CSS,
            "OEBPS/chapter.xhtml": chapter,
        }.items():
            archive.writestr(name, content.encode("utf-8"), compress_type=ZIP_DEFLATED)
        archive.writestr("OEBPS/visible.png", png(192, 96), compress_type=ZIP_DEFLATED)
        archive.writestr("OEBPS/css-image.png", png(96, 48), compress_type=ZIP_DEFLATED)

    print(output)


if __name__ == "__main__":
    main()
