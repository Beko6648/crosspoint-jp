#!/usr/bin/env python3
"""Generate a long, copyright-safe EPUB for SD-font page reuse testing."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "test" / "epubs" / "yomuka_v076_sd_font_reuse_vertical.epub"

TOPICS = (
    ("朝の駅", "改札 時刻表 切符 階段 乗換案内 発車ベル 車窓 通勤客"),
    ("港の倉庫", "岸壁 貨物 船倉 航路 灯台 潮風 荷札 作業灯"),
    ("山の観測所", "稜線 気圧 雲量 風向 温度計 望遠鏡 雪渓 日の出"),
    ("町の図書館", "書架 貸出票 閲覧席 目録 栞 製本 返却口 新聞"),
    ("雨の商店街", "軒先 雨粒 看板 包装紙 店番 長靴 水たまり 閉店"),
    ("川辺の工房", "作業台 木材 鉋 定規 接着剤 木屑 図面 仕上げ"),
    ("夜の劇場", "客席 舞台 袖幕 照明 台本 開演 拍手 楽屋"),
    ("丘の農園", "畑 土壌 苗木 収穫 籠 温室 用水路 夕焼け"),
    ("冬の診療所", "待合室 診察券 体温 薬袋 聴診器 受付 暖炉 予約"),
    ("春の学校", "校庭 教室 黒板 教科書 時間割 放課後 花壇 上履き"),
    ("古い時計店", "振り子 歯車 文字盤 秒針 工具箱 修理台 時報 ねじ"),
    ("海辺の研究室", "標本 顕微鏡 海藻 水槽 記録紙 試薬 採集 潮位"),
    ("森の遊歩道", "木漏れ日 落葉 野鳥 苔 小径 道標 湧水 枝先"),
    ("市場の朝", "青果 鮮魚 値札 台車 氷箱 掛け声 帳場 配達"),
    ("陶芸の窯場", "粘土 釉薬 ろくろ 窯 焼成 器 模様 灰"),
    ("小さな印刷所", "活字 原稿 校正紙 印刷機 裁断 製本 インク 見本"),
    ("高原の宿", "客室 暖炉 毛布 献立 玄関 鍵 朝霧 旅人"),
    ("地下の資料室", "保管箱 台帳 棚番号 写真 地図 封筒 年代 鍵穴"),
    ("風の発電所", "風車 羽根 発電機 制御盤 送電線 点検 計器 草原"),
    ("島の郵便局", "郵袋 消印 葉書 窓口 配達船 宛名 切手 集配"),
    ("路地の食堂", "献立 鍋 湯気 暖簾 食器 注文 厨房 常連"),
    ("湖畔の写真館", "三脚 暗室 印画紙 レンズ 額縁 露光 背景布 現像"),
    ("夕方の公園", "噴水 ベンチ 街灯 砂場 自転車 並木 散歩 影"),
    ("岬の気象台", "雨量計 観測表 無線 風速 海面 予報 気圧計 警報"),
)

TEMPLATES = (
    "{title}では、{a}の位置を確かめてから{b}へ進み、{c}の状態を記録した。"
    "担当者は昨日の値と照らし合わせ、違いが小さくても理由を欄外へ書き添えた。"
    "<strong>{d}と{e}は今回の重点確認項目である。</strong>"
    "その後、{f}の周囲を整え、次の担当者が迷わないよう{g}と{h}を順番に並べた。",
    "{title}の作業は、{a}を準備するところから始まった。{b}を動かす前に{c}を見直すと、"
    "予定表にはない小さな変更が必要だと分かった。"
    "<strong>変更後は{d}、{e}、{f}の順に再確認した。</strong>"
    "最後に{g}を片付け、{h}について短い引継ぎを書いた。",
    "午前の{title}は静かだったが、{a}の近くでは{b}を扱う音が続いていた。"
    "記録係は{c}を基準に時刻をそろえ、途中の判断も省略せず残した。"
    "<strong>{d}に変化が出た時点で、{e}の確認を追加する。</strong>"
    "作業が終わるころには{f}が整い、{g}と{h}も所定の場所へ戻っていた。",
    "{title}で新しい手順を試すため、まず{a}と{b}を別々に調べた。"
    "結果を比べると{c}の扱いに差があり、担当者同士で記録方法を統一した。"
    "<strong>太字の確認箇所は{d}と{e}で、読み落としを防ぐ。</strong>"
    "続いて{f}を点検し、{g}を経由して{h}まで問題なく進めることを確かめた。",
    "薄明るい{title}で、{a}を手にした係員が{b}の前に立った。"
    "{c}には前回の作業跡が残っていたため、消さずに新しい記録を隣へ加えた。"
    "<strong>{d}の数値と{e}の状態は必ず二人で照合する。</strong>"
    "確認後は{f}を戻し、{g}を閉じてから{h}を次の場所へ運んだ。",
    "{title}の一日は、{a}の確認と{b}の整理で区切られている。"
    "今日は{c}の順番を変えたので、作業時間と結果をいつもより細かく記録した。"
    "<strong>{d}と{e}を同じ条件で比べることが重要である。</strong>"
    "夕方には{f}の点検も終わり、{g}と{h}を残して予定どおり終了した。",
)


def build_paragraphs() -> str:
    paragraphs = []
    # Ninety short paragraphs keep a Bold token on practically every rendered
    # page while rotating vocabulary enough to avoid an artificial 100% hit.
    for index in range(90):
        title, words_text = TOPICS[index % len(TOPICS)]
        words = words_text.split()
        shift = (index // len(TOPICS)) % len(words)
        words = words[shift:] + words[:shift]
        text = TEMPLATES[index % len(TEMPLATES)].format(
            title=title,
            **dict(zip("abcdefgh", words, strict=True)),
        )
        paragraphs.append(f'<p><span class="marker">{index + 1:02d}</span>　{text}</p>')
    return "\n".join(paragraphs)


def files() -> dict[str, str]:
    chapter = f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
<head><title>Yomuka v0.7.6 SDフォント再利用確認</title><link rel="stylesheet" type="text/css" href="style.css"/></head>
<body>
<h1>SDフォント再利用確認</h1>
<p>Zen Maru Gothic 14ptのRegularとBoldを、単一セクションで連続測定するための文章です。各段落の太字が表示され、欠けや重なりがないことも確認してください。</p>
{build_paragraphs()}
</body></html>
"""
    return {
        "META-INF/container.xml": """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>
""",
        "OEBPS/content.opf": """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="ja"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="bookid">urn:uuid:069f3e77-c989-48d6-91ca-ff27692cf25b</dc:identifier><dc:title>Yomuka v0.7.6 SDフォント再利用確認</dc:title><dc:creator>Yomuka Test Fixtures</dc:creator><dc:language>ja</dc:language><meta property="dcterms:modified">2026-09-17T00:00:00Z</meta></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="style" href="style.css" media-type="text/css"/><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine page-progression-direction="rtl"><itemref idref="chapter"/></spine></package>
""",
        "OEBPS/nav.xhtml": """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="ja"><head><title>目次</title></head><body><nav epub:type="toc"><ol><li><a href="chapter.xhtml">SDフォント再利用確認</a></li></ol></nav></body></html>
""",
        "OEBPS/style.css": """html, body { writing-mode: vertical-rl; -epub-writing-mode: vertical-rl; }
body { margin: 0.8em; line-height: 1.65; }
h1 { font-size: 1.25em; margin: 0 0 1em; }
p { margin: 0 0.72em; text-align: justify; }
strong { font-weight: 700; }
.marker { font-size: 0.8em; }
""",
        "OEBPS/chapter.xhtml": chapter,
    }


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, content in files().items():
            archive.writestr(name, content.encode("utf-8"), compress_type=ZIP_DEFLATED)
    digest = sha256(OUTPUT.read_bytes()).hexdigest()
    print(f"{OUTPUT}\nsha256={digest}")


if __name__ == "__main__":
    main()
