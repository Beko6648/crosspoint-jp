#!/usr/bin/env python3
"""Test the pinned JPEGDEC with real synthetic JPEG fixtures and production patches.

Fixtures: libjpeg-turbo cjpeg 2.1.5, quality 85, dimensions 257x193/600x900,
sampling 1x1/2x1/2x2. All fixtures were independently accepted by djpeg.
The same RGB pattern is encoded as baseline, standard progressive, interleaved
full-precision DC, separated Y/Cb/Cr DC scans, and grayscale references.
Gray fullprecision has the same quantized Y data as separated/interleaved.
Progressive references use the same initial DC approximation as cjpeg defaults.
"""
import argparse
import csv
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
parser.add_argument('--asan', action='store_true')
args = parser.parse_args()
dependency = root / '.pio/libdeps/default/JPEGDEC'
fixtures = sorted((root / 'test/jpeg_progressive/fixtures').glob('*.jpg'))
assert len(fixtures) == 30
patches = sorted((root / 'scripts/jpegdec_patches').glob('*.patch'))
assert len(patches) == 3


def metrics(row):
    return row['result'], row['error'], row['pixels'], row['hash']


with tempfile.TemporaryDirectory(prefix='yomuka-jpeg-progressive-') as directory:
    work = Path(directory)
    # Load the actual build hook without allowing its entry point to touch deps.
    hook = runpy.run_path(str(root / 'scripts/patch_jpegdec.py'),
                         init_globals={'Import': lambda _: None,
                                       'env': {'PROJECT_DIR': str(work)}})
    pinned = subprocess.check_output(['git', '-C', str(dependency), 'show', 'HEAD:src/jpeg.inl'])
    results = {}
    for version in ['before', 'after']:
        dest = work / version
        src = dest / 'src'
        shutil.copytree(dependency / 'src', src)
        (src / 'jpeg.inl').write_bytes(pinned)
        subprocess.run(['git', 'init', '-q', str(dest)], check=True)
        subprocess.run(['git', '-C', str(dest), 'config', 'core.autocrlf', 'false'], check=True)
        hook['_apply_mcu_skip_pointer_fix'](str(src / 'jpeg.inl'))
        if version == 'after':
            for patch in patches:
                hook['_apply_patch'](str(dest), str(patch))
            content = (src / 'jpeg.inl').read_bytes()
            for patch in patches:
                hook['_apply_patch'](str(dest), str(patch))
            assert (src / 'jpeg.inl').read_bytes() == content, 'patches must be idempotent'
        exe = work / ('decode-' + version + ('.exe' if os.name == 'nt' else ''))
        command = [args.compiler, '-std=c++17', '-O1', '-g', '-D__LINUX__', '-I' + str(src)]
        if args.asan:
            command += ['-fsanitize=address', '-fno-omit-frame-pointer']
        subprocess.run(command + [str(root / 'test/jpeg_progressive/DecodeTest.cpp'),
                                  str(src / 'JPEGDEC.cpp'), '-o', str(exe)], check=True)
        output = subprocess.check_output([str(exe), *map(str, fixtures)], text=True, timeout=60)
        results[version] = {(Path(row['fixture']).name, row['scale']): row
                            for row in csv.DictReader(output.splitlines())}
    # A diverged patch target must abort without modifying its contents.
    target = work / 'before/src/jpeg.inl'
    content = target.read_text().replace('pMCU[0] = (short)*iDCPredictor;', 'pMCU[0] = 123;')
    target.write_text(content)
    before_bytes = target.read_bytes()
    try:
        hook['_apply_patch'](str(work / 'before'), str(patches[0]))
    except RuntimeError:
        pass
    else:
        raise AssertionError('diverged decoder must fail closed')
    assert target.read_bytes() == before_bytes

controls = exact = reproduced = 0
for key, after in results['after'].items():
    assert after['result'] == '1' and after['error'] == '0' and int(after['pixels']) > 0, key
    before = results['before'][key]
    name, scale = key
    if '-baseline.' in name or '-gray-' in name:
        assert metrics(after) == metrics(before), key
        controls += 1
    elif '-separated.' in name:
        reference = results['after'][(name.split('-')[0] + '-gray-fullprecision.jpg', scale)]
        assert metrics(after) == metrics(reference), key
        assert metrics(before) != metrics(reference), 'fixture must expose the original defect'
        reproduced += 1
        exact += 1
    elif scale in ['0', '8']:
        # The EPUB converter explicitly requests 1/8 for progressive images.
        # JPEGDEC's half/quarter overrides are outside that production path.
        mode = 'progressive' if '-progressive.' in name else 'fullprecision'
        reference = results['after'][(name.split('-')[0] + '-gray-' + mode + '.jpg', scale)]
        assert metrics(after) == metrics(reference), key
        exact += 1
print(f'PASS: 120 decodes per version; {controls} unchanged controls; {exact} exact Y-reference matches; '
      f'{reproduced} original separated-scan defects reproduced and fixed')
print('PASS: original sampling metadata retained; repeated patches unchanged; diverged source rejected without writes')
