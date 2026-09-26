#!/usr/bin/env python3
"""Test production Page ownership and deserialization with stub payload codecs."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
parser.add_argument('--zig', action='store_true')
args = parser.parse_args()
out = root / 'build' / 'batch_cache'
out.mkdir(parents=True, exist_ok=True)
exe = out / ('BatchCacheTest.exe' if os.name == 'nt' else 'BatchCacheTest')
sources = ['test/batch_cache/BatchCacheTest.cpp', 'lib/Epub/Epub/css/CssParser.cpp', 'lib/Epub/Epub/css/CssSelectorUsage.cpp', 'lib/Utf8/Utf8.cpp']
includes = ['test/batch_cache/stubs', 'test/text_emphasis/stubs', 'lib/Epub', 'lib/Utf8', 'src']
cmd = [args.compiler] + (['c++', '-Wno-nullability-completeness'] if args.zig else [])
cmd += ['-std=c++20', '-O0', '-g', '-fno-exceptions']
cmd += [f'-I{root / p}' for p in includes]
cmd += [str(root / p) for p in sources] + ['-o', str(exe)]
subprocess.run(cmd, check=True)
subprocess.run([str(exe)], check=True)
