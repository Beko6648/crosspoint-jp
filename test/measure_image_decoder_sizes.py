#!/usr/bin/env python3
"""Measure target ABI sizes without flashing. Run after pio run -e default.

Pass the ESP32-C3 toolchain g++ and nm executables, not host tools. __LINUX__
selects standard headers instead of Arduino; the classes still contain the same
PNGIMAGE/JPEGIMAGE structs. The cross compiler supplies the actual 32-bit ABI.
Runtime IMEM logs provide the independent firmware-side sizeof confirmation.
"""
import argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--nm', required=True)
args = parser.parse_args()
out = root / 'build/image_cache'
out.mkdir(parents=True, exist_ok=True)
source = out / 'sizes.cpp'
source.write_text('''#include <PNGdec.h>
#include <JPEGDEC.h>
extern "C" {
char measured_png[sizeof(PNG)];
char measured_jpeg[sizeof(JPEGDEC)];
char alignment_png[alignof(PNG)];
char alignment_jpeg[alignof(JPEGDEC)];
}
''', encoding='utf-8')
obj = out / 'sizes.o'
subprocess.run([args.compiler, '-D__LINUX__', '-DPNG_MAX_BUFFERED_PIXELS=15424',
                '-I' + str(root / '.pio/libdeps/default/PNGdec/src'),
                '-I' + str(root / '.pio/libdeps/default/JPEGDEC/src'),
                '-c', str(source), '-o', str(obj)], check=True)
subprocess.run([args.nm, '-S', '--radix=d', str(obj)], check=True)
