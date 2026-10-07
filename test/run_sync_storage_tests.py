"""Test production storage adapters against an SD failure/reboot model.

Uses the installed ArduinoJson header library. No download or device writes.
Run with --compiler g++ on Linux/WSL; --sanitize enables ASan/UBSan.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='g++')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
headers = root / '.pio/libdeps/default/ArduinoJson/src'
if not headers.is_dir():
    parser.error('Installed ArduinoJson headers missing; build default first.')
sources = ['src/sync/StorageIo.cpp', 'src/sync/ProgressStorage.cpp', 'src/sync/BookmarkStorage.cpp',
           'src/sync/SyncProgress.cpp', 'src/BookReaderSettings.cpp', 'src/ReadingHistoryStore.cpp',
           'test/sync_storage/test_sync_storage.cpp']
with tempfile.TemporaryDirectory(prefix='yomuka-sync-storage-') as temporary:
    binary = Path(temporary) / 'storage-test'
    command = [args.compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti',
               '-I' + str(root / 'test/sync_storage/stubs'), '-I' + str(root / 'src'), '-isystem', str(headers)]
    # GCC 13 warns inside ArduinoJson templates with sanitizer optimization; keep warnings visible.
    if args.sanitize:
        command += ['-Wno-error=maybe-uninitialized']
    command += ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined'] if args.sanitize else ['-O2']
    command += [str(root / s) for s in sources] + ['-o', str(binary)]
    subprocess.run(command, check=True, timeout=120)
    subprocess.run([str(binary)], check=True, timeout=60)
