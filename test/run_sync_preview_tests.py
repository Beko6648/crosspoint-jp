"""Host tests for the confirmation previews."""
import argparse
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
sources = ['SnapshotPreview']
with tempfile.TemporaryDirectory(prefix='yomuka-preview-') as temp:
    binary = Path(temp) / 'exchange'
    command = ['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',
               '-I' + str(root / 'test/sync_storage/stubs'), '-I' + str(root / 'src'), '-isystem', str(root / '.pio/libdeps/default/ArduinoJson/src')]
    command += ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-error=maybe-uninitialized'] if args.sanitize else ['-O2']
    command += [str(root / ('src/sync/' + s + '.cpp')) for s in sources]
    command += [str(root / p) for p in ['test/sync_storage/test_sync_preview.cpp']]
    command += ['-o', str(binary)]
    subprocess.run(command, check=True, timeout=180)
    subprocess.run([str(binary)], check=True, timeout=60)
