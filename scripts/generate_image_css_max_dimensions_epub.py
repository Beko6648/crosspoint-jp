#!/usr/bin/env python3
"""Create original EPUB fixtures for CSS max-width/max-height image sizing."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED, ZIP_STORED
import argparse
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


def png(width: int, height: int, seed: int) -> bytes:
    """Return a deterministic RGB checkerboard PNG without external dependencies."""
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            light = ((x // 24) + (y // 24)) % 2 == 0
            row.extend((40 + seed, 120 if light else 40, 210 - seed if light else 130))
        rows.append(bytes(row))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)

    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b'')


def chapter(title: str, css_class: str, image: str, text: str, inline_style: str = '') -> str:
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>{title}</title>
<link rel="stylesheet" type="text/css" href="style.css"/></head><body>
<h1>{title}</h1><p>{text}</p><img class="{css_class}"{inline_style} src="images/{image}" alt="寸法確認用の格子画像"/>
<p>画像の次に続く本文です。画像の縦横比を保ち、指定した上限に収まることを確認します。</p>
</body></html>'''


def generate(output: Path, vertical: bool) -> Path:
    suffix = 'vertical' if vertical else 'horizontal'
    writing_mode = 'vertical-rl' if vertical else 'horizontal-tb'
    label = '縦書き' if vertical else '横書き'
    files = {
        'META-INF/container.xml': '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>',
        'OEBPS/style.css': f'''body{{writing-mode:{writing_mode};}} p{{margin:0 0 1em;}}
.wide{{max-width:45%;}} .tall{{max-height:38%;}} .both{{max-width:58%;max-height:42%;}}
.width_then_max{{width:85%;max-width:36%;}} .height_then_max{{height:85%;max-height:34%;}}
.inline_limit{{max-width:1em;max-height:1em;}}''',
        'OEBPS/images/wide.png': png(720, 220, 10),
        'OEBPS/images/tall.png': png(180, 760, 35),
        'OEBPS/images/large.png': png(720, 620, 60),
        'OEBPS/images/icon.png': png(8, 8, 85),
    }
    cases = [
        ('max-width 45%', 'wide', 'wide.png', '横長の原寸画像を、横幅だけ45%に制限します。'),
        ('max-height 38%', 'tall', 'tall.png', '縦長の原寸画像を、縦幅だけ38%に制限します。'),
        ('max-width と max-height', 'both', 'large.png', '二つの上限のうち、先に達する方へ縮小します。'),
        ('width と max-width', 'width_then_max', 'wide.png', '幅85%の指定へ、max-width 36%を追加します。'),
        ('height と max-height', 'height_then_max', 'tall.png', '高さ85%の指定へ、max-height 34%を追加します。'),
        ('小さな画像の上限', 'inline_limit', 'icon.png', '上限だけの1em指定では、元の8px画像を拡大しません。'),
        ('インラインの上限', '', 'large.png', 'style属性のmax-widthとmax-heightも、外部CSSと同じように反映します。',
         ' style="max-width:31%; max-height:29%"'),
        ('縦書きの本文と画像', 'both', 'large.png',
         '画像の右側に本文列を残し、画像の左側にも続きの本文列を配置します。'),
        ('fit の単ページ', 'fit', 'large.png', 'fit指定の画像は縦書きでも単ページに保ちます。'),
    ]
    manifest = '<item id="style" href="style.css" media-type="text/css"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
    spine = ''
    nav_items = []
    for index, (title, css_class, image, text, *inline_style) in enumerate(cases):
        name = f'chapter{index}.xhtml'
        files[f'OEBPS/{name}'] = chapter(title, css_class, image, text, *inline_style)
        manifest += f'<item id="c{index}" href="{name}" media-type="application/xhtml+xml"/>'
        spine += f'<itemref idref="c{index}"/>'
        nav_items.append(f'<li><a href="{name}">{title}</a></li>')
    for image in ('wide.png', 'tall.png', 'large.png', 'icon.png'):
        manifest += f'<item id="{image}" href="images/{image}" media-type="image/png"/>'
    files['OEBPS/nav.xhtml'] = f'<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目次</title></head><body><nav epub:type="toc"><ol>{"".join(nav_items)}</ol></nav></body></html>'
    files['OEBPS/content.opf'] = f'''<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="id" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:image-css-max:{suffix}:1</dc:identifier><dc:title>Yomuka 画像CSS上限確認 {label}</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-08T00:00:00Z</meta></metadata><manifest>{manifest}</manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}">{spine}</spine></package>'''

    output.mkdir(parents=True, exist_ok=True)
    path = output / f'yomuka_image_css_max_{suffix}.epub'
    with ZipFile(path, 'w') as archive:
        for name, value in [('mimetype', b'application/epub+zip'), *files.items()]:
            info = ZipInfo(name, (2026, 9, 8, 0, 0, 0))
            info.compress_type = ZIP_STORED if name == 'mimetype' else ZIP_DEFLATED
            archive.writestr(info, value.encode('utf-8') if isinstance(value, str) else value)
    with ZipFile(path) as archive:
        assert archive.testzip() is None
        assert archive.infolist()[0].filename == 'mimetype' and archive.infolist()[0].compress_type == ZIP_STORED
    print(path)
    return path


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'test' / 'epubs')
    args = parser.parse_args()
    generate(args.output_dir, False)
    generate(args.output_dir, True)
