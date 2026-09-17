#!/usr/bin/env python3
"""Generate a focused EPUB for v0.7.6 vertical right-edge ruby checks."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v076_right_edge_ruby.epub"

CHAPTERS = [
    (
        "short",
        "短いルビ",
        """<p><ruby>端<rt>はし</rt></ruby>から始まる短いルビです。右端で文字が欠けず、本文との間隔が自然か確認します。</p>
<p><ruby>漢字<rt>かんじ</rt></ruby>の後にも通常本文を続け、次の列との間隔を確認します。</p>""",
    ),
    (
        "long",
        "長いルビ",
        """<p><ruby>東京<rt>とうきょうとちよだくまるのうち</rt></ruby>から始まる長いルビです。画面の上端と右端で欠けないことを確認します。</p>
<p>通常本文を続けた後に、<ruby>日本語<rt>にほんごひょうじかくにんよう</rt></ruby>の長いルビを配置します。</p>""",
    ),
    (
        "bold",
        "太字とルビ",
        """<p><strong><ruby>太字<rt>ふとじ</rt></ruby>から始まる本文です。右端の太字本文とルビが重ならないことを確認します。</strong></p>
<p><strong><ruby>確認<rt>かくにんひょうじ</rt></ruby>する太字</strong>と通常本文を同じページで比較します。</p>""",
    ),
    (
        "plain",
        "ルビなし比較",
        """<p>ルビなしの本文から始まります。右端に過剰な空白がなく、通常の列位置を維持していることを確認します。</p>
<p><ruby>比較<rt>ひかく</rt></ruby>用のルビ付き本文は必要な列だけ右側の領域を使います。</p>""",
    ),
    (
        "edges",
        "上下端",
        """<p><ruby>上端<rt>じょうたん</rt></ruby>から始まり、あいうえおかきくけこさしすせそたちつてとなにぬねの<ruby>下端<rt>かめんしたがわのながいよみ</rt></ruby>まで続きます。</p>
<p>短いルビと長いルビが同じ列にある場合も、互いに重ならず画面内へ収まることを確認します。</p>""",
    ),
]

STYLE = """body { writing-mode: vertical-rl; margin: 0; padding: 0; line-height: 1.0; }
p { margin: 0; padding: 0; }
ruby rt { font-size: 0.5em; }
strong { font-weight: bold; }
"""

ZIP_TIMESTAMP = (2026, 1, 1, 0, 0, 0)


def write_entry(archive: ZipFile, name: str, content: str, compression: int) -> None:
    info = ZipInfo(name, ZIP_TIMESTAMP)
    info.compress_type = compression
    info.external_attr = 0o644 << 16
    archive.writestr(info, content.encode("utf-8"))


def make_opf() -> str:
    manifest = [
        '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>',
        '<item id="style" href="style.css" media-type="text/css"/>',
    ]
    spine = []
    for chapter_id, _label, _body in CHAPTERS:
        manifest.append(
            f'<item id="{chapter_id}" href="{chapter_id}.xhtml" media-type="application/xhtml+xml"/>'
        )
        spine.append(f'<itemref idref="{chapter_id}"/>')
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:uuid:d5ec38c1-e83e-4ccb-ac47-dbd31887a80f</dc:identifier>
    <dc:title>v0.7.6 右端ルビ確認</dc:title>
    <dc:creator>Yomuka Test Fixtures</dc:creator>
    <dc:language>ja</dc:language>
  </metadata>
  <manifest>{''.join(manifest)}</manifest>
  <spine>{''.join(spine)}</spine>
</package>
"""


def make_nav() -> str:
    items = "".join(
        f'<li><a href="{chapter_id}.xhtml">{label}</a></li>'
        for chapter_id, label, _body in CHAPTERS
    )
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja">
<head><title>目次</title></head>
<body><nav epub:type="toc"><ol>{items}</ol></nav></body>
</html>
"""


def make_chapter(label: str, body: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
<head><title>{label}</title><link rel="stylesheet" type="text/css" href="style.css"/></head>
<body>{body}</body>
</html>
"""


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    container = """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
    </container>
"""
    with ZipFile(OUTPUT, "w") as archive:
        write_entry(archive, "mimetype", "application/epub+zip", ZIP_STORED)
        write_entry(archive, "META-INF/container.xml", container, ZIP_DEFLATED)
        write_entry(archive, "OEBPS/content.opf", make_opf(), ZIP_DEFLATED)
        write_entry(archive, "OEBPS/nav.xhtml", make_nav(), ZIP_DEFLATED)
        write_entry(archive, "OEBPS/style.css", STYLE, ZIP_DEFLATED)
        for chapter_id, label, body in CHAPTERS:
            write_entry(
                archive,
                f"OEBPS/{chapter_id}.xhtml",
                make_chapter(label, body),
                ZIP_DEFLATED,
            )
    print(OUTPUT)


if __name__ == "__main__":
    main()
