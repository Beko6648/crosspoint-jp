"""Check lossless UI font extension against the accepted base header."""
import importlib.util
import subprocess
import tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("generator",r/"scripts/generate_cjk_ui_font.py")
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
with tempfile.TemporaryDirectory() as d:
 old=Path(d)/"old.h"
 old.write_bytes(subprocess.check_output(["git","show","e9a5989a:lib/GfxRenderer/cjk_ui_font_21.h"],cwd=r))
 before=g.load_existing_glyphs(old,21)
after=g.load_existing_glyphs(r/"lib/GfxRenderer/cjk_ui_font_21.h",21)
expected={ord(c) for c in g.extract_codepoints_from_file(r/"scripts/codepoints/ui_jis2_symbols.txt")}
assert len(before)==3898 and len(after)==7190
assert set(after)-set(before)==expected and len(expected)==3292
assert all(after[cp]==data for cp,data in before.items()), "Existing glyph changed"
assert all(any(after[cp][1]) for cp in expected), "Blank new glyph"
assert all(0 < after[cp][0] <= 21 for cp in expected)
for row in range(48,85):
 for col in range(1,95):
  try: c=bytes([row+160,col+160]).decode("euc_jp")
  except UnicodeDecodeError: continue
  assert ord(c) in after
# The build guard must reject a missing required glyph, without trusting comments.
spec=importlib.util.spec_from_file_location("guard",r/"scripts/check_cjk_ui_font.py")
guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/"header.h"
 p.write_text("// 0x7F9E\nstatic const uint16_t CJK_UI_CODEPOINTS[] PROGMEM = {0x534D};")
 assert guard.extract_codepoints_from_header(p)=={0x534D}
print("PASS: 7190 glyphs; 3292 additions nonblank; all old widths/bitmaps unchanged; complete JIS2; guard ignores comments")
