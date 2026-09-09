#!/usr/bin/env python3
"""Generate original EPUB fixtures for underline and strikethrough device tests."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo
import argparse
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ("確認方法", """<p>下線は文字の下、取り消し線は文字の中央を通ることを確認します。</p>
<p>書籍のスタイルを有効にし、同じ章を閉じて開き直しても装飾が残ることを確認します。</p>"""),
    ("外部CSS", """<p class="under">外部CSSの下線です。</p><p class="strike">外部CSSの取り消し線です。</p>
<p class="both">外部CSSで両方の線です。</p><p class="line">text-decoration-line の取り消し線です。</p>"""),
    ("インラインCSSとタグ", """<p><span style="text-decoration:underline">インライン下線</span>、<span style="text-decoration:line-through">インライン取り消し線</span>、<span style="text-decoration:underline line-through">両方</span>。</p>
<p><u>uタグ</u>、<ins>insタグ</ins>、<s>sタグ</s>、<strike>strikeタグ</strike>、<del>delタグ</del>。</p>"""),
    ("継承と解除", """<p class="under">外側の下線と<span class="strike">内側の取り消し線</span>外側へ戻る。</p>
<p class="both">両方の外側と<span class="none">noneを指定した内側</span>両方へ戻る。</p>
<p>装飾が次の段落に漏れないことを確認します。</p>"""),
    ("長い装飾と改ページ", """<p class="both">""" + "長い装飾区間が改行と改ページの後も途切れず、次の通常文へ漏れないことを確認します。" * 60 + "</p>"
      + "<p class=\"both\">" + "abcdefghijklmnopqrstuvwxyz0123456789" * 8 + "</p>"),
    ("ルビとの併用", """<p><span class="under"><ruby>春風<rt>はるかぜ</rt></ruby>の中を歩く</span>。</p>
<p><span class="strike"><ruby>星空<rt>ほしぞら</rt></ruby>を見上げる</span>。</p>"""),
]

def add_file(z, name, text):
    if name.endswith((".xml", ".xhtml", ".opf")):
        ET.fromstring(text)
    info = ZipInfo(name, (2026, 9, 8, 0, 0, 0))
    info.compress_type = ZIP_STORED if name == "mimetype" else ZIP_DEFLATED
    z.writestr(info, text.encode("utf-8"))

def generate(output_dir: Path, vertical: bool):
    direction = "vertical" if vertical else "horizontal"
    label = "縦書き" if vertical else "横書き"
    files = {"META-INF/container.xml": "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>"}
    files["OEBPS/style.css"] = ("body{writing-mode:" + ("vertical-rl" if vertical else "horizontal-tb") + ";} "
        "p{margin:0 0 1em;} .under{text-decoration:underline;} .strike{text-decoration:line-through;} "
        ".both{text-decoration:underline line-through;} .line{text-decoration-line:line-through;} .none{text-decoration:none;}")
    manifest = '<item id="style" href="style.css" media-type="text/css"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
    spine, nav = [], []
    for i, (title, body) in enumerate(CASES):
        name = f"chapter{i}.xhtml"
        files[f"OEBPS/{name}"] = ("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\">"
            f"<head><title>{title}</title><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/></head><body><h1>{title}</h1>{body}</body></html>")
        manifest += f'<item id="c{i}" href="{name}" media-type="application/xhtml+xml"/>'
        spine.append(f'<itemref idref="c{i}"/>')
        nav.append(f'<li><a href="{name}">{title}</a></li>')
    files["OEBPS/nav.xhtml"] = "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\"><head><title>目次</title></head><body><nav epub:type=\"toc\"><ol>" + "".join(nav) + "</ol></nav></body></html>"
    files["OEBPS/content.opf"] = ("<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"id\" version=\"3.0\" xml:lang=\"ja\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        f"<dc:identifier id=\"id\">urn:yomuka:text-decoration:{direction}:1</dc:identifier><dc:title>Yomuka 下線・取り消し線確認 {label}</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language>"
        "<meta property=\"dcterms:modified\">2026-09-08T00:00:00Z</meta></metadata><manifest>" + manifest + "</manifest><spine page-progression-direction=\"" + ("rtl" if vertical else "ltr") + "\">" + "".join(spine) + "</spine></package>")
    output_dir.mkdir(parents=True, exist_ok=True)
    out = output_dir / f"yomuka_text_decoration_{direction}.epub"
    with ZipFile(out, "w") as z:
        add_file(z, "mimetype", "application/epub+zip")
        for name, content in files.items(): add_file(z, name, content)
    with ZipFile(out) as z:
        assert z.testzip() is None
        assert z.infolist()[0].filename == "mimetype" and z.infolist()[0].compress_type == ZIP_STORED
    print(out)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / "test" / "epubs")
    args = parser.parse_args()
    generate(args.output_dir, False)
    generate(args.output_dir, True)
