#!/usr/bin/env python3
"""Exercise the actual vertical glyph loader under simulated fragmented heap and failures."""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'lib/EpdFont/SdCardFont.cpp').read_text(encoding='utf-8')
body = source[source.index('bool SdCardFont::loadVertData('):source.index('// --- Advance table ---')]
with tempfile.TemporaryDirectory(prefix='yomuka-vertical-') as directory:
    out=Path(directory)
    (out/'VerticalLoad.inc').write_text(body,encoding='utf-8')
    exe=out/'test'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-include','initializer_list','-I'+str(out),'-I'+str(root/'lib/EpdFont'),str(root/'test/vertical_glyph_load/VerticalLoadTest.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: fragmented heap, unchanged total floor, cached reuse, release/reload, every allocation failure, truncated data and retry')
