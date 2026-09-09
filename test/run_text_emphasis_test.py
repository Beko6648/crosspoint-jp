#!/usr/bin/env python3
"""Run production CSS/cache and emphasis raster tests against host-only hardware stubs."""
import argparse
from pathlib import Path
import subprocess
import os

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
parser.add_argument('--zig', action='store_true', help='Compiler is a zig executable')
args = parser.parse_args()
output = root / 'build' / 'text_emphasis'
output.mkdir(parents=True, exist_ok=True)
exe = output / ('TextEmphasisTest.exe' if os.name == 'nt' else 'TextEmphasisTest')
sources = ['test/text_emphasis/TextEmphasisTest.cpp', 'lib/Epub/Epub/blocks/TextBlockEmphasis.cpp',
           'lib/Epub/Epub/css/CssParser.cpp', 'lib/Epub/Epub/css/CssSelectorUsage.cpp', 'lib/Utf8/Utf8.cpp']
includes = ['test/text_emphasis/stubs', 'lib/Epub', 'lib/Utf8', 'lib/GfxRenderer']
command = [args.compiler] + (['c++', '-Wno-nullability-completeness'] if args.zig else [])
command += ['-std=c++20', '-O0', '-g']
command += [f'-I{root / p}' for p in includes]
command += [str(root / p) for p in sources] + ['-o', str(exe)]
subprocess.run(command, check=True)
subprocess.run([str(exe), str(output / 'emphasis-render.pgm')], check=True)
