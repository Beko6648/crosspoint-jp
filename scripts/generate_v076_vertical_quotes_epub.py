#!/usr/bin/env python3
"""Generate the v0.7.6 vertical quotation-mark verification EPUB."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v076_vertical_quotes.epub"

FILES = {
    "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
""",
    "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:uuid:03292676-d241-4f43-bab5-c54d743ab239</dc:identifier>
    <dc:title>v0.7.6 縦書き引用符確認</dc:title>
    <dc:language>ja</dc:language>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine><itemref idref="chapter"/></spine>
</package>
""",
    "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja">
<head><title>目次</title></head>
<body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">縦書き引用符確認</a></li></ol></nav></body>
</html>
""",
    "OEBPS/chapter.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
<head>
  <title>v0.7.6 縦書き引用符確認</title>
  <style>
    body { writing-mode: vertical-rl; line-height: 1.7; margin: 0.8em; }
    h1 { font-size: 1.15em; }
    p { margin: 0 0.8em; }
  </style>
</head>
<body>
<h1>縦書き引用符確認</h1>
<p>通常　「開始」　『二重』　〈山括弧〉　《二重山括弧》</p>
<p>通常　〔甲〕　〖乙〗　〘丙〙　〚丁〛　［角］　｛波｝</p>
<p>通常　〝全国の話題”　〝始め〟　“引用”　‘引用’</p>
<p><strong>太字　「開始」　『二重』　〈山括弧〉　《二重山括弧》</strong></p>
<p><strong>太字　〔甲〕　〖乙〗　〘丙〙　〚丁〛　［角］　｛波｝</strong></p>
<p><strong>太字　〝全国の話題”　〝始め〟　“引用”　‘引用’</strong></p>
<p>連続確認　「一」『二』〈三〉《四》〔五〕〖六〗〘七〙〚八〛</p>
<p>英字は従来どおり横倒しで表示する。ＡＢＣ　ABC　sample text</p>
</body>
</html>
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
