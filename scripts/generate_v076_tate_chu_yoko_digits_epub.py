#!/usr/bin/env python3
"""Generate a focused vertical EPUB for configurable TateChuYoko digits."""

from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v076_tate_chu_yoko_digits.epub"
ZIP_TIMESTAMP = (2026, 9, 17, 0, 0, 0)

FILES = {
    "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
""",
    "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:yomuka:v076:tate-chu-yoko-digits</dc:identifier>
    <dc:title>Yomuka v0.7.6 縦中横桁数確認</dc:title>
    <dc:creator>Yomuka Test Fixtures</dc:creator>
    <dc:language>ja</dc:language>
    <meta property="dcterms:modified">2026-09-17T00:00:00Z</meta>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="style" href="style.css" media-type="text/css"/>
    <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine><itemref idref="chapter"/></spine>
</package>
""",
    "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja">
  <head><title>目次</title></head>
  <body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">縦中横桁数確認</a></li></ol></nav></body>
</html>
""",
    "OEBPS/style.css": """body {
  writing-mode: vertical-rl;
  -epub-writing-mode: vertical-rl;
  line-height: 1.7;
}
p { margin: 0 0.8em; }
""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
  <head>
    <title>Yomuka v0.7.6 縦中横桁数確認</title>
    <link rel="stylesheet" type="text/css" href="style.css"/>
  </head>
  <body>
    <h1>縦中横の数字</h1>
    <p>一桁は1、二桁は12、三桁は123、四桁は1234です。</p>
    <p>三桁比較は001、100、365、999です。</p>
    <p><strong>太字三桁は123、太字二桁は12です。</strong></p>
    <p>数式確認はx<sup>2</sup>、10<sup>3</sup>、H<sub>2</sub>O、CO<sub>2</sub>です。</p>
    <p>設定が二桁までなら123は横倒し、三桁までなら123だけ一文字分のセルに横並びで表示します。1234はどちらでも横倒しです。</p>
  </body>
</html>
""",
}


def add_entry(archive: ZipFile, name: str, data: bytes, compression: int) -> None:
    entry = ZipInfo(name, date_time=ZIP_TIMESTAMP)
    entry.compress_type = compression
    archive.writestr(entry, data)


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    for name, content in FILES.items():
        if name.endswith((".xml", ".opf", ".xhtml")):
            ET.fromstring(content.encode("utf-8"))

    with ZipFile(OUTPUT, "w") as archive:
        add_entry(archive, "mimetype", b"application/epub+zip", ZIP_STORED)
        for name, content in FILES.items():
            add_entry(archive, name, content.encode("utf-8"), ZIP_DEFLATED)

    with ZipFile(OUTPUT) as archive:
        assert archive.namelist()[0] == "mimetype"
        assert archive.getinfo("mimetype").compress_type == ZIP_STORED
        assert archive.read("mimetype") == b"application/epub+zip"
        assert archive.testzip() is None

    print(OUTPUT)


if __name__ == "__main__":
    main()
