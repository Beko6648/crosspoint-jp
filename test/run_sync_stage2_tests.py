"""Compile firmware parser/transaction and compare parser with frozen reference."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('reference', root / 'test/test_sync_snapshot.py')
ref = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)
subprocess.run(['python3', str(root / 'test/generate_snapshot_validator.py'), '--check'], check=True)
samples = [json.dumps(ref.EXAMPLE, ensure_ascii=False)]
def add(doc):
    samples.append(json.dumps(doc, ensure_ascii=False, separators=(',', ':')))
def mutate(path, value):
    doc = copy.deepcopy(ref.EXAMPLE)
    target = doc
    for k in path[:-1]: target = target[k]
    target[path[-1]] = value
    add(doc)
def visit(node, path=()):
    if isinstance(node, dict):
        mutate(path + ('unknown',), 0)
        for key, value in node.items():
            doc = copy.deepcopy(ref.EXAMPLE); obj = doc
            for part in path: obj = obj[part]
            del obj[key]; add(doc)
            for bad in [None, True, False, -1, 0, 1, 1.5, 65536, 4294967296, '', 'bogus', [], {}]:
                mutate(path + (key,), bad)
            visit(value, path + (key,))
    elif isinstance(node, list):
        for i, item in enumerate(node): visit(item, path + (i,))
visit(ref.EXAMPLE)
for unit in ref.EXAMPLE['units']:
    doc = copy.deepcopy(ref.EXAMPLE); doc['units'] = {unit: doc['units'][unit]}; add(doc)
for value in ['あ' * 512, 'あ' * 513, '\x00', '\U0001f600' * 512]:
    mutate(('units','bookmarks','data',0,'summary'), value)
for value in ['あ' * 10, 'あ' * 11]:
    mutate(('units','readerSettings','data','vertical','font','sdFamilyName'), value)
doc = copy.deepcopy(ref.EXAMPLE); doc['units']['bookmarks']['data'] *= 2; add(doc)
samples += ['{"format":1,"format":2}', '{"format":1,"\\u0066ormat":2}', "{'format':1}", '{"a":01}', '{"a":NaN}', '{}junk', '\ufeff{}', '{"a":"\\ud800"}', '{"a":"\\udc00"}', '{"a":"\\u0000"}']
valid = json.dumps(ref.EXAMPLE, separators=(',', ':'))
samples += [valid.replace('"format":', '"format":"yomuka-book-snapshot","format":', 1),
            valid.replace('"format":', '"\\u0066ormat":"yomuka-book-snapshot","format":', 1),
            valid.replace('"format":', '/*comment*/"format":', 1),
            valid + ' ' * (65536 - len(valid)), valid + ' ' * (65537 - len(valid))]
expected = []
for text in samples:
    try:
        ref.parse(text.encode('utf-8')); expected.append(True)
    except (ValueError, ref.ValidationError): expected.append(False)
with tempfile.TemporaryDirectory(prefix='yomuka-stage2-') as temp:
    binary = Path(temp) / 'stage2'
    command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti',
               '-I' + str(root / 'test/sync_storage/stubs'), '-I' + str(root / 'src'), '-isystem', str(root / '.pio/libdeps/default/ArduinoJson/src')]
    command += ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-error=maybe-uninitialized'] if args.sanitize else ['-O2']
    command += [str(root / p) for p in ['src/sync/StorageIo.cpp','src/sync/SyncProgress.cpp','src/sync/SnapshotValidation.cpp','src/sync/ImportTransaction.cpp','test/sync_storage/test_sync_stage2.cpp']]
    command += ['-o', str(binary)]
    subprocess.run(command, check=True, timeout=180)
    subprocess.run([str(binary)], check=True, timeout=180)
    result = subprocess.run([str(binary),'parse'], input='\n'.join(samples) + '\n', capture_output=True, text=True, check=True, timeout=60)
    rows = result.stdout.splitlines()
    assert len(rows) == len(samples), result.stderr
    for i, (row, valid) in enumerate(zip(rows, expected)):
        status, unchanged = row.split(':')
        assert (int(status) == 0) == valid and unchanged == '1', (i, valid, row, samples[i])
    invalid_bytes = [b'\xc0\x80', b'\xed\xa0\x80', b'\xf4\x90\x80\x80', b'\xe3\x81', b'\x00', b'\xff']
    result = subprocess.run([str(binary), 'parse'], input=b'\n'.join(invalid_bytes) + b'\n', capture_output=True, check=True, timeout=60)
    assert result.stdout.splitlines() == [b'2:1'] * len(invalid_bytes), result.stdout
    print(f'PASS snapshot parser: {len(samples)} schema/reference cases')
    print(f'PASS UTF-8: {len(invalid_bytes)} malformed byte sequences')
