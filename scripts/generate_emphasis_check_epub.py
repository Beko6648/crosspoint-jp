#!/usr/bin/env python3
"""Create deterministic, original EPUB fixtures for Issue #33 (no purchased text)."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED, ZIP_STORED
from html import escape
import argparse
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ('確認方法', '''<p>圏点の表示確認用に作成した文章です。書籍のスタイルを有効にして確認してください。</p>
<p>縦書きは本文の右、横書きは本文の上に圏点が出ます。黒と白の違いを確認してください。</p>
<p>ルビを無効にしても圏点は残ります。ルビと圏点がある場合、ルビは圏点より外側に出ます。</p>
<p>同じ章を閉じて開き直し、ページ送り後も表示が変わらないことを確認します。</p>
<p>胡麻点のないフォントでは点に置き換わります。点もない場合は円になります。通常のフォントだけでは全ての代替段階を通るとは限りません。</p>'''),
    ('基本の形', ''.join(f'<p>{label}：<span style="text-emphasis-style:{style}">春風の中を歩く</span>。通常の文字。</p>'
                       for label, style in [('黒胡麻','filled sesame'),('白胡麻','open sesame'),('黒点','filled dot'),
                         ('白点','open dot'),('黒丸','filled circle'),('白丸','open circle'),('黒三角','filled triangle'),
                         ('白三角','open triangle'),('黒二重丸','filled double-circle'),('白二重丸','open double-circle')])),
    ('外部CSSと接頭辞', '''<p class="sesame">外部指定の黒胡麻点。</p><p class="epub-open">外部指定の白胡麻点。</p>
<p class="webkit-dot">外部指定の黒点。</p><p><span style="-epub-text-emphasis:open circle">接頭辞付き短縮指定</span></p>
<p><span style="-webkit-text-emphasis-style:filled sesame">別の接頭辞による指定</span></p>
<p><span style="text-emphasis:filled sesame black !important">色付き短縮指定</span></p>'''),
    ('継承と解除', '''<div class="sesame"><p>段落へ継承する圏点。</p><p>外側は黒<span class="off">ここだけ圏点なし<span class="epub-open">内側で白点を再開</span>解除の続き</span>黒へ戻る。</p></div>
<p>この段落には圏点が付きません。</p><p>ab<span class="sesame">cd</span>ef：英字の途中に不要な空白が入りません。</p>
<p class="sesame">有効<span style="text-emphasis-style:nonsense">不正値でも親の指定を維持</span>有効。</p>'''),
    ('ルビとの併用', '''<p><span class="sesame"><ruby>春風<rt>はるかぜ</rt></ruby>の中を歩く</span>。</p>
<p><ruby class="epub-open">星空<rt>ほしぞら</rt></ruby>を見上げる。</p>
<p><ruby><rb class="sesame">朝日</rb><rt>あさひ</rt></ruby>が昇る。</p>
<p>ルビ設定を切り替えても、本文の圏点は残ります。通常の文字へ圏点が漏れません。</p>'''),
    ('空白と記号と縦中横', '''<p class="sesame">春、夏。秋「冬」！空白　空白。</p>
<p class="epub-open">春 ABC 12 345 12:34 １２ 秋。</p>
<p class="sesame">か&#x3099;き&#x3099;。花&#xE0100;。</p>
<p>空白や句読点、結合濁点、異体字セレクタには単独の点を付けません。縦中横と横倒しの英字のまとまりは一つの点です。</p>'''),
    ('長い強調と改ページ', '<p>長い区間も途中で圏点が消えず、次の章へ漏れないことを確認します。</p>' +
      '<p class="sesame">' + '朝の光が窓から差し込み静かな部屋で新しい物語を読み始める。' * 110 + '</p>' +
      '<p class="epub-open">' + 'abcdefghijklmnopqrstuvwxyz' * 5 + '</p>'),
    ('解除後と再表示', '<p>この章には圏点がありません。前の章の指定が漏れないことを確認してください。</p>' +
      '<p>基本の形の章へ戻り、キャッシュ読込後も点の形が同じであることを確認します。</p>'),
]

def generate(output: Path, vertical: bool):
    direction = 'vertical' if vertical else 'horizontal'
    label = '縦書き' if vertical else '横書き'
    files = {'META-INF/container.xml': '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'}
    css = 'body{writing-mode:'+('vertical-rl' if vertical else 'horizontal-tb')+';} p{margin:0 0 1em;} '
    css += '.sesame{text-emphasis:filled sesame;} .epub-open{-epub-text-emphasis-style:open sesame;} .webkit-dot{-webkit-text-emphasis:filled dot;} .off{text-emphasis:none;}'
    files['OEBPS/style.css'] = css
    manifest = '<item id="style" href="style.css" media-type="text/css"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
    spine = nav = ''
    for i, (title, body) in enumerate(CASES):
        name = f'chapter{i}.xhtml'
        files['OEBPS/'+name] = f'<?xml version="1.0" encoding="UTF-8"?><html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja"><head><title>{title}</title><link rel="stylesheet" type="text/css" href="style.css"/></head><body><h1>{title}</h1>{body}</body></html>'
        manifest += f'<item id="c{i}" href="{name}" media-type="application/xhtml+xml"/>'
        spine += f'<itemref idref="c{i}"/>'
        nav += f'<li><a href="{name}">{escape(title)}</a></li>'
    files['OEBPS/nav.xhtml'] = f'<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目次</title></head><body><nav epub:type="toc"><ol>{nav}</ol></nav></body></html>'
    files['OEBPS/content.opf'] = f'''<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="id" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:yomuka:emphasis-check:{direction}:1</dc:identifier><dc:title>Yomuka 圏点確認 {label}</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-08T00:00:00Z</meta></metadata><manifest>{manifest}</manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}">{spine}</spine></package>'''
    output.mkdir(parents=True, exist_ok=True)
    path = output / f'yomuka_emphasis_{direction}.epub'
    with ZipFile(path, 'w') as z:
        for name, value in [('mimetype', 'application/epub+zip'), *files.items()]:
            if name.endswith(('.xml', '.xhtml', '.opf')): ET.fromstring(value)
            info = ZipInfo(name, (2026, 9, 8, 0, 0, 0))
            info.compress_type = ZIP_STORED if name == 'mimetype' else ZIP_DEFLATED
            z.writestr(info, value.encode('utf-8'))
    with ZipFile(path) as z:
        assert z.testzip() is None
        assert z.infolist()[0].filename == 'mimetype' and z.infolist()[0].compress_type == ZIP_STORED
    print(path)
    return path

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'test' / 'epubs')
    args = parser.parse_args()
    generate(args.output_dir, True)
    generate(args.output_dir, False)
