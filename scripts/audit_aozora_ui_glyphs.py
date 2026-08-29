#!/usr/bin/env python3
"""List UI-font glyphs missing from Aozora work titles and author names."""
import json, re, urllib.parse, urllib.request
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "lib/GfxRenderer/cjk_ui_font_21.h").read_text(encoding="utf-8")
GLYPHS = {int(x, 16) for x in re.findall(r"0x([0-9A-F]{4})", HEADER.split("CJK_UI_GLYPH_WIDTHS", 1)[0])}
KANA = "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
BASE = "https://aozora-epub-api.vercel.app/api/works"
missing, seen = Counter(), 0
for kana in KANA:
    offset = 0
    while True:
        url = f"{BASE}?kana_prefix={urllib.parse.quote(kana)}&offset={offset}&limit=100"
        with urllib.request.urlopen(url, timeout=30) as response:
            data = json.load(response)
        works = data.get("works", [])
        for work in works:
            seen += 1
            for char in (work.get("title", "") + work.get("author", "")):
                if ord(char) >= 0x20 and ord(char) not in GLYPHS:
                    missing[char] += 1
        offset += len(works)
        if not works or offset >= data.get("total", 0): break
lines = [f"works={seen}"]
lines += [f"U+{ord(char):04X}\t{count}\t{char}" for char, count in missing.most_common()]
(ROOT / "test/aozora_ui_glyph_audit.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print("\n".join(lines[:21]))
