#!/usr/bin/env python3
"""Run the production PixelCache writer with allocation and storage failures."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
parser.add_argument('--zig', action='store_true')
args = parser.parse_args()
out = root / 'build' / 'image_cache'
out.mkdir(parents=True, exist_ok=True)
exe = out / ('PixelCacheTest.exe' if os.name == 'nt' else 'PixelCacheTest')
cmd = [args.compiler] + (['c++'] if args.zig else [])
cmd += ['-std=c++17', '-O0', '-g', '-I' + str(root / 'test/image_cache/stubs'),
        '-I' + str(root / 'lib/Epub/Epub/converters'), str(root / 'test/image_cache/PixelCacheTest.cpp'),
        '-o', str(exe)]
subprocess.run(cmd, check=True)
subprocess.run([str(exe)], check=True)
