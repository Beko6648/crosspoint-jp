#!/usr/bin/env python3
"""Generate the committed vertical punctuation verification EPUB."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_vertical_punctuation_check.epub"

FILES = {
    "META-INF/container.xml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">
  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>
</container>
""",
    "OEBPS/content.opf": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"bookid\" version=\"3.0\" xml:lang=\"ja\">
  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">
    <dc:identifier id=\"bookid\">urn:uuid:37c20798-18a7-4d17-b484-20d3f7cae200</dc:identifier>
    <dc:title>縦書き半角約物確認</dc:title><dc:language>ja</dc:language>
  </metadata>
  <manifest><item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>
  <spine><itemref idref=\"chapter\"/></spine>
</package>
""",
    "OEBPS/nav.xhtml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\"><head><title>目次</title></head><body><nav epub:type=\"toc\" xmlns:epub=\"http://www.idpf.org/2007/ops\"><ol><li><a href=\"chapter.xhtml\">確認ページ</a></li></ol></nav></body></html>
""",
    "OEBPS/chapter.xhtml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\">
<head><title>縦書き半角約物確認</title><style>body{writing-mode:vertical-rl;line-height:1.7;margin:0.8em;}h1{font-size:1.2em;}p{margin:0 0.7em;}</style></head>
<body>
<h1>縦書き 半角約物確認</h1>
<p>通常の約物：。「」ー</p>
<p>全角コロン・セミコロン：時刻１２：３０；比率１：２；確認</p>
<p>半角括弧：｢開始｣　｢終わり｣</p>
<p>半角句読点：｡､｡､｡､</p>
<p>半角中点：ア･イ･ウ･エ･オ</p>
<p>半角長音：ｶｰ　ｷｰ　ｸｰ　ｹｰ　ｺｰ</p>
<p>半角カナ（比較）：ｱｲｳ　ｶﾞ　ﾊﾟ</p>
<p>濁点・半濁点（比較）：ﾞ　ﾟ</p>
<p>確認項目：半角括弧・句読点・中点・長音が、縦書きの列内に収まること。</p>
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
