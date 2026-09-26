#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
parser.add_argument('--zig', action='store_true')
args = parser.parse_args()
out = root / 'build/decoder_loan'
out.mkdir(parents=True, exist_ok=True)
exe = out / ('DecoderLoanTest.exe' if os.name == 'nt' else 'DecoderLoanTest')
cmd = [args.compiler] + (['c++'] if args.zig else [])
cmd += ['-std=c++17', '-O0', '-g']
cmd += ['-I' + str(root / p) for p in ['test/decoder_loan/stubs', 'test/image_cache/stubs',
                                     'lib/Memory', 'lib/Epub/Epub/converters']]
cmd += [str(root / p) for p in ['test/decoder_loan/DecoderLoanTest.cpp', 'lib/Memory/BuildScratch.cpp']]
subprocess.run(cmd + ['-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)

# Also compare real JPEGDEC pixel output between heap and framebuffer placement.
sources = root / '.pio/libdeps/default/JPEGDEC/src'
if not (sources / 'JPEGDEC.cpp').is_file():
    raise SystemExit('Run pio run -e default first to install the pinned JPEGDEC dependency')
fixtures = []
with zipfile.ZipFile(root / 'test/epubs/test_jpeg_images.epub') as archive:
    for name in archive.namelist():
        if name.endswith('.jpg'):
            dest = out / Path(name).name
            dest.write_bytes(archive.read(name))
            fixtures.append(str(dest))
real_exe = out / ('JpegLoanDecodeTest.exe' if os.name == 'nt' else 'JpegLoanDecodeTest')
real_cmd = [args.compiler] + (['c++'] if args.zig else [])
real_cmd += ['-std=c++17', '-O0', '-g', '-D__LINUX__', '-I' + str(sources)]
if args.zig:
    real_cmd += ['-Wno-macro-redefined']
real_cmd += ['-I' + str(root / p) for p in ['test/decoder_loan/stubs', 'test/image_cache/stubs',
                                          'lib/Memory', 'lib/Epub/Epub/converters']]
# Build the unchanged external decoder with its normal non-sanitized behavior.
# Zig otherwise traps its x86 unaligned loads and existing negative shift in
# JPEGMakeHuffTables. Keep host safety checks on our holder/test translation unit.
lib_obj = out / 'jpegdec-host.o'
lib_cmd = [args.compiler] + (['c++'] if args.zig else [])
lib_cmd += ['-std=c++17', '-O0', '-g', '-D__LINUX__', '-I' + str(sources)]
if args.zig:
    lib_cmd += ['-fno-sanitize=undefined', '-Wno-macro-redefined']
subprocess.run(lib_cmd + ['-c', str(sources / 'JPEGDEC.cpp'), '-o', str(lib_obj)], check=True)
real_cmd += [str(root / 'test/decoder_loan/JpegLoanDecodeTest.cpp'), str(root / 'lib/Memory/BuildScratch.cpp'),
             str(lib_obj), '-o', str(real_exe)]
subprocess.run(real_cmd, check=True)
subprocess.run([str(real_exe)] + fixtures, check=True)
