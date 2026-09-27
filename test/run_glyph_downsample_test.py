import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--compiler', required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe = Path(directory) / 'glyph.exe'
    subprocess.run([args.compiler, 'c++', '-std=c++17',
                    '-I' + str(root / 'lib/GfxRenderer'),
                    str(root / 'test/glyph_downsample/GlyphDownsampleTest.cpp'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: thin rules, all four tones, packed edges and identity scaling')
