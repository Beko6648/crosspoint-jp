#!/usr/bin/env python3
"""Run the hardware/reader host regressions on Linux (including WSL)."""
import argparse
import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/hardware-regression')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    env_tool = shutil.which('env')
    if os.name != 'posix' or not env_tool or not shutil.which('c++') or not shutil.which('cc'):
        parser.error('Run in Linux/WSL with Python 3 and C/C++ compilers (cc, c++) installed.')
    cases = [(name, ROOT / ('test/run_' + name + '_test.py'), []) for name in (
        'battery_diagnostics', 'freeink_stage1', 'x4c_foundation',
        'render_scheduling', 'web_exit', 'vertical_glyph_load')]
    # These older runners expect a zig-style driver followed by cc/c++.
    # env dispatches those standard compiler commands without a local wrapper.
    cases += [(name, ROOT / ('test/run_' + name + '_test.py'), ['--compiler', env_tool]) for name in (
        'chapter_input_lock', 'idle_image', 'chapter_incremental', 'chapter_exit')]
    sdk_host = ROOT / 'freeink-sdk/libs/display/FreeInkDisplay/test/host'
    cases += [('sdk_' + name, sdk_host / ('run_' + name + '.py'), []) for name in (
        'pro', 'uc8279', 'uc8253_power')]
    report = {'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'scope': 'host tests only; no hardware validation', 'results': []}
    environment = os.environ.copy()
    environment.update(PYTHONUTF8='1', PYTHONIOENCODING='utf-8')
    failed = False
    for name, script, extra in cases:
        started = time.monotonic()
        log = output / (name + '.log')
        print('RUN ' + name, flush=True)
        with log.open('w', encoding='utf-8') as stream:
            try:
                result = subprocess.run([sys.executable, str(script), *extra], cwd=ROOT,
                                        env=environment, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=300)
                code = result.returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                stream.write('\nRunner error: ' + str(error) + '\n')
                code = 1
        failed |= code != 0
        report['results'].append({'name': name, 'returncode': code,
                                  'seconds': round(time.monotonic() - started, 2), 'log': log.name})
        print(('PASS ' if code == 0 else 'FAIL ') + name, flush=True)
        (output / 'summary.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'{len(cases)} suites; ' + ('FAILED' if failed else 'PASSED'), flush=True)
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
