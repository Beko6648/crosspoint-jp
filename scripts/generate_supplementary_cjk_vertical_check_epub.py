#!/usr/bin/env python3
"""Generate a vertical EPUB regression fixture for supplementary CJK ideographs."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_supplementary_cjk_vertical_check.epub"

FILES = {
    "META-INF/container.xml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">
  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>
</container>
""",
    "OEBPS/content.opf": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"bookid\" version=\"3.0\" xml:lang=\"ja\">
  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">
    <dc:identifier id=\"bookid\">urn:uuid:64a5e31c-1f1d-48fa-9975-d9f3639d3e23</dc:identifier>
    <dc:title>補助漢字縦書き確認</dc:title><dc:language>ja</dc:language>
  </metadata>
  <manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>
  <spine page-progression-direction=\"rtl\"><itemref idref=\"chapter\"/></spine>
</package>
""",
    "OEBPS/chapter.xhtml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\">
<head><title>補助漢字縦書き確認</title><style>body{writing-mode:vertical-rl;line-height:1.7;margin:0.8em;}p{margin:0 0.7em;}</style></head>
<body>
<p>基本漢字：叱りつける。</p>
<p>補助漢字：𠮟りつける。</p>
<p>比較：叱責と𠮟責。どちらも縦向きの一文字として表示されること。</p>
</body></html>
""",
}


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, contents in FILES.items():
            archive.writestr(name, contents.encode("utf-8"), compress_type=ZIP_DEFLATED)
    print(OUTPUT)


if __name__ == "__main__":
    main()
