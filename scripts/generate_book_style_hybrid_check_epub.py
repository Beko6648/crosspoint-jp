"""Create a compact EPUB for checking CrossPoint's fixed hybrid book style."""

from pathlib import Path
import struct
import zlib
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "book-style-hybrid-check.epub"


def png(width: int, height: int) -> bytes:
    """Return a small black-and-white checkerboard PNG without dependencies."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter: None
        for x in range(width):
            white = ((x // 8) + (y // 8)) % 2 == 0
            rows.extend((255, 255, 255, 255) if white else (0, 0, 0, 255))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(
        b"IDAT", zlib.compress(bytes(rows), 9)
    ) + chunk(b"IEND", b"")


CONTAINER = """<?xml version=\"1.0\"?>
<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">
  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>
</container>
"""

OPF = """<?xml version=\"1.0\" encoding=\"utf-8\"?>
<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"book-id\" xml:lang=\"ja\">
  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">
    <dc:identifier id=\"book-id\">crosspoint-book-style-hybrid-check</dc:identifier>
    <dc:title>CrossPoint ハイブリッド固定 確認</dc:title>
    <dc:language>ja</dc:language>
  </metadata>
  <manifest>
    <item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>
    <item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/>
    <item id=\"css\" href=\"styles.css\" media-type=\"text/css\"/>
    <item id=\"image\" href=\"checker.png\" media-type=\"image/png\"/>
  </manifest>
  <spine page-progression-direction=\"rtl\"><itemref idref=\"chapter\"/></spine>
</package>
"""

NAV = """<?xml version=\"1.0\" encoding=\"utf-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>目次</title></head>
<body><nav epub:type=\"toc\" xmlns:epub=\"http://www.idpf.org/2007/ops\"><ol><li><a href=\"chapter.xhtml\">確認ページ</a></li></ol></nav></body></html>
"""

CSS = """/* The body probe must NOT affect body text in CrossPoint's fixed hybrid style. */
.body-probe { font-size: 250%; line-height: 2.2; margin: 2em; padding: 1em; text-indent: 3em; text-align: right; }
/* The heading probe SHOULD keep alignment, margins and boldness in hybrid style. */
h1 { text-align: center; margin-top: 2em; margin-bottom: 1.5em; font-weight: bold; text-decoration: underline; }
/* These image instructions must NOT affect the fixed hybrid style. */
.tiny-image { width: 25%; height: 25%; display: none; }
"""

CHAPTER = """<?xml version=\"1.0\" encoding=\"utf-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">
<head><title>ハイブリッド固定確認</title><link rel=\"stylesheet\" type=\"text/css\" href=\"styles.css\"/></head>
<body>
  <h1>見出し: EPUB CSS を保持</h1>
  <p class=\"body-probe\">本文CSS確認: この段落は大きな文字、広い行間、右寄せ、太い余白、字下げを指定しています。固定ハイブリッドでは、本文は現在のCrossPoint設定のまま表示されます。</p>
  <p>ルビ確認: <ruby>漢字<rt>かんじ</rt></ruby> と <ruby>読書<rt>どくしょ</rt></ruby> が本文を覆わずに読めることを確認してください。</p>
  <p>画像確認: 下のチェッカー画像にはCSSで「25%かつ非表示」を指定しています。固定ハイブリッドでは無視され、画像は通常のfit-to-viewport表示になります。</p>
  <p><img class=\"tiny-image\" src=\"checker.png\" alt=\"白黒チェッカー\"/></p>
  <p>改ページマーカー前: 次のページ番号マーカーの後に、ここで即時にページが切り替わるかを観察してください。</p>
  <span epub:type=\"pagebreak\" role=\"doc-pagebreak\" title=\"テスト改ページ\"/>
  <p>改ページマーカー後: この段落が直後の新ページに配置されるかを確認します。</p>
  <p>縦書き確認: 縦書きに切り替え、ルビ、見出し、画像の重なりや欠落がないことを確認してください。0123 ABC。</p>
</body></html>
"""


def main() -> None:
    with ZipFile(OUTPUT, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, content in {
            "META-INF/container.xml": CONTAINER,
            "OEBPS/content.opf": OPF,
            "OEBPS/nav.xhtml": NAV,
            "OEBPS/styles.css": CSS,
            "OEBPS/chapter.xhtml": CHAPTER,
        }.items():
            archive.writestr(name, content.encode("utf-8"), compress_type=ZIP_DEFLATED)
        archive.writestr("OEBPS/checker.png", png(96, 48), compress_type=ZIP_DEFLATED)
    print(OUTPUT)


if __name__ == "__main__":
    main()
