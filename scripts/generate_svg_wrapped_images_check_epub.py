#!/usr/bin/env python3
"""Create vertical and horizontal SVG-wrapped raster-image EPUB fixtures."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]


def png(width: int, height: int, seed: int) -> bytes:
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            light = ((x // 20) + (y // 20)) % 2 == 0
            row.extend((35 + seed, 120 if light else 45, 210 - seed if light else 135))
        rows.append(bytes(row))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + \
        chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")


def chapter(title: str, body: str) -> str:
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>{title}</title>
<link rel="stylesheet" type="text/css" href="style.css"/></head><body>
<h1>{title}</h1><p>画像の前にある本文です。</p>{body}
<p><ruby>画像直後<rt>がぞうちょくご</rt></ruby>の本文です。画像の寸法制約と本文の配置を確認します。</p>
</body></html>'''


def generate(vertical: bool) -> Path:
    suffix = "vertical" if vertical else "horizontal"
    label = "縦書き" if vertical else "横書き"
    writing_mode = "vertical-rl" if vertical else "horizontal-tb"
    files = {
        "META-INF/container.xml": '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>',
        "OEBPS/style.css": f'''body{{writing-mode:{writing_mode};}} p{{margin:0 0 1em;}}
svg.svg-limited{{max-width:42%;max-height:44%;}} image.image-limited{{max-width:31%;max-height:36%;}}''',
        "OEBPS/images/xlink.png": png(720, 500, 10),
        "OEBPS/images/href.png": png(500, 760, 45),
        "OEBPS/images/cover.png": png(600, 900, 80),
        "OEBPS/cover.svg": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 600 900"><image href="images/cover.png" width="600" height="900"/></svg>''',
    }
    chapters = [
        ("xlink:href と SVG属性", '''<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="46%" height="39%" viewBox="0 0 720 500"><image xlink:href="images/xlink.png" width="720" height="500"/></svg>'''),
        ("href と SVG/image CSS", '''<svg class="svg-limited" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 500 760"><image class="image-limited" href="images/href.png" width="500" height="760"/></svg>'''),
    ]
    manifest = '<item id="style" href="style.css" media-type="text/css"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
    spine = ""
    nav = []
    for index, (title, body) in enumerate(chapters):
        name = f"chapter{index}.xhtml"
        files[f"OEBPS/{name}"] = chapter(title, body)
        manifest += f'<item id="c{index}" href="{name}" media-type="application/xhtml+xml"/>'
        spine += f'<itemref idref="c{index}"/>'
        nav.append(f'<li><a href="{name}">{title}</a></li>')
    manifest += '<item id="cover" href="cover.svg" media-type="image/svg+xml" properties="cover-image"/>'
    for image in ("xlink.png", "href.png", "cover.png"):
        manifest += f'<item id="{image}" href="images/{image}" media-type="image/png"/>'
    files["OEBPS/nav.xhtml"] = f'<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目次</title></head><body><nav epub:type="toc"><ol>{"".join(nav)}</ol></nav></body></html>'
    files["OEBPS/content.opf"] = f'''<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="id" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:svg-wrapped:{suffix}:1</dc:identifier><dc:title>Yomuka SVGラップ画像確認 {label}</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-09T00:00:00Z</meta></metadata><manifest>{manifest}</manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}">{spine}</spine></package>'''

    output = ROOT / "test" / "epubs"
    output.mkdir(parents=True, exist_ok=True)
    path = output / f"yomuka_svg_wrapped_images_{suffix}.epub"
    with ZipFile(path, "w") as archive:
        for name, value in [("mimetype", b"application/epub+zip"), *files.items()]:
            info = ZipInfo(name, (2026, 9, 9, 0, 0, 0))
            info.compress_type = ZIP_STORED if name == "mimetype" else ZIP_DEFLATED
            archive.writestr(info, value.encode("utf-8") if isinstance(value, str) else value)
    with ZipFile(path) as archive:
        assert archive.testzip() is None
        assert archive.infolist()[0].filename == "mimetype"
        assert archive.infolist()[0].compress_type == ZIP_STORED
    return path


if __name__ == "__main__":
    print(generate(False))
    print(generate(True))
