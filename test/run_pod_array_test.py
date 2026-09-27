import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--compiler', required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe = Path(directory) / 'array.exe'
    subprocess.run([args.compiler, 'c++', '-std=c++17',
                    '-I' + str(root / 'test/pod_array'),
                    '-I' + str(root / 'lib/Serialization'),
                    str(root / 'test/pod_array/PodArrayTest.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: identical POD bytes, empty arrays, signed coordinates, styles, long arrays, short writes and overflow')
