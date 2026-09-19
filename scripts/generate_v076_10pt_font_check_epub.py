#!/usr/bin/env python3
"""Generate the focused v0.7.6 EPUB for 10pt SD-font visual checks."""

from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v076_10pt_font_check.epub"
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
    <dc:identifier id="bookid">urn:yomuka:v076:10pt-font-check</dc:identifier>
    <dc:title>Yomuka v0.7.6 10pt文字確認</dc:title>
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
  <body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">10pt文字確認</a></li></ol></nav></body>
</html>
""",
    "OEBPS/style.css": """body {
  writing-mode: horizontal-tb;
  -epub-writing-mode: horizontal-tb;
  line-height: 1.5;
}
p { margin: 0.7em 0; }
""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
  <head>
    <title>Yomuka v0.7.6 10pt文字確認</title>
    <link rel="stylesheet" type="text/css" href="style.css"/>
  </head>
  <body>
    <table>
      <thead>
        <tr><th>太字見出し</th><th>英数字見出し</th></tr>
      </thead>
      <tbody>
        <tr><td>通常セル 漢字かな</td><td>ABC 12345</td></tr>
        <tr><td>句読点、。括弧「」</td><td>細線 太線</td></tr>
      </tbody>
    </table>
    <p>上付き確認：x<sup>2</sup>、10<sup>3</sup>、漢<sup>上</sup>。</p>
    <p>下付き確認：H<sub>2</sub>O、CO<sub>2</sub>、漢<sub>下</sub>。</p>
    <p>比較用本文：上付きと下付きが本文より小さく、上下の位置が異なることを確認します。</p>
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
