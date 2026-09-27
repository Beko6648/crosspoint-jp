"""Compile the production SD list against a fault-injectable storage double."""
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--compiler', required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe = Path(directory) / 'paths.exe'
    subprocess.run([args.compiler, 'c++', '-std=c++17',
                    '-I' + str(root / 'test/batch_epub_paths'),
                    '-I' + str(root / 'src/util'),
                    str(root / 'test/batch_epub_paths/BatchEpubPathsTest.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: UTF-8 paths, repeat reads, empty/partial/500-book lists, cleanup and seven I/O faults')
