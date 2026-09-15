#!/usr/bin/env python3
"""Generate compact horizontal/vertical regression EPUBs for v0.7.5 kinsoku."""
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile


ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "test" / "epubs"

CHAPTERS = [
    ("中点・引用符", "コーヒー・紅茶・ジュース。「引用文」と、その続き。" * 12),
    ("繰返し・長音・ハイフン", "日々は続く。いろゝゝの事。東京〜大阪、カレー・ライス、スーパー。" * 10),
    ("省略記号・ダッシュ", "そして……彼は――去った。残されたのは沈黙……だけだった。" * 12),
    ("前置・後置記号", "価格は〒千円、温度は20℃、割引は10％、角度は90°である。" * 10),
]


def write_book(vertical: bool):
    direction = "vertical" if vertical else "horizontal"
    mode = "vertical-rl" if vertical else "horizontal-tb"
    files = {
        "mimetype": "application/epub+zip",
        "META-INF/container.xml": """<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>""",
    }
    manifest = ['<item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/>']
    spine = []
    nav = []
    for index, (title, text) in enumerate(CHAPTERS, 1):
        item_id = f"chapter{index}"
        href = f"chapter{index}.xhtml"
        manifest.append(f'<item id="{item_id}" href="{href}" media-type="application/xhtml+xml"/>')
        spine.append(f'<itemref idref="{item_id}"/>')
        nav.append(f'<li><a href="{href}">{title}</a></li>')
        files[f"OEBPS/{href}"] = f'''<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title><style>body{{writing-mode:{mode};-epub-writing-mode:{mode};}}</style></head><body><h1>{title}</h1><p>{text}</p></body></html>'''
    files["OEBPS/content.opf"] = f'''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:v075:kinsoku:{direction}</dc:identifier><dc:title>Yomuka v0.7.5 禁則 {direction}</dc:title><dc:language>ja</dc:language></metadata><manifest>{''.join(manifest)}</manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}">{''.join(spine)}</spine></package>'''
    files["OEBPS/nav.xhtml"] = f'''<?xml version="1.0"?><html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol>{''.join(nav)}</ol></nav></body></html>'''
    path = DEST / f"v075_kinsoku_{direction}.epub"
    with zipfile.ZipFile(path, "w") as book:
        for name, data in files.items():
            payload = data.encode("utf-8")
            if name.endswith((".xml", ".opf", ".xhtml")):
                ET.fromstring(payload)
            info = zipfile.ZipInfo(name, date_time=(2026, 9, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED if name == "mimetype" else zipfile.ZIP_DEFLATED
            book.writestr(info, payload)
    with zipfile.ZipFile(path) as book:
        assert book.testzip() is None
    print(path)


if __name__ == "__main__":
    DEST.mkdir(parents=True, exist_ok=True)
    write_book(False)
    write_book(True)
