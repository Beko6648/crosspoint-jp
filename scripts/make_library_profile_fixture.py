"""Build a small, deterministic EPUB library for directory timing (not layout stress)."""
import argparse
import io
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED, ZIP_STORED
from xml.etree import ElementTree


def epub(number):
    title = f"一覧確認 {number:03d} 日本語の本"
    files = {
        "META-INF/container.xml": '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>',
        "OEBPS/content.opf": f'''<?xml version="1.0" encoding="UTF-8"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:library-profile-v1:{number}</dc:identifier><dc:title>{title}</dc:title><dc:language>ja</dc:language><dc:creator>Yomuka test</dc:creator></metadata><manifest><item id="text" href="text.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest><spine toc="ncx"><itemref idref="text"/></spine></package>''',
        "OEBPS/toc.ncx": f'''<?xml version="1.0" encoding="UTF-8"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content="urn:yomuka:library-profile-v1:{number}"/><meta name="dtb:depth" content="1"/><meta name="dtb:totalPageCount" content="0"/><meta name="dtb:maxPageNumber" content="0"/></head><docTitle><text>{title}</text></docTitle><navMap><navPoint id="n1" playOrder="1"><navLabel><text>本文</text></navLabel><content src="text.xhtml"/></navPoint></navMap></ncx>''',
        "OEBPS/text.xhtml": f'''<?xml version="1.0" encoding="UTF-8"?><html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>{title}</title></head><body><h1>{title}</h1><p>これはファイル一覧の速度を確認するための短い日本語の本です。</p><p>読書位置やキャッシュ状態を確認できます。</p></body></html>''',
    }
    out = io.BytesIO()
    with ZipFile(out, "w", compression=ZIP_DEFLATED) as z:
        z.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, text in files.items():
            ElementTree.fromstring(text)
            z.writestr(name, text.encode("utf-8"))
    with ZipFile(io.BytesIO(out.getvalue())) as z:
        assert z.testzip() is None
        assert z.infolist()[0].filename == "mimetype"
        assert z.infolist()[0].compress_type == ZIP_STORED
    return out.getvalue()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    # Refuse replacement; keep earlier fixtures and user files intact.
    with args.output.open("xb") as target, ZipFile(target, "w", ZIP_DEFLATED) as package:
        for count in (100, 300, 500):
            for n in range(1, count + 1):
                package.writestr(f"library-profile-v1/books-{count}/一覧確認-{n:03d}.epub", epub(n))
    with ZipFile(args.output) as package:
        assert len(package.namelist()) == 900
        assert package.testzip() is None
    print(f"Validated 900 EPUB files: {args.output}")


if __name__ == "__main__":
    main()
