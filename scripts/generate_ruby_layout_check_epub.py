#!/usr/bin/env python3
"""Generate a copyright-safe EPUB for vertical ruby clearance checks."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_ruby_layout_check.epub"

FILES = {
    "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>""",
    "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="bookid">urn:uuid:8b5e5154-9569-4648-82c5-0e5ee0059b46</dc:identifier><dc:title>Yomuka ルビ配置確認</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="style" href="style.css" media-type="text/css"/><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>""",
    "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja"><head><title>目次</title></head><body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">ルビ配置確認</a></li></ol></nav></body></html>""",
    "OEBPS/style.css": """body { margin: 0; } p { margin: 0; } ruby rt { font-size: 0.5em; }""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>Yomuka ルビ配置確認</title><link rel="stylesheet" type="text/css" href="style.css"/></head><body>
<p><ruby>第一列<rt>だいいちれつ</rt></ruby>の本文です。</p>
<p><ruby>第二列<rt>だいにれつ</rt></ruby>の本文です。</p>
<p><ruby>第三列<rt>だいさんれつ</rt></ruby>の本文です。</p>
<p>第四列はルビなし本文です。</p>
<p><ruby>第五列<rt>だいごれつ</rt></ruby>の本文です。</p>
<p><ruby>第六列<rt>だいろくれつ</rt></ruby>の本文です。</p>
<p>第七列はルビなし本文です。</p>
<p><ruby>第八列<rt>だいはちれつ</rt></ruby>の本文です。</p>
<p><ruby>第九列<rt>だいきゅうれつ</rt></ruby>の本文です。</p>
</body></html>""",
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
