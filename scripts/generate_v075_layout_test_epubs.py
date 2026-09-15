#!/usr/bin/env python3
"""Generate deterministic, synthetic horizontal/vertical v0.7.5 regression books."""
from pathlib import Path
import struct
import zlib
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'test' / 'epubs'


def png_chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


PNG = (b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('>IIBBBBB', 8, 8, 8, 0, 0, 0, 0))
       + png_chunk(b'IDAT', zlib.compress(b''.join(b'\0' + bytes([0 if (x + y) % 2 else 255 for x in range(8)])
                                                 for y in range(8)))) + png_chunk(b'IEND', b''))

NORMAL = '''<h1>通常回帰</h1>
<p>第一章 序。<ruby>日本語<rt>にほんご</rt></ruby>の<b>太字</b>と<span style="text-emphasis: filled dot">圏点</span>。
画像前<img src="mark.png" style="display:inline;width:20px;height:20px" alt="印"/>画像後。
別画像<img src="mark.png" style="display:inline;width:12px;height:12px" alt="小印"/>末尾。</p>
<p>「括弧」句読点、。小書きゃゅょ、々・：；〜“引用”……――。ab&#173;cd efghijklmnopqrstuvwxyz。</p>
<p>原文スペースの比較：日本 語／日本\n語／日本\n  語／日本&#160;語／abc def。</p>
<ol start="3"><li>番号三の長い文章。長い文章。長い文章。<ol start="7"><li>入れ子七</li></ol></li>
<li value="9">番号九</li><li>番号十</li></ol><hr/>
<p>上付き x<sup>2</sup>、下付き H<sub>2</sub>O。</p>
<pre>\n  leading spaces\n\n\ttab\nlast line\n</pre>
<p>見出し前の本文。</p><h1>見出し一（既定余白）</h1><p>見出し一の直後の本文。</p>
<h2 style="margin-top:0.5em;margin-bottom:0.5em">見出し二（CSS余白）</h2><p>見出し二の直後の本文。</p>
<h3>見出し三</h3><p>見出し三の直後の本文。</p>'''
STRESS = ('<h1>低メモリ・長文回帰</h1><p>' + '本文と句読点、。' * 900 + '</p><p>'
          + '<ruby>' + '長い親文字' * 180 + '<rt>' + 'ながいルビ' * 180 + '</rt></ruby></p>'
          + '<p><span style="text-emphasis: filled sesame">' + '圏点付き本文' * 500 + '</span></p>'
          + '<p>' + 'abcdefghijklmnopqrstuvwxyz' * 80 + '</p>' + NORMAL)


def write_book(vertical):
    direction = 'vertical' if vertical else 'horizontal'
    mode = 'vertical-rl' if vertical else 'horizontal-tb'
    title = f'Yomuka v0.7.5 layout {direction}'
    files = {
        'mimetype': 'application/epub+zip',
        'META-INF/container.xml': '''<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>''',
        'OEBPS/content.opf': f'''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:v075:{direction}</dc:identifier><dc:title>{title}</dc:title><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-14T00:00:00Z</meta></metadata><manifest><item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/><item id="normal" href="normal.xhtml" media-type="application/xhtml+xml"/><item id="stress" href="stress.xhtml" media-type="application/xhtml+xml"/><item id="mark" href="mark.png" media-type="image/png"/></manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}"><itemref idref="normal"/><itemref idref="stress"/></spine></package>''',
        'OEBPS/nav.xhtml': '''<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目次</title></head><body><nav epub:type="toc"><ol><li><a href="normal.xhtml">通常回帰</a></li><li><a href="stress.xhtml">低メモリ・長文回帰</a></li></ol></nav></body></html>''',
        'OEBPS/mark.png': PNG,
    }
    for name, body in [('normal', NORMAL), ('stress', STRESS)]:
        files[f'OEBPS/{name}.xhtml'] = f'''<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title><style>body {{ writing-mode:{mode}; -epub-writing-mode:{mode}; }}</style></head><body>{body}</body></html>'''
    path = DEST / f'v075_layout_{direction}.epub'
    with zipfile.ZipFile(path, 'w') as book:
        for name, data in files.items():
            if isinstance(data, str):
                data = data.encode('utf-8')
            if name.endswith(('.xml', '.opf', '.xhtml')):
                ET.fromstring(data)
            entry = zipfile.ZipInfo(name, date_time=(2026, 9, 14, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_STORED if name == 'mimetype' else zipfile.ZIP_DEFLATED
            book.writestr(entry, data)
    with zipfile.ZipFile(path) as book:
        assert book.testzip() is None
    print(path)


if __name__ == '__main__':
    DEST.mkdir(parents=True, exist_ok=True)
    write_book(False)
    write_book(True)
