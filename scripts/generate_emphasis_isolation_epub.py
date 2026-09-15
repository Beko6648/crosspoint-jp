#!/usr/bin/env python3
"""Generate a small vertical EPUB that isolates inline text-emphasis boundaries."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "test" / "epubs" / "v075_emphasis_isolation_vertical.epub"

BODY = '''<h1>圏点境界確認</h1>
<p>A 短縮指定：<span style="text-emphasis:filled dot">圏点</span>。後続。</p>
<p>B style指定：<span style="text-emphasis-style:filled dot">圏点</span>。後続。</p>
<p>C ルビ前：<ruby>日本語<rt>にほんご</rt></ruby>の<span style="text-emphasis:filled dot">圏点</span>。後続。</p>
<p>D 太字前：<b>太字</b>と<span style="text-emphasis:filled dot">圏点</span>。後続。</p>
<p>E 画像前：画像<img src="mark.png" style="display:inline;width:12px;height:12px" alt="印"/>後の<span style="text-emphasis:filled dot">圏点</span>。後続。</p>
<p>F 元の並び：<ruby>日本語<rt>にほんご</rt></ruby>の<b>太字</b>と<span style="text-emphasis:filled dot">圏点</span>。画像前<img src="mark.png" style="display:inline;width:20px;height:20px" alt="印"/>画像後。</p>'''

PNG = bytes.fromhex(
    "89504e470d0a1a0a0000000d4948445200000008000000080800000000e164e157"
    "0000001649444154789c6360f8cf800d18181818fe03316400009d5a07f95b896765"
    "0000000049454e44ae426082"
)


def write_entry(book: ZipFile, name: str, value: str | bytes, compression: int) -> None:
    data = value.encode("utf-8") if isinstance(value, str) else value
    if name.endswith((".xml", ".opf", ".xhtml")):
        ET.fromstring(data)
    info = ZipInfo(name, date_time=(2026, 9, 15, 0, 0, 0))
    info.compress_type = compression
    book.writestr(info, data)


def main() -> None:
    files = {
        "mimetype": "application/epub+zip",
        "META-INF/container.xml": '''<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>''',
        "OEBPS/content.opf": '''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:v075:emphasis-isolation:1</dc:identifier><dc:title>Yomuka v0.7.5 圏点境界確認</dc:title><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-15T00:00:00Z</meta></metadata><manifest><item id="body" href="body.xhtml" media-type="application/xhtml+xml"/><item id="mark" href="mark.png" media-type="image/png"/></manifest><spine page-progression-direction="rtl"><itemref idref="body"/></spine></package>''',
        "OEBPS/body.xhtml": f'''<html xmlns="http://www.w3.org/1999/xhtml"><head><title>圏点境界確認</title><style>body {{ writing-mode:vertical-rl; -epub-writing-mode:vertical-rl; }}</style></head><body>{BODY}</body></html>''',
        "OEBPS/mark.png": PNG,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, "w") as book:
        for name, value in files.items():
            write_entry(book, name, value, ZIP_STORED if name == "mimetype" else ZIP_DEFLATED)
    with ZipFile(OUTPUT) as book:
        assert book.testzip() is None
    print(OUTPUT)


if __name__ == "__main__":
    main()
