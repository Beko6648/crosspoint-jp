#!/usr/bin/env python3
"""Generate a copyright-safe EPUB for Yomuka v0.6.4 layout regressions."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v064_layout_regression.epub"


def kinsoku_runs() -> str:
    """Exercise every plausible column position for omitted small kana."""
    targets = "ゎヮゕゖヵヶㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾㇿ"
    runs = []
    for index, target in enumerate(targets):
        # Vary the number of preceding full-width characters. At least one
        # occurrence of every target reaches a column head for normal X3
        # reader sizes; the kinsoku rule must move it away from that position.
        runs.append("一" * (index + 1) + target + "終")
    return "　".join(runs)


FILES = {
    "META-INF/container.xml": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">
  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>
</container>
""",
    "OEBPS/content.opf": """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"bookid\" version=\"3.0\" xml:lang=\"ja\">
  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">
    <dc:identifier id=\"bookid\">urn:uuid:da50bc60-e650-41d1-9947-978899b1ed1c</dc:identifier>
    <dc:title>Yomuka v0.6.4 レイアウト確認</dc:title><dc:language>ja</dc:language>
  </metadata>
  <manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>
  <spine page-progression-direction=\"rtl\"><itemref idref=\"chapter\"/></spine>
</package>
""",
    "OEBPS/chapter.xhtml": f"""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\">
<head><title>v0.6.4 レイアウト確認</title><style>
body{{writing-mode:vertical-rl;line-height:1.7;margin:0.8em;}} p{{margin:0 0.7em;}}
</style></head>
<body>
<p>Yomuka v0.6.4 レイアウト確認。</p>
<p>欠字幅確認：次の私用領域文字は多くのSDフォントに字形がない。表示は「？」へ代替され、前後の文字と重ならず、行・列からはみ出さないこと。</p>
<p>実データ：一一一一一一一一{chr(0xE000)}一一一一一一一一。</p>
<p>小書き仮名の行頭禁則確認：以下の文字が縦書きの列先頭にならないこと。ゎ、ヮ、ゕ、ゖ、ヵ、ヶ、およびアイヌ語用小書きカタカナを確認する。</p>
<p>{kinsoku_runs()}</p>
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
